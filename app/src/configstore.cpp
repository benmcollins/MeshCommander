// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "configstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

namespace qumesh::config {

namespace {

constexpr char kComputersFile[] = "computers.json";
constexpr char kSettingsFile[] = "settings.json";

} // namespace

ConfigStore::ConfigStore(QString dir)
    : m_dir(dir.isEmpty() ? defaultDir() : std::move(dir))
{
}

QString ConfigStore::defaultDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool ConfigStore::hasNativeConfig() const
{
    return QFileInfo::exists(QDir(m_dir).filePath(QString::fromLatin1(kComputersFile)));
}

QString ConfigStore::pathFor(const QString &name) const
{
    return QDir(m_dir).filePath(name);
}

QByteArray ConfigStore::readFile(const QString &name)
{
    m_lastError.clear();
    const QString path = pathFor(name);
    if (!QFileInfo::exists(path)) return {};

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("could not open %1: %2").arg(path, f.errorString());
        return {};
    }
    return f.readAll();
}

bool ConfigStore::writeFile(const QString &name, const QByteArray &bytes)
{
    m_lastError.clear();
    if (!QDir().mkpath(m_dir)) {
        m_lastError = QStringLiteral("could not create %1").arg(m_dir);
        return false;
    }
    // Restrict the parent dir to 0700 (POSIX) so the directory listing
    // doesn't leak filenames. On Windows %APPDATA% is already
    // user-isolated; setPermissions maps to an ACL adjustment there
    // and is at worst a no-op. See #273.
    QFile::setPermissions(m_dir,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    const QString path = pathFor(name);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = QStringLiteral("could not open %1 for writing: %2")
                          .arg(f.fileName(), f.errorString());
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        m_lastError = QStringLiteral("short write to %1: %2").arg(f.fileName(), f.errorString());
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        m_lastError = QStringLiteral("could not commit %1: %2").arg(f.fileName(), f.errorString());
        return false;
    }
    // computers.json carries v2-"encrypted" passwords whose key+IV
    // ship with the ciphertext (see #274), so the file is effectively
    // plaintext credentials. settings.json is less sensitive but we
    // restrict both for consistency — there's no reason for any other
    // local user to read either. Done after commit so the rename has
    // landed the final inode. See #273.
    QFile::setPermissions(path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QJsonArray ConfigStore::loadComputers()
{
    const QByteArray bytes = readFile(QString::fromLatin1(kComputersFile));
    if (bytes.isEmpty()) return {};
    QJsonParseError pe = {};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        m_lastError = QStringLiteral("computers.json is not a JSON array: %1").arg(pe.errorString());
        return {};
    }
    return doc.array();
}

QJsonObject ConfigStore::loadSettings()
{
    const QByteArray bytes = readFile(QString::fromLatin1(kSettingsFile));
    if (bytes.isEmpty()) return {};
    QJsonParseError pe = {};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("settings.json is not a JSON object: %1").arg(pe.errorString());
        return {};
    }
    return doc.object();
}

bool ConfigStore::saveComputers(const QJsonArray &arr)
{
    return writeFile(QString::fromLatin1(kComputersFile),
                     QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

bool ConfigStore::saveSettings(const QJsonObject &obj)
{
    return writeFile(QString::fromLatin1(kSettingsFile),
                     QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace qumesh::config
