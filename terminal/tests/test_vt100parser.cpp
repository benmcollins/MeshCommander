// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "terminal/terminalscreen.h"
#include "terminal/vt100parser.h"

#include <QSignalSpy>
#include <QtTest>

using namespace qumesh::terminal;

class TestVt100Parser : public QObject
{
    Q_OBJECT
private slots:
    void plainAsciiLandsAtCursor();
    void carriageReturnAndLineFeed();
    void cursorPositioning();
    void eraseInLineModes();
    void eraseInDisplayAll();
    void sgrColorAndReset();
    void sgrBoldAndUnderline();
    void sgr256ColorFgAndBg();
    void sgrTruecolorIsQuantized();
    void sgrExtendedColorDoesNotLeakIntoNextCode();
    void windowOpsReportsTextAreaSize();
    void windowOpsIgnoresActiveOps();
    void utf8MultiByteIsPreserved();
    void utf8FourByteNonBmpRoundTrips();
    void backspaceBeforeBol();
    void osCSequenceIsIgnored();
    void scrolling();
    void csiOverflowResetsToGround();
};

namespace {

QString rowText(const TerminalScreen &s, int row)
{
    QString out;
    out.reserve(s.columns());
    for (int c = 0; c < s.columns(); ++c) {
        out += s.cell(row, c).ch;
    }
    return out;
}

} // namespace

void TestVt100Parser::plainAsciiLandsAtCursor()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("hello"));
    QCOMPARE(rowText(s, 0).left(5), QStringLiteral("hello"));
    QCOMPARE(s.cursorRow(), 0);
    QCOMPARE(s.cursorColumn(), 5);
}

void TestVt100Parser::carriageReturnAndLineFeed()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("ab\r\ncd"));
    QCOMPARE(rowText(s, 0).left(2), QStringLiteral("ab"));
    QCOMPARE(rowText(s, 1).left(2), QStringLiteral("cd"));
    QCOMPARE(s.cursorRow(), 1);
    QCOMPARE(s.cursorColumn(), 2);
}

void TestVt100Parser::cursorPositioning()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("\x1b[5;10HX"));
    // CSI 5;10 H => row 5, col 10 (1-based) -> (4, 9) 0-based.
    QCOMPARE(s.cell(4, 9).ch, QChar(u'X'));
    QCOMPARE(s.cursorRow(), 4);
    QCOMPARE(s.cursorColumn(), 10);
}

void TestVt100Parser::eraseInLineModes()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("AAAAA\r"));
    s.feed(QByteArrayLiteral("\x1b[3C\x1b[0K"));
    QCOMPARE(s.cell(0, 0).ch, QChar(u'A'));
    QCOMPARE(s.cell(0, 1).ch, QChar(u'A'));
    QCOMPARE(s.cell(0, 2).ch, QChar(u'A'));
    QCOMPARE(s.cell(0, 3).ch, QChar(u' '));
    QCOMPARE(s.cell(0, 4).ch, QChar(u' '));
}

void TestVt100Parser::eraseInDisplayAll()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("hello\r\nworld"));
    s.feed(QByteArrayLiteral("\x1b[2J"));
    QCOMPARE(s.cell(0, 0).ch, QChar(u' '));
    QCOMPARE(s.cell(1, 0).ch, QChar(u' '));
}

void TestVt100Parser::sgrColorAndReset()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("\x1b[31mR\x1b[0mN"));
    QCOMPARE(s.cell(0, 0).fg, quint8(1));
    QCOMPARE(s.cell(0, 1).fg, quint8(0xFF));
}

void TestVt100Parser::sgrBoldAndUnderline()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("\x1b[1;4mX\x1b[22;24mY"));
    QCOMPARE(s.cell(0, 0).attrs & AttrBold, quint8(AttrBold));
    QCOMPARE(s.cell(0, 0).attrs & AttrUnderline, quint8(AttrUnderline));
    QCOMPARE(s.cell(0, 1).attrs & AttrBold, quint8(0));
    QCOMPARE(s.cell(0, 1).attrs & AttrUnderline, quint8(0));
}

void TestVt100Parser::utf8MultiByteIsPreserved()
{
    TerminalScreen s;
    s.feed(QStringLiteral("ä好").toUtf8());
    QCOMPARE(s.cell(0, 0).ch, QChar(u'ä'));
    QCOMPARE(s.cell(0, 1).ch, QChar(u'好'));
}

