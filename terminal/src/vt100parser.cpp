// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "terminal/vt100parser.h"

#include "terminal/terminalscreen.h"

#include <optional>

namespace qumesh::terminal {

namespace {

constexpr unsigned char kEsc = 0x1B;
constexpr unsigned char kBel = 0x07;

/// Upper bound on a single CSI accumulator. Legitimate CSI bodies
/// (parameters + intermediates + final) are at most a handful of
/// bytes; cap at 4 KB so a malformed stream like
/// `\x1B[ + megabytes of digits` can't grow `m_seq` without bound.
/// See #289.
constexpr qsizetype kCsiMaxBytes = 4096;

bool isCsiFinal(unsigned char b) { return b >= 0x40 && b <= 0x7E; }

QVector<int> parseParams(QByteArrayView body, int *outIntermediateStart)
{
    // Body excludes the leading "[" and the trailing final byte; it
    // may contain digits, ';', and (optionally) intermediate bytes
    // 0x20–0x2F before the final. We split on ';'.
    QVector<int> out;
    int n = -1;
    int i = 0;
    for (; i < body.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(body[i]);
        if (b == ';') {
            out.push_back(n < 0 ? 0 : n);
            n = -1;
        } else if (b >= '0' && b <= '9') {
            if (n < 0) n = 0;
            n = n * 10 + (b - '0');
        } else {
            break; // intermediate or junk — stop parsing params
        }
    }
    out.push_back(n < 0 ? 0 : n);
    if (outIntermediateStart != nullptr) *outIntermediateStart = i;
    return out;
}

} // namespace

Vt100Parser::Vt100Parser(TerminalScreen *screen) : m_screen(screen) {}

void Vt100Parser::reset()
{
    m_state = State::Ground;
    m_seq.clear();
    m_utf8.clear();
    m_utf8Remaining = 0;
    m_csiPrivate = false;
}

void Vt100Parser::feed(QByteArrayView bytes)
{
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(bytes[i]);

        switch (m_state) {
        case State::Ground:
            if (b == kEsc) {
                m_state = State::Escape;
                m_seq.clear();
                m_csiPrivate = false;
            } else if (b < 0x20) {
                handleC0(b);
            } else if (b < 0x80) {
                const QChar c(b);
                putChar(QStringView(&c, 1));
            } else {
                const QString s = consumeUtf8(b);
                if (!s.isEmpty()) putChar(QStringView(s));
            }
            break;

        case State::Escape:
            if (b == '[') {
                m_state = State::Csi;
            } else if (b == ']') {
                m_state = State::Osc;
            } else if (b == 'c') {
                // RIS — full reset.
                m_screen->clear();
                m_state = State::Ground;
            } else if (b == '7') {
                m_screen->saveCursor();
                m_state = State::Ground;
            } else if (b == '8') {
                m_screen->restoreCursor();
                m_state = State::Ground;
            } else {
                // Two-byte ESC sequences we don't model — drop and resume.
                m_state = State::Ground;
            }
            break;

        case State::Csi:
            if (m_seq.isEmpty() && b == '?') {
                m_csiPrivate = true;
                continue;
            }
            // Cap the CSI accumulator. Real CSI sequences are tiny
            // (a handful of digits + ';' separators); a malformed
            // stream of `\x1B[` followed by megabytes of digits used
            // to grow m_seq without bound. On overflow drop back to
            // Ground and discard the partial sequence — the rest of
            // the stream will resync at the next ESC. See #289.
            if (m_seq.size() >= kCsiMaxBytes) {
                m_seq.clear();
                m_csiPrivate = false;
                m_state = State::Ground;
                break;
            }
            m_seq.append(static_cast<char>(b));
            if (isCsiFinal(b)) {
                handleCsi();
                m_seq.clear();
                m_csiPrivate = false;
                m_state = State::Ground;
            }
            break;

        case State::Osc:
            // OSC ends on BEL or ST (ESC \). We ignore the payload —
            // BIOS rarely sends OSC, and Linux's window-title escape
            // is harmless to drop.
            if (b == kBel) {
                m_state = State::Ground;
            } else if (b == kEsc) {
                // ST is the next byte; consume the trailing '\' on next pass.
                m_state = State::Escape; // re-enter ESC to swallow the '\'
            }
            break;
        }
    }
}

