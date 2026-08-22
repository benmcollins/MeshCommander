// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/redir_client.h"

#include <QCryptographicHash>
#include <QLoggingCategory>
#include <QPointer>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpSocket>

#ifdef Q_OS_WIN
#  include <winsock2.h>
#else
#  include <unistd.h>
#endif

// Three-arg form on purpose: `Q_LOGGING_CATEGORY`'s default severity
// is QtDebugMsg, so the two-arg form would ship every qCDebug below to
// every user's stderr with no environment variable set. Qt only
// hard-wires debug off for categories literally named "qt"/"qt.*".
// QtInfoMsg keeps warnings and the handful of once-per-session info
// lines on, and holds the phase-by-phase transcript behind
// QT_LOGGING_RULES='qumesh.*=true'. See #438.
Q_LOGGING_CATEGORY(lcRedirClient, "qumesh.redir.client", QtInfoMsg)

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

const char *stateName(RedirectionClient::State s)
{
    switch (s) {
    case RedirectionClient::State::Disconnected:   return "Disconnected";
    case RedirectionClient::State::Connecting:     return "Connecting";
    case RedirectionClient::State::SelectorSent:   return "SelectorSent";
    case RedirectionClient::State::SessionOpened:  return "SessionOpened";
    case RedirectionClient::State::AuthQuerying:   return "AuthQuerying";
    case RedirectionClient::State::AuthChallenging:return "AuthChallenging";
    case RedirectionClient::State::AuthResponding: return "AuthResponding";
    case RedirectionClient::State::KvmStarting:    return "KvmStarting";
    case RedirectionClient::State::Authenticated:  return "Authenticated";
    case RedirectionClient::State::Failed:         return "Failed";
    }
    return "?";
}

const char *protocolName(Protocol p)
{
    switch (p) {
    case Protocol::Sol:  return "SOL";
    case Protocol::Kvm:  return "KVM";
    case Protocol::Ider: return "IDE-R";
    }
    return "?";
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
    if (m_tunnelOpener) {
        // Tunnel open is async — show the user "Connecting…" up front
        // and let the callback either drive the rest of the connect or
        // surface the failure. The UI thread never blocks on libssh.
        setState(State::Connecting);
        QPointer<RedirectionClient> self(this);
        m_tunnelOpener(host, port,
                       [self, host, port](qintptr fd, QString error) {
            if (self.isNull()) {
                // Caller went away while the tunnel was opening. Close
                // the orphan fd so the pump on the other end of the
                // socketpair sees EOF and exits cleanly.
                if (fd >= 0) {
#ifdef Q_OS_WIN
                    ::closesocket(static_cast<SOCKET>(fd));
#else
                    ::close(static_cast<int>(fd));
#endif
                }
                return;
            }
            if (fd < 0) {
                qCWarning(lcRedirClient) << "SSH tunnel open failed:" << error;
                self->fail(error.isEmpty()
                               ? QStringLiteral("SSH tunnel could not be opened to %1:%2")
                                     .arg(host).arg(port)
                               : error);
                return;
            }
            self->connectViaSocketDescriptor(fd);
        });
        return;
    }

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

    // Never log m_pass. README.md draws exactly this line for users
    // deciding whether a transcript is safe to paste publicly.
    qCInfo(lcRedirClient) << "opening" << protocolName(m_protocol)
                           << "redirection to" << host << "port" << port
                           << (m_tls ? "TLS" : "plaintext") << "as user"
                           << (m_user.isEmpty() ? QStringLiteral("(none)") : m_user);
    setState(State::Connecting);
    if (m_tls) {
        m_sslSocket->connectToHostEncrypted(host, port);
    } else {
        m_socket->connectToHost(host, port);
    }
}

