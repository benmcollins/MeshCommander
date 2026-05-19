// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "qml_registration.h"

#include "appinfo.h"
#include "certmodel.h"
#include "computermodel.h"
#include "filename_formatter.h"
#include "idercontroller.h"
#include "kvmcontroller.h"
#include "kvmframebuffer.h"
#include "kvmviewer.h"
#include "machinedetailscontroller.h"
#include "migrationcontroller.h"
#include "scannercontroller.h"
#include "setupbincontroller.h"
#include "solcontroller.h"
#include "terminal/terminalscreen.h"
#include "updater.h"
#include "wsman/local_amt_probe.h"

#include <QStringLiteral>
#include <QtQml/qqml.h>

namespace qumesh::app {

void registerQumeshQmlTypes(const QmlSingletonContext &ctx)
{
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "Updater", ctx.updater);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "ComputerModel", ctx.computerModel);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "LocalAmtProbe", ctx.localAmtProbe);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "MigrationController",
                                 ctx.migrationController);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "CertModel", ctx.certModel);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "FilenameFormatter",
                                 ctx.filenameFormatter);
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "AppInfo", ctx.appInfo);

    qmlRegisterType<SolController>("QuMesh", 1, 0, "SolController");
    qmlRegisterType<IderController>("QuMesh", 1, 0, "IderController");
    qmlRegisterType<KvmController>("QuMesh", 1, 0, "KvmController");
    qmlRegisterType<KvmViewer>("QuMesh", 1, 0, "KvmViewer");
    qmlRegisterType<MachineDetailsController>(
        "QuMesh", 1, 0, "MachineDetailsController");
    qmlRegisterType<ScannerController>("QuMesh", 1, 0, "ScannerController");
    qmlRegisterType<SetupBinController>("QuMesh", 1, 0, "SetupBinController");

    qmlRegisterUncreatableType<qumesh::terminal::TerminalScreen>(
        "QuMesh", 1, 0, "TerminalScreen",
        QStringLiteral("Owned by SolController"));
    qmlRegisterUncreatableType<KvmFramebuffer>(
        "QuMesh", 1, 0, "KvmFramebuffer",
        QStringLiteral("Owned by KvmController"));
}

} // namespace qumesh::app
