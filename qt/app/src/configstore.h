#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace meshcommander::config {

/// Owns the on-disk JSON files that hold the new app's persisted state:
/// `computers.json`, `certificates.json`, `settings.json`. All writes go
/// through `QSaveFile` for crash-safe atomic replace.
///
/// The store is intentionally schema-agnostic — it returns and accepts
/// raw `QJsonArray` / `QJsonObject` and leaves typed parsing to the
/// callers (`ComputerModel`, etc.). This keeps the format easy to evolve.
class ConfigStore
{
public:
    /// Create a store rooted at `dir`. An empty `dir` selects
    /// `defaultDir()`, i.e. `QStandardPaths::AppDataLocation`.
    explicit ConfigStore(QString dir = {});

    /// Platform default: `QStandardPaths::AppDataLocation` for the running
    /// `QCoreApplication`.
    [[nodiscard]] static QString defaultDir();

    [[nodiscard]] QString dir() const { return m_dir; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

    /// `true` if `computers.json` exists under `dir()`. Used to detect that
    /// a migration is needed when starting fresh.
    [[nodiscard]] bool hasNativeConfig() const;

    /// Read JSON arrays/objects from disk. Returns an empty value when the
    /// file does not exist; populates `lastError()` and returns an empty
    /// value when it exists but is malformed.
    [[nodiscard]] QJsonArray loadComputers();
    [[nodiscard]] QJsonArray loadCertificates();
    [[nodiscard]] QJsonObject loadSettings();

    /// Atomic write via `QSaveFile`. Returns false (and sets `lastError`)
    /// on any I/O failure; the on-disk file is left untouched in that case.
    [[nodiscard]] bool saveComputers(const QJsonArray &arr);
    [[nodiscard]] bool saveCertificates(const QJsonArray &arr);
    [[nodiscard]] bool saveSettings(const QJsonObject &obj);

private:
    [[nodiscard]] QString pathFor(const QString &name) const;
    [[nodiscard]] QByteArray readFile(const QString &name);
    [[nodiscard]] bool writeFile(const QString &name, const QByteArray &bytes);

    QString m_dir;
    mutable QString m_lastError;
};

} // namespace meshcommander::config
