// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>

namespace qumesh::app {

/// Small QML-side accessor for build/release metadata: version, build
/// date, author line, and the full Apache-2.0 license text. Exposed
/// as a QML singleton from `main.cpp` and consumed by `TitleBar.qml`
/// + `AboutDialog.qml`.
///
/// Version and build date are baked in at configure time via
/// `appinfo_config.h.in`. The license text is embedded as a Qt
/// resource (`:/QuMesh/LICENSE.md`) read lazily on first access.
class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version    READ version    CONSTANT)
    Q_PROPERTY(QString buildDate  READ buildDate  CONSTANT)
    Q_PROPERTY(QString author     READ author     CONSTANT)
    Q_PROPERTY(QString licenseText READ licenseText CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    [[nodiscard]] QString version() const;
    [[nodiscard]] QString buildDate() const;
    [[nodiscard]] QString author() const;
    [[nodiscard]] QString licenseText() const;
};

} // namespace qumesh::app
