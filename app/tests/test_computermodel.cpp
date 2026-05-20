// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "computermodel.h"
#include "configstore.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using qumesh::config::ConfigStore;
using qumesh::model::Computer;
using qumesh::model::ComputerModel;

class TestComputerModel : public QObject
{
    Q_OBJECT
private slots:
    void contractWithItemModelTester();
    void rolesNameMatchEnum();
    void addEmitsRowsInsertedAndPersists();
    void removeEmitsRowsRemovedAndPersists();
    void setDataEmitsDataChanged();
    void persistFailureRollsBack();
    void reloadFromStoreOnSetStore();
    void sshConfigRoundTripsCompression();
    void sshMutationsEmitDataChanged();
};

void TestComputerModel::contractWithItemModelTester()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    ComputerModel model;
    model.setStore(&store);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QCOMPARE(model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                                QStringLiteral("u"), QStringLiteral("p"), false),
             0);
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.removeAt(0));
    QCOMPARE(model.rowCount(), 0);
}

void TestComputerModel::rolesNameMatchEnum()
{
    ComputerModel m;
    const auto roles = m.roleNames();
    QCOMPARE(roles.value(ComputerModel::IdRole), QByteArrayLiteral("id"));
    QCOMPARE(roles.value(ComputerModel::NameRole), QByteArrayLiteral("name"));
    QCOMPARE(roles.value(ComputerModel::HostRole), QByteArrayLiteral("host"));
    QCOMPARE(roles.value(ComputerModel::UserRole), QByteArrayLiteral("user"));
    QCOMPARE(roles.value(ComputerModel::PassRole), QByteArrayLiteral("pass"));
    QCOMPARE(roles.value(ComputerModel::TlsRole), QByteArrayLiteral("tls"));
    QCOMPARE(roles.value(ComputerModel::DigestRealmRole), QByteArrayLiteral("digestrealm"));
}

void TestComputerModel::addEmitsRowsInsertedAndPersists()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    ComputerModel model;
    model.setStore(&store);

    QSignalSpy beginSpy(&model, &QAbstractListModel::rowsAboutToBeInserted);
    QSignalSpy endSpy(&model, &QAbstractListModel::rowsInserted);

    const int row = model.addComputer(QStringLiteral("amt-01"), QStringLiteral("10.0.0.5"),
                                      QStringLiteral("admin"),
                                      QStringLiteral("secret"), true);
    QCOMPARE(row, 0);
    QCOMPARE(beginSpy.size(), 1);
    QCOMPARE(endSpy.size(), 1);

    // Persisted to disk.
    ConfigStore reread(tmp.path());
    QCOMPARE(reread.loadComputers().size(), 1);
    QCOMPARE(reread.loadComputers().first().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("amt-01"));
}

void TestComputerModel::removeEmitsRowsRemovedAndPersists()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    ComputerModel model;
    model.setStore(&store);
    model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                      QStringLiteral("u"), QStringLiteral("p"), false);

    QSignalSpy removedSpy(&model, &QAbstractListModel::rowsRemoved);
    QVERIFY(model.removeAt(0));
    QCOMPARE(removedSpy.size(), 1);
    QCOMPARE(model.rowCount(), 0);

    ConfigStore reread(tmp.path());
    QVERIFY(reread.loadComputers().isEmpty());
}

void TestComputerModel::setDataEmitsDataChanged()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    ComputerModel model;
    model.setStore(&store);
    model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                      QStringLiteral("u"), QStringLiteral("p"), false);

    QSignalSpy changedSpy(&model, &QAbstractListModel::dataChanged);
    QVERIFY(model.setData(model.index(0), QStringLiteral("renamed"), ComputerModel::NameRole));
    // NameRole change emits both NameRole and DisplayRole notifications.
    QCOMPARE(changedSpy.size(), 2);

    ConfigStore reread(tmp.path());
    QCOMPARE(reread.loadComputers().first().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("renamed"));
}

