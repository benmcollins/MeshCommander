// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/redir_codec.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace qumesh::redir {

namespace {

// Protocol selectors per Intel's redirection spec.
constexpr unsigned char kSolSelector[]  = {0x10, 0x00, 0x00, 0x00, 0x53, 0x4F, 0x4C, 0x20}; // "SOL "
constexpr unsigned char kKvmSelector[]  = {0x10, 0x01, 0x00, 0x00, 0x4B, 0x56, 0x4D, 0x52}; // "KVMR"
constexpr unsigned char kIderSelector[] = {0x10, 0x00, 0x00, 0x00, 0x49, 0x44, 0x45, 0x52}; // "IDER"

constexpr int kStartReplyFixedSize = 13;
constexpr int kAuthReplyFixedSize  = 9;

QByteArray pack32Le(quint32 v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    return b;
}

quint32 read32Le(QByteArrayView v, int off)
{
    return static_cast<quint32>(static_cast<unsigned char>(v[off]))
         | static_cast<quint32>(static_cast<unsigned char>(v[off + 1])) << 8
         | static_cast<quint32>(static_cast<unsigned char>(v[off + 2])) << 16
         | static_cast<quint32>(static_cast<unsigned char>(v[off + 3])) << 24;
}

/// Append a length-prefixed string to a buffer. Each field in the AMT
/// auth wire format is `<u8 length><bytes>`. Lengths over 255 are not
/// representable; callers must keep usernames/nonces under that.
void appendLp(QByteArray *buf, const QString &s)
{
    const QByteArray b = s.toUtf8();
    Q_ASSERT(b.size() <= 0xFF);
    buf->append(static_cast<char>(b.size() & 0xFF));
    buf->append(b);
}

QByteArray buildAuthFrame(AuthType type, const QByteArray &authData)
{
    QByteArray frame;
    frame.append(static_cast<char>(0x13));
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>(static_cast<unsigned char>(type)));
    frame.append(pack32Le(static_cast<quint32>(authData.size())));
    frame.append(authData);
    return frame;
}

QString md5Hex(const QByteArray &in)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(in, QCryptographicHash::Md5).toHex());
}

} // namespace

QByteArray buildSelector(Protocol p)
{
    switch (p) {
    case Protocol::Sol:
        return QByteArray(reinterpret_cast<const char *>(kSolSelector), sizeof(kSolSelector));
    case Protocol::Kvm:
        return QByteArray(reinterpret_cast<const char *>(kKvmSelector), sizeof(kKvmSelector));
    case Protocol::Ider:
        return QByteArray(reinterpret_cast<const char *>(kIderSelector), sizeof(kIderSelector));
    }
    return {};
}

bool tryParseStartSessionReply(QByteArrayView buffer, StartSessionReply *reply, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < 1) return false;
    if (static_cast<unsigned char>(buffer[0]) != 0x11) return false;

    if (buffer.size() < kStartReplyFixedSize) return false;
    const int oemLen = static_cast<unsigned char>(buffer[12]);
    if (buffer.size() < kStartReplyFixedSize + oemLen) return false;

    if (reply != nullptr) {
        reply->status = static_cast<StartSessionStatus>(static_cast<unsigned char>(buffer[1]));
        reply->oemData = QByteArray(buffer.data() + kStartReplyFixedSize, oemLen);
    }
    if (consumed != nullptr) *consumed = kStartReplyFixedSize + oemLen;
    return true;
}

QByteArray buildAuthQuery()
{
    return buildAuthFrame(AuthType::Query, {});
}

QByteArray buildDigestQuery(const QString &user, const QString &authUri)
{
    QByteArray data;
    appendLp(&data, user);
    appendLp(&data, QString());     // realm (empty)
    appendLp(&data, QString());     // nonce (empty)
    appendLp(&data, authUri);
    appendLp(&data, QString());     // cnonce (empty)
    appendLp(&data, QString());     // nc (empty)
    appendLp(&data, QString());     // response (empty)
    appendLp(&data, QString());     // qop (empty)
    return buildAuthFrame(AuthType::DigestWithQop, data);
}

QByteArray buildDigestResponse(const QString &user, const QString &realm,
                                const QString &nonce, const QString &authUri,
                                const QString &cnonce, const QString &nc,
                                const QString &response, const QString &qop)
{
    QByteArray data;
    appendLp(&data, user);
    appendLp(&data, realm);
    appendLp(&data, nonce);
    appendLp(&data, authUri);
    appendLp(&data, cnonce);
    appendLp(&data, nc);
    appendLp(&data, response);
    appendLp(&data, qop);
    return buildAuthFrame(AuthType::DigestWithQop, data);
}

