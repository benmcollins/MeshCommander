// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "migrationcontroller.h"

#include "configstore.h"
#include "migrate/migrate.h"

#include <QFileInfo>

namespace qumesh::app {

MigrationController::MigrationController(QObject *parent)
    : QObject(parent)
{
}

void MigrationController::setStore(config::ConfigStore *store)
{
    m_store = store;
}

void MigrationController::checkAndMaybeMigrate()
{
    if (m_store == nullptr) {
        setMessage(QStringLiteral("MigrationController has no ConfigStore"));
        setState(State::Failed);
        return;
    }
    if (m_store->hasNativeConfig()) {
        setState(State::Idle);
        return;
    }

    const QString legacyDir = m_legacyDataDirOverride.isEmpty()
                                  ? migrate::Migrator::defaultLegacyDataDir()
                                  : m_legacyDataDirOverride;
    if (!QFileInfo(legacyDir).isDir()) {
        setMessage(QStringLiteral("No legacy MeshCommander data found at %1").arg(legacyDir));
        setState(State::NoLegacyData);
        return;
    }

    setState(State::Migrating);

    migrate::MigrationOptions opts;
    opts.legacyDataDir = legacyDir;
    opts.outputDir = m_store->dir();

    migrate::Migrator m;
    const migrate::MigrationResult r = m.run(opts);

    if (!r.ok) {
        setMessage(r.error);
        setState(State::Failed);
        return;
    }

    m_computersImported = r.computersCount;
    emit computersImportedChanged();
    setMessage(QStringLiteral("Imported %1 computers, %2 certificates, %3 settings from %4")
                   .arg(r.computersCount)
                   .arg(r.certificatesCount)
                   .arg(r.settingsCount)
                   .arg(legacyDir));
    setState(State::Imported);
}

void MigrationController::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void MigrationController::setMessage(QString m)
{
    if (m_message == m) return;
    m_message = std::move(m);
    emit messageChanged();
}

} // namespace qumesh::app
