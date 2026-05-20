// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/wsman_client.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QtTest>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

using namespace qumesh::wsman;

namespace {

/// Self-signed cert + matching key + the SHA-256 fingerprint the
/// WsmanClient's trust-on-first-use surface uses. Mirrors the helper
/// in `redir/tests/test_redir_tls.cpp` — kept inline here so the two
/// test executables don't grow a shared "tls test fixtures" library
/// just for one function.
struct SelfSignedCert {
    QSslCertificate cert;
    QSslKey key;
    QByteArray der;
    QString fingerprintHex; ///< Uppercase, colon-separated.
};

SelfSignedCert generateSelfSigned(const QString &cn)
{
    SelfSignedCert s;
    EVP_PKEY *pkey = EVP_RSA_gen(2048);
    Q_ASSERT(pkey != nullptr);
    X509 *x = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_getm_notBefore(x), 0);
    X509_gmtime_adj(X509_getm_notAfter(x), 60L * 60 * 24 * 365);
    X509_set_pubkey(x, pkey);
    X509_NAME *name = X509_get_subject_name(x);
    const QByteArray cnUtf = cn.toUtf8();
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
                                reinterpret_cast<const unsigned char *>(cnUtf.constData()),
                                -1, -1, 0);
    X509_set_issuer_name(x, name);
    X509_sign(x, pkey, EVP_sha256());

    unsigned char *derBuf = nullptr;
    const int derLen = i2d_X509(x, &derBuf);
    s.der = QByteArray(reinterpret_cast<const char *>(derBuf), derLen);
    OPENSSL_free(derBuf);
    s.cert = QSslCertificate(s.der, QSsl::Der);

    BIO *kb = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(kb, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    char *kp = nullptr;
    const long kn = BIO_get_mem_data(kb, &kp);
    s.key = QSslKey(QByteArray(kp, static_cast<int>(kn)), QSsl::Rsa, QSsl::Pem);
    BIO_free(kb);

    X509_free(x);
    EVP_PKEY_free(pkey);

    const QByteArray hash = QCryptographicHash::hash(s.der, QCryptographicHash::Sha256);
    QString fp;
    for (int i = 0; i < hash.size(); ++i) {
        if (i > 0) fp += QLatin1Char(':');
        fp += QString::asprintf("%02X", static_cast<quint8>(hash.at(i)));
    }
    s.fingerprintHex = fp;
    return s;
}

constexpr char kIdentifyResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsmid=\"http://schemas.dmtf.org/wbem/wsman/identity/1/wsmanidentity.xsd\">"
    "<s:Header/><s:Body>"
    "<wsmid:IdentifyResponse>"
    "<wsmid:ProtocolVersion>http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd</wsmid:ProtocolVersion>"
    "<wsmid:ProductVendor>Intel(r) AMT</wsmid:ProductVendor>"
    "<wsmid:ProductVersion>16.0.0</wsmid:ProductVersion>"
    "</wsmid:IdentifyResponse>"
    "</s:Body></s:Envelope>";

