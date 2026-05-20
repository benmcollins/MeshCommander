// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "keystore.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

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

QByteArray KeyStore::platformLoad()
{
    QFile f(keystorePath());
    if (!f.exists()) return {};
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray blob = f.readAll();
    f.close();
    if (blob.isEmpty()) return {};

    // DPAPI unwraps the blob with the current user's keys. The
    // wrapped form is only decryptable by this user on this machine
    // — copying keystore.bin elsewhere yields nothing useful.
    DATA_BLOB in {};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData()));
    in.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB out {};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }
    QByteArray plain(reinterpret_cast<const char *>(out.pbData),
                     static_cast<qsizetype>(out.cbData));
    LocalFree(out.pbData);
    return plain;
}

bool KeyStore::platformStore(const QByteArray &key)
{
    DATA_BLOB in {};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(key.constData()));
    in.cbData = static_cast<DWORD>(key.size());
    DATA_BLOB out {};
    // CRYPTPROTECT_LOCAL_MACHINE is deliberately NOT set — we want
    // per-user binding, not machine-wide. The default is "current
    // user only", which is exactly the threat model we want.
    if (!CryptProtectData(&in, L"qumesh.secret-codec.v3", nullptr, nullptr,
                          nullptr, 0, &out)) {
        return false;
    }
    QByteArray blob(reinterpret_cast<const char *>(out.pbData),
                    static_cast<qsizetype>(out.cbData));
    LocalFree(out.pbData);

    QSaveFile f(keystorePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    if (f.write(blob) != blob.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

} // namespace qumesh::app
