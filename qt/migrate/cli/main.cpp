// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "migrate/migrate.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

using namespace meshcommander::migrate;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qumesh-migrate"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));
    QCoreApplication::setOrganizationName(QStringLiteral("Insynergy"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("insynergy.com"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Import MeshCommander config from a legacy NW.js install."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption legacyOpt(
        {QStringLiteral("legacy-dir")},
        QStringLiteral("Path to the legacy Chromium leveldb directory (default: platform default)."),
        QStringLiteral("path"));
    QCommandLineOption outputOpt(
        {QStringLiteral("output-dir")},
        QStringLiteral("Destination for the migrated JSON (default: AppDataLocation)."),
        QStringLiteral("path"));
    QCommandLineOption overwriteOpt(
        {QStringLiteral("overwrite")},
        QStringLiteral("Re-run even if a migration.json stamp already exists."));
    QCommandLineOption originOpt(
        {QStringLiteral("origin")},
        QStringLiteral("Chrome-extension origin to read (default: the published MeshCommander id)."),
        QStringLiteral("origin"));
    parser.addOptions({legacyOpt, outputOpt, overwriteOpt, originOpt});
    parser.process(app);

    MigrationOptions opts;
    if (parser.isSet(legacyOpt)) opts.legacyDataDir = parser.value(legacyOpt);
    if (parser.isSet(outputOpt)) opts.outputDir = parser.value(outputOpt);
    if (parser.isSet(originOpt)) opts.origin = parser.value(originOpt);
    opts.overwrite = parser.isSet(overwriteOpt);

    QTextStream out(stdout);
    QTextStream err(stderr);

    Migrator migrator;
    const MigrationResult r = migrator.run(opts);

    if (!r.ok) {
        err << "migrate failed: " << r.error << Qt::endl;
        return 1;
    }

    out << "Migration complete." << Qt::endl;
    out << "  Source : " << r.legacyDataDirUsed << Qt::endl;
    out << "  Output : " << r.outputDirUsed << Qt::endl;
    out << "  Computers   : " << r.computersCount << Qt::endl;
    out << "  Certificates: " << r.certificatesCount << Qt::endl;
    out << "  Settings    : " << r.settingsCount << Qt::endl;
    return 0;
}
