// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "backup_codec.h"
#include "backupcontroller.h"
#include "computermodel.h"
#include "configstore.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using qumesh::app::BackupController;
using qumesh::config::ConfigStore;
using qumesh::model::Computer;
using qumesh::model::ComputerModel;

class TestBackupController : public QObject
{
    Q_OBJECT
private slots:
    void exportThenImportRoundTrips();
    void importIntoEmptyModelCountsAllAsNew();
    void importMergesMatchingHostsByName();
    void importMatchesByIdAcrossRename();
    void wrongPasswordYieldsBadPasswordError();
    void cancelImportClearsPreview();
    void applyImportLeavesIdleOnSuccess();
    void exportFailsWithoutPassword();
    void importIsRobustToModelMutationBetweenPrepareAndApply();
    void cancelImportResetsAfterDoneState();
    void applyImportWithoutPreparedPlanRecordsError();
};

namespace {

void seed(ComputerModel &m, const QString &name, const QString &host,
          const QString &pass = QStringLiteral("pw"))
{
    QCOMPARE(m.addComputer(name, host, QStringLiteral("admin"), pass, false) >= 0, true);
}

QUrl tmpFile(const QTemporaryDir &dir, const QString &name)
{
    return QUrl::fromLocalFile(QDir(dir.path()).filePath(name));
}

} // namespace

void TestBackupController::exportThenImportRoundTrips()
{
    QTemporaryDir src, dst, file;
    QVERIFY(src.isValid() && dst.isValid() && file.isValid());

    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("amt-01"), QStringLiteral("10.0.0.1"),
          QStringLiteral("alpha"));
    seed(srcModel, QStringLiteral("amt-02"), QStringLiteral("10.0.0.2"),
          QStringLiteral("beta"));

    BackupController exporter;
    exporter.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("backup.qumesh-backup"));
    QVERIFY(exporter.exportTo(path, QStringLiteral("rosebud-99")));
    QCOMPARE(exporter.state(), BackupController::State::Done);

    // Fresh model + store on the import side — simulates a different install.
    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);

    BackupController importer;
    importer.setModel(&dstModel);

    QVERIFY(importer.prepareImport(path, QStringLiteral("rosebud-99")));
    QCOMPARE(importer.state(), BackupController::State::NeedsConfirm);
    QCOMPARE(importer.previewAddCount(), 2);
    QCOMPARE(importer.previewUpdateCount(), 0);

    QVERIFY(importer.applyImport());
    QCOMPARE(importer.state(), BackupController::State::Done);
    QCOMPARE(dstModel.rowCount(), 2);
    QCOMPARE(dstModel.at(0).pass, QStringLiteral("alpha"));
    QCOMPARE(dstModel.at(1).pass, QStringLiteral("beta"));
}

