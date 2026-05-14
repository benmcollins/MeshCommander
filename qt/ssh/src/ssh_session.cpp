// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh/ssh_session.h"
#include "ssh_session_worker.h"

#include <QFile>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>
#include <QtCore/qcoreapplication.h>

Q_LOGGING_CATEGORY(qumeshSshSession, "qumesh.ssh.session", QtWarningMsg)

namespace qumesh::ssh {

namespace {

QString sshErrorString(ssh_session s, const QString &fallback)
{
    if (s == nullptr) return fallback;
    const char *raw = ssh_get_error(s);
    if (raw == nullptr || *raw == '\0') return fallback;
    return QString::fromUtf8(raw);
}

QString hexFingerprint(const unsigned char *hash, size_t len)
{
    QString out;
    out.reserve(static_cast<int>(len) * 3);
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) out += QLatin1Char(':');
        out += QString::asprintf("%02X", hash[i]);
    }
    return out;
}

QString keyTypeName(ssh_keytypes_e t)
{
    switch (t) {
    case SSH_KEYTYPE_RSA:     return QStringLiteral("RSA");
    case SSH_KEYTYPE_ECDSA_P256: return QStringLiteral("ECDSA P-256");
    case SSH_KEYTYPE_ECDSA_P384: return QStringLiteral("ECDSA P-384");
    case SSH_KEYTYPE_ECDSA_P521: return QStringLiteral("ECDSA P-521");
    case SSH_KEYTYPE_ED25519: return QStringLiteral("Ed25519");
    case SSH_KEYTYPE_DSS:     return QStringLiteral("DSA");
    default:                  return QStringLiteral("(unknown)");
    }
}

} // namespace

SshSessionWorker::SshSessionWorker(QObject *parent) : QObject(parent) {}

SshSessionWorker::~SshSessionWorker()
{
    if (m_session != nullptr) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
}

QString SshSessionWorker::pendingFingerprint() const
{
    QMutexLocker lock(&const_cast<QMutex &>(m_sessionMutex));
    return m_pendingHostKeyFingerprint;
}
QString SshSessionWorker::pendingKeyType() const
{
    QMutexLocker lock(&const_cast<QMutex &>(m_sessionMutex));
    return m_pendingHostKeyType;
}

void SshSessionWorker::open(SshSession::Params p)
{
    m_params = std::move(p);
    qCWarning(qumeshSshSession)
        << "SshSession::open host=" << m_params.host
        << "port=" << m_params.port
        << "user=" << m_params.user
        << "authMode=" << int(m_params.authMode)
        << "trustedHostKeys=" << m_params.trustedHostKeyFingerprints.size();
    emit stateChanged(SshSession::Connecting);

    QMutexLocker lock(&m_sessionMutex);
    if (m_session != nullptr) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }

    m_session = ssh_new();
    if (m_session == nullptr) {
        fail(QStringLiteral("ssh_new() failed"));
        return;
    }

    const QByteArray hostBytes = m_params.host.toUtf8();
    const QByteArray userBytes = m_params.user.toUtf8();
    const unsigned int portU = m_params.port;
    const long timeoutSec = qMax(1L, long(m_params.connectTimeoutMs / 1000));

    if (ssh_options_set(m_session, SSH_OPTIONS_HOST,
                        hostBytes.constData()) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_PORT, &portU) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_USER,
                           userBytes.constData()) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT,
                           &timeoutSec) != SSH_OK) {
        fail(sshErrorString(m_session, QStringLiteral("ssh_options_set failed")));
        return;
    }

    qCWarning(qumeshSshSession) << "calling ssh_connect…";
    const int crc = ssh_connect(m_session);
    if (crc != SSH_OK) {
        qCWarning(qumeshSshSession) << "ssh_connect returned" << crc;
        fail(sshErrorString(m_session, QStringLiteral("ssh_connect failed")));
        return;
    }
    qCWarning(qumeshSshSession) << "ssh_connect OK; verifying host key…";

    if (!verifyHostKey()) return; // either prompted user or failed.
    proceedWithAuth();
}

