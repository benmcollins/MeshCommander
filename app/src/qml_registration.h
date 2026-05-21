// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

namespace qumesh::app {
class AppInfo;
class BackupController;
class BatchController;
class CertModel;
class FilenameFormatter;
class MigrationController;
class Updater;
} // namespace qumesh::app

namespace qumesh::model {
class ComputerModel;
} // namespace qumesh::model

namespace qumesh::wsman {
class LocalAmtProbe;
} // namespace qumesh::wsman

namespace qumesh::app {

// Singleton instances `registerQumeshQmlTypes` exposes to QML. The
// caller owns the objects' lifetimes — `qmlRegisterSingletonInstance`
// does not take ownership and the QML engine outlives nothing here.
struct QmlSingletonContext {
    Updater *updater;
    qumesh::model::ComputerModel *computerModel;
    qumesh::wsman::LocalAmtProbe *localAmtProbe;
    MigrationController *migrationController;
    CertModel *certModel;
    FilenameFormatter *filenameFormatter;
    AppInfo *appInfo;
    BatchController *batchController;
    BackupController *backupController;
};

// Register every `QuMesh.<Type>` the QML files import. Factored out of
// main.cpp so the test in `app/tests/test_qml_engine_loads.cpp` can
// load `Main.qml` against the exact same type table the production
// app uses, with no copy-paste drift.
void registerQumeshQmlTypes(const QmlSingletonContext &ctx);

} // namespace qumesh::app