void RedirectionClient::connectViaSocketDescriptor(qintptr fd)
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
        QSslConfiguration cfg = m_sslSocket->sslConfiguration();
        cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_sslSocket->setSslConfiguration(cfg);
        m_socket = m_sslSocket;
        connect(m_sslSocket, &QSslSocket::encrypted, this,
                &RedirectionClient::handleEncrypted);
    } else {
        QTcpSocket *tcp = new QTcpSocket(this);
        m_socket = tcp;
    }
    connect(m_socket, &QAbstractSocket::readyRead, this,
            &RedirectionClient::handleReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this,
            &RedirectionClient::handleSocketError);

    if (!m_socket->setSocketDescriptor(fd, QAbstractSocket::ConnectedState)) {
        fail(QStringLiteral("setSocketDescriptor failed: %1").arg(m_socket->errorString()));
        return;
    }

    setState(State::Connecting);
    if (m_tls) {
        // SSH tunnel terminates at the AMT side, but TLS still
        // terminates locally; do the handshake on the adopted fd.
        m_sslSocket->startClientEncryption();
    } else {
        // For plain TCP-over-SSH-tunnel we never saw a `connected`
        // signal because the socket was already in `ConnectedState`
        // before we attached the slot. Drive `handleConnected` manually
        // on the next event loop tick so the selector + state machine
        // start.
        QMetaObject::invokeMethod(this, &RedirectionClient::handleConnected,
                                   Qt::QueuedConnection);
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
            // Neither we nor the reference decode the 0x41 body; eight
            // bytes of session-open ack is protocol metadata, and the
            // only way anyone learns what a given firmware puts there.
            qCDebug(lcRedirClient) << "0x41 KVM start reply" << m_inbox.left(8).toHex(' ')
                                    << "-" << leftover.size()
                                    << "byte(s) of RFB stream follow";
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
            // oemData is opaque vendor bytes — size only, never content.
            qCDebug(lcRedirClient) << "0x11 StartSessionReply status"
                                    << static_cast<int>(reply.status)
                                    << describe(reply.status) << "- oemData"
                                    << reply.oemData.size() << "bytes";
            if (reply.status != StartSessionStatus::Success) {
                // BUSY / UNSUPPORTED reached only lastError before this;
                // "device is busy" is a #433 failure mode.
                qCWarning(lcRedirClient) << "device refused the redirection session:"
                                          << describe(reply.status);
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
                qCDebug(lcRedirClient) << "0x14 auth caps — basic" << reply.caps.hasBasic
                                        << "kerberos" << reply.caps.hasKerberos
                                        << "digest-no-qop" << reply.caps.hasDigestNoQop
                                        << "digest-with-qop" << reply.caps.hasDigestWithQop;
                if (!reply.caps.hasDigestWithQop) {
                    qCWarning(lcRedirClient) << "device offers no auth type we implement";
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
                // Presence only. Logging the realm, nonce, cnonce or the
                // computed response would turn a pasted transcript into
                // an offline dictionary attack on the AMT password.
                qCDebug(lcRedirClient) << "0x14 challenge status" << reply.status
                                        << "- realm" << (reply.realm.isEmpty() ? "MISSING" : "present")
                                        << "nonce" << (reply.nonce.isEmpty() ? "MISSING" : "present")
                                        << "qop" << (reply.qop.isEmpty()
                                                         ? QStringLiteral("(absent, defaulting to auth)")
                                                         : reply.qop);
                if (reply.status != 1 || reply.realm.isEmpty() || reply.nonce.isEmpty()) {
                    qCWarning(lcRedirClient) << "malformed digest challenge — status"
                                              << reply.status << "realm"
                                              << (reply.realm.isEmpty() ? "MISSING" : "present")
                                              << "nonce"
                                              << (reply.nonce.isEmpty() ? "MISSING" : "present");
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
                    qCWarning(lcRedirClient) << "redirection auth rejected, 0x14 status"
                                              << reply.status;
                    fail(QStringLiteral("authentication failed (status %1)").arg(reply.status));
                    return;
                }
                qCInfo(lcRedirClient) << "redirection auth accepted for user" << m_user;
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
                    // The last thing that happens before the firmware
                    // hangs up on a consent-less KVM attempt, which is
                    // why describeSocketFailure special-cases this state.
                    qCDebug(lcRedirClient) << "wrote 0x40 KVM start, awaiting 0x41";
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
                qCWarning(lcRedirClient) << "unexpected 0x14 auth frame in state"
                                          << stateName(m_state);
                fail(QStringLiteral("unexpected 0x14 in state %1").arg(static_cast<int>(m_state)));
                return;
            }
            continue;
        }

        // Length only, never m_inbox's contents — by this point the
        // buffer may hold application payload.
        qCWarning(lcRedirClient) << "unexpected frame" << Qt::hex << tag << Qt::dec
                                  << "in state" << stateName(m_state) << "- inbox"
                                  << m_inbox.size() << "bytes";
        fail(QStringLiteral("unexpected frame 0x%1 in state %2")
                 .arg(tag, 2, 16, QLatin1Char('0'))
                 .arg(static_cast<int>(m_state)));
        return;
    }
}