QString Vt100Parser::consumeUtf8(unsigned char b)
{
    if (m_utf8Remaining == 0) {
        if ((b & 0xE0) == 0xC0) {
            m_utf8Remaining = 1;
        } else if ((b & 0xF0) == 0xE0) {
            m_utf8Remaining = 2;
        } else if ((b & 0xF8) == 0xF0) {
            m_utf8Remaining = 3;
        } else {
            return QStringLiteral("?");
        }
        m_utf8.clear();
        m_utf8.append(static_cast<char>(b));
        return {};
    }

    m_utf8.append(static_cast<char>(b));
    if (--m_utf8Remaining == 0) {
        // Decode the complete UTF-8 sequence. For non-BMP code points
        // this is a 2-`QChar` surrogate pair; passing the whole string
        // through (not just `s.at(0)`) is what fixes #370.
        QString s = QString::fromUtf8(m_utf8);
        m_utf8.clear();
        return s.isEmpty() ? QStringLiteral("?") : s;
    }
    return {};
}

void Vt100Parser::handleC0(unsigned char b)
{
    switch (b) {
    case 0x07: emit m_screen->bell(); break;
    case 0x08: m_screen->backspace(); break;
    case 0x09: m_screen->tab(); break;
    case 0x0A:
    case 0x0B:
    case 0x0C:
        m_screen->lineFeed();
        break;
    case 0x0D: m_screen->carriageReturn(); break;
    default: break; // SI/SO/etc. — ignore
    }
}

void Vt100Parser::putChar(QStringView fragment) { m_screen->putCellAtCursor(fragment); }

void Vt100Parser::handleCsi()
{
    if (m_seq.isEmpty()) return;
    const char final = m_seq.back();
    const QByteArrayView body(m_seq.constData(), m_seq.size() - 1);

    int intermediateStart = 0;
    const QVector<int> params = parseParams(body, &intermediateStart);

    auto p = [&](int idx, int defaultVal) {
        return (idx < params.size() && params.at(idx) > 0) ? params.at(idx) : defaultVal;
    };

    if (m_csiPrivate) {
        // DEC private mode set/reset (?25h/?25l for cursor visibility,
        // ?1049h for alternate screen, etc.) — consumed but not modeled.
        return;
    }

    switch (final) {
    case 'A': m_screen->moveCursor(-p(0, 1), 0); break;
    case 'B': m_screen->moveCursor(p(0, 1), 0); break;
    case 'C': m_screen->moveCursor(0, p(0, 1)); break;
    case 'D': m_screen->moveCursor(0, -p(0, 1)); break;
    case 'E': m_screen->setCursor(m_screen->cursorRow() + p(0, 1), 0); break;
    case 'F': m_screen->setCursor(m_screen->cursorRow() - p(0, 1), 0); break;
    case 'G': m_screen->setCursor(m_screen->cursorRow(), p(0, 1) - 1); break;
    case 'H':
    case 'f':
        m_screen->setCursor(p(0, 1) - 1, p(1, 1) - 1);
        break;
    case 'J': m_screen->eraseInDisplay(params.value(0, 0)); break;
    case 'K': m_screen->eraseInLine(params.value(0, 0)); break;
    case 'm': handleSgr(params); break;
    case 's': m_screen->saveCursor(); break;
    case 't': handleWindowOps(params); break;
    case 'u': m_screen->restoreCursor(); break;
    default: break;
    }
}

void Vt100Parser::handleWindowOps(const QVector<int> &params)
{
    // XTWINOPS (xterm window manipulation). We answer the read-only
    // size queries — that's what `resize(1)` uses to learn the
    // terminal geometry without the user having to type `stty`.
    // Active operations (move window, raise, iconify, …) are ignored:
    // we are a terminal emulator, not a window manager for the
    // remote host.
    const int op = params.value(0, 0);
    const int rows = m_screen->rows();
    const int cols = m_screen->columns();
    QByteArray reply;
    switch (op) {
    case 14:
        // Pixel size: report cells × nominal 8×16. `resize` doesn't
        // consult pixel size and the few apps that do (legacy X) get
        // a plausible number rather than nothing.
        reply = QByteArrayLiteral("\x1b[4;")
              + QByteArray::number(rows * 16) + ';'
              + QByteArray::number(cols * 8) + 't';
        break;
    case 18:
        reply = QByteArrayLiteral("\x1b[8;")
              + QByteArray::number(rows) + ';'
              + QByteArray::number(cols) + 't';
        break;
    case 19:
        // Screen size matches text-area size for serial; we have no
        // separate "monitor" geometry to report.
        reply = QByteArrayLiteral("\x1b[9;")
              + QByteArray::number(rows) + ';'
              + QByteArray::number(cols) + 't';
        break;
    default:
        return;
    }
    m_screen->emitResponse(reply);
}

