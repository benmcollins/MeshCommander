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
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QtQml/qqml.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Insynergy"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("insynergy.com"));
    QCoreApplication::setApplicationName(QStringLiteral("QuMesh"));

    // Window-manager icon. macOS reads the bundle's .icns directly and
    // ignores this; Windows uses the embedded .ico from the .rc. This
    // setter covers Linux + Wayland in-app contexts (e.g. about boxes).
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/qumesh-mark-slate.svg")));

    // Load the .qm matching the system locale. qt_add_translations()
    // embeds the compiled .qm under :/i18n/ in the QML resource
    // module. Falling through to the English source baked into the
    // qsTr() calls is intentional when no .qm matches.
    static QTranslator s_appTranslator;
    if (s_appTranslator.load(QLocale(), QStringLiteral("qumesh"),
                              QStringLiteral("_"),
                              QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&s_appTranslator);
    }
    static QTranslator s_qtTranslator;
    if (s_qtTranslator.load(QLocale(), QStringLiteral("qtbase"),
                             QStringLiteral("_"),
                             QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&s_qtTranslator);
    }

    // Release-workflow smoke launch: when QUMESH_SMOKE_EXIT_MS is set
    // to a positive integer, exit cleanly after that delay. CI uses
    // this to confirm the bundled .app / .exe starts without missing
    // QML imports, dynamic libraries, etc.
    if (const QByteArray smokeMs = qgetenv("QUMESH_SMOKE_EXIT_MS");
        !smokeMs.isEmpty()) {
        bool ok = false;
        const int ms = smokeMs.toInt(&ok);
        if (ok && ms > 0) {
            QTimer::singleShot(ms, &app, []() { QCoreApplication::quit(); });
        }
    }

    qumesh::config::ConfigStore configStore;

    qumesh::app::MigrationController migrationController;
    migrationController.setStore(&configStore);
    migrationController.checkAndMaybeMigrate();

    qumesh::model::ComputerModel computerModel;
    computerModel.setStore(&configStore);

    qumesh::app::CertModel certModel;
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
    qmlRegisterType<qumesh::app::SolController>("QuMesh", 1, 0, "SolController");
    qmlRegisterType<qumesh::app::IderController>("QuMesh", 1, 0, "IderController");
    qmlRegisterType<qumesh::app::KvmController>("QuMesh", 1, 0, "KvmController");
    qmlRegisterType<qumesh::app::KvmViewer>("QuMesh", 1, 0, "KvmViewer");
    qmlRegisterUncreatableType<qumesh::terminal::TerminalScreen>(
        "QuMesh", 1, 0, "TerminalScreen",
        QStringLiteral("Owned by SolController"));
    qmlRegisterUncreatableType<qumesh::app::KvmFramebuffer>(
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
