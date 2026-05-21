// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh/ssh_session.h"
#include "ssh_poll.h"
#include "ssh_session_worker.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <memory>
#include <QtCore/qcoreapplication.h>

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

/// Resolve `hostOrIp` to an IP literal, cancellable via `stopFlag`.
/// Returns the IP on success or empty on cancel / timeout / failure.
/// If `hostOrIp` is already an IP literal it's returned unchanged
/// without touching the resolver.
///
/// Pre-#290 SshSession called `ssh_connect()` with whatever string
/// the caller passed, which libssh handed to `getaddrinfo()` —
/// uninterruptible at the OS level. A DNS-blackhole host name could
/// hold up `requestStop()` for the full resolver timeout (often
/// 30 s+). Here we run `QHostInfo::fromName()` on a temporary worker
/// thread and poll the cancel flag every 50 ms. If cancellation or
/// timeout wins we abandon the lookup thread (it self-deletes when
/// the resolver eventually returns).
///
/// The result is exchanged via a `std::shared_ptr<std::atomic<...>>`
/// co-owned by the caller and the lookup thread, so whichever side
/// finishes last reclaims the heap holder — no leak on cancel. See
/// #393.
QString resolveHostCancellable(const QString &hostOrIp,
                                int timeoutMs,
                                std::atomic<bool> &stopFlag,
                                bool *aborted)
{
    *aborted = false;
    const QHostAddress probe(hostOrIp);
    if (!probe.isNull()) return hostOrIp;

    // Sentinel value distinct from any valid heap pointer: tagged
    // address `1` is never returned by `operator new` (alignment
    // requires at least 2 on every platform we ship to). Using a
    // non-null sentinel lets us keep `nullptr` meaning "worker has
    // not yet stored a result"; the sentinel means "caller bailed
    // before consuming, worker should clean up the result".
    static QHostInfo *const kAbandoned =
        reinterpret_cast<QHostInfo *>(static_cast<std::uintptr_t>(1));

    // Co-owned by caller (this stack frame) and the lookup lambda.
    // Last to drop the shared_ptr frees the atomic holder; the
    // exchange dance below decides which side owns the QHostInfo*
    // (if any was produced).
    auto holder = std::make_shared<std::atomic<QHostInfo *>>(nullptr);

    QThread *t = QThread::create([host = hostOrIp, holder]() {
        // fromName blocks here. There's no cross-platform way to
        // interrupt it from outside.
        QHostInfo *info = new QHostInfo(QHostInfo::fromName(host));
        // Atomically publish the result. If the caller already
        // moved the holder to the `kAbandoned` sentinel, they have
        // walked away and we own the heap-allocated QHostInfo —
        // delete it. Otherwise the caller's load will pick up our
        // result and free it.
        QHostInfo *prev = holder->exchange(info, std::memory_order_acq_rel);
        if (prev == kAbandoned) delete info;
    });
    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();

    // Helper: caller's bail-out path. Move the holder to `kAbandoned`
    // and, if the worker had already published a result before we
    // got here, take ownership of that result and free it (the
    // worker is done with it — its exchange returned the previous
    // `nullptr`, so it has no reference left). After this returns
    // the caller drops its shared_ptr; the worker drops its own
    // shared_ptr when its lambda exits; the holder is freed by
    // whichever drop is last.
    auto abandon = [&holder]() {
        QHostInfo *prev =
            holder->exchange(kAbandoned, std::memory_order_acq_rel);
        if (prev != nullptr && prev != kAbandoned) delete prev;
    };

    QElapsedTimer deadline;
    deadline.start();
    constexpr int kTickMs = 50;
    while (true) {
        QHostInfo *cur = holder->load(std::memory_order_acquire);
        if (cur != nullptr && cur != kAbandoned) {
            // Worker delivered. Take ownership and consume. No need
            // to mutate the holder — the worker is done with it
            // (its exchange returned the previous `nullptr`, so it
            // has no reference left). The atomic stays alive as
            // long as either party still holds a shared_ptr; it's
            // freed when both have dropped it.
            const QHostInfo r = *cur;
            delete cur;
            if (r.error() != QHostInfo::NoError || r.addresses().isEmpty())
                return {};
            return r.addresses().first().toString();
        }
        if (stopFlag.load(std::memory_order_acquire)) {
            abandon();
            *aborted = true;
            return {};
        }
        if (deadline.elapsed() >= timeoutMs) {
            abandon();
            *aborted = false; // timeout, not cancel
            return {};
        }
        QThread::msleep(kTickMs);
    }
}

} // namespace

