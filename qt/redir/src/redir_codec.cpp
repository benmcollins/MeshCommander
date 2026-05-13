#include "redir/redir_codec.h"

namespace meshcommander::redir {

namespace {

// Fixed selector bytes per Intel's redirection protocol. The 0x10 command
// byte is followed by a 3-byte header (version + reserved) and then the
// 4-byte ASCII tag identifying the sub-protocol.
constexpr unsigned char kSolSelector[] = {0x10, 0x00, 0x00, 0x00,
                                          0x53, 0x4F, 0x4C, 0x20}; // "SOL "
constexpr unsigned char kKvmSelector[] = {0x10, 0x01, 0x00, 0x00,
                                          0x4B, 0x56, 0x4D, 0x52}; // "KVMR"
constexpr unsigned char kIderSelector[] = {0x10, 0x00, 0x00, 0x00,
                                           0x49, 0x44, 0x45, 0x52}; // "IDER"

constexpr int kStartSessionReplyFixedSize = 13;

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

    // The reply has a 13-byte fixed header. Byte [12] is the OEM data length.
    if (buffer.size() < kStartSessionReplyFixedSize) return false;
    const int oemLen = static_cast<unsigned char>(buffer[12]);
    if (buffer.size() < kStartSessionReplyFixedSize + oemLen) return false;

    if (reply != nullptr) {
        reply->status = static_cast<StartSessionStatus>(static_cast<unsigned char>(buffer[1]));
        reply->oemData = QByteArray(buffer.data() + kStartSessionReplyFixedSize, oemLen);
    }
    if (consumed != nullptr) *consumed = kStartSessionReplyFixedSize + oemLen;
    return true;
}

} // namespace meshcommander::redir
