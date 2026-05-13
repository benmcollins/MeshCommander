#pragma once

#include <QByteArray>
#include <QtTypes>

namespace meshcommander::redir {

/// Sub-protocols multiplexed over Intel's AMT Redirection port (16994/16995).
/// The value of the enumerator is the wire identifier byte used in the
/// 8-byte protocol selector that opens every session. See `buildSelector`.
enum class Protocol : quint8 {
    Sol = 1,
    Kvm = 2,
    Ider = 3,
};

/// Build the 8-byte StartRedirectionSession message (command `0x10`) for the
/// given sub-protocol. The byte sequences are fixed by Intel's spec:
/// - SOL : `10 00 00 00 53 4F 4C 20`   ("..SOL ")
/// - KVM : `10 01 00 00 4B 56 4D 52`   ("..KVMR")
/// - IDER: `10 00 00 00 49 44 45 52`   ("..IDER")
[[nodiscard]] QByteArray buildSelector(Protocol p);

/// Status codes the device may return inside a `StartRedirectionSessionReply`
/// (command `0x11`). Names mirror Intel's documentation.
enum class StartSessionStatus : quint8 {
    Success = 0,
    Busy = 2,
    Unsupported = 3,
    UnknownError = 0xFF,
};

/// Decoded `0x11` reply payload. The device may include arbitrary OEM bytes
/// after the fixed 13-byte header; they are surfaced unchanged in `oemData`.
struct StartSessionReply
{
    StartSessionStatus status = StartSessionStatus::UnknownError;
    QByteArray oemData;
};

/// Try to parse a `0x11` frame from the head of a buffer.
/// Returns `true` and fills `*reply` + `*consumed` when a complete frame is
/// available. Returns `false` when more bytes are needed (in that case
/// `*consumed` is left at 0) or when the buffer does not start with `0x11`.
[[nodiscard]] bool tryParseStartSessionReply(QByteArrayView buffer,
                                              StartSessionReply *reply, int *consumed);

} // namespace meshcommander::redir
