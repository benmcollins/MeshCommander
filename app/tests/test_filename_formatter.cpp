// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "filename_formatter.h"

#include <QDateTime>
#include <QLocale>
#include <QtTest>

using qumesh::app::FilenameFormatter;

namespace {

QDateTime fixedDateTime()
{
    // 2026-05-18 21:42:13 — chosen to give a memorable PM time in
    // en-US (9.42.13 PM) and a 24-hour rendering (21.42.13) elsewhere.
    return QDateTime(QDate(2026, 5, 18), QTime(21, 42, 13));
}

} // namespace

class TestFilenameFormatter : public QObject
{
    Q_OBJECT
private slots:
    void enUS_matchesMacScreenshotConvention();
    void deDE_uses24HourTime();
    void frFR_uses24HourTime();
    void emptyPrefix_dropsLeadingSpace();
    void emptyExtension_dropsTrailingDot();
    void prefixWithIllegalChars_isSanitized();
};

void TestFilenameFormatter::enUS_matchesMacScreenshotConvention()
{
    const QString got = FilenameFormatter::formatName(
        QStringLiteral("Watter01"), QStringLiteral("mov"),
        fixedDateTime(), QLocale(QLocale::English, QLocale::UnitedStates));
    QCOMPARE(got, QStringLiteral("Watter01 2026-05-18 at 9.42.13 PM.mov"));
}

void TestFilenameFormatter::deDE_uses24HourTime()
{
    const QString got = FilenameFormatter::formatName(
        QStringLiteral("Watter01"), QStringLiteral("png"),
        fixedDateTime(), QLocale(QLocale::German, QLocale::Germany));
    QCOMPARE(got, QStringLiteral("Watter01 2026-05-18 at 21.42.13.png"));
}

void TestFilenameFormatter::frFR_uses24HourTime()
{
    const QString got = FilenameFormatter::formatName(
        QStringLiteral("Watter01"), QStringLiteral("cast"),
        fixedDateTime(), QLocale(QLocale::French, QLocale::France));
    QCOMPARE(got, QStringLiteral("Watter01 2026-05-18 at 21.42.13.cast"));
}

void TestFilenameFormatter::emptyPrefix_dropsLeadingSpace()
{
    const QString got = FilenameFormatter::formatName(
        QString(), QStringLiteral("png"),
        fixedDateTime(), QLocale(QLocale::English, QLocale::UnitedStates));
    QCOMPARE(got, QStringLiteral("2026-05-18 at 9.42.13 PM.png"));
}

void TestFilenameFormatter::emptyExtension_dropsTrailingDot()
{
    const QString got = FilenameFormatter::formatName(
        QStringLiteral("Watter01"), QString(),
        fixedDateTime(), QLocale(QLocale::English, QLocale::UnitedStates));
    QCOMPARE(got, QStringLiteral("Watter01 2026-05-18 at 9.42.13 PM"));
}

void TestFilenameFormatter::prefixWithIllegalChars_isSanitized()
{
    // Path separators and other filesystem-illegal characters in the
    // user-given machine name must be neutered so the result is safe
    // to pass straight to FileDialog.selectedFile.
    const QString got = FilenameFormatter::formatName(
        QStringLiteral("dir/sub:host?"), QStringLiteral("png"),
        fixedDateTime(), QLocale(QLocale::English, QLocale::UnitedStates));
    QCOMPARE(got, QStringLiteral("dir.sub.host. 2026-05-18 at 9.42.13 PM.png"));
}

QTEST_MAIN(TestFilenameFormatter)
#include "test_filename_formatter.moc"