SshSessionWorker::SshSessionWorker(QObject *parent) : QObject(parent) {}

int SshSessionWorker::waitForLibssh(const std::function<int()> &call, int timeoutMs)
{
    constexpr int kTickMs = 50;
    if (timeoutMs <= 0) timeoutMs = qMax(1, m_params.connectTimeoutMs);
    QElapsedTimer deadline;
    deadline.start();
    while (true) {
        if (m_stopRequested.load(std::memory_order_acquire))
            return SSH_ABORTED;
        const int rc = call();
        if (rc != SSH_AGAIN && rc != SSH_AUTH_AGAIN)
            return rc;
        if (m_session == nullptr) return SSH_ABORTED;
        if (deadline.elapsed() >= timeoutMs)
            return SSH_ERROR;
        const int flags = ssh_get_poll_flags(m_session);
        const socket_t fd = ssh_get_fd(m_session);
        detail::waitSessionReady(fd, flags, kTickMs);
    }
}

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
    QMutexLocker lock(&m_pendingHostKeyMutex);
    return m_pendingHostKeyFingerprint;
}
QString SshSessionWorker::pendingKeyType() const
{
    QMutexLocker lock(&m_pendingHostKeyMutex);
    return m_pendingHostKeyType;
}

void SshSessionWorker::open(SshSession::Params p)
{
    m_params = std::move(p);
    emit stateChanged(SshSession::Connecting);

    // `m_session` is worker-thread-local: open()/close()/
    // openForwardChannel() all run here (close() is QueuedConnection
    // from SshSession; openForwardChannel is invokeMethod'd from
    // SshTunnel). No mutex needed on this side. See #272.
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
    // libssh's blocking-mode default starves the worker's event loop
    // during ssh_connect / ssh_userauth_* — so a destructor that wants
    // to land on this thread can't, until libssh's full timeout
    // expires. Switching to non-blocking + a short-tick poll loop
    // (see `waitForLibssh`) bounds the worker's responsiveness at
    // ~50 ms regardless of what libssh is doing. See #233.
    ssh_set_blocking(m_session, 0);

    // Resolve hostname → IP on a cancellable thread before handing to
    // libssh. libssh's internal getaddrinfo() is uninterruptible at
    // the OS level — a DNS-blackhole hostname would otherwise stall
    // requestStop() for the full system resolver timeout. See #290.
    bool dnsAborted = false;
    const QString resolved = resolveHostCancellable(
        m_params.host, m_params.connectTimeoutMs, m_stopRequested,
        &dnsAborted);
    if (dnsAborted) return; // caller is tearing us down
    if (resolved.isEmpty()) {
        fail(QStringLiteral("Could not resolve host: %1").arg(m_params.host));
        return;
    }

    const QByteArray hostBytes = resolved.toUtf8();
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

    // SSH transport-level compression. libssh defaults to "none"; when
    // the user opts in, prefer the OpenSSH "delayed" variant (negotiated
    // post-auth) and fall back to plain zlib then no-compression. The
    // bandwidth win matters for KVM / IDER over slow uplinks; CPU cost
    // is small for typical jump-host hardware.
    if (m_params.compression) {
        if (ssh_options_set(m_session, SSH_OPTIONS_COMPRESSION,
                            "zlib@openssh.com,zlib,none") != SSH_OK) {
            fail(sshErrorString(m_session,
                    QStringLiteral("ssh_options_set(COMPRESSION) failed")));
            return;
        }
    }

    const int connectRc = waitForLibssh([this]{ return ssh_connect(m_session); });
    if (connectRc == SSH_ABORTED) {
        // Caller is tearing us down; skip the user-facing error path,
        // just clean up the session. close() (queued from the dtor)
        // will fire next and emit Disconnected.
        return;
    }
    if (connectRc != SSH_OK) {
        fail(sshErrorString(m_session, QStringLiteral("ssh_connect failed")));
        return;
    }

    if (!verifyHostKey()) return; // either prompted user or failed.
    proceedWithAuth();
}

void SshSessionWorker::resumeAfterHostKeyTrust()
{
    if (!m_awaitingHostKeyTrust) return;
    QString fp;
    {
        QMutexLocker lock(&m_pendingHostKeyMutex);
        fp = m_pendingHostKeyFingerprint;
        m_pendingHostKeyFingerprint.clear();
        m_pendingHostKeyType.clear();
    }
    m_awaitingHostKeyTrust = false;
    emit hostKeyTrusted(fp);
    proceedWithAuth();
}