/// #370 — `consumeUtf8` used to return only `s.at(0)` (the high
/// surrogate) for a 4-byte UTF-8 sequence, silently dropping the low
/// surrogate. We now thread both halves through to the cell. Use
/// U+10000 (LINEAR B SYLLABLE B008 A) — a non-BMP codepoint encoded
/// in UTF-8 as `F0 90 80 80` — so the assertion doesn't rely on
/// emojis or shaping behaviour.
void TestVt100Parser::utf8FourByteNonBmpRoundTrips()
{
    TerminalScreen s;
    const QByteArray bytes = QByteArrayLiteral("\xF0\x90\x80\x80");
    s.feed(bytes);

    const Cell &c0 = s.cell(0, 0);
    QVERIFY(c0.ch.isHighSurrogate());
    QVERIFY(c0.lowSurrogate.isLowSurrogate());
    QCOMPARE(QChar::surrogateToUcs4(c0.ch, c0.lowSurrogate), char32_t(0x10000));

    // The parser advances the cursor by exactly one cell — one
    // user-visible character occupies one cell regardless of how
    // many UTF-16 code units it takes.
    QCOMPARE(s.cursorRow(), 0);
    QCOMPARE(s.cursorColumn(), 1);

    // `lineHtml` must surface both surrogate halves so the rendered
    // string is valid UTF-16. A lone high surrogate would round-trip
    // through `QString::toUtf8` as the replacement character.
    const QString html = s.lineHtml(0);
    const char32_t codepoint = 0x10000;
    QVERIFY(html.contains(QString::fromUcs4(&codepoint, 1)));
}

void TestVt100Parser::backspaceBeforeBol()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("ab\b"));
    QCOMPARE(s.cursorColumn(), 1);
    s.feed(QByteArrayLiteral("\b\b\b"));
    QCOMPARE(s.cursorColumn(), 0);
}

void TestVt100Parser::osCSequenceIsIgnored()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("A\x1b]0;ignored title\x07Z"));
    QCOMPARE(s.cell(0, 0).ch, QChar(u'A'));
    QCOMPARE(s.cell(0, 1).ch, QChar(u'Z'));
}

void TestVt100Parser::sgr256ColorFgAndBg()
{
    TerminalScreen s;
    // 256-color: fg 208 (orange), bg 17 (deep blue).
    s.feed(QByteArrayLiteral("\x1b[38;5;208mA\x1b[48;5;17mB\x1b[0mC"));
    QCOMPARE(s.cell(0, 0).fg, quint8(208));
    QCOMPARE(s.cell(0, 0).bg, quint8(0xFF));
    QCOMPARE(s.cell(0, 1).fg, quint8(208));
    QCOMPARE(s.cell(0, 1).bg, quint8(17));
    // Reset returns to default.
    QCOMPARE(s.cell(0, 2).fg, quint8(0xFF));
    QCOMPARE(s.cell(0, 2).bg, quint8(0xFF));
}

void TestVt100Parser::sgrTruecolorIsQuantized()
{
    TerminalScreen s;
    // Pure red 255,0,0 — quantizes to xterm cube index 16 + 36*5 = 196.
    s.feed(QByteArrayLiteral("\x1b[38;2;255;0;0mR\x1b[0m"));
    QCOMPARE(s.cell(0, 0).fg, quint8(196));
    // Truecolor bg with mid grey — should map somewhere inside the cube,
    // not into ANSI base. We only assert the index is non-default and >= 16.
    s.feed(QByteArrayLiteral("\x1b[48;2;128;128;128mG"));
    QVERIFY(s.cell(0, 1).bg != quint8(0xFF));
    QVERIFY(s.cell(0, 1).bg >= quint8(16));
}

