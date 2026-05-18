// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "rmcp/target_range.h"

#include <QtTest>

using namespace qumesh::rmcp;

class TestTargetRange : public QObject
{
    Q_OBJECT
private slots:
    void singleIp();
    void inclusiveRange();
    void cidrSlash24();
    void cidrSlash30();
    void cidrTooBroadRejected();
    void rangeReversedRejected();
    void rangeTooLargeRejected();
    void malformedRejected();
    void ipv6Rejected();
    void emptyRejected();
};

void TestTargetRange::singleIp()
{
    QString err;
    const auto out = parseTargets(QStringLiteral("10.0.0.5"), &err);
    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0].toString(), QStringLiteral("10.0.0.5"));
    QVERIFY(err.isEmpty());
}

void TestTargetRange::inclusiveRange()
{
    const auto out = parseTargets(QStringLiteral("10.0.0.5-10.0.0.10"));
    QCOMPARE(out.size(), 6);
    QCOMPARE(out.first().toString(), QStringLiteral("10.0.0.5"));
    QCOMPARE(out.last().toString(),  QStringLiteral("10.0.0.10"));
}

void TestTargetRange::cidrSlash24()
{
    const auto out = parseTargets(QStringLiteral("10.0.0.0/24"));
    QCOMPARE(out.size(), 256);
    QCOMPARE(out.first().toString(), QStringLiteral("10.0.0.0"));
    QCOMPARE(out.last().toString(),  QStringLiteral("10.0.0.255"));
}

void TestTargetRange::cidrSlash30()
{
    const auto out = parseTargets(QStringLiteral("10.0.0.16/30"));
    QCOMPARE(out.size(), 4);
    QCOMPARE(out.first().toString(), QStringLiteral("10.0.0.16"));
    QCOMPARE(out.last().toString(),  QStringLiteral("10.0.0.19"));
}

void TestTargetRange::cidrTooBroadRejected()
{
    QString err;
    const auto out = parseTargets(QStringLiteral("10.0.0.0/15"), &err);
    QVERIFY(out.isEmpty());
    QVERIFY(err.contains(QStringLiteral("/15")));
}

void TestTargetRange::rangeReversedRejected()
{
    QString err;
    QVERIFY(parseTargets(QStringLiteral("10.0.0.10-10.0.0.5"), &err).isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestTargetRange::rangeTooLargeRejected()
{
    QString err;
    // 10.0.0.0 → 10.1.0.0 = 65 537 hosts, one past the cap.
    QVERIFY(parseTargets(QStringLiteral("10.0.0.0-10.1.0.0"), &err).isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestTargetRange::malformedRejected()
{
    QString err;
    QVERIFY(parseTargets(QStringLiteral("not.an.ip"), &err).isEmpty());
    QVERIFY(!err.isEmpty());
    QVERIFY(parseTargets(QStringLiteral("10.0.0.0/abc"), &err).isEmpty());
    QVERIFY(parseTargets(QStringLiteral("10.0.0.0/33"), &err).isEmpty());
}

void TestTargetRange::ipv6Rejected()
{
    QString err;
    QVERIFY(parseTargets(QStringLiteral("fe80::1"), &err).isEmpty());
}

void TestTargetRange::emptyRejected()
{
    QString err;
    QVERIFY(parseTargets(QStringLiteral("   "), &err).isEmpty());
    QVERIFY(err.contains(QStringLiteral("empty"), Qt::CaseInsensitive));
}

QTEST_APPLESS_MAIN(TestTargetRange)
#include "test_target_range.moc"