void TestComputerModel::persistFailureRollsBack()
{
    // Point the store at a path we can't write to. Use a non-existent root
    // that mkpath can't create (an existing file blocks dir creation).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString fakeRoot = QDir(tmp.path()).filePath(QStringLiteral("blocker"));
    QFile f(fakeRoot);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    ConfigStore store(QDir(fakeRoot).filePath(QStringLiteral("sub")));
    ComputerModel model;
    model.setStore(&store);

    QCOMPARE(model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                                QStringLiteral("u"), QStringLiteral("p"), false),
             -1);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.lastError().isEmpty());
}

void TestComputerModel::reloadFromStoreOnSetStore()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    {
        ConfigStore store(tmp.path());
        ComputerModel m;
        m.setStore(&store);
        m.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                       QStringLiteral("u"), QStringLiteral("p"), false);
    }
    ConfigStore store(tmp.path());
    ComputerModel m;
    m.setStore(&store);
    QCOMPARE(m.rowCount(), 1);
    QCOMPARE(m.at(0).name, QStringLiteral("a"));
}

void TestComputerModel::sshConfigRoundTripsCompression()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Save with compression enabled.
    {
        ConfigStore store(tmp.path());
        ComputerModel m;
        m.setStore(&store);
        const int row = m.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                                      QStringLiteral("u"), QStringLiteral("p"), false);
        QCOMPARE(row, 0);
        const QVariantMap cfg = {
            {QStringLiteral("enabled"), true},
            {QStringLiteral("host"), QStringLiteral("jump.example.com")},
            {QStringLiteral("port"), 22},
            {QStringLiteral("user"), QStringLiteral("ubuntu")},
            {QStringLiteral("authMode"), 0},
            {QStringLiteral("password"), QStringLiteral("pw")},
            {QStringLiteral("compression"), true},
        };
        QVERIFY(m.setSshConfig(row, cfg));
        const QVariantMap readBack = m.sshConfigFor(row);
        QCOMPARE(readBack.value(QStringLiteral("compression")).toBool(), true);
    }

    // Reload from disk; flag must survive.
    {
        ConfigStore store(tmp.path());
        ComputerModel m;
        m.setStore(&store);
        QCOMPARE(m.rowCount(), 1);
        const QVariantMap cfg = m.sshConfigFor(0);
        QCOMPARE(cfg.value(QStringLiteral("compression")).toBool(), true);
    }

    // Configs without the field default to false.
    {
        QTemporaryDir tmp2;
        QVERIFY(tmp2.isValid());
        ConfigStore store(tmp2.path());
        ComputerModel m;
        m.setStore(&store);
        m.addComputer(QStringLiteral("b"), QStringLiteral("2.2.2.2"),
                       QStringLiteral("u"), QStringLiteral("p"), false);
        const QVariantMap cfg = m.sshConfigFor(0);
        QCOMPARE(cfg.value(QStringLiteral("compression")).toBool(), false);
    }
}

/// #284 — both setSshConfig() and addTrustedSshHostKey() mutate row
/// state. QAbstractItemModel's contract requires dataChanged() after
/// any mutation, regardless of whether the field has an explicit
/// role declared.
void TestComputerModel::sshMutationsEmitDataChanged()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    ComputerModel m;
    m.setStore(&store);
    const int row = m.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"),
                                  QStringLiteral("u"), QStringLiteral("p"), false);
    QCOMPARE(row, 0);

    QSignalSpy spy(&m, &QAbstractItemModel::dataChanged);

    const QVariantMap cfg = {
        {QStringLiteral("enabled"), true},
        {QStringLiteral("host"), QStringLiteral("jump")},
        {QStringLiteral("user"), QStringLiteral("ubuntu")},
        {QStringLiteral("authMode"), 0},
        {QStringLiteral("password"), QStringLiteral("pw")},
    };
    QVERIFY(m.setSshConfig(row, cfg));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<QModelIndex>().row(), row);

    spy.clear();
    QVERIFY(m.addTrustedSshHostKey(row, QStringLiteral("AA:BB:CC")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<QModelIndex>().row(), row);
}

QTEST_GUILESS_MAIN(TestComputerModel)
#include "test_computermodel.moc"