void TestVt100Parser::sgrExtendedColorDoesNotLeakIntoNextCode()
{
    TerminalScreen s;
    // The tail of an extended-color sequence must not be re-interpreted
    // as a separate SGR. Here the `4` (would-be underline) is actually
    // the green component of a truecolor fg.
    s.feed(QByteArrayLiteral("\x1b[38;2;0;0;4mX"));
    QCOMPARE(s.cell(0, 0).attrs & AttrUnderline, quint8(0));
    // And in 256-color form: `1` after `5` is the palette index, not bold.
    s.feed(QByteArrayLiteral("\x1b[0m\x1b[38;5;1;48;5;0mY"));
    QCOMPARE(s.cell(0, 1).attrs & AttrBold, quint8(0));
    QCOMPARE(s.cell(0, 1).fg, quint8(1));
    QCOMPARE(s.cell(0, 1).bg, quint8(0));
}

void TestVt100Parser::windowOpsReportsTextAreaSize()
{
    TerminalScreen s;
    s.resize(28, 108);
    QSignalSpy spy(&s, &TerminalScreen::respond);

    // CSI 18 t — "report text area size in characters".
    s.feed(QByteArrayLiteral("\x1b[18t"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArrayLiteral("\x1b[8;28;108t"));

    // CSI 19 t — "report screen size in characters". For a serial
    // session the screen size matches the text area.
    s.feed(QByteArrayLiteral("\x1b[19t"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArrayLiteral("\x1b[9;28;108t"));

    // CSI 14 t — pixel size. We don't track pixels; the response
    // approximates via cells × nominal 8×16 so callers that read it
    // get a self-consistent number.
    s.feed(QByteArrayLiteral("\x1b[14t"));
    QCOMPARE(spy.count(), 1);
    const QByteArray pix = spy.takeFirst().at(0).toByteArray();
    QCOMPARE(pix, QByteArrayLiteral("\x1b[4;448;864t"));
}

void TestVt100Parser::windowOpsIgnoresActiveOps()
{
    TerminalScreen s;
    QSignalSpy spy(&s, &TerminalScreen::respond);

    // Active ops we deliberately don't honor: 1=de-iconify, 2=iconify,
    // 3=move, 4=resize-pixels, 5=raise, 8=resize-chars.
    s.feed(QByteArrayLiteral("\x1b[1t"));
    s.feed(QByteArrayLiteral("\x1b[2t"));
    s.feed(QByteArrayLiteral("\x1b[3;100;100t"));
    s.feed(QByteArrayLiteral("\x1b[8;50;80t"));
    QCOMPARE(spy.count(), 0);
}

void TestVt100Parser::scrolling()
{
    TerminalScreen s;
    s.resize(3, 4);
    s.feed(QByteArrayLiteral("AA\r\nBB\r\nCC\r\nDD"));
    // Top row should have scrolled off; row 0 should now be "BB".
    QCOMPARE(s.cell(0, 0).ch, QChar(u'B'));
    QCOMPARE(s.cell(0, 1).ch, QChar(u'B'));
    QCOMPARE(s.cell(2, 0).ch, QChar(u'D'));
    QCOMPARE(s.cell(2, 1).ch, QChar(u'D'));
}

/// #289 — A malformed CSI of `\x1B[` followed by megabytes of digits
/// used to grow the parser's accumulator without bound. The parser
/// now caps at 4 KB; on overflow it drops to Ground and discards
/// the partial sequence. Bytes that follow are then processed
/// normally. We prove that by feeding a known-good CSI after the
/// runaway one and confirming it ran: the trailing CUP (`H`) homes
/// the cursor, and the character right after lands at (0,0).
void TestVt100Parser::csiOverflowResetsToGround()
{
    TerminalScreen s;
    s.resize(4, 8);

    QByteArray junk;
    junk.append('\x1B');
    junk.append('[');
    junk.append(QByteArray(8192, '5'));  // way over the 4 KB cap

    // Drive the parser through the runaway CSI. Should not allocate
    // 8 KB into m_seq, throw, or deadlock.
    s.feed(junk);

    // After the cap is hit, m_seq was cleared and we're back in
    // Ground. Send a valid CUP + a character to home the cursor and
    // land the character at (0,0). This proves the parser resynced
    // cleanly — if we'd been stuck in CSI state, the leading 0x1B
    // wouldn't have re-entered.
    s.feed(QByteArrayLiteral("\x1B[H!"));
    QCOMPARE(s.cell(0, 0).ch, QChar(u'!'));
}

QTEST_GUILESS_MAIN(TestVt100Parser)
#include "test_vt100parser.moc"
