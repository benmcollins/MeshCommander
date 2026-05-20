// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "qml_registration.h"

#include "appinfo.h"
#include "batchcontroller.h"
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
    qmlRegisterSingletonInstance("QuMesh", 1, 0, "BatchController",
                                 ctx.batchController);

    // SolController / IderController / KvmController / KvmViewer /
    // MachineDetailsController / ScannerController /
    // SetupBinController are now declared with `QML_ELEMENT` in
    // their headers — qmltyperegistrar picks them up from
    // qumesh_appcore at build time. KvmFramebuffer is
    // `QML_UNCREATABLE` for the same reason. TerminalScreen still
    // uses imperative registration because it lives in qumesh_terminal
    // (a non-QML module); wiring it via QML_FOREIGN is the planned
    // follow-up to #294.
    qmlRegisterUncreatableType<qumesh::terminal::TerminalScreen>(
        "QuMesh", 1, 0, "TerminalScreen",
        QStringLiteral("Owned by SolController"));
}

} // namespace qumesh::app
