#include <QtTest>

class SmokeTest : public QObject
{
    Q_OBJECT
private slots:
    void scaffolding()
    {
        QVERIFY(true);
    }
};

QTEST_GUILESS_MAIN(SmokeTest)
#include "test_smoke.moc"
