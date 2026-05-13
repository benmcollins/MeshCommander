// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "terminal/vt100parser.h"

#include "terminal/terminalscreen.h"

namespace meshcommander::terminal {

namespace {

constexpr unsigned char kEsc = 0x1B;
constexpr unsigned char kBel = 0x07;

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
                putChar(QChar(b));
            } else {
                const QChar c = consumeUtf8(b);
                if (!c.isNull()) putChar(c);
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

QChar Vt100Parser::consumeUtf8(unsigned char b)
{
    if (m_utf8Remaining == 0) {
        if ((b & 0xE0) == 0xC0) {
            m_utf8Remaining = 1;
        } else if ((b & 0xF0) == 0xE0) {
            m_utf8Remaining = 2;
        } else if ((b & 0xF8) == 0xF0) {
            m_utf8Remaining = 3;
        } else {
            return QChar(u'?');
        }
        m_utf8.clear();
        m_utf8.append(static_cast<char>(b));
        return {};
    }

    m_utf8.append(static_cast<char>(b));
    if (--m_utf8Remaining == 0) {
        const QString s = QString::fromUtf8(m_utf8);
        m_utf8.clear();
        return s.isEmpty() ? QChar(u'?') : s.at(0);
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

void Vt100Parser::putChar(QChar c) { m_screen->putCellAtCursor(c); }

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
    case 'u': m_screen->restoreCursor(); break;
    default: break;
    }
}

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
        } else if (code == 39) {
            m_screen->setFg(0xFF);
        } else if (code >= 40 && code <= 47) {
            m_screen->setBg(static_cast<quint8>(code - 40));
        } else if (code == 49) {
            m_screen->setBg(0xFF);
        } else if (code >= 90 && code <= 97) {
            m_screen->setFg(static_cast<quint8>(code - 90 + 8));
        } else if (code >= 100 && code <= 107) {
            m_screen->setBg(static_cast<quint8>(code - 100 + 8));
        }
        // 256-color (38;5;n / 48;5;n) and truecolor (38;2;r;g;b)
        // intentionally not modeled in v1.
    }
}

} // namespace meshcommander::terminal
