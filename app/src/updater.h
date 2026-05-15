// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <memory>

namespace qumesh::app {

/// Q_OBJECT facade over the platform's auto-update backend (Sparkle on
/// macOS via `SPUStandardUpdaterController`, WinSparkle planned). Lives
/// on the GUI thread, owned by `main.cpp` for the lifetime of
/// QGuiApplication. The platform backend drives its own scheduled
/// background-check timer; this wrapper exists so QML can also fire
/// the manual "Check for updates…" flow.
///
/// Two compilation paths share this header: `updater.mm` provides the
/// real Sparkle implementation on Apple builds with auto-update
/// enabled; `updater.cpp` provides a no-op stub everywhere else so the
/// QML layer can `import` and reference `Updater` unconditionally and
/// hide its UI when `available()` returns false.
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

    /// Manual user-initiated check. The backend handles the entire UI
    /// (progress, release notes, install prompt). No-op when
    /// `available()` is false.
    Q_INVOKABLE void checkForUpdates();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace qumesh::app