void RedirectionClient::handleSocketError()
{
    const QAbstractSocket::SocketError err = m_socket->error();
    // A peer-initiated close is RemoteHostClosedError, which Qt reports
    // through errorOccurred even when it is the normal end of a
    // session. Distinguish the two: after `Authenticated` the
    // application protocol owns the stream and its own failure reason
    // (already recorded) is far more useful than the socket string.
    if (err == QAbstractSocket::RemoteHostClosedError
        && m_state == State::Authenticated) {
        qCInfo(lcRedirClient) << "peer closed the redirection stream after"
                               << "authentication";
        setState(State::Disconnected);
        emit remoteClosed();
        return;
    }
    qCWarning(lcRedirClient) << "socket error in state" << stateName(m_state)
                              << ":" << m_socket->errorString();
    fail(describeSocketFailure(err, m_socket->errorString()));
}

QString RedirectionClient::describeSocketFailure(int err, const QString &fallback) const
{
    // The generic Qt strings ("The remote host closed the connection")
    // tell the operator nothing about *which* step AMT rejected, and
    // the redirection phase is the single most useful clue when
    // diagnosing a firmware-side refusal. See #433.
    if (err != QAbstractSocket::RemoteHostClosedError) return fallback;
    switch (m_state) {
    case State::Connecting:
    case State::SelectorSent:
        return tr("Intel AMT closed the connection during redirection setup. "
                  "The redirection service may be disabled on the target.");
    case State::SessionOpened:
    case State::AuthQuerying:
    case State::AuthChallenging:
    case State::AuthResponding:
        return tr("Intel AMT closed the connection during authentication. "
                  "Check the AMT username and password.");
    case State::KvmStarting:
        return tr("Intel AMT closed the connection when starting the KVM "
                  "stream. This usually means user consent has not been "
                  "granted, or KVM redirection is disabled on the target.");
    default:
        return fallback;
    }
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
    // Fingerprint is the identifying value and is public; never log
    // summary.der.
    qCInfo(lcRedirClient) << "TLS peer cert subject" << summary.subject
                           << "issuer" << summary.issuer
                           << "sha256" << summary.fingerprintSha256
                           << "valid" << summary.notBefore << "-" << summary.notAfter;
    if (m_trustedFingerprints.contains(summary.fingerprintSha256)) {
        // Fingerprint pinned — proceed with the redirection selector.
        // Signal the consumer so it can surface a small "verified
        // against your pinned certificate" affordance.
        qCInfo(lcRedirClient) << "peer cert matched a pinned fingerprint";
        emit peerCertVerifiedByPin(summary.fingerprintSha256);
        handleConnected();
        return;
    }
    // Hold the selector and ask the consumer to confirm.
    // "It just sits there" on a TLS connect is this branch; nothing in
    // the log distinguished it from a hung socket before.
    qCInfo(lcRedirClient) << "peer cert not pinned — waiting for the user to decide";
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
    // Logged before the assignment so the line names both ends of the
    // transition. This is the single most useful line in the file: a
    // failure report that says which phase the session reached is
    // triageable, one that says "the remote host closed the connection"
    // is not (#433).
    qCDebug(lcRedirClient) << "state" << stateName(m_state) << "->" << stateName(s);
    m_state = s;
    emit stateChanged(s);
}

void RedirectionClient::fail(QString error)
{
    m_lastError = std::move(error);
    // One line for all ~10 fail() sites, phase-tagged. The overlap with
    // handleSocketError is deliberate: that one carries Qt's raw socket
    // string, this one carries the reason we chose to surface.
    qCWarning(lcRedirClient) << "session failed in state" << stateName(m_state)
                              << ":" << m_lastError;
    // Close the transport before announcing the failure. AMT keeps the
    // redirection session allocated for as long as the TCP connection
    // is alive, and a half-open socket makes every subsequent connect
    // fail with "device is busy" (#433).
    if (m_socket != nullptr) m_socket->abort();
    setState(State::Failed);
    emit failed(m_lastError);
}

} // namespace qumesh::redir
