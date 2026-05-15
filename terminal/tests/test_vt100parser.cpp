// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "terminal/terminalscreen.h"
#include "terminal/vt100parser.h"

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
    void utf8MultiByteIsPreserved();
    void backspaceBeforeBol();
    void osCSequenceIsIgnored();
    void scrolling();
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

QTEST_GUILESS_MAIN(TestVt100Parser)
#include "test_vt100parser.moc"
