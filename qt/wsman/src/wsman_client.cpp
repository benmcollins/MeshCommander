// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/wsman_client.h"

#include <QCryptographicHash>
#include <QPointer>
#include <QQueue>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace qumesh::wsman {

namespace {

/// Match the formatting used elsewhere: uppercase hex, colon-separated.
QString fingerprintFor(const QSslCertificate &cert)
{
    const QByteArray hash = QCryptographicHash::hash(cert.toDer(),
                                                      QCryptographicHash::Sha256);
    QString out;
    out.reserve(hash.size() * 3);
    for (int i = 0; i < hash.size(); ++i) {
        if (i > 0) out += QLatin1Char(':');
        out += QString::asprintf("%02X", static_cast<quint8>(hash.at(i)));
    }
    return out;
}

QByteArray md5Hex(const QByteArray &in)
{
    return QCryptographicHash::hash(in, QCryptographicHash::Md5).toHex();
}

/// Parse the directives out of a `WWW-Authenticate: Digest ...` value.
/// Handles `key=value` and `key="value"` forms, ignores anything outside
/// of the leading "Digest" scheme.
QHash<QByteArray, QByteArray> parseDigestChallenge(const QByteArray &headerValue)
{
    QHash<QByteArray, QByteArray> out;
    int i = 0;
    // Skip leading "Digest " token if present (case-insensitive).
    if (headerValue.size() >= 7
        && headerValue.left(6).toLower() == "digest") {
        i = 6;
    }
    while (i < headerValue.size()) {
        while (i < headerValue.size() && (headerValue[i] == ' ' || headerValue[i] == ','))
            ++i;
        int keyStart = i;
        while (i < headerValue.size() && headerValue[i] != '=') ++i;
        if (i == headerValue.size()) break;
        const QByteArray key = headerValue.mid(keyStart, i - keyStart).trimmed().toLower();
        ++i; // skip '='
        QByteArray value;
        if (i < headerValue.size() && headerValue[i] == '"') {
            ++i;
            int valStart = i;
            while (i < headerValue.size() && headerValue[i] != '"') ++i;
            value = headerValue.mid(valStart, i - valStart);
            if (i < headerValue.size()) ++i; // skip closing quote
        } else {
            int valStart = i;
            while (i < headerValue.size() && headerValue[i] != ',') ++i;
            value = headerValue.mid(valStart, i - valStart).trimmed();
        }
        if (!key.isEmpty()) out.insert(key, value);
    }
    return out;
}

QByteArray randomCnonce()
{
    auto *rng = QRandomGenerator::system();
    quint64 buf[2] = { rng->generate64(), rng->generate64() };
    return QByteArray::fromRawData(reinterpret_cast<const char *>(buf), sizeof(buf)).toHex();
}

} // namespace

// ---- WsmanReply --------------------------------------------------------------

struct WsmanReply::Private
{
    QPointer<WsmanClient> client;
    QSslSocket *socket = nullptr;
    QTimer *timeoutTimer = nullptr;
    QString errorString;
    int httpStatus = 0;
    QByteArray body;

    QByteArray method = "POST";
    QByteArray requestPath;
    QByteArray hostHeader;
    QByteArray envelope;
    QByteArray soapAction;

    QByteArray responseBuffer;
    QHash<QByteArray, QByteArray> responseHeaders;

    enum State {
        Idle,
        Connecting,
        TlsHandshaking,
        WritingRequest,
        ReadingHeaders,
        ReadingBodyContentLength,
        ReadingBodyChunked,
        Done,
        Failed,
    };
    State state = Idle;

    qint64 expectedBody = -1;
    bool ownsSocket = true;
    bool finished = false;

    // Chunked transfer state.
    qint64 currentChunkRemaining = 0;
    bool readingChunkSize = true;
};

