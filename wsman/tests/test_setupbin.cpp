// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/setupbin.h"

#include <QtTest>

using namespace qumesh::wsman;

class TestSetupBin : public QObject
{
    Q_OBJECT
private slots:
    void scrambleIsInverseOfDescramble();
    void encodeProducesHeaderPlusRecordSizedOutput();
    void encodedHeaderCarriesCorrectUuidPerVersion();
    void roundTripIsByteIdentical();
    void scrambledRecordRoundTrips();
    void decodeRejectsBadUuid();
    void decodeRejectsMismatchedRecordCount();
    void decodeHandlesOversizeLengthFieldSafely();
    void variableEntriesArePaddedToFourBytes();
    void validatorAcceptsTypicalV2File();
    void validatorRejectsCertAddWithoutConfig();
    void validatorRejectsCertAddWithDeleteConfig();
    void validatorRejectsAdminWithoutNewPasswordOnV2();
    void validatorRejectsOversizeHostname();
    void validatorRejectsDuplicateVariable();
    void validatorRejectsBadGuidLength();
    void catalogContainsKnownEntries();
};

namespace {

QByteArray u8(unsigned v)  { return QByteArray(1, static_cast<char>(v & 0xFF)); }
QByteArray u16(unsigned v) { QByteArray b(2, '\0'); b[0] = char(v & 0xFF); b[1] = char((v >> 8) & 0xFF); return b; }
QByteArray u32(unsigned v) {
    QByteArray b(4, '\0');
    b[0] = char(v & 0xFF);
    b[1] = char((v >> 8) & 0xFF);
    b[2] = char((v >> 16) & 0xFF);
    b[3] = char((v >> 24) & 0xFF);
    return b;
}
QByteArray guid16(char fill) { return QByteArray(16, fill); }

SetupBinFile sampleV2File()
{
    SetupBinFile f;
    f.version = 2;
    f.flags = 1; // do not consume records

    SetupBinRecord rec;
    rec.flags = 1; // valid

    rec.variables.append({1, 1, QByteArray("password1")});
    rec.variables.append({1, 2, QByteArray("newpassword!2")});
    rec.variables.append({1, 3, u8(1)});  // Manageability = Intel AMT
    rec.variables.append({1, 4, u8(0)});  // Firmware Local Update = Disabled
    rec.variables.append({1, 6, guid16('\x42')}); // Power Package GUID
    rec.variables.append({2, 7, u8(1)});  // User Defined Cert Config = Enabled
    rec.variables.append({2, 11, QByteArray("vpro-test")}); // hostname
    rec.variables.append({2, 13, u8(2)}); // DHCP = Enabled
    rec.variables.append({2, 18, u16(16993)}); // Provision server port
    rec.variables.append({2, 34, u32(1)});      // Service Type = Reactive
    rec.variables.append({2, 35, guid16('\x99')}); // Service Provider GUID

    f.records.append(rec);
    return f;
}

} // namespace

void TestSetupBin::scrambleIsInverseOfDescramble()
{
    QByteArray data;
    for (int i = 0; i < 256; ++i) data.append(static_cast<char>(i));
    const auto scrambled = scrambleSetupBinBody(data);
    QCOMPARE(descrambleSetupBinBody(scrambled), data);

    // Forward transform = +0x11 (mod 256) per byte.
    QCOMPARE(static_cast<std::uint8_t>(scrambled.at(0)), std::uint8_t{0x11});
    QCOMPARE(static_cast<std::uint8_t>(scrambled.at(0xEF)), std::uint8_t{0x00});
}

void TestSetupBin::encodeProducesHeaderPlusRecordSizedOutput()
{
    auto file = sampleV2File();
    const QByteArray encoded = encodeSetupBin(file);
    QVERIFY(!encoded.isEmpty());
    QCOMPARE(encoded.size() % 512, 0);
    QCOMPARE(encoded.size(), 512 + 512 * file.records.size());
}

void TestSetupBin::encodedHeaderCarriesCorrectUuidPerVersion()
{
    for (int v = 1; v <= 4; ++v) {
        SetupBinFile f;
        f.version = v;
        // GUID lifted from the legacy table for cross-check.
        const QByteArray encoded = encodeSetupBin(f);
        QVERIFY2(encoded.size() == 512, qPrintable(QString::number(encoded.size())));
        bool ok = false;
        const auto decoded = decodeSetupBin(encoded, &ok);
        QVERIFY(ok);
        QCOMPARE(decoded.version, v);
    }
}