namespace {

/// Map an 8-bit RGB triple to the nearest xterm-256 cube index. Truecolor
/// in (38;2;r;g;b) form gets quantized through the 6×6×6 palette since
/// our Cell only stores a single byte per channel.
quint8 quantizeRgbTo256(int r, int g, int b)
{
    auto step = [](int v) {
        // xterm cube anchors at 0, 95, 135, 175, 215, 255. Pick the
        // closest one rather than naive `v / 51` so values just under
        // 95 don't round down to black.
        if (v < 48) return 0;
        if (v < 115) return 1;
        return (v - 35) / 40; // 2..5 for the 135/175/215/255 buckets
    };
    const int ri = qBound(0, step(r), 5);
    const int gi = qBound(0, step(g), 5);
    const int bi = qBound(0, step(b), 5);
    return static_cast<quint8>(16 + 36 * ri + 6 * gi + bi);
}

/// Consume an extended-color tail from an SGR param run starting at
/// `*i` (which points at the `38` or `48`). Advances `*i` past the
/// consumed indices and returns the resolved palette index, or
/// `std::nullopt` if the sequence is malformed (in which case `*i`
/// already points past the bad subseq so the outer loop can keep
/// going without re-interpreting our tail as ordinary SGR codes).
std::optional<quint8> consumeExtendedColor(const QVector<int> &params, int *i)
{
    const int n = params.size();
    if (*i + 1 >= n) {
        *i = n;
        return std::nullopt;
    }
    const int mode = params.at(*i + 1);
    if (mode == 5) {
        if (*i + 2 >= n) {
            *i = n;
            return std::nullopt;
        }
        const int idx = params.at(*i + 2);
        *i += 2;
        return static_cast<quint8>(qBound(0, idx, 255));
    }
    if (mode == 2) {
        if (*i + 4 >= n) {
            *i = n;
            return std::nullopt;
        }
        const int r = params.at(*i + 2);
        const int g = params.at(*i + 3);
        const int b = params.at(*i + 4);
        *i += 4;
        return quantizeRgbTo256(r, g, b);
    }
    // Unknown extension; skip just `mode` so we resync on the next ';'.
    *i += 1;
    return std::nullopt;
}

} // namespace

void Vt100Parser::handleSgr(const QVector<int> &params)
{
    if (params.isEmpty()) {
        m_screen->resetAttributes();
        return;
    }
    for (int i = 0; i < params.size(); ++i) {
        const int code = params.at(i);
        if (code == 0) {
            m_screen->resetAttributes();
        } else if (code == 1) {
            m_screen->setAttr(AttrBold, true);
        } else if (code == 4) {
            m_screen->setAttr(AttrUnderline, true);
        } else if (code == 7) {
            m_screen->setAttr(AttrReverse, true);
        } else if (code == 22) {
            m_screen->setAttr(AttrBold, false);
        } else if (code == 24) {
            m_screen->setAttr(AttrUnderline, false);
        } else if (code == 27) {
            m_screen->setAttr(AttrReverse, false);
        } else if (code >= 30 && code <= 37) {
            m_screen->setFg(static_cast<quint8>(code - 30));
        } else if (code == 38) {
            const std::optional<quint8> idx = consumeExtendedColor(params, &i);
            if (idx.has_value()) m_screen->setFg(*idx);
        } else if (code == 39) {
            m_screen->setFg(0xFF);
        } else if (code >= 40 && code <= 47) {
            m_screen->setBg(static_cast<quint8>(code - 40));
        } else if (code == 48) {
            const std::optional<quint8> idx = consumeExtendedColor(params, &i);
            if (idx.has_value()) m_screen->setBg(*idx);
        } else if (code == 49) {
            m_screen->setBg(0xFF);
        } else if (code >= 90 && code <= 97) {
            m_screen->setFg(static_cast<quint8>(code - 90 + 8));
        } else if (code >= 100 && code <= 107) {
            m_screen->setBg(static_cast<quint8>(code - 100 + 8));
        }
    }
}

} // namespace qumesh::terminal
