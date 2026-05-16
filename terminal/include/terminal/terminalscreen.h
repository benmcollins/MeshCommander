// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QChar>
#include <QObject>
#include <QString>
#include <QVector>

#include "terminal/vt100parser.h"

namespace qumesh::terminal {

/// Cell attribute bits. Reverse swaps fg/bg at render time.
enum CellAttr : quint8 {
    AttrNone      = 0x00,
    AttrBold      = 0x01,
    AttrUnderline = 0x02,
    AttrReverse   = 0x04,
};

/// Single character cell. Colors are 16-color ANSI indices (0..15),
/// with 0xFF meaning "default".
struct Cell {
    QChar ch = u' ';
    quint8 fg = 0xFF;
    quint8 bg = 0xFF;
    quint8 attrs = AttrNone;
};

/// A fixed-size character grid driven by `Vt100Parser`. Owns the cursor
/// and current pen. Exposed to QML as a singleton-like context object
/// (the app creates one per SOL session and registers it).
///
/// The model is "dumb": no scrollback (scrollback is handled in the QML
/// layer by appending lines on LF), no line wrap (cursor stops at right
/// margin until a CR or move command).
class TerminalScreen : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY geometryChanged)
    Q_PROPERTY(int columns READ columns NOTIFY geometryChanged)
    Q_PROPERTY(int cursorRow READ cursorRow NOTIFY cursorMoved)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY cursorMoved)
    Q_PROPERTY(int version READ version NOTIFY screenUpdated)

public:
    explicit TerminalScreen(QObject *parent = nullptr);

    [[nodiscard]] int rows() const { return m_rows; }
    [[nodiscard]] int columns() const { return m_columns; }
    [[nodiscard]] int cursorRow() const { return m_cursorRow; }
    [[nodiscard]] int cursorColumn() const { return m_cursorColumn; }

    /// Monotonically increasing counter, bumped after every `feed`. QML
    /// binds to this so a single property change triggers a repaint.
    [[nodiscard]] int version() const { return m_version; }

    [[nodiscard]] const Cell &cell(int row, int column) const;

    /// Resize the grid. Content is preserved where possible (top-left
    /// origin); cells outside the new bounds are dropped.
    Q_INVOKABLE void resize(int newRows, int newColumns);

    /// Push raw bytes through the VT100 parser.
    Q_INVOKABLE void feed(const QByteArray &bytes);

    /// Reset cursor, pen, and clear the grid.
    Q_INVOKABLE void clear();

    /// Render a row as HTML (one `<span>` per attribute run) for use
    /// with `Text.RichText`. Background/foreground are resolved to
    /// 16-color ANSI hex; default colours become empty (terminal
    /// inherits its parent palette).
    Q_INVOKABLE QString lineHtml(int row) const;

    /// Internal: cursor and attribute mutators driven by the parser.
    void putCellAtCursor(QChar c);
    void setCursor(int row, int column);
    void moveCursor(int rowDelta, int columnDelta);
    void carriageReturn();
    void lineFeed();
    void backspace();
    void tab();
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void resetAttributes();
    void setFg(quint8 idx) { m_pen.fg = idx; }
    void setBg(quint8 idx) { m_pen.bg = idx; }
    void setAttr(quint8 bit, bool on);
    void saveCursor();
    void restoreCursor();

    /// Emit screenUpdated. Parser calls this after each `feed`.
    void touch();

    /// Forward the bytes through the `respond` signal — used by the
    /// parser to surface replies (e.g. xterm window-ops responses)
    /// without depending on the SOL session directly.
    void emitResponse(const QByteArray &bytes);

signals:
    void geometryChanged();
    void cursorMoved();
    void screenUpdated();
    /// Emitted when the host requests a bell. The QML layer may flash
    /// the panel; no audible bell in v1.
    void bell();
    /// Bytes the parser wants to send back to the host in reply to an
    /// in-band query (xterm CSI Ps;Pt t window-ops, DSR, etc.). The
    /// controller wires this to `SolSession::sendInput` so replies
    /// travel up the redirection channel.
    void respond(const QByteArray &bytes);

private:
    int m_rows = 24;
    int m_columns = 80;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    int m_savedRow = 0;
    int m_savedColumn = 0;
    int m_version = 0;
    Cell m_pen;
    QVector<Cell> m_cells; // row-major, size = rows * columns
    Vt100Parser m_parser;

    Cell &cellMut(int row, int column);
    void scrollUp();
};

} // namespace qumesh::terminal