void TestSetupBin::roundTripIsByteIdentical()
{
    const auto original = sampleV2File();
    const QByteArray first = encodeSetupBin(original);
    QVERIFY(!first.isEmpty());

    bool ok = false;
    const auto decoded = decodeSetupBin(first, &ok);
    QVERIFY(ok);

    const QByteArray second = encodeSetupBin(decoded);
    QCOMPARE(second, first);

    // Spot-check semantic round-trip too.
    QCOMPARE(decoded.version, original.version);
    QCOMPARE(decoded.flags, original.flags);
    QCOMPARE(decoded.records.size(), original.records.size());
    QCOMPARE(decoded.records.at(0).variables.size(), original.records.at(0).variables.size());
}

void TestSetupBin::scrambledRecordRoundTrips()
{
    auto original = sampleV2File();
    original.records[0].flags = 1 | 2; // valid | scrambled

    const QByteArray encoded = encodeSetupBin(original);
    QCOMPARE(encoded.size(), 1024);

    // The body bytes following the 24-byte record header must not match
    // the plaintext entries — the scramble actually fired.
    const QByteArray header = encoded.left(512);
    Q_UNUSED(header);
    const QByteArray recBody = encoded.mid(512 + 24, 488);
    bool sawNonZero = false;
    for (char c : recBody) if (c != 0) { sawNonZero = true; break; }
    QVERIFY(sawNonZero);

    // Hostname plaintext must not appear in the scrambled body.
    QVERIFY(!recBody.contains(QByteArray("vpro-test")));

    bool ok = false;
    const auto decoded = decodeSetupBin(encoded, &ok);
    QVERIFY(ok);
    QCOMPARE(decoded.records.size(), 1);
    QCOMPARE(decoded.records.at(0).flags, original.records.at(0).flags);

    // Re-encoding the scrambled record must produce identical bytes:
    // scramble is a fixed permutation and our padding is deterministic.
    const QByteArray re = encodeSetupBin(decoded);
    QCOMPARE(re, encoded);
}

void TestSetupBin::decodeRejectsBadUuid()
{
    QByteArray junk(512, '\xab');
    bool ok = true;
    const auto decoded = decodeSetupBin(junk, &ok);
    QVERIFY(!ok);
    QVERIFY(decoded.records.isEmpty());
}

void TestSetupBin::decodeRejectsMismatchedRecordCount()
{
    // Start from a real v2 file with one record, then bump the
    // declared dataRecordCount field at offset 28 so the on-disk count
    // disagrees with the actual record body.
    QByteArray encoded = encodeSetupBin(sampleV2File());
    QVERIFY(encoded.size() >= 32);
    encoded[28] = 5; // claim 5 records but only one follows

    bool ok = true;
    const auto decoded = decodeSetupBin(encoded, &ok);
    QVERIFY(!ok);
}

void TestSetupBin::decodeHandlesOversizeLengthFieldSafely()
{
    // Build a valid file, then corrupt the first entry's length field
    // to 0xFFFF (max u16). The decoder must stop reading the record
    // rather than over-read or wraparound in the pad-advance arithmetic.
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({1, 1, QByteArray("pw")});
    f.records.append(rec);
    QByteArray encoded = encodeSetupBin(f);
    QCOMPARE(encoded.size(), 1024);

    // First entry starts at offset 512 + 24. Its 2-byte length field
    // sits at offsets 4..5 within the entry.
    const int lengthOffset = 512 + 24 + 4;
    encoded[lengthOffset] = char(0xFF);
    encoded[lengthOffset + 1] = char(0xFF);

    bool ok = false;
    const auto decoded = decodeSetupBin(encoded, &ok);
    QVERIFY(ok); // top-level record count still matches; parser just drops the bogus entry
    QCOMPARE(decoded.records.size(), 1);
    // The oversize entry must be dropped; nothing else should be invented.
    QVERIFY(decoded.records.at(0).variables.isEmpty());
}

