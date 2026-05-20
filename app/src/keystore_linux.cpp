// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "keystore.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace qumesh::app {

namespace {

QString keystorePath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("keystore.bin"));
}

} // namespace

/// Per-user file-based fallback for Linux. Not a "real" platform
/// keystore — libsecret would require D-Bus + a running secret-
/// service (gnome-keyring / ksecret-service), which CI runners and
/// headless servers often lack. Instead we drop a 0600-perm file
/// under XDG_DATA_HOME/qumesh/. Threat model: same as #273's file-
/// permission work — the limit is the file owner. Users wanting
/// stronger protection should configure their distro's keyring
/// service and we can wire libsecret later. See #306.
QByteArray KeyStore::platformLoad()
{
    QFile f(keystorePath());
    if (!f.exists()) return {};
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray blob = f.readAll();
    f.close();
    return blob;
}

bool KeyStore::platformStore(const QByteArray &key)
{
    const QString path = keystorePath();
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    if (f.write(key) != key.size()) {
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) return false;

    // 0600 on the file; 0700 on the parent. Same pattern as #273's
    // configstore. The QSaveFile rename loses any initial perms we
    // set, so we have to do this after commit().
    QFile::setPermissions(path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QFile::setPermissions(QFileInfo(path).path(),
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return true;
}

} // namespace qumesh::app
