// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "appinfo.h"
#include "appinfo_config.h"

#include <QFile>
#include <QStringLiteral>

namespace qumesh::app {

AppInfo::AppInfo(QObject *parent) : QObject(parent) {}

QString AppInfo::version() const
{
    return QStringLiteral(QUMESH_VERSION);
}

QString AppInfo::buildDate() const
{
    return QStringLiteral(QUMESH_BUILD_DATE);
}

QString AppInfo::author() const
{
    return QStringLiteral("Ben Collins <ben@ironrocketsmc.org>");
}

QString AppInfo::licenseText() const
{
    QFile f(QStringLiteral(":/QuMesh/LICENSE.md"));
    if (!f.open(QIODevice::ReadOnly)) {
        // Resource registration regressed somehow — fall back to a
        // pointer at the on-disk file so users can still find the
        // license rather than seeing a blank dialog.
        return QStringLiteral("LICENSE.md not bundled. See https://github.com/benmcollins/QuMesh/blob/main/LICENSE.md");
    }
    return QString::fromUtf8(f.readAll());
}

} // namespace qumesh::app