void SshSessionWorker::resumeAfterHostKeyTrust()
{
    if (!m_awaitingHostKeyTrust) return;
    QMutexLocker lock(&m_sessionMutex);
    const QString fp = m_pendingHostKeyFingerprint;
    m_awaitingHostKeyTrust = false;
    m_pendingHostKeyFingerprint.clear();
    m_pendingHostKeyType.clear();
    lock.unlock();
    emit hostKeyTrusted(fp);
    proceedWithAuth();
}

void SshSessionWorker::close()
{
    QMutexLocker lock(&m_sessionMutex);
    if (m_session != nullptr) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
    m_awaitingHostKeyTrust = false;
    m_pendingHostKeyFingerprint.clear();
    m_pendingHostKeyType.clear();
    lock.unlock();
    emit stateChanged(SshSession::Disconnected);
}

ssh_channel SshSessionWorker::openForwardChannel(const QString &remoteHost,
                                                  quint16 remotePort)
{
    QMutexLocker lock(&m_sessionMutex);
    if (m_session == nullptr) return nullptr;
    ssh_channel ch = ssh_channel_new(m_session);
    if (ch == nullptr) return nullptr;
    const QByteArray rhost = remoteHost.toUtf8();
    const int rc = ssh_channel_open_forward(ch, rhost.constData(),
                                             remotePort, "127.0.0.1", 0);
    if (rc != SSH_OK) {
        ssh_channel_free(ch);
        return nullptr;
    }
    return ch;
}

void SshSessionWorker::fail(const QString &msg)
{
    qCWarning(qumeshSshSession) << "FAIL:" << msg;
    if (m_session != nullptr) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
    emit errorOccurred(msg);
    emit stateChanged(SshSession::Failed);
}

bool SshSessionWorker::verifyHostKey()
{
    ssh_key key = nullptr;
    if (ssh_get_server_publickey(m_session, &key) != SSH_OK || key == nullptr) {
        fail(QStringLiteral("Failed to read remote host key"));
        return false;
    }

    unsigned char *hash = nullptr;
    size_t hashLen = 0;
    const int rc = ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256,
                                           &hash, &hashLen);
    const ssh_keytypes_e keyType = ssh_key_type(key);
    ssh_key_free(key);
    if (rc != SSH_OK || hash == nullptr) {
        fail(QStringLiteral("Failed to compute host key fingerprint"));
        return false;
    }
    const QString fp = hexFingerprint(hash, hashLen);
    ssh_clean_pubkey_hash(&hash);

    qCWarning(qumeshSshSession) << "host key fingerprint:" << fp
                                << "type:" << keyTypeName(keyType)
                                << "(in trusted list?" << m_params.trustedHostKeyFingerprints.contains(fp) << ")";

    if (m_params.trustedHostKeyFingerprints.contains(fp)) {
        emit hostKeyVerifiedByPin(fp);
        return true;
    }

    m_pendingHostKeyFingerprint = fp;
    m_pendingHostKeyType = keyTypeName(keyType);
    m_awaitingHostKeyTrust = true;
    emit stateChanged(SshSession::NeedsHostKeyTrust);
    emit hostKeyPromptRequired(fp, m_pendingHostKeyType);
    return false;
}