constexpr char kIdentifyEnvelope[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsmid=\"http://schemas.dmtf.org/wbem/wsman/identity/1/wsmanidentity.xsd\">"
    "<s:Header/><s:Body><wsmid:Identify/></s:Body></s:Envelope>";

/// Wait for `pred` to become true while pumping the event loop. The
/// existing `test_redir_tls.cpp` uses the same helper for the same
/// reason — QSignalSpy::wait can drop events when multiple Qt threads
/// race the same signal, and the trust-retry flow crosses a couple.
template <typename Pred>
bool waitFor(int ms, Pred p)
{
    QElapsedTimer t;
    t.start();
    while (!p()) {
        if (t.elapsed() > ms) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return true;
}

/// QHttpServer bound to a QSslServer that serves the canned Identify
/// response on POST /wsman. Counts handshakes so tests can verify
/// "second request opened a fresh socket".
class HttpsIdentifyServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpsIdentifyServer(const SelfSignedCert &c)
        : m_ssl(new QSslServer(this))
    {
        QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
        cfg.setLocalCertificate(c.cert);
        cfg.setPrivateKey(c.key);
        cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_ssl->setSslConfiguration(cfg);
        connect(m_ssl, &QSslServer::pendingConnectionAvailable,
                this, [this]() { ++m_handshakes; });

        m_http.route(QStringLiteral("/wsman"),
                      QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &) {
                          ++m_requests;
                          return QHttpServerResponse(
                              QByteArrayLiteral("application/soap+xml"),
                              QByteArray(kIdentifyResponse));
                      });
    }
    bool listen()
    {
        if (!m_ssl->listen(QHostAddress::LocalHost)) return false;
        return m_http.bind(m_ssl);
    }
    quint16 port() const { return m_ssl->serverPort(); }
    int handshakes() const { return m_handshakes; }
    int requests() const { return m_requests; }

private:
    QSslServer *m_ssl;
    QHttpServer m_http;
    int m_handshakes = 0;
    int m_requests = 0;
};

QUrl endpointFor(quint16 port)
{
    QUrl u;
    u.setScheme(QStringLiteral("https"));
    u.setHost(QStringLiteral("127.0.0.1"));
    u.setPort(port);
    u.setPath(QStringLiteral("/wsman"));
    return u;
}

} // namespace

class TestWsmanTrustRetry : public QObject
{
    Q_OBJECT
private slots:
    void untrustedCertFiresTrustPromptAndFailsHandshake();
    void trustingPendingCertAllowsRetry();
    void preTrustedFingerprintConnectsSilently();
    void wrongFingerprintStillPrompts();
};

void TestWsmanTrustRetry::untrustedCertFiresTrustPromptAndFailsHandshake()
{
    const SelfSignedCert sc = generateSelfSigned(QStringLiteral("amt.test"));
    HttpsIdentifyServer server(sc);
    QVERIFY(server.listen());

    WsmanClient client;
    client.setEndpoint(endpointFor(server.port()));

    QSignalSpy promptSpy(&client, &WsmanClient::trustPromptRequired);
    QSignalSpy pinSpy(&client, &WsmanClient::peerCertVerifiedByPin);

    WsmanReply *reply = client.sendEnvelope(QByteArray(kIdentifyEnvelope));
    QVERIFY(reply);
    QSignalSpy finishedSpy(reply, &WsmanReply::finished);
    QVERIFY(finishedSpy.wait(8000));

    // The TOFU surface fired once; the by-pin surface did NOT.
    QCOMPARE(promptSpy.size(), 1);
    QCOMPARE(pinSpy.size(), 0);
    QVERIFY(client.awaitingTrust());

    const PeerCertSummary p = client.pendingPeerCert();
    QCOMPARE(p.fingerprintSha256, sc.fingerprintHex);
    // CN comes back from the cert verbatim; we generated "amt.test".
    QCOMPARE(p.subject, QStringLiteral("amt.test"));

    // Reply terminated in error — the controller is the one that
    // surfaces the dialog and re-issues.
    QVERIFY(reply->hasError());
    QVERIFY(!reply->errorString().isEmpty());

    reply->deleteLater();
}

