// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "appinfo.h"
#include "backupcontroller.h"
#include "batchcontroller.h"
#include "certmodel.h"
#include "computermodel.h"
#include "configstore.h"
#include "filename_formatter.h"
#include "migrationcontroller.h"
#include "qml_registration.h"
#include "updater.h"
#include "wsman/local_amt_probe.h"

#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
    // macOS by default swaps Ctrl ↔ Cmd at the Qt key-event layer:
    // pressing ⌃ fires `Key_Meta`/`MetaModifier`, pressing ⌘ fires
    // `Key_Control`/`ControlModifier`. That breaks the SOL terminal
    // (Ctrl+C never lands in the `Ctrl+A..Z` branch) and KVM (the
    // remote receives Meta+C instead of Ctrl+C). Disable the swap so
    // Ctrl is always Ctrl across platforms — Cmd remains usable via
    // `MetaModifier` if the QML side ever needs it.
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);

    // Pin the Qt Quick Controls style explicitly (#388). Without this,
    // Qt picks per-platform defaults — `macos` on macOS, `windows` on
    // Windows, `Default` on Linux — so the same QML renders differently
    // across the three targets. Basic matches what every QML file in
    // this app already pins with `import QtQuick.Controls.Basic`, so
    // the C++ declaration and the QML imports agree: unstyled,
    // theme-driven controls that our own themed wrappers
    // (FlatButton/AccentButton/Section/ConfirmDialog/etc.) layer on top
    // of. Picking Fusion (or any other style) here would have no visible
    // effect — explicit `.Basic` imports in QML override the C++
    // selection — but the declaration would then misrepresent what the
    // app actually renders. Must be set before any QML loads a Controls
    // module; placing it before QGuiApplication is the safe spot.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Insynergy"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("insynergy.com"));
    QCoreApplication::setApplicationName(QStringLiteral("QuMesh"));

    // Window-manager icon. macOS overrides the bundle .icns with this
    // at runtime (Dock + Cmd+Tab), Windows decorates non-bundle window
    // chrome with it, and Linux/Wayland use it for the about box and
    // desktop entries. Use the tile variant (slate background + light
    // Q) so the icon reads as a finished tile rather than a floating
    // outline.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/qumesh-mark-tile.svg")));

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

    // Sparkle wires itself into NSApplication during construction; the
    // GUI thread must already exist (QGuiApplication above is what
    // creates it). On non-Apple builds (or with auto-update disabled)
    // this is the stub from updater.cpp — same QML surface, no-op.
    qumesh::app::Updater updater;

    // LocalAmtProbe — sniffs 127.0.0.1:16992/:16993 to decide whether
    // Intel LMS is exposing the local machine's AMT firmware on the
    // loopback. Result lives behind a QML property; ComputerListView
    // shows "This PC" when available.
    qumesh::wsman::LocalAmtProbe localAmtProbe;
    QTimer::singleShot(0, &app, [&localAmtProbe]() { localAmtProbe.start(); });

    // Stateless QML-side helper that turns a (machine name, extension)
    // pair into the suggested save filename for KVM/SOL dialogs.
    qumesh::app::FilenameFormatter filenameFormatter;

    // Build / release metadata for the About dialog (#246). The version
    // and build date are baked in at configure time; the license text
    // is read lazily from `:/QuMesh/LICENSE.md`.
    qumesh::app::AppInfo appInfo;

    // Batch-actions controller (#202): one per app, owns a BatchRunner
    // and the per-host dispatcher that translates an action enum + row
    // index into a fresh WsmanClient + the matching wsman::* primitive.
    qumesh::app::BatchController batchController;
    batchController.setComputerModel(&computerModel);

    // BackupController drives the password-protected `.qumesh-backup`
    // export/import flow (#429). Pure UI controller — file IO + crypto
    // live in BackupCodec; the model handles the upsert.
    qumesh::app::BackupController backupController;
    backupController.setModel(&computerModel);

    qumesh::app::registerQumeshQmlTypes({
        .updater              = &updater,
        .computerModel        = &computerModel,
        .localAmtProbe        = &localAmtProbe,
        .migrationController  = &migrationController,
        .certModel            = &certModel,
        .filenameFormatter    = &filenameFormatter,
        .appInfo              = &appInfo,
        .batchController      = &batchController,
        .backupController     = &backupController,
    });

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