void SshSessionWorker::close()
{
    if (m_session != nullptr) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
    m_awaitingHostKeyTrust = false;
    {
        QMutexLocker lock(&m_pendingHostKeyMutex);
        m_pendingHostKeyFingerprint.clear();
        m_pendingHostKeyType.clear();
    }
    emit stateChanged(SshSession::Disconnected);
}

ssh_channel SshSessionWorker::openForwardChannel(const QString &remoteHost,
                                                  quint16 remotePort)
{
    // Worker-thread-only: SshTunnel uses QMetaObject::invokeMethod
    // against the worker thread to land here, so `m_session` access
    // is single-threaded.
    if (m_session == nullptr) return nullptr;
    ssh_channel ch = ssh_channel_new(m_session);
    if (ch == nullptr) return nullptr;
    const QByteArray rhost = remoteHost.toUtf8();
    const int rc = waitForLibssh([ch, &rhost, remotePort]{
        return ssh_channel_open_forward(ch, rhost.constData(),
                                         remotePort, "127.0.0.1", 0);
    });
    if (rc != SSH_OK) {
        ssh_channel_free(ch);
        return nullptr;
    }
    return ch;
}

void SshSessionWorker::fail(const QString &msg)
{
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

    if (m_params.trustedHostKeyFingerprints.contains(fp)) {
        emit hostKeyVerifiedByPin(fp);
        return true;
    }

    QString keyTypeStr = keyTypeName(keyType);
    {
        // Cross-thread visibility for the QML-side `pendingFingerprint`
        // / `pendingKeyType` readers.
        QMutexLocker lock(&m_pendingHostKeyMutex);
        m_pendingHostKeyFingerprint = fp;
        m_pendingHostKeyType = keyTypeStr;
    }
    m_awaitingHostKeyTrust = true;
    emit stateChanged(SshSession::NeedsHostKeyTrust);
    emit hostKeyPromptRequired(fp, keyTypeStr);
    return false;
}

void SshSessionWorker::proceedWithAuth()
{
    emit stateChanged(SshSession::Authenticating);

    int rc = SSH_AUTH_DENIED;
    switch (m_params.authMode) {
    case SshSession::AuthPassword: {
        const QByteArray pw = m_params.password.toUtf8();
        rc = waitForLibssh([this, &pw]{
            return ssh_userauth_password(m_session, nullptr, pw.constData());
        });
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
        rc = waitForLibssh([this, pkey]{
            return ssh_userauth_publickey(m_session, nullptr, pkey);
        });
        ssh_key_free(pkey);
        break;
    }
    }

    if (rc == SSH_ABORTED) {
        // Same teardown path as the connect aborted case.
        return;
    }
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
    // Give any active SshTunnel a chance to stop and join its pump
    // thread before we tear down the worker. Connected handlers are
    // DirectConnection on the same (caller's) thread, so this returns
    // only once every pump has wound down. Skipping this lets the pump
    // race ahead and BlockingQueuedConnection-invoke a freed worker.
    emit aboutToDestroy();

    if (d->worker != nullptr) {
        // Cancel any in-flight libssh call at the next ~50 ms poll tick.
        // requestStop() is a plain atomic store, so it doesn't need the
        // worker's event loop — which is exactly why we can't go back
        // to the old BlockingQueuedConnection here (the worker can be
        // mid-`ssh_connect` / `ssh_userauth_*` with its event loop
        // starved, and the blocking invoke would freeze for up to
        // libssh's full `connectTimeoutMs`). See #233.
        d->worker->requestStop();
        // Post the actual close() on the worker thread; the poll loop
        // above exits first, then the worker's event loop picks this
        // up. QueuedConnection so we don't wait.
        QMetaObject::invokeMethod(d->worker, "close", Qt::QueuedConnection);
    }
    d->thread.quit();
    // Bounded wait: the worker's libssh polls tick every 50 ms, so
    // 5000 ms is two orders of magnitude more than enough. If we ever
    // do hit it, force-terminate as last resort — this destructor
    // existing to fix a freeze must not regress into one itself.
    if (!d->thread.wait(5000)) {
        d->thread.terminate();
        d->thread.wait();
    }
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
