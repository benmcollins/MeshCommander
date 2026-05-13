#include "migrate/migrate.h"

#include "migrate/chromium_storage.h"
#include "migrate/legacy_decrypt.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace meshcommander::migrate {

namespace {

constexpr char kComputersKey[] = "computers";
constexpr char kCertificatesKey[] = "certificates";
constexpr char kStampFile[] = "migration.json";
constexpr char kComputersFile[] = "computers.json";
constexpr char kCertificatesFile[] = "certificates.json";
constexpr char kSettingsFile[] = "settings.json";

bool writeFile(const QString &path, const QByteArray &contents, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr)
            *error = QStringLiteral("could not write %1: %2").arg(path, f.errorString());
        return false;
    }
    if (f.write(contents) != contents.size()) {
        if (error != nullptr)
            *error = QStringLiteral("short write to %1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

bool decryptInto(const ChromiumEntry &entry, QJsonArray *out, int *count, QString *error)
{
    if (entry.value.isEmpty()) {
        *out = QJsonArray();
        return true;
    }
    const auto result = LegacyCrypto::decrypt(entry.value);
    if (!result.ok()) {
        if (error != nullptr)
            *error = QStringLiteral("decrypting '%1': %2").arg(entry.key, result.error);
        return false;
    }
    QJsonParseError je{};
    const QJsonDocument doc = QJsonDocument::fromJson(result.plaintext, &je);
    if (je.error != QJsonParseError::NoError) {
        if (error != nullptr)
            *error = QStringLiteral("parsing decrypted '%1' as JSON: %2")
                         .arg(entry.key, je.errorString());
        return false;
    }
    if (!doc.isArray()) {
        if (error != nullptr)
            *error = QStringLiteral("decrypted '%1' was not a JSON array").arg(entry.key);
        return false;
    }
    *out = doc.array();
    if (count != nullptr) *count = out->size();
    return true;
}

} // namespace

QString Migrator::defaultLegacyDataDir()
{
#if defined(Q_OS_MACOS)
    return QDir::homePath()
        + QStringLiteral("/Library/Application Support/meshcommander/Default/Local Storage/leveldb");
#elif defined(Q_OS_WIN)
    const QString base = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    return QDir::fromNativeSeparators(base)
        + QStringLiteral("/meshcommander/Default/Local Storage/leveldb");
#else
    return QDir::homePath()
        + QStringLiteral("/.config/meshcommander/Default/Local Storage/leveldb");
#endif
}

QString Migrator::defaultOutputDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

MigrationResult Migrator::run(const MigrationOptions &opts)
{
    MigrationResult result;
    result.legacyDataDirUsed = opts.legacyDataDir.isEmpty() ? defaultLegacyDataDir()
                                                            : opts.legacyDataDir;
    result.outputDirUsed = opts.outputDir.isEmpty() ? defaultOutputDir() : opts.outputDir;

    if (!QFileInfo(result.legacyDataDirUsed).isDir()) {
        result.error =
            QStringLiteral("legacy data directory does not exist: %1").arg(result.legacyDataDirUsed);
        return result;
    }

    if (!opts.overwrite
        && QFile::exists(QDir(result.outputDirUsed).filePath(QString::fromLatin1(kStampFile)))) {
        result.error = QStringLiteral(
                           "%1 already contains a migration; pass overwrite to redo")
                           .arg(result.outputDirUsed);
        return result;
    }

    if (!QDir().mkpath(result.outputDirUsed)) {
        result.error = QStringLiteral("could not create output directory: %1")
                           .arg(result.outputDirUsed);
        return result;
    }

    ChromiumStorageReader reader(result.legacyDataDirUsed);
    if (!reader.open()) {
        result.error = QStringLiteral("opening leveldb: %1").arg(reader.lastError());
        return result;
    }
    const QList<ChromiumEntry> entries = reader.readOrigin(opts.origin);
    if (entries.isEmpty() && !reader.lastError().isEmpty()) {
        result.error = QStringLiteral("reading origin: %1").arg(reader.lastError());
        return result;
    }

    QJsonArray computers;
    QJsonArray certificates;
    QJsonObject settings;

    for (const ChromiumEntry &entry : entries) {
        if (entry.key == QString::fromLatin1(kComputersKey)) {
            QString err;
            if (!decryptInto(entry, &computers, &result.computersCount, &err)) {
                result.error = err;
                return result;
            }
        } else if (entry.key == QString::fromLatin1(kCertificatesKey)) {
            QString err;
            if (!decryptInto(entry, &certificates, &result.certificatesCount, &err)) {
                result.error = err;
                return result;
            }
        } else {
            // Treat all other entries as opaque scalar settings. They are
            // typically short ASCII or UTF-8 strings; we already decoded the
            // Chromium type prefix in ChromiumStorageReader, so the value
            // bytes are ready to embed as a JSON string.
            settings.insert(entry.key, QString::fromUtf8(entry.value));
        }
    }
    result.settingsCount = settings.size();

    const QDir outDir(result.outputDirUsed);
    QString err;
    if (!writeFile(outDir.filePath(QString::fromLatin1(kComputersFile)),
                   QJsonDocument(computers).toJson(QJsonDocument::Indented), &err)
        || !writeFile(outDir.filePath(QString::fromLatin1(kCertificatesFile)),
                      QJsonDocument(certificates).toJson(QJsonDocument::Indented), &err)
        || !writeFile(outDir.filePath(QString::fromLatin1(kSettingsFile)),
                      QJsonDocument(settings).toJson(QJsonDocument::Indented), &err)) {
        result.error = err;
        return result;
    }

    QJsonObject stamp{
        {QStringLiteral("from"), result.legacyDataDirUsed},
        {QStringLiteral("at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("computers"), result.computersCount},
        {QStringLiteral("certificates"), result.certificatesCount},
        {QStringLiteral("settings"), result.settingsCount},
    };
    if (!writeFile(outDir.filePath(QString::fromLatin1(kStampFile)),
                   QJsonDocument(stamp).toJson(QJsonDocument::Indented), &err)) {
        result.error = err;
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace meshcommander::migrate
