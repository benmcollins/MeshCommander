// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "filename_formatter.h"

#include <QCoreApplication>

namespace qumesh::app {

namespace {

/// Strip characters that are illegal or troublesome in filenames on
/// macOS or Windows. The locale-formatted time string normally only
/// contains `:` (Windows), but we also defensively strip path
/// separators and other reserved characters in case a translation of
/// the time pattern injects something exotic.
QString sanitize(QString s)
{
    static const QString kBad = QStringLiteral("\\/:*?\"<>|");
    for (QChar &c : s) {
        if (kBad.contains(c)) c = QLatin1Char('.');
    }
    return s;
}

} // namespace

FilenameFormatter::FilenameFormatter(QObject *parent) : QObject(parent) {}

QString FilenameFormatter::defaultName(const QString &prefix,
                                        const QString &ext) const
{
    return formatName(prefix, ext, QDateTime::currentDateTime(),
                       QLocale::system());
}

QString FilenameFormatter::formatName(const QString &prefix,
                                       const QString &ext,
                                       const QDateTime &when,
                                       const QLocale &locale)
{
    // Date is always ISO so the filenames sort chronologically as a
    // side-effect of sorting alphabetically — this matters in Finder
    // and Explorer where users will often eyeball a list of captures.
    const QString date = when.toString(QStringLiteral("yyyy-MM-dd"));

    // Time is formatted to match the locale's 12-vs-24-hour preference
    // (sniffed from `timeFormat(ShortFormat)` — lowercase 'h' = 12-hour,
    // uppercase 'H' = 24-hour) but with an explicit dot-separated
    // pattern. Going via `locale.toString(time, …)` directly drags in
    // colons (`HH:mm:ss`) that Windows forbids in filenames and, on
    // recent Qt builds, a U+202F narrow-no-break space before AM/PM
    // that's a head-scratcher to see in a file manager.
    const QString shortPattern = locale.timeFormat(QLocale::ShortFormat);
    const bool is12Hour = shortPattern.contains(QLatin1Char('h'));
    const QString timePattern = is12Hour ? QStringLiteral("h.mm.ss AP")
                                         : QStringLiteral("HH.mm.ss");
    QString time = locale.toString(when.time(), timePattern);
    time = sanitize(std::move(time));

    // "at" is the connector — translatable so localized builds can
    // swap in "a las" / "um" / etc., but defaults to English when no
    // translation is loaded.
    const QString at = QCoreApplication::translate("FilenameFormatter", "at");

    QString out;
    if (!prefix.isEmpty()) out += sanitize(prefix) + QLatin1Char(' ');
    out += date + QLatin1Char(' ') + at + QLatin1Char(' ') + time;
    if (!ext.isEmpty()) out += QLatin1Char('.') + ext;
    return out;
}

} // namespace qumesh::app