bool tryParseAuthReply(QByteArrayView buffer, AuthReply *reply, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < 1) return false;
    if (static_cast<unsigned char>(buffer[0]) != 0x14) return false;
    if (buffer.size() < kAuthReplyFixedSize) return false;

    const quint32 dataLen = read32Le(buffer, 5);
    if (buffer.size() < kAuthReplyFixedSize + static_cast<int>(dataLen)) return false;

    if (reply != nullptr) {
        reply->status = static_cast<unsigned char>(buffer[1]);
        reply->authType = static_cast<AuthType>(static_cast<unsigned char>(buffer[4]));
        reply->caps = {};
        reply->realm.clear();
        reply->nonce.clear();
        reply->qop.clear();

        const QByteArrayView data = buffer.sliced(kAuthReplyFixedSize, dataLen);

        if (reply->authType == AuthType::Query) {
            // Body is a bitmap of supported auth types — each byte names a
            // supported type. Iterate them.
            for (qsizetype i = 0; i < data.size(); ++i) {
                switch (static_cast<unsigned char>(data[i])) {
                case 1: reply->caps.hasBasic = true; break;
                case 2: reply->caps.hasKerberos = true; break;
                case 3: reply->caps.hasDigestNoQop = true; break;
                case 4: reply->caps.hasDigestWithQop = true; break;
                default: break;
                }
            }
        } else if ((reply->authType == AuthType::DigestWithQop
                    || reply->authType == AuthType::DigestNoQop)
                   && reply->status == 1) {
            // Body is `<realmlen><realm><noncelen><nonce>[<qoplen><qop>]`.
            // Only populated on a challenge (status=1). status=0 (success)
            // has an empty body.
            qsizetype p = 0;
            auto readLp = [&](QString *out) {
                if (p >= data.size()) return false;
                const int n = static_cast<unsigned char>(data[p++]);
                if (data.size() - p < n) return false;
                *out = QString::fromUtf8(data.data() + p, n);
                p += n;
                return true;
            };
            if (!readLp(&reply->realm)) return false;
            if (!readLp(&reply->nonce)) return false;
            if (reply->authType == AuthType::DigestWithQop) {
                readLp(&reply->qop); // optional — some firmwares omit it
            }
        }
    }

    if (consumed != nullptr) *consumed = kAuthReplyFixedSize + static_cast<int>(dataLen);
    return true;
}

QString computeDigest(const QString &user, const QString &realm, const QString &pass,
                     const QString &nonce, const QString &nc, const QString &cnonce,
                     const QString &qop, const QString &authUri)
{
    const QString ha1 = md5Hex((user + QLatin1Char(':') + realm + QLatin1Char(':') + pass).toUtf8());
    const QString ha2 = md5Hex((QStringLiteral("POST:") + authUri).toUtf8());
    return md5Hex((ha1 + QLatin1Char(':') + nonce + QLatin1Char(':')
                   + nc + QLatin1Char(':') + cnonce + QLatin1Char(':')
                   + qop + QLatin1Char(':') + ha2).toUtf8());
}

QString makeClientNonce()
{
    quint64 r1 = QRandomGenerator::securelySeeded().generate64();
    quint64 r2 = QRandomGenerator::securelySeeded().generate64();
    return QStringLiteral("%1%2")
        .arg(r1, 16, 16, QLatin1Char('0'))
        .arg(r2, 16, 16, QLatin1Char('0'));
}

namespace {

QByteArray pack16Le(quint16 v)
{
    QByteArray b(2, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    return b;
}

quint16 read16Le(QByteArrayView v, int off)
{
    return static_cast<quint16>(static_cast<unsigned char>(v[off]))
         | static_cast<quint16>(static_cast<unsigned char>(v[off + 1])) << 8;
}

} // namespace

QByteArray buildSolOpen(quint32 sequence, const SolOpenParams &params)
{
    QByteArray f;
    f.append(char(0x20));
    f.append(char(0x00));
    f.append(char(0x00));
    f.append(char(0x00));
    f.append(pack32Le(sequence));
    f.append(pack16Le(params.maxTxBuffer));
    f.append(pack16Le(params.txTimeoutMs));
    f.append(pack16Le(params.txOverflowTimeoutMs));
    f.append(pack16Le(params.rxTimeoutMs));
    f.append(pack16Le(params.rxFlushTimeoutMs));
    f.append(pack16Le(params.heartbeatMs));
    f.append(pack32Le(0));
    return f;
}

bool tryParseSolOpenReply(QByteArrayView buffer, SolOpenReply *reply, int *consumed)
{
    constexpr int kSize = 23;
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < 1) return false;
    if (static_cast<unsigned char>(buffer[0]) != 0x21) return false;
    if (buffer.size() < kSize) return false;
    if (reply != nullptr) reply->status = static_cast<unsigned char>(buffer[9]);
    if (consumed != nullptr) *consumed = kSize;
    return true;
}

QByteArray buildSolSettings(quint32 sequence)
{
    QByteArray f;
    f.append(char(0x27));
    f.append(char(0x00));
    f.append(char(0x00));
    f.append(char(0x00));
    f.append(pack32Le(sequence));
    static constexpr unsigned char kFixed[] = {0x00, 0x00, 0x1B, 0x00, 0x00, 0x00};
    f.append(reinterpret_cast<const char *>(kFixed), sizeof(kFixed));
    return f;
}

QByteArray buildSolDataToHost(QByteArrayView data)
{
    Q_ASSERT(data.size() <= 0xFFFF);
    QByteArray f;
    f.append(char(0x28));
    f.append(7, '\0');
    f.append(pack16Le(static_cast<quint16>(data.size())));
    f.append(data.data(), static_cast<int>(data.size()));
    return f;
}

bool tryParseSolDisplayData(QByteArrayView buffer, QByteArray *data, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < 1) return false;
    if (static_cast<unsigned char>(buffer[0]) != 0x2A) return false;
    if (buffer.size() < 10) return false;
    const quint16 n = read16Le(buffer, 8);
    if (buffer.size() < 10 + static_cast<int>(n)) return false;
    if (data != nullptr) *data = QByteArray(buffer.data() + 10, n);
    if (consumed != nullptr) *consumed = 10 + static_cast<int>(n);
    return true;
}

QByteArray buildSolKeepalive()
{
    QByteArray f;
    f.append(char(0x2B));
    f.append(7, '\0');
    return f;
}

} // namespace qumesh::redir
