// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <memory>

namespace qumesh::app {

/// Q_OBJECT facade over the platform's auto-update backend (Sparkle on
/// macOS via `SPUStandardUpdaterController`, WinSparkle on Windows,
/// appcast-parsing-only on Linux). Lives on the GUI thread, owned by
/// `main.cpp` for the lifetime of QGuiApplication. The Sparkle/WinSparkle
/// backends drive their own scheduled background-check timer and own
/// the dialog UI; this wrapper exists so QML can fire the manual
/// "Check for updates…" flow uniformly across platforms.
///
/// Three compilation paths share this header: `updater.mm` (Sparkle
/// on Apple), `updater_win.cpp` (WinSparkle on Windows),
/// `updater_linux.cpp` (Linux appcast reader — has no installer
/// because apt does that; emits the signals below so QML can render
/// its own dialog), and `updater.cpp` (no-op stub for any other
/// platform / when QUMESH_AUTOUPDATE is off).
class Updater : public QObject
{
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    /// True when a real auto-update backend is wired in. The QML side
    /// uses this to decide whether to render the "Check for updates…"
    /// affordance.
    Q_INVOKABLE bool available() const;

    /// Manual user-initiated check. On macOS/Windows the backend drives
    /// its own progress/notes/install UI. On Linux the platform has no
    /// in-app installer (apt owns the upgrade), so this kicks off an
    /// appcast fetch and emits one of the signals below for QML to
    /// render a small dialog. No-op when `available()` is false.
    Q_INVOKABLE void checkForUpdates();

signals:
    /// Linux only: a newer release exists. `version` is the bare
    /// semver string ("1.2.3"), `notesHtml` the rendered release notes
    /// (HTML from the appcast's <description> CDATA), `releasePageUrl`
    /// the github.com/.../releases/tag/v... URL.
    void updateAvailable(const QString &version,
                         const QString &notesHtml,
                         const QString &releasePageUrl);

    /// Linux only: check completed, this build is current.
    void upToDate(const QString &currentVersion);

    /// Linux only: appcast fetch or parse failed; `error` is a short
    /// human-readable reason.
    void checkFailed(const QString &error);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace qumesh::app
