// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArrayView>
#include <QString>
#include <QVector>

namespace qumesh::terminal {

class TerminalScreen;

/// Minimal VT100/ANSI parser sufficient for AMT BIOS prompts and Linux
/// serial console output. Drives a `TerminalScreen`. The parser is
/// byte-oriented; it accumulates UTF-8 multi-byte sequences across feeds.
///
/// Supported escapes:
///   - C0:                BS, CR, LF, HT, BEL
///   - CSI Pn ; Pn ... A  cursor up
///   - CSI Pn ; Pn ... B  cursor down
///   - CSI Pn ; Pn ... C  cursor forward
///   - CSI Pn ; Pn ... D  cursor back
///   - CSI Pn ; Pn ... H  cursor position (row;col, 1-based)
///   - CSI Pn ; Pn ... f  same as H
///   - CSI Pn J           erase in display (0=below, 1=above, 2=all)
///   - CSI Pn K           erase in line (0=right, 1=left, 2=whole)
///   - CSI Pn m           SGR (colors / attributes; 16/256/truecolor)
///   - CSI s              save cursor
///   - CSI Ps t           XTWINOPS — 14/18/19 reply with size; rest ignored
///   - CSI u              restore cursor
///   - CSI ? Pn h/l       DEC private modes (consumed, mostly ignored)
///   - OSC ... BEL/ST     consumed (window title etc., ignored)
class Vt100Parser
{
public:
    explicit Vt100Parser(TerminalScreen *screen);

    /// Feed raw bytes from the serial channel. Multi-byte UTF-8 sequences
    /// and partial escape sequences are buffered across calls.
    void feed(QByteArrayView bytes);

    /// Drop any in-progress state (escape buffer, UTF-8 carry).
    void reset();

private:
    enum class State {
        Ground,
        Escape,
        Csi,
        Osc,
    };

    void handleCsi();
    void handleSgr(const QVector<int> &params);
    void handleWindowOps(const QVector<int> &params);
    void handleC0(unsigned char b);
    void putChar(QChar c);
    QChar consumeUtf8(unsigned char b);

    TerminalScreen *m_screen;
    State m_state = State::Ground;
    QByteArray m_seq;
    QByteArray m_utf8;
    int m_utf8Remaining = 0;
    bool m_csiPrivate = false;
};

} // namespace qumesh::terminal
