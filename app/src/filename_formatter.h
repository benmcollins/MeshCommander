// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QDateTime>
#include <QLocale>
#include <QObject>
#include <QString>

namespace qumesh::app {

/// Default-filename helper exposed to QML as a singleton.
///
/// The save dialogs for KVM/SOL screenshots and recordings need a
/// sensible suggested name so users don't have to invent one mid-
/// session. We follow the macOS Screenshot convention:
///
///     <prefix> YYYY-MM-DD at H.MM.SS [AP]M.<ext>
///
/// The date is always ISO (locale-independent so files sort
/// chronologically by name everywhere); the time is formatted via the
/// system locale and `:` is rewritten to `.` because Windows refuses
/// colons in filenames. `prefix` is the user-given machine name (or
/// the host fallback when no name is set).
class FilenameFormatter : public QObject
{
    Q_OBJECT
public:
    explicit FilenameFormatter(QObject *parent = nullptr);

    /// Format a default save filename for the current wall clock and
    /// system locale. Empty `prefix` and empty `ext` are both legal
    /// — empty `prefix` collapses the leading "<prefix> " away, empty
    /// `ext` drops the trailing dot.
    [[nodiscard]] Q_INVOKABLE QString defaultName(const QString &prefix,
                                                   const QString &ext) const;

    /// Same as `defaultName` but with `when` and `locale` provided
    /// explicitly. Exposed for unit tests so the output is
    /// reproducible regardless of when / where they run.
    [[nodiscard]] static QString formatName(const QString &prefix,
                                             const QString &ext,
                                             const QDateTime &when,
                                             const QLocale &locale);
};

} // namespace qumesh::app
