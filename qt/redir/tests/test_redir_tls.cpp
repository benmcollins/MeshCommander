// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/redir_client.h"
#include "redir/redir_codec.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QtTest>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

using namespace meshcommander::redir;

namespace {

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

/// Bare TLS server that only completes the handshake. We don't need
/// to walk the redirection protocol for the TLS tests — the unit
/// under test is the SSL setup.
class TlsHandshakeServer : public QObject
{
    Q_OBJECT
public:
    TlsHandshakeServer(const SelfSignedCert &c)
        : m_cert(c), m_server(new QSslServer(this))
    {
        QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
        cfg.setLocalCertificate(c.cert);
        cfg.setPrivateKey(c.key);
        cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_server->setSslConfiguration(cfg);

        connect(m_server, &QSslServer::pendingConnectionAvailable, this, [this]() {
            QTcpSocket *raw = m_server->nextPendingConnection();
            m_handshakes++;
            (void)raw;
        });
    }
    bool listen() { return m_server->listen(QHostAddress::LocalHost); }
    quint16 port() const { return m_server->serverPort(); }
    int handshakes() const { return m_handshakes; }

private:
    SelfSignedCert m_cert;
    QSslServer *m_server;
    int m_handshakes = 0;
};

} // namespace

class TestRedirTls : public QObject
{
    Q_OBJECT
private slots:
    void unknownCertFiresTrustPrompt();
    void trustingPendingCertCompletesHandshake();
    void preTrustedFingerprintConnectsSilently();
};

void TestRedirTls::unknownCertFiresTrustPrompt()
{
    if (!QSslSocket::supportsSsl()) QSKIP("Qt built without TLS support");
    const SelfSignedCert c = generateSelfSigned(QStringLiteral("tls-untrusted"));
    TlsHandshakeServer server(c);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setTls(true);
    QSignalSpy promptSpy(&client, &RedirectionClient::trustPromptRequired);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return promptSpy.count() == 1; }));
    const PeerCertSummary summary = promptSpy.first().at(0)
                                        .value<PeerCertSummary>();
    QCOMPARE(summary.subject, QStringLiteral("tls-untrusted"));
    QCOMPARE(summary.fingerprintSha256, c.fingerprintHex);
    QVERIFY(!summary.der.isEmpty());
}

void TestRedirTls::trustingPendingCertCompletesHandshake()
{
    if (!QSslSocket::supportsSsl()) QSKIP("Qt built without TLS support");
    const SelfSignedCert c = generateSelfSigned(QStringLiteral("tls-accept"));
    TlsHandshakeServer server(c);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setTls(true);
    QSignalSpy promptSpy(&client, &RedirectionClient::trustPromptRequired);
    QSignalSpy stateSpy(&client, &RedirectionClient::stateChanged);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());
    QVERIFY(waitFor(5000, [&]() { return promptSpy.count() == 1; }));
    client.trustPendingPeerCert();
    QVERIFY(waitFor(5000, [&]() { return server.handshakes() >= 1; }));
    // Once handshake completed, the client transitions to SelectorSent
    // (it writes the protocol selector immediately after the encrypted
    // signal fires).
    bool sawSelector = false;
    for (const auto &args : stateSpy) {
        if (args.at(0).value<RedirectionClient::State>()
            == RedirectionClient::State::SelectorSent) sawSelector = true;
    }
    QVERIFY(sawSelector);
}

void TestRedirTls::preTrustedFingerprintConnectsSilently()
{
    if (!QSslSocket::supportsSsl()) QSKIP("Qt built without TLS support");
    const SelfSignedCert c = generateSelfSigned(QStringLiteral("tls-pretrusted"));
    TlsHandshakeServer server(c);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setTls(true);
    client.setTrustedFingerprints({c.fingerprintHex});
    QSignalSpy promptSpy(&client, &RedirectionClient::trustPromptRequired);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());
    QVERIFY(waitFor(5000, [&]() { return server.handshakes() >= 1; }));
    QCOMPARE(promptSpy.count(), 0);
}

QTEST_MAIN(TestRedirTls)
#include "test_redir_tls.moc"