void TestBackupController::importIntoEmptyModelCountsAllAsNew()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"));
    seed(srcModel, QStringLiteral("b"), QStringLiteral("2.2.2.2"));
    seed(srcModel, QStringLiteral("c"), QStringLiteral("3.3.3.3"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("b.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));

    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    BackupController imp;
    imp.setModel(&dstModel);
    QVERIFY(imp.prepareImport(path, QStringLiteral("pw")));
    QCOMPARE(imp.previewAddCount(), 3);
    QCOMPARE(imp.previewUpdateCount(), 0);
}

void TestBackupController::importMergesMatchingHostsByName()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"),
          QStringLiteral("newpass"));
    seed(srcModel, QStringLiteral("b"), QStringLiteral("2.2.2.2"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("m.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));

    // Destination has an existing machine with the same host+name (case-
    // insensitive). Should be flagged as Update, with the new password
    // overwriting the old one.
    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    seed(dstModel, QStringLiteral("A"), QStringLiteral("1.1.1.1"),
          QStringLiteral("oldpass"));

    BackupController imp;
    imp.setModel(&dstModel);
    QVERIFY(imp.prepareImport(path, QStringLiteral("pw")));
    QCOMPARE(imp.previewUpdateCount(), 1);
    QCOMPARE(imp.previewAddCount(), 1);

    QVERIFY(imp.applyImport());
    QCOMPARE(dstModel.rowCount(), 2);

    // Find the updated row by host.
    int aRow = -1;
    for (int i = 0; i < dstModel.rowCount(); ++i)
        if (dstModel.at(i).host == QStringLiteral("1.1.1.1")) aRow = i;
    QVERIFY(aRow >= 0);
    QCOMPARE(dstModel.at(aRow).pass, QStringLiteral("newpass"));
}

void TestBackupController::importMatchesByIdAcrossRename()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("after-rename"), QStringLiteral("10.0.0.7"));
    const QString id = srcModel.at(0).id;

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("id.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));

    // Destination has the same id but a different host AND name. id
    // match should still pull it in as an Update.
    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    seed(dstModel, QStringLiteral("old-name"), QStringLiteral("10.0.0.99"));
    // Force the id to match by re-loading after a hand-edited save.
    QJsonArray reload;
    auto obj = dstModel.at(0).toJson();
    obj[QStringLiteral("id")] = id;
    reload.push_back(obj);
    QVERIFY(dstStore.saveComputers(reload));
    dstModel.setStore(&dstStore);

    BackupController imp;
    imp.setModel(&dstModel);
    QVERIFY(imp.prepareImport(path, QStringLiteral("pw")));
    QCOMPARE(imp.previewUpdateCount(), 1);
    QCOMPARE(imp.previewAddCount(), 0);
    QVERIFY(imp.applyImport());
    QCOMPARE(dstModel.at(0).name, QStringLiteral("after-rename"));
    QCOMPARE(dstModel.at(0).host, QStringLiteral("10.0.0.7"));
}

void TestBackupController::wrongPasswordYieldsBadPasswordError()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("w.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("right")));

    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    BackupController imp;
    imp.setModel(&dstModel);
    QVERIFY(!imp.prepareImport(path, QStringLiteral("wrong")));
    QCOMPARE(imp.state(), BackupController::State::Failed);
    QCOMPARE(imp.lastError(), BackupController::Error::BadPassword);

    // Model untouched.
    QCOMPARE(dstModel.rowCount(), 0);
}

void TestBackupController::cancelImportClearsPreview()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("c.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));

    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    BackupController imp;
    imp.setModel(&dstModel);
    QVERIFY(imp.prepareImport(path, QStringLiteral("pw")));
    QCOMPARE(imp.previewAddCount(), 1);

    imp.cancelImport();
    QCOMPARE(imp.state(), BackupController::State::Idle);
    QCOMPARE(imp.previewAddCount(), 0);
    QCOMPARE(imp.previewUpdateCount(), 0);
    QCOMPARE(dstModel.rowCount(), 0);
}

void TestBackupController::applyImportLeavesIdleOnSuccess()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("ok.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));

    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    BackupController imp;
    imp.setModel(&dstModel);

    QSignalSpy stateSpy(&imp, &BackupController::stateChanged);
    QVERIFY(imp.prepareImport(path, QStringLiteral("pw")));
    QVERIFY(imp.applyImport());
    QCOMPARE(imp.state(), BackupController::State::Done);
    // Each transition emits stateChanged at least once.
    QVERIFY(stateSpy.count() >= 3); // Working, NeedsConfirm, Working, Done
}

void TestBackupController::exportFailsWithoutPassword()
{
    QTemporaryDir src, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("none.qumesh-backup"));
    QVERIFY(!exp.exportTo(path, QString()));
    QCOMPARE(exp.lastError(), BackupController::Error::BadPassword);
}

/// Regression: prepareImport returns row positions in `m_pendingPlan`,
/// but applyImport must re-resolve by stable id at apply time.
/// Otherwise a model mutation while the confirm dialog is open could
/// silently overwrite an unrelated row at the captured index.
///
/// Scenario: the destination has machines X and Y. The user imports
/// a backup that updates X. While the confirm dialog is open, the
/// user removes X from a different surface. applyImport must NOT
/// then overwrite Y; it should append the imported X.
void TestBackupController::importIsRobustToModelMutationBetweenPrepareAndApply()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("x"), QStringLiteral("10.0.0.1"),
         QStringLiteral("newpass"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("mut.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));

    ConfigStore dstStore(dst.path());
    ComputerModel dstModel;
    dstModel.setStore(&dstStore);
    seed(dstModel, QStringLiteral("x"), QStringLiteral("10.0.0.1"),
         QStringLiteral("oldpass"));
    seed(dstModel, QStringLiteral("y"), QStringLiteral("10.0.0.2"),
         QStringLiteral("ypass"));

    BackupController imp;
    imp.setModel(&dstModel);
    QVERIFY(imp.prepareImport(path, QStringLiteral("pw")));
    QCOMPARE(imp.previewUpdateCount(), 1);

    // Mutate the model between prepare and apply: remove the matched
    // row. The pre-fix code captured `existingRow = 0`, so apply
    // would have written through row 0 — which is now `y`.
    QVERIFY(dstModel.removeAt(0));
    QCOMPARE(dstModel.rowCount(), 1);
    QCOMPARE(dstModel.at(0).name, QStringLiteral("y"));

    QVERIFY(imp.applyImport());

    // After: `y` must NOT have been overwritten by the imported `x`.
    // The import should have appended the imported row instead.
    QCOMPARE(dstModel.rowCount(), 2);
    bool foundY = false, foundX = false;
    for (int i = 0; i < dstModel.rowCount(); ++i) {
        if (dstModel.at(i).name == QStringLiteral("y")
            && dstModel.at(i).pass == QStringLiteral("ypass")) foundY = true;
        if (dstModel.at(i).name == QStringLiteral("x")
            && dstModel.at(i).pass == QStringLiteral("newpass")) foundX = true;
    }
    QVERIFY2(foundY, "y must survive — its row was overwritten by stale-index apply");
    QVERIFY2(foundX, "imported x must be appended");
}

/// cancelImport is the QML "dismiss this dialog and reset me" hook.
/// It must clear state even after a Done transition so the next
/// dialog open starts from Idle instead of reading a stale message.
void TestBackupController::cancelImportResetsAfterDoneState()
{
    QTemporaryDir src, dst, file;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    seed(srcModel, QStringLiteral("a"), QStringLiteral("1.1.1.1"));

    BackupController exp;
    exp.setModel(&srcModel);
    const QUrl path = tmpFile(file, QStringLiteral("d.qumesh-backup"));
    QVERIFY(exp.exportTo(path, QStringLiteral("pw")));
    QCOMPARE(exp.state(), BackupController::State::Done);
    QVERIFY(!exp.message().isEmpty());

    exp.cancelImport();
    QCOMPARE(exp.state(), BackupController::State::Idle);
    QVERIFY(exp.message().isEmpty());
    QCOMPARE(exp.lastError(), BackupController::Error::None);
}

/// applyImport called outside NeedsConfirm must record an error
/// rather than failing silently — QML observers wired to messageChanged
/// would never know why the call returned false.
void TestBackupController::applyImportWithoutPreparedPlanRecordsError()
{
    QTemporaryDir src;
    ConfigStore srcStore(src.path());
    ComputerModel srcModel;
    srcModel.setStore(&srcStore);
    BackupController c;
    c.setModel(&srcModel);
    QVERIFY(!c.applyImport());
    QCOMPARE(c.state(), BackupController::State::Failed);
    QCOMPARE(c.lastError(), BackupController::Error::Internal);
    QVERIFY(!c.message().isEmpty());
}

QTEST_GUILESS_MAIN(TestBackupController)
#include "test_backupcontroller.moc"