WsmanReply::WsmanReply(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

WsmanReply::~WsmanReply()
{
    if (d->socket != nullptr && d->ownsSocket) {
        d->socket->abort();
        d->socket->deleteLater();
    }
}

QByteArray WsmanReply::readAll() const { return d->body; }
bool WsmanReply::hasError() const { return !d->errorString.isEmpty(); }
QString WsmanReply::errorString() const { return d->errorString; }
int WsmanReply::httpStatus() const { return d->httpStatus; }

void WsmanReply::abort()
{
    if (d->finished) return;
    if (d->socket != nullptr) d->socket->abort();
    d->errorString = QStringLiteral("Aborted");
    d->state = Private::Failed;
    d->finished = true;
    emit finished();
}

// ---- WsmanClient -------------------------------------------------------------

struct WsmanClient::Private
{
    QUrl endpoint;
    QString user;
    QString pass;
    QStringList trustedFingerprints;
    PeerCertSummary pendingPeerCert;
    bool awaitingTrust = false;
    int timeoutMs = 30000;
    SocketFactory socketFactory;

    // Cached Digest challenge state. Refreshed on every successful 401
    // dance; subsequent requests reuse it and increment `nc`.
    bool haveDigestChallenge = false;
    QByteArray digestRealm;
    QByteArray digestNonce;
    QByteArray digestQop;
    QByteArray digestAlgorithm;
    QByteArray digestOpaque;
    quint64 nc = 0;

    // Single-flight queue used when `serializeRequests == true`.
    bool serializeRequests = false;
    QPointer<WsmanReply> currentRequest;
    QQueue<QPointer<WsmanReply>> pendingQueue;
};

WsmanClient::WsmanClient(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

WsmanClient::~WsmanClient() = default;

void WsmanClient::setEndpoint(QUrl endpoint) { d->endpoint = std::move(endpoint); }
void WsmanClient::setCredentials(QString user, QString pass)
{
    d->user = std::move(user);
    d->pass = std::move(pass);
    d->haveDigestChallenge = false; // Old nonce won't match new creds.
}
void WsmanClient::setTrustedFingerprints(QStringList fingerprints)
{
    d->trustedFingerprints = std::move(fingerprints);
}
void WsmanClient::setTransferTimeoutMs(int ms) { d->timeoutMs = ms; }
void WsmanClient::setSocketFactory(SocketFactory factory)
{
    d->socketFactory = std::move(factory);
}
void WsmanClient::setSerializeRequests(bool serialize)
{
    d->serializeRequests = serialize;
}
QUrl WsmanClient::endpoint() const { return d->endpoint; }
PeerCertSummary WsmanClient::pendingPeerCert() const { return d->pendingPeerCert; }
bool WsmanClient::awaitingTrust() const { return d->awaitingTrust; }

void WsmanClient::trustPendingPeerCert()
{
    if (!d->awaitingTrust || d->pendingPeerCert.fingerprintSha256.isEmpty()) return;
    if (!d->trustedFingerprints.contains(d->pendingPeerCert.fingerprintSha256))
        d->trustedFingerprints.append(d->pendingPeerCert.fingerprintSha256);
    d->pendingPeerCert = {};
    d->awaitingTrust = false;
}

namespace {

/// Compute the `response` value of an RFC 7616 / 2617 Digest auth
/// challenge with `qop=auth, algorithm=MD5`. AMT firmware accepts this
/// and rejects unspecified algorithms.
QByteArray computeDigestResponse(const QByteArray &user, const QByteArray &realm,
                                  const QByteArray &pass, const QByteArray &nonce,
                                  const QByteArray &cnonce, const QByteArray &nc,
                                  const QByteArray &qop, const QByteArray &method,
                                  const QByteArray &uri)
{
    const QByteArray ha1 = md5Hex(user + ":" + realm + ":" + pass);
    const QByteArray ha2 = md5Hex(method + ":" + uri);
    return md5Hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
}

/// Build the `Authorization: Digest …` header for one request.
QByteArray buildDigestHeader(const WsmanClient::Private &c, const QByteArray &method,
                              const QByteArray &uri, const QByteArray &user,
                              const QByteArray &pass, quint64 nc)
{
    const QByteArray ncBytes = QByteArray::number(nc, 16).rightJustified(8, '0');
    const QByteArray cnonce = randomCnonce();
    const QByteArray qop = c.digestQop.isEmpty() ? QByteArrayLiteral("auth") : c.digestQop;
    const QByteArray response = computeDigestResponse(user, c.digestRealm, pass,
                                                      c.digestNonce, cnonce,
                                                      ncBytes, qop, method, uri);

    QByteArray out = "Digest ";
    out += "username=\"" + user + "\", ";
    out += "realm=\"" + c.digestRealm + "\", ";
    out += "nonce=\"" + c.digestNonce + "\", ";
    out += "uri=\"" + uri + "\", ";
    out += "response=\"" + response + "\", ";
    if (!c.digestAlgorithm.isEmpty())
        out += "algorithm=" + c.digestAlgorithm + ", ";
    else
        out += "algorithm=MD5, ";
    out += "qop=" + qop + ", ";
    out += "nc=" + ncBytes + ", ";
    out += "cnonce=\"" + cnonce + "\"";
    if (!c.digestOpaque.isEmpty())
        out += ", opaque=\"" + c.digestOpaque + "\"";
    return out;
}

void writeRequestBytes(QSslSocket *sock, const QByteArray &method,
                       const QByteArray &path, const QByteArray &hostHeader,
                       const QByteArray &soapAction, const QByteArray &body,
                       const QByteArray &authHeader)
{
    QByteArray req;
    req += method + " " + path + " HTTP/1.1\r\n";
    req += "Host: " + hostHeader + "\r\n";
    req += "Content-Type: application/soap+xml;charset=UTF-8\r\n";
    if (!soapAction.isEmpty())
        req += "SOAPAction: \"" + soapAction + "\"\r\n";
    req += "User-Agent: QuMesh/0.1\r\n";
    req += "Accept: */*\r\n";
    req += "Connection: close\r\n";
    if (!authHeader.isEmpty())
        req += "Authorization: " + authHeader + "\r\n";
    req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    req += body;
    sock->write(req);
}

} // namespace

namespace {

/// Open or adopt the socket the reply will use. Sets `Connecting` or
/// `TlsHandshaking` and connects all the socket signals.
void startTransport(WsmanReply *reply, WsmanClient *client,
                    WsmanClient::Private &cd, WsmanReply::Private &rd)
{
    QSslSocket *sock = nullptr;
    bool fromFactory = false;
    if (cd.socketFactory) {
        const qintptr fd = cd.socketFactory();
        if (fd < 0) {
            rd.errorString = QStringLiteral("SSH tunnel socket could not be opened");
            rd.state = WsmanReply::Private::Failed;
            rd.finished = true;
            QMetaObject::invokeMethod(reply, &WsmanReply::finished, Qt::QueuedConnection);
            return;
        }
        sock = new QSslSocket(reply);
        if (!sock->setSocketDescriptor(fd, QAbstractSocket::ConnectedState)) {
            rd.errorString = QStringLiteral("setSocketDescriptor failed: %1").arg(sock->errorString());
            rd.state = WsmanReply::Private::Failed;
            rd.finished = true;
            sock->deleteLater();
            QMetaObject::invokeMethod(reply, &WsmanReply::finished, Qt::QueuedConnection);
            return;
        }
        fromFactory = true;
    } else {
        sock = new QSslSocket(reply);
    }
    rd.socket = sock;

    if (rd.timeoutTimer == nullptr && cd.timeoutMs > 0) {
        rd.timeoutTimer = new QTimer(reply);
        rd.timeoutTimer->setSingleShot(true);
        rd.timeoutTimer->setInterval(cd.timeoutMs);
        QObject::connect(rd.timeoutTimer, &QTimer::timeout, reply, [reply, &rd]() {
            if (rd.finished) return;
            rd.errorString = QStringLiteral("Request timed out");
            rd.state = WsmanReply::Private::Failed;
            if (rd.socket != nullptr) rd.socket->abort();
            rd.finished = true;
            emit reply->finished();
        });
        rd.timeoutTimer->start();
    }

    const bool tls = cd.endpoint.scheme().compare(
        QStringLiteral("https"), Qt::CaseInsensitive) == 0;

    if (tls) {
        // Same TLS-with-fingerprint-pinning behavior we had under QNAM.
        QObject::connect(sock, &QSslSocket::sslErrors,
                         reply, [sock, &cd, client](const QList<QSslError> &errors) {
            const QList<QSslCertificate> chain = sock->peerCertificateChain();
            if (chain.isEmpty()) return;
            const QSslCertificate &leaf = chain.first();
            const QString fp = fingerprintFor(leaf);
            if (cd.trustedFingerprints.contains(fp)) {
                emit client->peerCertVerifiedByPin(fp);
                sock->ignoreSslErrors(errors);
                return;
            }
            PeerCertSummary s;
            const auto subjects = leaf.subjectInfo(QSslCertificate::CommonName);
            const auto issuers  = leaf.issuerInfo(QSslCertificate::CommonName);
            if (!subjects.isEmpty()) s.subject = subjects.first();
            if (!issuers.isEmpty())  s.issuer  = issuers.first();
            s.fingerprintSha256 = fp;
            s.notBefore = leaf.effectiveDate().toString(Qt::ISODate);
            s.notAfter  = leaf.expiryDate().toString(Qt::ISODate);
            s.der = leaf.toDer();
            cd.pendingPeerCert = s;
            cd.awaitingTrust = true;
            emit client->trustPromptRequired(s);
            // Leave the errors unignored — handshake will fail, the
            // reply surfaces an error, the controller surfaces the
            // dialog and re-issues the request once trusted.
        });
    }

    QObject::connect(sock, &QSslSocket::errorOccurred, reply,
                     [reply, &rd](QAbstractSocket::SocketError) {
        if (rd.finished) return;
        rd.errorString = rd.socket->errorString();
        rd.state = WsmanReply::Private::Failed;
        rd.finished = true;
        emit reply->finished();
    });

    if (fromFactory) {
        // Skip TCP connect entirely; socket is already in ConnectedState.
        if (tls) {
            QObject::connect(sock, &QSslSocket::encrypted, reply, [reply]() {
                // emit Qt::QueuedConnection through invokeMethod so
                // any pending sslErrors handler has run first.
                QMetaObject::invokeMethod(reply, [reply]() {
                    auto &rd = *reply->d;
                    if (rd.finished) return;
                    rd.state = WsmanReply::Private::WritingRequest;
                    writeRequestBytes(rd.socket, rd.method, rd.requestPath,
                                       rd.hostHeader, rd.soapAction, rd.envelope,
                                       QByteArray());
                    rd.state = WsmanReply::Private::ReadingHeaders;
                }, Qt::QueuedConnection);
            });
            rd.state = WsmanReply::Private::TlsHandshaking;
            sock->setPeerVerifyName(cd.endpoint.host());
            sock->startClientEncryption();
        } else {
            // Plain HTTP — skip TLS, write immediately.
            rd.state = WsmanReply::Private::WritingRequest;
            writeRequestBytes(sock, rd.method, rd.requestPath, rd.hostHeader,
                              rd.soapAction, rd.envelope, QByteArray());
            rd.state = WsmanReply::Private::ReadingHeaders;
        }
    } else {
        // Direct connect.
        QObject::connect(sock, &QSslSocket::connected, reply, [reply, sock, tls, &cd]() {
            auto &rd = *reply->d;
            if (rd.finished) return;
            if (tls) {
                rd.state = WsmanReply::Private::TlsHandshaking;
                sock->setPeerVerifyName(cd.endpoint.host());
                sock->startClientEncryption();
            } else {
                rd.state = WsmanReply::Private::WritingRequest;
                writeRequestBytes(sock, rd.method, rd.requestPath, rd.hostHeader,
                                   rd.soapAction, rd.envelope, QByteArray());
                rd.state = WsmanReply::Private::ReadingHeaders;
            }
        });
        if (tls) {
            QObject::connect(sock, &QSslSocket::encrypted, reply, [reply]() {
                auto &rd = *reply->d;
                if (rd.finished) return;
                rd.state = WsmanReply::Private::WritingRequest;
                writeRequestBytes(rd.socket, rd.method, rd.requestPath,
                                   rd.hostHeader, rd.soapAction, rd.envelope,
                                   QByteArray());
                rd.state = WsmanReply::Private::ReadingHeaders;
            });
        }
        rd.state = WsmanReply::Private::Connecting;
        const QString host = cd.endpoint.host();
        const quint16 port = static_cast<quint16>(cd.endpoint.port(tls ? 16993 : 16992));
        sock->connectToHost(host, port);
    }
}

void readMore(WsmanReply *reply, WsmanClient *client,
               WsmanClient::Private &cd, WsmanReply::Private &rd);

void finishReply(WsmanReply *reply, WsmanReply::Private &rd, const QString &err)
{
    if (rd.finished) return;
    if (!err.isEmpty()) {
        rd.errorString = err;
        rd.state = WsmanReply::Private::Failed;
    } else {
        rd.state = WsmanReply::Private::Done;
    }
    rd.finished = true;
    if (rd.timeoutTimer != nullptr) rd.timeoutTimer->stop();
    emit reply->finished();
}

/// Retry the same request with a freshly-computed `Authorization`
/// header. Opens a *new* socket because we already closed the previous
/// one (Connection: close after 401).
void retryWithDigest(WsmanReply *reply, WsmanClient *client,
                     WsmanClient::Private &cd, WsmanReply::Private &rd)
{
    if (rd.socket != nullptr) {
        rd.socket->disconnectFromHost();
        rd.socket->deleteLater();
        rd.socket = nullptr;
    }
    rd.responseBuffer.clear();
    rd.responseHeaders.clear();
    rd.body.clear();
    rd.httpStatus = 0;
    rd.expectedBody = -1;
    rd.currentChunkRemaining = 0;
    rd.readingChunkSize = true;

    // Open a new transport and write with auth.
    QSslSocket *sock = nullptr;
    bool fromFactory = false;
    if (cd.socketFactory) {
        const qintptr fd = cd.socketFactory();
        if (fd < 0) {
            finishReply(reply, rd, QStringLiteral("SSH tunnel socket could not be opened"));
            return;
        }
        sock = new QSslSocket(reply);
        if (!sock->setSocketDescriptor(fd, QAbstractSocket::ConnectedState)) {
            finishReply(reply, rd, QStringLiteral("setSocketDescriptor failed"));
            sock->deleteLater();
            return;
        }
        fromFactory = true;
    } else {
        sock = new QSslSocket(reply);
    }
    rd.socket = sock;

    const bool tls = cd.endpoint.scheme().compare(
        QStringLiteral("https"), Qt::CaseInsensitive) == 0;

    if (tls) {
        QObject::connect(sock, &QSslSocket::sslErrors,
                         reply, [sock, &cd, client](const QList<QSslError> &errors) {
            const QList<QSslCertificate> chain = sock->peerCertificateChain();
            if (chain.isEmpty()) return;
            const QSslCertificate &leaf = chain.first();
            const QString fp = fingerprintFor(leaf);
            if (cd.trustedFingerprints.contains(fp)) {
                emit client->peerCertVerifiedByPin(fp);
                sock->ignoreSslErrors(errors);
            }
        });
    }
    QObject::connect(sock, &QSslSocket::errorOccurred, reply,
                     [reply, &rd](QAbstractSocket::SocketError) {
        if (rd.finished) return;
        finishReply(reply, rd, rd.socket->errorString());
    });
    QObject::connect(sock, &QSslSocket::readyRead, reply,
                     [reply, client, &cd, &rd]() { readMore(reply, client, cd, rd); });

    const QByteArray auth = buildDigestHeader(cd, rd.method, rd.requestPath,
                                              cd.user.toUtf8(), cd.pass.toUtf8(),
                                              ++cd.nc);

    auto doWrite = [reply, auth]() {
        auto &rd = *reply->d;
        if (rd.finished) return;
        writeRequestBytes(rd.socket, rd.method, rd.requestPath, rd.hostHeader,
                          rd.soapAction, rd.envelope, auth);
        rd.state = WsmanReply::Private::ReadingHeaders;
    };

    if (fromFactory) {
        if (tls) {
            QObject::connect(sock, &QSslSocket::encrypted, reply, doWrite);
            rd.state = WsmanReply::Private::TlsHandshaking;
            sock->setPeerVerifyName(cd.endpoint.host());
            sock->startClientEncryption();
        } else {
            doWrite();
        }
    } else {
        QObject::connect(sock, &QSslSocket::connected, reply, [sock, tls, &cd, doWrite, reply]() {
            auto &rd = *reply->d;
            if (rd.finished) return;
            if (tls) {
                rd.state = WsmanReply::Private::TlsHandshaking;
                sock->setPeerVerifyName(cd.endpoint.host());
                sock->startClientEncryption();
            } else {
                doWrite();
            }
        });
        if (tls) QObject::connect(sock, &QSslSocket::encrypted, reply, doWrite);
        rd.state = WsmanReply::Private::Connecting;
        const QString host = cd.endpoint.host();
        const quint16 port = static_cast<quint16>(cd.endpoint.port(tls ? 16993 : 16992));
        sock->connectToHost(host, port);
    }
}

/// Single read pass: consume the socket buffer, advance the state
/// machine, and decide whether we need to read more or finish.
void readMore(WsmanReply *reply, WsmanClient *client,
               WsmanClient::Private &cd, WsmanReply::Private &rd)
{
    if (rd.finished || rd.socket == nullptr) return;
    const QByteArray chunk = rd.socket->readAll();
    if (chunk.isEmpty() && rd.socket->state() != QAbstractSocket::ConnectedState
        && rd.state == WsmanReply::Private::ReadingHeaders) {
        finishReply(reply, rd, QStringLiteral("Connection closed before headers"));
        return;
    }
    rd.responseBuffer.append(chunk);

    if (rd.state == WsmanReply::Private::ReadingHeaders) {
        const int headerEnd = rd.responseBuffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) return; // need more bytes
        const QByteArray rawHeaders = rd.responseBuffer.left(headerEnd);
        rd.responseBuffer.remove(0, headerEnd + 4);

        QList<QByteArray> lines = rawHeaders.split('\n');
        if (lines.isEmpty()) {
            finishReply(reply, rd, QStringLiteral("Empty response"));
            return;
        }
        // Status line: HTTP/1.1 200 OK
        QByteArray statusLine = lines.takeFirst();
        if (statusLine.endsWith('\r')) statusLine.chop(1);
        const QList<QByteArray> statusParts = statusLine.split(' ');
        if (statusParts.size() < 2) {
            finishReply(reply, rd, QStringLiteral("Malformed status line"));
            return;
        }
        rd.httpStatus = statusParts.at(1).toInt();

        rd.responseHeaders.clear();
        for (const QByteArray &lineRaw : lines) {
            QByteArray line = lineRaw;
            if (line.endsWith('\r')) line.chop(1);
            const int colon = line.indexOf(':');
            if (colon < 0) continue;
            const QByteArray name = line.left(colon).trimmed().toLower();
            const QByteArray value = line.mid(colon + 1).trimmed();
            rd.responseHeaders.insert(name, value);
        }

        // 401 → parse challenge and retry on a new socket.
        if (rd.httpStatus == 401) {
            const QByteArray wwwAuth = rd.responseHeaders.value("www-authenticate");
            if (wwwAuth.isEmpty()) {
                finishReply(reply, rd, QStringLiteral("401 without WWW-Authenticate"));
                return;
            }
            const auto params = parseDigestChallenge(wwwAuth);
            cd.digestRealm = params.value("realm");
            cd.digestNonce = params.value("nonce");
            // AMT sometimes advertises `qop="auth,auth-int"`. We have
            // to pick a single value and send THAT back in the
            // Authorization header (and use it in the response hash)
            // — sending the whole comma-joined string makes the
            // server's hash diverge from ours and the auth fails.
            const QByteArray rawQop = params.value("qop");
            cd.digestQop = QByteArrayLiteral("auth");
            if (!rawQop.isEmpty()) {
                bool foundAuth = false;
                for (const QByteArray &p : rawQop.split(',')) {
                    if (p.trimmed() == "auth") { foundAuth = true; break; }
                }
                if (!foundAuth) cd.digestQop = rawQop.split(',').first().trimmed();
            }
            cd.digestAlgorithm = params.value("algorithm");
            cd.digestOpaque = params.value("opaque");
            cd.haveDigestChallenge = !cd.digestRealm.isEmpty() && !cd.digestNonce.isEmpty();
            cd.nc = 0;
            if (!cd.haveDigestChallenge) {
                finishReply(reply, rd, QStringLiteral("Unsupported auth challenge"));
                return;
            }
            retryWithDigest(reply, client, cd, rd);
            return;
        }

        // Decide body-read mode.
        const QByteArray te = rd.responseHeaders.value("transfer-encoding");
        const QByteArray cl = rd.responseHeaders.value("content-length");
        if (te.toLower().contains("chunked")) {
            rd.state = WsmanReply::Private::ReadingBodyChunked;
            rd.readingChunkSize = true;
            rd.currentChunkRemaining = 0;
        } else if (!cl.isEmpty()) {
            rd.state = WsmanReply::Private::ReadingBodyContentLength;
            rd.expectedBody = cl.toLongLong();
            if (rd.expectedBody < 0) {
                finishReply(reply, rd, QStringLiteral("Bad Content-Length"));
                return;
            }
        } else {
            // No Content-Length, no chunked → read until close.
            rd.state = WsmanReply::Private::ReadingBodyContentLength;
            rd.expectedBody = -2; // sentinel: read-to-close
        }
    }

    if (rd.state == WsmanReply::Private::ReadingBodyContentLength) {
        if (rd.expectedBody == -2) {
            rd.body.append(rd.responseBuffer);
            rd.responseBuffer.clear();
            if (rd.socket->state() != QAbstractSocket::ConnectedState) {
                finishReply(reply, rd, QString());
            }
            return;
        }
        const qint64 need = rd.expectedBody - rd.body.size();
        if (need <= 0) {
            finishReply(reply, rd, QString());
            return;
        }
        const qint64 take = qMin<qint64>(need, rd.responseBuffer.size());
        rd.body.append(rd.responseBuffer.left(take));
        rd.responseBuffer.remove(0, take);
        if (rd.body.size() >= rd.expectedBody) {
            finishReply(reply, rd, QString());
        }
        return;
    }

    if (rd.state == WsmanReply::Private::ReadingBodyChunked) {
        for (;;) {
            if (rd.readingChunkSize) {
                const int eol = rd.responseBuffer.indexOf("\r\n");
                if (eol < 0) return;
                QByteArray sizeLine = rd.responseBuffer.left(eol);
                rd.responseBuffer.remove(0, eol + 2);
                const int semi = sizeLine.indexOf(';');
                if (semi >= 0) sizeLine = sizeLine.left(semi);
                bool ok = false;
                rd.currentChunkRemaining = sizeLine.toLongLong(&ok, 16);
                if (!ok || rd.currentChunkRemaining < 0) {
                    finishReply(reply, rd, QStringLiteral("Bad chunk size"));
                    return;
                }
                if (rd.currentChunkRemaining == 0) {
                    // Trailer headers and final CRLF — read until done
                    // or close. AMT never emits trailers, so simplify
                    // by finishing here.
                    finishReply(reply, rd, QString());
                    return;
                }
                rd.readingChunkSize = false;
            }
            if (rd.responseBuffer.isEmpty()) return;
            const qint64 take = qMin<qint64>(rd.currentChunkRemaining,
                                             rd.responseBuffer.size());
            rd.body.append(rd.responseBuffer.left(take));
            rd.responseBuffer.remove(0, take);
            rd.currentChunkRemaining -= take;
            if (rd.currentChunkRemaining == 0) {
                // Each chunk is followed by CRLF.
                if (rd.responseBuffer.size() < 2) return;
                rd.responseBuffer.remove(0, 2);
                rd.readingChunkSize = true;
            } else {
                return; // need more bytes
            }
        }
    }
}

} // namespace

WsmanReply *WsmanClient::sendEnvelope(const QByteArray &envelope, const char *soapAction)
{
    auto *reply = new WsmanReply(this);
    auto &rd = *reply->d;
    rd.client = this;
    rd.envelope = envelope;
    if (soapAction != nullptr) rd.soapAction = QByteArray(soapAction);
    rd.requestPath = d->endpoint.path(QUrl::FullyEncoded).toUtf8();
    if (rd.requestPath.isEmpty()) rd.requestPath = "/";
    rd.hostHeader = (d->endpoint.host() + QStringLiteral(":")
                     + QString::number(d->endpoint.port(
                         d->endpoint.scheme().compare(QStringLiteral("https"),
                             Qt::CaseInsensitive) == 0 ? 16993 : 16992))).toUtf8();

    auto *client = this;
    auto &cd = *d;

    auto startNow = [reply, client, &cd, &rd]() {
        // If we already have a valid Digest challenge cached, send the
        // first attempt pre-authed and skip the 401 round-trip.
        if (cd.haveDigestChallenge && !cd.user.isEmpty()) {
            retryWithDigest(reply, client, cd, rd);
            return;
        }
        startTransport(reply, client, cd, rd);
        if (rd.socket != nullptr) {
            QObject::connect(rd.socket, &QSslSocket::readyRead, reply,
                             [reply, client, &cd, &rd]() { readMore(reply, client, cd, rd); });
        }
    };

    if (!cd.serializeRequests) {
        startNow();
        return reply;
    }

    // Serialized mode: enqueue. The single-flight drainer picks the
    // next reply off the queue when the in-flight one finishes.
    cd.pendingQueue.enqueue(reply);
    QObject::connect(reply, &WsmanReply::finished, client, [client]() {
        auto &cd2 = *client->d;
        cd2.currentRequest = nullptr;
        while (!cd2.pendingQueue.isEmpty()) {
            QPointer<WsmanReply> next = cd2.pendingQueue.dequeue();
            if (next == nullptr) continue; // reply got destroyed while queued
            cd2.currentRequest = next;
            // The pending reply's `rd` already has envelope / path /
            // host set from `sendEnvelope`; we just need to fire its
            // transport via the same logic the un-serialized path uses.
            auto &nrd = *next->d;
            auto *c = client;
            if (cd2.haveDigestChallenge && !cd2.user.isEmpty()) {
                retryWithDigest(next, c, cd2, nrd);
            } else {
                startTransport(next, c, cd2, nrd);
                if (nrd.socket != nullptr) {
                    QObject::connect(nrd.socket, &QSslSocket::readyRead, next,
                                     [next, c, &cd2, &nrd]() { readMore(next, c, cd2, nrd); });
                }
            }
            return;
        }
    });

    if (cd.currentRequest == nullptr) {
        cd.currentRequest = cd.pendingQueue.dequeue();
        startNow();
    }
    return reply;
}

} // namespace qumesh::wsman