void TestSetupBin::variableEntriesArePaddedToFourBytes()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    // Single-byte value forces 3 bytes of trailing pad inside the entry.
    rec.variables.append({1, 3, u8(1)});
    f.records.append(rec);

    const QByteArray encoded = encodeSetupBin(f);
    QCOMPARE(encoded.size(), 1024);

    // The first entry sits at offset 512 + 24. Its declared length is 1.
    const int entryStart = 512 + 24;
    QCOMPARE(static_cast<std::uint8_t>(encoded.at(entryStart + 4)), std::uint8_t{1}); // length lo
    QCOMPARE(static_cast<std::uint8_t>(encoded.at(entryStart + 5)), std::uint8_t{0}); // length hi

    // The next entry header must start 12 bytes after entryStart
    // (8-byte entry header + 1-byte value + 3-byte pad = 12).
    // That position is where the terminator (moduleId=0) lives, so the
    // bytes at entryStart+12 .. +15 should all be zero.
    for (int i = 0; i < 4; ++i) {
        QCOMPARE(static_cast<std::uint8_t>(encoded.at(entryStart + 12 + i)),
                 std::uint8_t{0});
    }

    bool ok = false;
    const auto decoded = decodeSetupBin(encoded, &ok);
    QVERIFY(ok);
    QCOMPARE(decoded.records.size(), 1);
    QCOMPARE(decoded.records.at(0).variables.size(), 1);
    QCOMPARE(decoded.records.at(0).variables.at(0).value, u8(1));
}

void TestSetupBin::validatorAcceptsTypicalV2File()
{
    const auto f = sampleV2File();
    const auto err = validateSetupBin(f);
    QVERIFY2(err.isEmpty(), qPrintable(err));
}

void TestSetupBin::validatorRejectsCertAddWithoutConfig()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({2, 8, QByteArray(40, '\x01')}); // cert-add blob
    f.records.append(rec);

    const auto err = validateSetupBin(f);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains("2/8") || err.contains("Certificate Addition"));
}

void TestSetupBin::validatorRejectsCertAddWithDeleteConfig()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({2, 7, u8(2)}); // "Delete"
    rec.variables.append({2, 8, QByteArray(40, '\x01')});
    f.records.append(rec);

    const auto err = validateSetupBin(f);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains("Delete"));
}

void TestSetupBin::validatorRejectsAdminWithoutNewPasswordOnV2()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({1, 1, QByteArray("admin")});
    f.records.append(rec);

    const auto err = validateSetupBin(f);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains("admin"));
}

void TestSetupBin::validatorRejectsOversizeHostname()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({2, 11, QByteArray(64, 'h')}); // 64 > 63 cap
    f.records.append(rec);

    const auto err = validateSetupBin(f);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains("Hostname"));
}

void TestSetupBin::validatorRejectsDuplicateVariable()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({1, 1, QByteArray("first")});
    rec.variables.append({1, 1, QByteArray("again")});
    f.records.append(rec);

    const auto err = validateSetupBin(f);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains("duplicate"));
}

void TestSetupBin::validatorRejectsBadGuidLength()
{
    SetupBinFile f;
    f.version = 2;
    SetupBinRecord rec;
    rec.flags = 1;
    rec.variables.append({1, 6, QByteArray(8, '\x42')}); // GUID needs 16
    f.records.append(rec);

    const auto err = validateSetupBin(f);
    QVERIFY(!err.isEmpty());
}

void TestSetupBin::catalogContainsKnownEntries()
{
    const auto *hostname = findSetupBinVarSpec(2, 11);
    QVERIFY(hostname != nullptr);
    QCOMPARE(QString::fromLatin1(hostname->name), QStringLiteral("Hostname"));
    QCOMPARE(hostname->type, SetupBinVarType::Binary);

    const auto *dhcp = findSetupBinVarSpec(2, 13);
    QVERIFY(dhcp != nullptr);
    QCOMPARE(dhcp->type, SetupBinVarType::U8);
    QVERIFY(dhcp->enumValues != nullptr);

    QVERIFY(findSetupBinVarSpec(2, 9) == nullptr); // 2/9 is unassigned

    const auto all = allSetupBinVarSpecs();
    QVERIFY(all.size() >= 30);
}

QTEST_GUILESS_MAIN(TestSetupBin)
#include "test_setupbin.moc"