void TestWsmanTrustRetry::trustingPendingCertAllowsRetry()
{
    const SelfSignedCert sc = generateSelfSigned(QStringLiteral("amt.retry"));
    HttpsIdentifyServer server(sc);
    QVERIFY(server.listen());

    WsmanClient client;
    client.setEndpoint(endpointFor(server.port()));

    QSignalSpy promptSpy(&client, &WsmanClient::trustPromptRequired);
    QSignalSpy pinSpy(&client, &WsmanClient::peerCertVerifiedByPin);

    // First attempt — handshake fails because the fingerprint is
    // unknown. We capture the pending cert summary off the prompt.
    WsmanReply *r1 = client.sendEnvelope(QByteArray(kIdentifyEnvelope));
    QSignalSpy fin1(r1, &WsmanReply::finished);
    QVERIFY(fin1.wait(8000));
    QCOMPARE(promptSpy.size(), 1);
    QVERIFY(r1->hasError());
    r1->deleteLater();

    // Operator clicks Trust — promote the pending cert's fingerprint.
    client.trustPendingPeerCert();
    QVERIFY(!client.awaitingTrust());

    // Second attempt — handshake succeeds, by-pin signal fires, body
    // arrives intact. QSslServer::pendingConnectionAvailable only
    // fires for handshakes that completed successfully, so the
    // counter ticks for this attempt (the first didn't, since we
    // never called ignoreSslErrors on the client side).
    WsmanReply *r2 = client.sendEnvelope(QByteArray(kIdentifyEnvelope));
    QSignalSpy fin2(r2, &WsmanReply::finished);
    QVERIFY(fin2.wait(8000));
    QCOMPARE(pinSpy.size(), 1);
    QCOMPARE(pinSpy.last().value(0).toString(), sc.fingerprintHex);
    QVERIFY(!r2->hasError());
    QVERIFY(r2->readAll().contains("IdentifyResponse"));
    r2->deleteLater();

    QVERIFY(waitFor(2000, [&] { return server.handshakes() >= 1; }));
    QCOMPARE(server.requests(), 1);
}

void TestWsmanTrustRetry::preTrustedFingerprintConnectsSilently()
{
    const SelfSignedCert sc = generateSelfSigned(QStringLiteral("amt.pinned"));
    HttpsIdentifyServer server(sc);
    QVERIFY(server.listen());

    WsmanClient client;
    client.setTrustedFingerprints({sc.fingerprintHex});
    client.setEndpoint(endpointFor(server.port()));

    QSignalSpy promptSpy(&client, &WsmanClient::trustPromptRequired);
    QSignalSpy pinSpy(&client, &WsmanClient::peerCertVerifiedByPin);

    WsmanReply *reply = client.sendEnvelope(QByteArray(kIdentifyEnvelope));
    QSignalSpy finishedSpy(reply, &WsmanReply::finished);
    QVERIFY(finishedSpy.wait(8000));

    QCOMPARE(promptSpy.size(), 0);
    QCOMPARE(pinSpy.size(), 1);
    QCOMPARE(pinSpy.last().value(0).toString(), sc.fingerprintHex);
    QVERIFY(!reply->hasError());
    QVERIFY(reply->readAll().contains("IdentifyResponse"));
    reply->deleteLater();
}

void TestWsmanTrustRetry::wrongFingerprintStillPrompts()
{
    const SelfSignedCert sc = generateSelfSigned(QStringLiteral("amt.wrong"));
    HttpsIdentifyServer server(sc);
    QVERIFY(server.listen());

    WsmanClient client;
    // A 256-bit "other" fingerprint that won't match the cert we
    // present — exercises the negative path of the pinned-list check.
    const QString bogus = QStringLiteral(
        "DE:AD:BE:EF:00:11:22:33:44:55:66:77:88:99:AA:BB:"
        "CC:DD:EE:FF:01:23:45:67:89:AB:CD:EF:11:22:33:44");
    client.setTrustedFingerprints({bogus});
    client.setEndpoint(endpointFor(server.port()));

    QSignalSpy promptSpy(&client, &WsmanClient::trustPromptRequired);
    QSignalSpy pinSpy(&client, &WsmanClient::peerCertVerifiedByPin);

    WsmanReply *reply = client.sendEnvelope(QByteArray(kIdentifyEnvelope));
    QSignalSpy finishedSpy(reply, &WsmanReply::finished);
    QVERIFY(finishedSpy.wait(8000));

    QCOMPARE(promptSpy.size(), 1);
    QCOMPARE(pinSpy.size(), 0);
    QVERIFY(reply->hasError());
    reply->deleteLater();
}

QTEST_MAIN(TestWsmanTrustRetry)
#include "test_wsman_trust_retry.moc"
