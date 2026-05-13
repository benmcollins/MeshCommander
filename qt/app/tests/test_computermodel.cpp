#include "computermodel.h"
#include "configstore.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using meshcommander::config::ConfigStore;
using meshcommander::model::Computer;
using meshcommander::model::ComputerModel;

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
};

void TestComputerModel::contractWithItemModelTester()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    ComputerModel model;
    model.setStore(&store);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QCOMPARE(model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"), 16992,
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
    QCOMPARE(roles.value(ComputerModel::PortRole), QByteArrayLiteral("port"));
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
                                      16992, QStringLiteral("admin"),
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
    model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"), 16992,
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
    model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"), 16992,
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

    QCOMPARE(model.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"), 16992,
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
        m.addComputer(QStringLiteral("a"), QStringLiteral("1.1.1.1"), 16992,
                       QStringLiteral("u"), QStringLiteral("p"), false);
    }
    ConfigStore store(tmp.path());
    ComputerModel m;
    m.setStore(&store);
    QCOMPARE(m.rowCount(), 1);
    QCOMPARE(m.at(0).name, QStringLiteral("a"));
}

QTEST_GUILESS_MAIN(TestComputerModel)
#include "test_computermodel.moc"
