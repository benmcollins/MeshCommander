#pragma once

#include <QString>

namespace meshcommander::migrate {

struct MigrationOptions
{
    /// Path to the legacy `Local Storage/leveldb/` directory. Empty selects
    /// the platform default returned by `Migrator::defaultLegacyDataDir()`.
    QString legacyDataDir;

    /// Destination directory for the migrated JSON files. Empty selects
    /// `Migrator::defaultOutputDir()`.
    QString outputDir;

    /// The chrome-extension origin the legacy app stored its data under.
    QString origin = QStringLiteral(
        "chrome-extension://gkmjdlhkgdlflppihloafibfdkhfnfco");

    /// If `false`, refuse to run when `outputDir/migration.json` already
    /// exists (i.e. a previous migration has already happened).
    bool overwrite = false;
};

struct MigrationResult
{
    bool ok = false;
    QString error;

    int computersCount = 0;
    int certificatesCount = 0;
    int settingsCount = 0;

    QString legacyDataDirUsed;
    QString outputDirUsed;
};

class Migrator
{
public:
    /// Platform-specific default location of the legacy NW.js data dir.
    [[nodiscard]] static QString defaultLegacyDataDir();

    /// Default output directory: `QStandardPaths::AppDataLocation`.
    [[nodiscard]] static QString defaultOutputDir();

    [[nodiscard]] MigrationResult run(const MigrationOptions &opts = {});
};

} // namespace meshcommander::migrate
