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

namespace meshcommander::config {

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

    QSaveFile f(pathFor(name));
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

} // namespace meshcommander::config
