// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "computermodel.h"
#include "configstore.h"
#include "migrationcontroller.h"

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

    qmlRegisterSingletonInstance("QuMesh", 1, 0, "ComputerModel", &computerModel);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "MigrationController",
                                 &migrationController);

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
