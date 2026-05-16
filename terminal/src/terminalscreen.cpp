// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "terminal/terminalscreen.h"

#include <cstdio>

namespace qumesh::terminal {

namespace {

// xterm-256 palette. Indices 0–7 are the standard ANSI base, 8–15
// are the "bright" variants (both tuned to match qml/Theme/Colors.qml
// where reasonable). 16–231 are the 6×6×6 RGB cube. 232–255 are the
// 24-step grayscale ramp. Index 0xFF (the default sentinel) is
// special-cased by `lineHtml` and never indexed into this table; that
// also means the literal xterm-256 entry 255 is unreachable, which we
// accept as it is near-white grayscale and rarely emitted.
struct Palette
{
    char entries[256][8];
    Palette()
    {
        static constexpr const char *kBase[16] = {
            "#0E0F12", "#E25555", "#5EE08A", "#E0C46C",
            "#5BB1F0", "#C58CF0", "#5EE0D8", "#C8CBD1",
            "#3A3D45", "#FF7878", "#7CF4A8", "#FFD888",
            "#7DC8FF", "#D9A8FF", "#7CFFF4", "#FFFFFF",
        };
        for (int i = 0; i < 16; ++i) {
            std::snprintf(entries[i], sizeof(entries[i]), "%s", kBase[i]);
        }
        // 6×6×6 cube: r,g,b ∈ {0, 95, 135, 175, 215, 255}.
        static constexpr int kSteps[6] = {0, 95, 135, 175, 215, 255};
        for (int r = 0; r < 6; ++r) {
            for (int g = 0; g < 6; ++g) {
                for (int b = 0; b < 6; ++b) {
                    const int idx = 16 + 36 * r + 6 * g + b;
                    std::snprintf(entries[idx], sizeof(entries[idx]),
                                  "#%02X%02X%02X", kSteps[r], kSteps[g], kSteps[b]);
                }
            }
        }
        // Grayscale ramp: 8, 18, 28, … 238.
        for (int i = 0; i < 24; ++i) {
            const int v = 8 + i * 10;
            std::snprintf(entries[232 + i], sizeof(entries[232 + i]),
                          "#%02X%02X%02X", v, v, v);
        }
    }
};

const Palette &palette()
{
    static const Palette p;
    return p;
}

QString escapeHtml(QChar c)
{
    switch (c.unicode()) {
    case u'<': return QStringLiteral("&lt;");
    case u'>': return QStringLiteral("&gt;");
    case u'&': return QStringLiteral("&amp;");
    case u' ': return QStringLiteral("&nbsp;");
    default: return QString(c);
    }
}

} // namespace

TerminalScreen::TerminalScreen(QObject *parent)
    : QObject(parent), m_parser(this)
{
    m_cells.resize(m_rows * m_columns);
}

const Cell &TerminalScreen::cell(int row, int column) const
{
    static const Cell empty;
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
        return empty;
    return m_cells.at(row * m_columns + column);
}

Cell &TerminalScreen::cellMut(int row, int column)
{
    return m_cells[row * m_columns + column];
}

void TerminalScreen::resize(int newRows, int newColumns)
{
    if (newRows <= 0 || newColumns <= 0) return;
    if (newRows == m_rows && newColumns == m_columns) return;

    QVector<Cell> next(newRows * newColumns);
    const int copyRows = qMin(m_rows, newRows);
    const int copyCols = qMin(m_columns, newColumns);
    for (int r = 0; r < copyRows; ++r) {
        for (int c = 0; c < copyCols; ++c) {
            next[r * newColumns + c] = m_cells.at(r * m_columns + c);
        }
    }
    m_cells = std::move(next);
    m_rows = newRows;
    m_columns = newColumns;
    m_cursorRow = qBound(0, m_cursorRow, m_rows - 1);
    m_cursorColumn = qBound(0, m_cursorColumn, m_columns - 1);
    emit geometryChanged();
    emit cursorMoved();
    touch();
}

void TerminalScreen::feed(const QByteArray &bytes)
{
    m_parser.feed(QByteArrayView(bytes));
    touch();
}

void TerminalScreen::clear()
{
    for (Cell &c : m_cells) c = Cell{};
    m_cursorRow = 0;
    m_cursorColumn = 0;
    m_pen = Cell{};
    emit cursorMoved();
    touch();
}

QString TerminalScreen::lineHtml(int row) const
{
    if (row < 0 || row >= m_rows) return {};
    QString out;
    out.reserve(m_columns * 6);

    int runStart = 0;
    auto flushRun = [&](int endExclusive) {
        if (endExclusive <= runStart) return;
        const Cell &c0 = m_cells.at(row * m_columns + runStart);

        quint8 fgIdx = c0.fg;
        quint8 bgIdx = c0.bg;
        if (c0.attrs & AttrReverse) std::swap(fgIdx, bgIdx);

        QString styles;
        if (fgIdx != 0xFF) {
            styles += QStringLiteral("color:");
            styles += QLatin1String(palette().entries[fgIdx]);
            styles += QLatin1Char(';');
        }
        if (bgIdx != 0xFF) {
            styles += QStringLiteral("background-color:");
            styles += QLatin1String(palette().entries[bgIdx]);
            styles += QLatin1Char(';');
        }
        if (c0.attrs & AttrBold)      styles += QStringLiteral("font-weight:bold;");
        if (c0.attrs & AttrUnderline) styles += QStringLiteral("text-decoration:underline;");

        if (!styles.isEmpty()) {
            out += QStringLiteral("<span style=\"");
            out += styles;
            out += QStringLiteral("\">");
        }
        for (int i = runStart; i < endExclusive; ++i) {
            out += escapeHtml(m_cells.at(row * m_columns + i).ch);
        }
        if (!styles.isEmpty()) out += QStringLiteral("</span>");
    };

    auto sameRun = [&](int a, int b) {
        const Cell &x = m_cells.at(row * m_columns + a);
        const Cell &y = m_cells.at(row * m_columns + b);
        return x.fg == y.fg && x.bg == y.bg && x.attrs == y.attrs;
    };

    for (int i = 1; i < m_columns; ++i) {
        if (!sameRun(runStart, i)) {
            flushRun(i);
            runStart = i;
        }
    }
    flushRun(m_columns);
    return out;
}

void TerminalScreen::putCellAtCursor(QChar c)
{
    if (m_cursorColumn >= m_columns) {
        // Stay on the last column until an explicit move; matches most
        // BIOS firmware that emits its own CR before wrapping.
        m_cursorColumn = m_columns - 1;
    }
    Cell &dst = cellMut(m_cursorRow, m_cursorColumn);
    dst = m_pen;
    dst.ch = c;
    if (m_cursorColumn < m_columns - 1) {
        ++m_cursorColumn;
    } else {
        // One past the last column to signal "next write triggers wrap";
        // we clamp again on the next putCellAtCursor.
        m_cursorColumn = m_columns;
    }
    emit cursorMoved();
}

void TerminalScreen::setCursor(int row, int column)
{
    m_cursorRow = qBound(0, row, m_rows - 1);
    m_cursorColumn = qBound(0, column, m_columns - 1);
    emit cursorMoved();
}

void TerminalScreen::moveCursor(int rowDelta, int columnDelta)
{
    setCursor(m_cursorRow + rowDelta, m_cursorColumn + columnDelta);
}

void TerminalScreen::carriageReturn()
{
    m_cursorColumn = 0;
    emit cursorMoved();
}

void TerminalScreen::lineFeed()
{
    if (m_cursorRow + 1 >= m_rows) {
        scrollUp();
    } else {
        ++m_cursorRow;
    }
    emit cursorMoved();
}

void TerminalScreen::backspace()
{
    if (m_cursorColumn > 0) {
        --m_cursorColumn;
        emit cursorMoved();
    }
}

void TerminalScreen::tab()
{
    // 8-column stops.
    int next = ((m_cursorColumn / 8) + 1) * 8;
    if (next >= m_columns) next = m_columns - 1;
    m_cursorColumn = next;
    emit cursorMoved();
}

void TerminalScreen::eraseInLine(int mode)
{
    if (mode == 0) {
        for (int c = m_cursorColumn; c < m_columns; ++c) cellMut(m_cursorRow, c) = Cell{};
    } else if (mode == 1) {
        for (int c = 0; c <= m_cursorColumn && c < m_columns; ++c) cellMut(m_cursorRow, c) = Cell{};
    } else if (mode == 2) {
        for (int c = 0; c < m_columns; ++c) cellMut(m_cursorRow, c) = Cell{};
    }
}

void TerminalScreen::eraseInDisplay(int mode)
{
    auto clearRow = [&](int r) {
        for (int c = 0; c < m_columns; ++c) cellMut(r, c) = Cell{};
    };
    if (mode == 0) {
        for (int c = m_cursorColumn; c < m_columns; ++c) cellMut(m_cursorRow, c) = Cell{};
        for (int r = m_cursorRow + 1; r < m_rows; ++r) clearRow(r);
    } else if (mode == 1) {
        for (int r = 0; r < m_cursorRow; ++r) clearRow(r);
        for (int c = 0; c <= m_cursorColumn && c < m_columns; ++c) cellMut(m_cursorRow, c) = Cell{};
    } else {
        for (int r = 0; r < m_rows; ++r) clearRow(r);
    }
}

void TerminalScreen::resetAttributes()
{
    m_pen = Cell{};
}

void TerminalScreen::setAttr(quint8 bit, bool on)
{
    if (on) m_pen.attrs |= bit;
    else m_pen.attrs &= ~bit;
}

void TerminalScreen::saveCursor()
{
    m_savedRow = m_cursorRow;
    m_savedColumn = m_cursorColumn;
}

void TerminalScreen::restoreCursor()
{
    setCursor(m_savedRow, m_savedColumn);
}

void TerminalScreen::scrollUp()
{
    for (int r = 0; r < m_rows - 1; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            m_cells[r * m_columns + c] = m_cells.at((r + 1) * m_columns + c);
        }
    }
    for (int c = 0; c < m_columns; ++c) {
        m_cells[(m_rows - 1) * m_columns + c] = Cell{};
    }
}

void TerminalScreen::touch()
{
    ++m_version;
    emit screenUpdated();
}

void TerminalScreen::emitResponse(const QByteArray &bytes)
{
    if (bytes.isEmpty()) return;
    emit respond(bytes);
}

} // namespace qumesh::terminal