void SshSessionWorker::proceedWithAuth()
{
    qCWarning(qumeshSshSession) << "proceedWithAuth: mode =" << int(m_params.authMode);
    emit stateChanged(SshSession::Authenticating);

    int rc = SSH_AUTH_DENIED;
    switch (m_params.authMode) {
    case SshSession::AuthPassword: {
        const QByteArray pw = m_params.password.toUtf8();
        rc = ssh_userauth_password(m_session, nullptr, pw.constData());
        break;
    }
    case SshSession::AuthKey: {
        if (m_params.privateKeyPath.isEmpty()) {
            fail(QStringLiteral("Private key path is empty"));
            return;
        }
        if (!QFile::exists(m_params.privateKeyPath)) {
            fail(QStringLiteral("Private key file does not exist: %1")
                     .arg(m_params.privateKeyPath));
            return;
        }
        ssh_key pkey = nullptr;
        const QByteArray pathBytes = m_params.privateKeyPath.toUtf8();
        const QByteArray passBytes = m_params.privateKeyPassphrase.toUtf8();
        const int prc = ssh_pki_import_privkey_file(
            pathBytes.constData(),
            passBytes.isEmpty() ? nullptr : passBytes.constData(),
            nullptr, nullptr, &pkey);
        if (prc != SSH_OK || pkey == nullptr) {
            fail(QStringLiteral("Failed to read private key (wrong passphrase?)"));
            return;
        }
        rc = ssh_userauth_publickey(m_session, nullptr, pkey);
        ssh_key_free(pkey);
        break;
    }
    }

    qCWarning(qumeshSshSession) << "ssh_userauth_* returned" << rc
                                << "(SUCCESS=0 DENIED=1 PARTIAL=2 INFO=3 AGAIN=4 ERROR=-1)";
    if (rc == SSH_AUTH_SUCCESS) {
        emit stateChanged(SshSession::Connected);
        return;
    }
    if (rc == SSH_AUTH_DENIED) {
        fail(QStringLiteral("SSH authentication denied"));
        return;
    }
    fail(sshErrorString(m_session, QStringLiteral("SSH authentication failed")));
}

struct SshSession::Private
{
    QThread thread;
    SshSessionWorker *worker = nullptr;
    SshSession::State state = SshSession::Disconnected;
    QString lastError;
};

SshSession::SshSession(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    qRegisterMetaType<SshSession::State>();
    qRegisterMetaType<SshSession::Params>("qumesh::ssh::SshSession::Params");

    d->worker = new SshSessionWorker();
    d->worker->moveToThread(&d->thread);
    connect(&d->thread, &QThread::finished, d->worker, &QObject::deleteLater);

    connect(d->worker, &SshSessionWorker::stateChanged, this,
            [this](SshSession::State s) {
                d->state = s;
                emit stateChanged(s);
            });
    connect(d->worker, &SshSessionWorker::errorOccurred, this,
            [this](const QString &e) {
                d->lastError = e;
                emit errorOccurred(e);
            });
    connect(d->worker, &SshSessionWorker::hostKeyPromptRequired, this,
            &SshSession::hostKeyPromptRequired);
    connect(d->worker, &SshSessionWorker::hostKeyVerifiedByPin, this,
            &SshSession::hostKeyVerifiedByPin);
    connect(d->worker, &SshSessionWorker::hostKeyTrusted, this,
            &SshSession::hostKeyTrusted);

    d->thread.start();
}

SshSession::~SshSession()
{
    if (d->worker != nullptr) {
        QMetaObject::invokeMethod(d->worker, "close", Qt::BlockingQueuedConnection);
    }
    d->thread.quit();
    d->thread.wait();
}

void SshSession::open(Params params)
{
    QMetaObject::invokeMethod(d->worker, "open", Qt::QueuedConnection,
                              Q_ARG(qumesh::ssh::SshSession::Params, params));
}

void SshSession::close()
{
    QMetaObject::invokeMethod(d->worker, "close", Qt::QueuedConnection);
}

void SshSession::trustPendingHostKey()
{
    QMetaObject::invokeMethod(d->worker, "resumeAfterHostKeyTrust",
                              Qt::QueuedConnection);
}

SshSession::State SshSession::state() const { return d->state; }
QString SshSession::lastError() const { return d->lastError; }
QString SshSession::pendingHostKeyFingerprint() const
{
    return d->worker->pendingFingerprint();
}
QString SshSession::pendingHostKeyType() const
{
    return d->worker->pendingKeyType();
}

SshSessionWorker *SshSession::worker() const { return d->worker; }

} // namespace qumesh::ssh
