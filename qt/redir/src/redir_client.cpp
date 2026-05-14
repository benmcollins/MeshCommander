// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/redir_client.h"

#include <QCryptographicHash>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpSocket>

namespace qumesh::redir {

namespace {

QString describe(StartSessionStatus s)
{
    switch (s) {
    case StartSessionStatus::Success:
        return QStringLiteral("Success");
    case StartSessionStatus::Busy:
        return QStringLiteral("device is busy with another session");
    case StartSessionStatus::Unsupported:
        return QStringLiteral("device does not support this redirection protocol");
    case StartSessionStatus::UnknownError:
        return QStringLiteral("device reported an unknown error");
    }
    return QStringLiteral("unknown status %1").arg(static_cast<int>(s));
}

} // namespace

RedirectionClient::RedirectionClient(QObject *parent) : QObject(parent) {}

RedirectionClient::~RedirectionClient() = default;

void RedirectionClient::setCredentials(QString user, QString pass)
{
    m_user = std::move(user);
    m_pass = std::move(pass);
}

void RedirectionClient::connectTo(const QString &host, quint16 port)
{
    m_inbox.clear();
    m_lastError.clear();
    m_startStatus = StartSessionStatus::UnknownError;
    m_oemData.clear();
    m_pendingPeerCert = {};
    m_awaitingTrust = false;

    if (m_socket != nullptr) {
        m_socket->disconnect(this);
        m_socket->deleteLater();
        m_socket = nullptr;
        m_sslSocket = nullptr;
    }

    if (m_tls) {
        m_sslSocket = new QSslSocket(this);
        // AMT devices generally ship self-signed certs, so we can't
        // rely on CA-trust validation. Use VerifyNone and pin by
        // fingerprint in handleEncrypted() instead.
        QSslConfiguration cfg = m_sslSocket->sslConfiguration();
        cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_sslSocket->setSslConfiguration(cfg);
        m_socket = m_sslSocket;
        connect(m_sslSocket, &QSslSocket::encrypted, this,
                &RedirectionClient::handleEncrypted);
    } else {
        QTcpSocket *tcp = new QTcpSocket(this);
        m_socket = tcp;
        connect(tcp, &QTcpSocket::connected, this, &RedirectionClient::handleConnected);
    }
    connect(m_socket, &QAbstractSocket::readyRead, this,
            &RedirectionClient::handleReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this,
            &RedirectionClient::handleSocketError);

    setState(State::Connecting);
    if (m_tls) {
        m_sslSocket->connectToHostEncrypted(host, port);
    } else {
        m_socket->connectToHost(host, port);
    }
}

void RedirectionClient::disconnectFromHost()
{
    if (m_socket != nullptr) m_socket->disconnectFromHost();
    setState(State::Disconnected);
}

void RedirectionClient::trustPendingPeerCert()
{
    if (m_sslSocket == nullptr || !m_awaitingTrust) return;
    if (!m_pendingPeerCert.fingerprintSha256.isEmpty()
        && !m_trustedFingerprints.contains(m_pendingPeerCert.fingerprintSha256)) {
        m_trustedFingerprints.append(m_pendingPeerCert.fingerprintSha256);
    }
    m_awaitingTrust = false;
    // The TLS handshake is already complete — the only thing held back
    // was the redirection selector. Proceed as if we had just connected.
    handleConnected();
}

void RedirectionClient::handleConnected()
{
    const QByteArray selector = buildSelector(m_protocol);
    if (m_socket->write(selector) != selector.size()) {
        fail(QStringLiteral("could not write protocol selector: %1").arg(m_socket->errorString()));
        return;
    }
    setState(State::SelectorSent);
}

void RedirectionClient::handleReadyRead()
{
    const QByteArray chunk = m_socket->readAll();
    if (m_state == State::Authenticated) {
        emit rawBytes(chunk);
        return;
    }
    m_inbox.append(chunk);
    drainInbox();
}

qint64 RedirectionClient::writeRaw(const QByteArray &bytes)
{
    if (m_socket == nullptr) return -1;
    return m_socket->write(bytes);
}

void RedirectionClient::drainInbox()
{
    while (!m_inbox.isEmpty()) {
        const unsigned char tag = static_cast<unsigned char>(m_inbox.at(0));

        // KVM-only: 0x41 is the start-session reply for the 0x40 frame
        // we sent right after authentication. The reply is 8 bytes; any
        // bytes past those belong to the RFB protocol stream.
        if (m_state == State::KvmStarting && tag == 0x41) {
            if (m_inbox.size() < 8) return;
            QByteArray leftover = m_inbox.mid(8);
            m_inbox.clear();
            setState(State::Authenticated);
            emit authenticated();
            if (!leftover.isEmpty()) emit rawBytes(leftover);
            return;
        }

        if (tag == 0x11) {
            StartSessionReply reply;
            int consumed = 0;
            if (!tryParseStartSessionReply(m_inbox, &reply, &consumed)) return;
            m_inbox.remove(0, consumed);
            m_startStatus = reply.status;
            m_oemData = reply.oemData;
            if (reply.status != StartSessionStatus::Success) {
                fail(describe(reply.status));
                return;
            }
            emit sessionOpened();
            setState(State::SessionOpened);

            if (!m_user.isEmpty()) {
                if (m_socket->write(buildAuthQuery()) < 0) {
                    fail(m_socket->errorString());
                    return;
                }
                setState(State::AuthQuerying);
            }
            continue;
        }

        if (tag == 0x14) {
            AuthReply reply;
            int consumed = 0;
            if (!tryParseAuthReply(m_inbox, &reply, &consumed)) return;
            m_inbox.remove(0, consumed);

            switch (m_state) {
            case State::AuthQuerying: {
                if (!reply.caps.hasDigestWithQop) {
                    fail(QStringLiteral("device does not offer DigestWithQop auth"));
                    return;
                }
                if (m_socket->write(buildDigestQuery(m_user, m_authUri)) < 0) {
                    fail(m_socket->errorString());
                    return;
                }
                setState(State::AuthChallenging);
                break;
            }
            case State::AuthChallenging: {
                if (reply.status != 1 || reply.realm.isEmpty() || reply.nonce.isEmpty()) {
                    fail(QStringLiteral("authentication rejected by device"));
                    return;
                }
                const QString qop = reply.qop.isEmpty() ? QStringLiteral("auth") : reply.qop;
                const QString nc = QStringLiteral("00000001");
                const QString cnonce = makeClientNonce();
                const QString response = computeDigest(m_user, reply.realm, m_pass,
                                                       reply.nonce, nc, cnonce, qop,
                                                       m_authUri);
                if (m_socket->write(buildDigestResponse(m_user, reply.realm, reply.nonce,
                                                       m_authUri, cnonce, nc, response,
                                                       qop)) < 0) {
                    fail(m_socket->errorString());
                    return;
                }
                setState(State::AuthResponding);
                break;
            }
            case State::AuthResponding: {
                if (reply.status != 0) {
                    fail(QStringLiteral("authentication failed (status %1)").arg(reply.status));
                    return;
                }
                // KVM needs a redir-level 0x40 → 0x41 round-trip before
                // the firmware will push the RFB version banner. SOL and
                // IDE-R do not — they go straight to Authenticated and
                // their session class drives any further negotiation.
                if (m_protocol == Protocol::Kvm) {
                    static const unsigned char kKvmStart[8] = {
                        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    };
                    if (m_socket->write(reinterpret_cast<const char *>(kKvmStart),
                                        sizeof(kKvmStart)) < 0) {
                        fail(m_socket->errorString());
                        return;
                    }
                    setState(State::KvmStarting);
                    continue;
                }
                setState(State::Authenticated);
                emit authenticated();
                // Any bytes still buffered after the 0x14 success belong
                // to the application protocol — IDE-R's session opener,
                // for example, can arrive in the same TCP read. Hand them
                // off via rawBytes and stop parsing as auth frames.
                if (!m_inbox.isEmpty()) {
                    const QByteArray leftover = m_inbox;
                    m_inbox.clear();
                    emit rawBytes(leftover);
                }
                return;
            }
            default:
                fail(QStringLiteral("unexpected 0x14 in state %1").arg(static_cast<int>(m_state)));
                return;
            }
            continue;
        }

        fail(QStringLiteral("unexpected frame 0x%1 in state %2")
                 .arg(tag, 2, 16, QLatin1Char('0'))
                 .arg(static_cast<int>(m_state)));
        return;
    }
}

void RedirectionClient::handleSocketError()
{
    fail(m_socket->errorString());
}

void RedirectionClient::handleEncrypted()
{
    Q_ASSERT(m_sslSocket != nullptr);
    const QSslCertificate cert = m_sslSocket->peerCertificate();
    if (cert.isNull()) {
        fail(QStringLiteral("TLS peer presented no certificate"));
        return;
    }
    const PeerCertSummary summary = summarize(cert);
    if (m_trustedFingerprints.contains(summary.fingerprintSha256)) {
        // Fingerprint pinned — proceed with the redirection selector.
        handleConnected();
        return;
    }
    // Hold the selector and ask the consumer to confirm.
    m_pendingPeerCert = summary;
    m_awaitingTrust = true;
    emit trustPromptRequired(summary);
}

PeerCertSummary RedirectionClient::summarize(const QSslCertificate &cert)
{
    PeerCertSummary s;
    const auto subjects = cert.subjectInfo(QSslCertificate::CommonName);
    const auto issuers  = cert.issuerInfo(QSslCertificate::CommonName);
    if (!subjects.isEmpty()) s.subject = subjects.first();
    if (!issuers.isEmpty())  s.issuer  = issuers.first();

    s.der = cert.toDer();
    const QByteArray hash = QCryptographicHash::hash(s.der, QCryptographicHash::Sha256);
    QString fp;
    fp.reserve(hash.size() * 3);
    for (int i = 0; i < hash.size(); ++i) {
        if (i > 0) fp += QLatin1Char(':');
        fp += QString::asprintf("%02X", static_cast<quint8>(hash.at(i)));
    }
    s.fingerprintSha256 = fp;
    s.notBefore = cert.effectiveDate().toString(Qt::ISODate);
    s.notAfter  = cert.expiryDate().toString(Qt::ISODate);
    return s;
}

void RedirectionClient::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void RedirectionClient::fail(QString error)
{
    m_lastError = std::move(error);
    setState(State::Failed);
    emit failed(m_lastError);
}

} // namespace qumesh::redir
