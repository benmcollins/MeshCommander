// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "certmodel.h"
#include "computermodel.h"
#include "configstore.h"
#include "idercontroller.h"
#include "kvmcontroller.h"
#include "kvmframebuffer.h"
#include "kvmviewer.h"
#include "migrationcontroller.h"
#include "solcontroller.h"
#include "terminal/terminalscreen.h"

#include <QDir>
#include <QStandardPaths>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Insynergy"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("insynergy.com"));
    QCoreApplication::setApplicationName(QStringLiteral("QuMesh"));

    meshcommander::config::ConfigStore configStore;

    meshcommander::app::MigrationController migrationController;
    migrationController.setStore(&configStore);
    migrationController.checkAndMaybeMigrate();

    meshcommander::model::ComputerModel computerModel;
    computerModel.setStore(&configStore);

    meshcommander::app::CertModel certModel;
    {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        certModel.setStorePath(QDir(dir).filePath(QStringLiteral("certificates.json")));
    }

    qmlRegisterSingletonInstance("QuMesh", 1, 0, "ComputerModel", &computerModel);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "MigrationController",
                                 &migrationController);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "CertModel", &certModel);
    qmlRegisterType<meshcommander::app::SolController>("QuMesh", 1, 0, "SolController");
    qmlRegisterType<meshcommander::app::IderController>("QuMesh", 1, 0, "IderController");
    qmlRegisterType<meshcommander::app::KvmController>("QuMesh", 1, 0, "KvmController");
    qmlRegisterType<meshcommander::app::KvmViewer>("QuMesh", 1, 0, "KvmViewer");
    qmlRegisterUncreatableType<meshcommander::terminal::TerminalScreen>(
        "QuMesh", 1, 0, "TerminalScreen",
        QStringLiteral("Owned by SolController"));
    qmlRegisterUncreatableType<meshcommander::app::KvmFramebuffer>(
        "QuMesh", 1, 0, "KvmFramebuffer",
        QStringLiteral("Owned by KvmController"));

    QQmlApplicationEngine engine;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QuMesh", "Main");

    return app.exec();
}
