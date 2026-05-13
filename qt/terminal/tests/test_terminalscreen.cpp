// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "terminal/terminalscreen.h"

#include <QSignalSpy>
#include <QtTest>

using namespace meshcommander::terminal;

class TestTerminalScreen : public QObject
{
    Q_OBJECT
private slots:
    void defaultGeometryIs24x80();
    void resizePreservesContent();
    void versionIncrementsOnFeed();
    void lineHtmlContainsCharsAndColor();
    void lineHtmlEscapesMetacharacters();
    void clearResetsCursorAndCells();
};

void TestTerminalScreen::defaultGeometryIs24x80()
{
    TerminalScreen s;
    QCOMPARE(s.rows(), 24);
    QCOMPARE(s.columns(), 80);
}

void TestTerminalScreen::resizePreservesContent()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("hello"));
    s.resize(10, 40);
    QCOMPARE(s.rows(), 10);
    QCOMPARE(s.columns(), 40);
    QCOMPARE(s.cell(0, 0).ch, QChar(u'h'));
    QCOMPARE(s.cell(0, 4).ch, QChar(u'o'));
}

void TestTerminalScreen::versionIncrementsOnFeed()
{
    TerminalScreen s;
    const int before = s.version();
    QSignalSpy spy(&s, &TerminalScreen::screenUpdated);
    s.feed(QByteArrayLiteral("x"));
    QVERIFY(s.version() > before);
    QCOMPARE(spy.count(), 1);
}

void TestTerminalScreen::lineHtmlContainsCharsAndColor()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("\x1b[32mok"));
    const QString html = s.lineHtml(0);
    QVERIFY(html.contains(QStringLiteral("color:#5EE08A")));
    QVERIFY(html.contains(QStringLiteral("ok")));
}

void TestTerminalScreen::lineHtmlEscapesMetacharacters()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("<&>"));
    const QString html = s.lineHtml(0);
    QVERIFY(html.contains(QStringLiteral("&lt;")));
    QVERIFY(html.contains(QStringLiteral("&amp;")));
    QVERIFY(html.contains(QStringLiteral("&gt;")));
}

void TestTerminalScreen::clearResetsCursorAndCells()
{
    TerminalScreen s;
    s.feed(QByteArrayLiteral("\x1b[10;20H!"));
    s.clear();
    QCOMPARE(s.cursorRow(), 0);
    QCOMPARE(s.cursorColumn(), 0);
    QCOMPARE(s.cell(9, 19).ch, QChar(u' '));
}

QTEST_GUILESS_MAIN(TestTerminalScreen)
#include "test_terminalscreen.moc"
