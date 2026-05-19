// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/setupbin.h"

#include <QHash>

#include <array>
#include <cstring>

namespace qumesh::wsman {

namespace {

constexpr int kHeaderSize = 512;
constexpr int kRecordSize = 512;
constexpr int kRecordHeaderSize = 24;
constexpr int kRecordBodySize = kRecordSize - kRecordHeaderSize;

[[maybe_unused]] constexpr std::uint32_t kRecordFlagValid = 0x1;
constexpr std::uint32_t kRecordFlagScrambled = 0x2;

// File-type UUIDs at offset 0 of the header. Order matches version 1..4.
// Bytes are the on-disk little-endian-first-three-groups encoding lifted
// from the legacy `AmtSetupBinSetupGuids` table.
constexpr std::array<std::array<std::uint8_t, 16>, 4> kFileTypeGuids = {{
    {{0xb5, 0x16, 0xfb, 0x71, 0x87, 0xcb, 0xf9, 0x4a,
      0xb4, 0x41, 0xca, 0x7b, 0x38, 0x35, 0x78, 0xf9}}, // v1
    {{0x96, 0xb2, 0x81, 0x58, 0xcf, 0x6b, 0x72, 0x4c,
      0x8b, 0x91, 0xa1, 0x5e, 0x51, 0x2e, 0x99, 0xc4}}, // v2
    {{0xa7, 0xf7, 0xf6, 0xc6, 0x89, 0xc4, 0xf6, 0x47,
      0x93, 0xed, 0xe2, 0xe5, 0x02, 0x0d, 0xa5, 0x1d}}, // v3
    {{0xaa, 0xa9, 0x34, 0x52, 0xe1, 0x29, 0xa9, 0x44,
      0x8d, 0x4d, 0x08, 0x1c, 0x07, 0xb9, 0x63, 0x53}}, // v4
}};

// Variable-ID catalog. Lifted verbatim from `AmtSetupBinVarIds` in the
// legacy `amt-setupbin-0.1.0.js`. Keeping the same numbering and enum
// labels so a file authored against either implementation round-trips.

constexpr SetupBinEnumLabel kManageabilityFeatureValues[] = {
    {0, "None"}, {1, "Intel AMT"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kFirmwareLocalUpdateValues[] = {
    {0, "Disabled"}, {1, "Enabled"}, {2, "Password Protected"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kFirmwareUpdateQualifierValues[] = {
    {0, "Always"}, {1, "Never"}, {2, "Restricted"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kOnOffValues[] = {
    {0, "Off"}, {1, "On"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kUserCertConfigValues[] = {
    {0, "Disabled"}, {1, "Enabled"}, {2, "Delete"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kSolIderMaskValues[] = {
    {0, "None"},
    {1, "SOL only - User/Pass Disabled"},
    {2, "IDER only - User/Pass Disabled"},
    {3, "SOL+IDER - User/Pass Disabled"},
    {4, "None - User/Pass Enabled"},
    {5, "SOL only - User/Pass Enabled"},
    {6, "IDER only - User/Pass Enabled"},
    {7, "SOL+IDER - User/Pass Enabled"},
    {-1, nullptr},
};
constexpr SetupBinEnumLabel kDhcpValues[] = {
    {1, "Disabled"}, {2, "Enabled"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kEnabledDisabledValues[] = {
    {0, "Disabled"}, {1, "Enabled"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kProvisioningModeValues[] = {
    {0, "Enterprise"}, {1, "Small Business"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kSharedDedicatedValues[] = {
    {0, "Dedicated"}, {1, "Shared"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kOptInConsentValues[] = {
    {0, "Disabled"}, {1, "KVM"}, {255, "All"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kMeHaltActiveValues[] = {
    {0, "Stop"}, {1, "Start"}, {-1, nullptr},
};
constexpr SetupBinEnumLabel kManualSetupValues[] = {
    {0, "Automated"}, {1, "Manual"}, {-1, nullptr},
};

constexpr SetupBinVarSpec kVarSpecs[] = {
    // Module 1: MEBx
    {1,  1, SetupBinVarType::Binary, "Current MEBx Password",          nullptr},
    {1,  2, SetupBinVarType::Binary, "New MEBx Password",              nullptr},
    {1,  3, SetupBinVarType::U8,     "Manageability Feature Selection", kManageabilityFeatureValues},
    {1,  4, SetupBinVarType::U8,     "Firmware Local Update",           kFirmwareLocalUpdateValues},
    {1,  5, SetupBinVarType::U8,     "Firmware Update Qualifier",       kFirmwareUpdateQualifierValues},
    {1,  6, SetupBinVarType::Guid,   "Power Package",                   nullptr},

    // Module 2: Provisioning
    {2,  1, SetupBinVarType::Binary, "Provisioning Preshared Key ID (PID)",      nullptr},
    {2,  2, SetupBinVarType::Binary, "Provisioning Preshared Key (PPS)",         nullptr},
    {2,  3, SetupBinVarType::Binary, "PKI DNS Suffix",                            nullptr},
    {2,  4, SetupBinVarType::Binary, "Configuration Server FQDN",                 nullptr},
    {2,  5, SetupBinVarType::U8,     "Remote Configuration Enabled (RCFG)",       kOnOffValues},
    {2,  6, SetupBinVarType::U8,     "Pre-Installed Certificates Enabled",        kOnOffValues},
    {2,  7, SetupBinVarType::U8,     "User Defined Certificate Configuration",    kUserCertConfigValues},
    {2,  8, SetupBinVarType::Binary, "User Defined Certificate Addition",         nullptr},
    {2, 10, SetupBinVarType::U8,     "SOL/IDER Redirection Configuration",        kSolIderMaskValues},
    {2, 11, SetupBinVarType::Binary, "Hostname",                                   nullptr},
    {2, 12, SetupBinVarType::Binary, "Domain Name",                                nullptr},
    {2, 13, SetupBinVarType::U8,     "DHCP",                                       kDhcpValues},
    {2, 14, SetupBinVarType::U8,     "Secure Firmware Update (SFWU)",              kEnabledDisabledValues},
    {2, 15, SetupBinVarType::Binary, "ITO",                                        nullptr},
    {2, 16, SetupBinVarType::U8,     "Provisioning Mode (PM)",                     kProvisioningModeValues},
    {2, 17, SetupBinVarType::Binary, "Provisioning Server Address",                nullptr},
    {2, 18, SetupBinVarType::U16,    "Provision Server Port Number (PSPO)",        nullptr},
    {2, 19, SetupBinVarType::Binary, "Static IPv4 Parameters",                     nullptr},
    {2, 20, SetupBinVarType::Binary, "VLAN",                                       nullptr},
    {2, 21, SetupBinVarType::Binary, "PASS Policy Flag",                           nullptr},
    {2, 22, SetupBinVarType::Binary, "IPv6",                                       nullptr},
    {2, 23, SetupBinVarType::U8,     "Shared/Dedicated FQDN",                      kSharedDedicatedValues},
    {2, 24, SetupBinVarType::U8,     "Dynamic DNS Update",                         kEnabledDisabledValues},
    {2, 25, SetupBinVarType::U8,     "Remote Desktop (KVM) State",                 kEnabledDisabledValues},
    {2, 26, SetupBinVarType::U8,     "Opt-in User Consent Option",                 kOptInConsentValues},
    {2, 27, SetupBinVarType::U8,     "Opt-in Remote IT Consent Policy",            kEnabledDisabledValues},
    {2, 28, SetupBinVarType::U8,     "ME Provision Halt/Active",                   kMeHaltActiveValues},
    {2, 29, SetupBinVarType::U8,     "Manual Setup and Configuration",             kManualSetupValues},
    {2, 30, SetupBinVarType::U32,    "Support Channel Identifier",                 nullptr},
    {2, 31, SetupBinVarType::Binary, "Support Channel Description",                nullptr},
    {2, 32, SetupBinVarType::Binary, "Service Account Number",                     nullptr},
    {2, 33, SetupBinVarType::Binary, "Enrollment Passcode",                        nullptr},
    {2, 34, SetupBinVarType::U32,    "Service Type",                               nullptr},
    {2, 35, SetupBinVarType::Guid,   "Service Provider Identifier",                nullptr},
};

void writeU16Le(QByteArray &out, std::uint16_t value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

void writeU32Le(QByteArray &out, std::uint32_t value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

std::uint16_t readU16Le(const QByteArray &bytes, int offset)
{
    const auto b0 = static_cast<std::uint8_t>(bytes.at(offset));
    const auto b1 = static_cast<std::uint8_t>(bytes.at(offset + 1));
    return static_cast<std::uint16_t>(b0 | (b1 << 8));
}

std::uint32_t readU32Le(const QByteArray &bytes, int offset)
{
    const auto b0 = static_cast<std::uint8_t>(bytes.at(offset));
    const auto b1 = static_cast<std::uint8_t>(bytes.at(offset + 1));
    const auto b2 = static_cast<std::uint8_t>(bytes.at(offset + 2));
    const auto b3 = static_cast<std::uint8_t>(bytes.at(offset + 3));
    return static_cast<std::uint32_t>(b0)
        | (static_cast<std::uint32_t>(b1) << 8)
        | (static_cast<std::uint32_t>(b2) << 16)
        | (static_cast<std::uint32_t>(b3) << 24);
}

int fileVersionForUuid(const QByteArray &bytes)
{
    if (bytes.size() < kHeaderSize) return 0;
    for (std::size_t i = 0; i < kFileTypeGuids.size(); ++i) {
        if (std::memcmp(bytes.constData(), kFileTypeGuids[i].data(), 16) == 0) {
            return static_cast<int>(i + 1);
        }
    }
    return 0;
}

} // namespace

QByteArray scrambleSetupBinBody(const QByteArray &body)
{
    QByteArray out;
    out.resize(body.size());
    for (int i = 0; i < body.size(); ++i) {
        const auto b = static_cast<std::uint8_t>(body.at(i));
        out[i] = static_cast<char>((b + 0x11) & 0xFF);
    }
    return out;
}

QByteArray descrambleSetupBinBody(const QByteArray &body)
{
    QByteArray out;
    out.resize(body.size());
    for (int i = 0; i < body.size(); ++i) {
        const auto b = static_cast<std::uint8_t>(body.at(i));
        out[i] = static_cast<char>((b + 0xEF) & 0xFF);
    }
    return out;
}

const SetupBinVarSpec *findSetupBinVarSpec(int moduleId, int varId)
{
    for (const auto &spec : kVarSpecs) {
        if (spec.moduleId == moduleId && spec.varId == varId) {
            return &spec;
        }
    }
    return nullptr;
}

QList<const SetupBinVarSpec *> allSetupBinVarSpecs()
{
    QList<const SetupBinVarSpec *> out;
    out.reserve(static_cast<int>(std::size(kVarSpecs)));
    for (const auto &spec : kVarSpecs) {
        out.append(&spec);
    }
    return out;
}

SetupBinFile decodeSetupBin(const QByteArray &bytes, bool *ok)
{
    SetupBinFile file;
    if (ok) *ok = false;

    const int version = fileVersionForUuid(bytes);
    if (version == 0) return file;

    file.version = version;
    file.flags = readU16Le(bytes, 26);
    const auto declaredRecordCount = readU32Le(bytes, 28);
    file.dataRecordsConsumed = readU32Le(bytes, 32);

    // Walk fixed-size 512-byte records. The legacy decoder rejects any
    // file whose tail isn't an exact multiple of 512 by stopping early
    // — we follow that and validate the count below.
    int ptr = kHeaderSize;
    while (ptr + kRecordSize <= bytes.size()) {
        SetupBinRecord rec;
        // const auto typeId = readU32Le(bytes, ptr);
        rec.flags = readU32Le(bytes, ptr + 4);
        // chunkCount @ +8, headerByteCount @ +10, recordNumber @ +12 —
        // all derivable on encode, no semantic meaning beyond framing.

        QByteArray body = bytes.mid(ptr + kRecordHeaderSize, kRecordBodySize);
        if ((rec.flags & kRecordFlagScrambled) != 0) {
            body = descrambleSetupBinBody(body);
        }

        int pos = 0;
        while (pos + 8 <= body.size()) {
            const int moduleId = readU16Le(body, pos);
            const int varId = readU16Le(body, pos + 2);
            if (moduleId == 0 || varId == 0) break;

            const int length = readU16Le(body, pos + 4);
            // Reserved @ pos+6 (2 bytes), value starts at pos+8. Bound
            // `length` against the remaining body space *before* computing
            // the padded advance, so a malformed file with an oversize
            // length field can't drive `pos` past `body.size()` (the
            // pad-up-to-4 step adds up to 3 to `length`, which we account
            // for explicitly below).
            const int remaining = body.size() - pos - 8;
            if (length > remaining) break;

            // Skip unknown variables (matches legacy: silently drop tuples
            // that aren't in the catalog so newer-firmware setup.bins
            // don't make older parsers reject them entirely).
            if (findSetupBinVarSpec(moduleId, varId) != nullptr) {
                SetupBinVariable var;
                var.moduleId = moduleId;
                var.varId = varId;
                var.value = body.mid(pos + 8, length);
                rec.variables.append(var);
            }

            // Each entry is padded to a multiple of 4 bytes.
            pos += 8 + ((length + 3) / 4) * 4;
        }

        file.records.append(rec);
        ptr += kRecordSize;
    }

    if (static_cast<int>(declaredRecordCount) != file.records.size()) {
        return SetupBinFile{};
    }

    if (ok) *ok = true;
    return file;
}

namespace {

// Stable-sort variables in encode order: (moduleId, varId) ascending.
// This matches the legacy `AmtSetupBinVariableCompare` and happens to
// satisfy every ordering rule the AMT format imposes (see header).
QList<SetupBinVariable> sortedVariables(const QList<SetupBinVariable> &input)
{
    QList<SetupBinVariable> sorted = input;
    std::sort(sorted.begin(), sorted.end(),
              [](const SetupBinVariable &a, const SetupBinVariable &b) {
                  if (a.moduleId != b.moduleId) return a.moduleId < b.moduleId;
                  return a.varId < b.varId;
              });
    return sorted;
}

} // namespace

QByteArray encodeSetupBin(const SetupBinFile &file)
{
    if (file.version < 1 || file.version > static_cast<int>(kFileTypeGuids.size())) {
        return {};
    }

    // Collect the set of module IDs that actually appear, preserving
    // first-appearance order so the encoded list matches legacy output.
    QList<std::uint16_t> modulesInUse;
    for (const auto &rec : file.records) {
        for (const auto &var : rec.variables) {
            const auto mid = static_cast<std::uint16_t>(var.moduleId);
            if (!modulesInUse.contains(mid)) {
                modulesInUse.append(mid);
            }
        }
    }

    QByteArray out;
    out.reserve(kHeaderSize + file.records.size() * kRecordSize);

    // File header.
    const auto &uuid = kFileTypeGuids[file.version - 1];
    out.append(reinterpret_cast<const char *>(uuid.data()), 16);
    writeU16Le(out, 0); // recordChunkCount — header is a single chunk; legacy writes 0 here.
    // recordHeaderByteCount: 42 fixed-prefix bytes + 2 per module ID.
    // (Legacy: `42 + (modulesInUse.length * 2)`.)
    writeU16Le(out, static_cast<std::uint16_t>(42 + modulesInUse.size() * 2));
    writeU32Le(out, 0); // recordNumber — always 0 in the file header.
    out.append(static_cast<char>(file.version & 0xFF));
    out.append(static_cast<char>(0)); // minor version
    writeU16Le(out, file.flags);
    writeU32Le(out, static_cast<std::uint32_t>(file.records.size()));
    writeU32Le(out, file.dataRecordsConsumed);
    writeU16Le(out, 1); // dataRecordChunkCount — each data record is one 512-byte chunk.
    writeU16Le(out, 0); // reserved
    for (auto mid : modulesInUse) {
        writeU16Le(out, mid);
    }
    while (out.size() < kHeaderSize) {
        out.append('\0');
    }

    // Data records.
    std::uint32_t recordNumber = 0;
    for (const auto &rec : file.records) {
        QByteArray record;
        record.reserve(kRecordSize);

        // Record header (24 bytes).
        writeU32Le(record, 1); // typeIdentifier — 1 = data record.
        writeU32Le(record, rec.flags);
        writeU32Le(record, 0); // reserved
        writeU32Le(record, 0); // reserved
        writeU16Le(record, 1); // chunkCount
        writeU16Le(record, kRecordHeaderSize);
        writeU32Le(record, ++recordNumber);

        // Body: sorted TLV entries, terminated implicitly by zero-padding
        // to 512 bytes (the parser stops at the first zero moduleId/varid).
        QByteArray body;
        for (const auto &var : sortedVariables(rec.variables)) {
            QByteArray entry;
            writeU16Le(entry, static_cast<std::uint16_t>(var.moduleId));
            writeU16Le(entry, static_cast<std::uint16_t>(var.varId));
            writeU16Le(entry, static_cast<std::uint16_t>(var.value.size()));
            writeU16Le(entry, 0); // reserved
            entry.append(var.value);
            while (entry.size() % 4 != 0) {
                entry.append('\0');
            }
            body.append(entry);
        }
        while (body.size() < kRecordBodySize) {
            body.append('\0');
        }
        body.truncate(kRecordBodySize);

        if ((rec.flags & kRecordFlagScrambled) != 0) {
            body = scrambleSetupBinBody(body);
        }

        record.append(body);
        out.append(record);
    }

    return out;
}

QString validateSetupBin(const SetupBinFile &file)
{
    if (file.version < 1 || file.version > static_cast<int>(kFileTypeGuids.size())) {
        return QStringLiteral("File version must be 1..4 (got %1).").arg(file.version);
    }

    // Track presence across all records so the cert-add/cert-config check
    // works when the operator splits them across two records (BIOS reads
    // them in order anyway, and `2/7` always sorts before `2/8` so a
    // valid `2/8` always has its `2/7` precondition visible by the time
    // BIOS hits it).
    bool sawUserDefinedCertConfig = false;
    int userDefinedCertConfigValue = -1;

    for (int recIdx = 0; recIdx < file.records.size(); ++recIdx) {
        const auto &rec = file.records.at(recIdx);

        // Pack (moduleId, varId) into a 32-bit key. Both IDs are
        // u16-sized in the wire format, so this is lossless and lets us
        // avoid pulling QPair / std::pair into the hash key type.
        QHash<std::uint32_t, int> seen;
        const SetupBinVariable *currentMebxPassword = nullptr;
        const SetupBinVariable *newMebxPassword = nullptr;
        const SetupBinVariable *userDefinedCertAdd = nullptr;

        for (const auto &var : rec.variables) {
            const auto key = static_cast<std::uint32_t>(
                (var.moduleId << 16) | (var.varId & 0xFFFF));
            if (seen.contains(key)) {
                return QStringLiteral(
                    "Record %1 has duplicate variable %2/%3.")
                    .arg(recIdx + 1).arg(var.moduleId).arg(var.varId);
            }
            seen.insert(key, 1);

            const auto *spec = findSetupBinVarSpec(var.moduleId, var.varId);
            if (spec == nullptr) {
                return QStringLiteral(
                    "Record %1 contains unknown variable %2/%3.")
                    .arg(recIdx + 1).arg(var.moduleId).arg(var.varId);
            }

            // Type width: numeric values must fit their declared encoding.
            switch (spec->type) {
            case SetupBinVarType::U8:
                if (var.value.size() != 1) {
                    return QStringLiteral("Variable \"%1\" must be exactly 1 byte.")
                        .arg(QString::fromLatin1(spec->name));
                }
                break;
            case SetupBinVarType::U16:
                if (var.value.size() != 2) {
                    return QStringLiteral("Variable \"%1\" must be exactly 2 bytes.")
                        .arg(QString::fromLatin1(spec->name));
                }
                break;
            case SetupBinVarType::U32:
                if (var.value.size() != 4) {
                    return QStringLiteral("Variable \"%1\" must be exactly 4 bytes.")
                        .arg(QString::fromLatin1(spec->name));
                }
                break;
            case SetupBinVarType::Guid:
                if (var.value.size() != 16) {
                    return QStringLiteral("Variable \"%1\" must be exactly 16 bytes (GUID).")
                        .arg(QString::fromLatin1(spec->name));
                }
                break;
            case SetupBinVarType::Binary:
                break;
            }

            // Field-specific length caps (from the legacy file's comments,
            // which are the only documentation we have for these limits).
            if (spec->type == SetupBinVarType::Binary) {
                const int len = var.value.size();
                auto cap = [&](int max, const char *what) -> QString {
                    if (len > max) {
                        return QStringLiteral("Variable \"%1\" exceeds %2-byte limit (got %3).")
                            .arg(QString::fromLatin1(spec->name)).arg(max).arg(len);
                    }
                    Q_UNUSED(what);
                    return QString{};
                };
                if (var.moduleId == 2 && var.varId == 11) {
                    auto err = cap(63, "hostname");
                    if (!err.isEmpty()) return err;
                }
                if (var.moduleId == 2 && (var.varId == 3 || var.varId == 4 || var.varId == 12)) {
                    auto err = cap(255, "fqdn");
                    if (!err.isEmpty()) return err;
                }
                if (var.moduleId == 2 && var.varId == 31) {
                    auto err = cap(60, "support-channel-desc");
                    if (!err.isEmpty()) return err;
                }
                if (var.moduleId == 2 && (var.varId == 32 || var.varId == 33)) {
                    auto err = cap(32, "service-account-or-passcode");
                    if (!err.isEmpty()) return err;
                }
            }

            // Capture references for the inter-variable checks below.
            if (var.moduleId == 1 && var.varId == 1) currentMebxPassword = &var;
            if (var.moduleId == 1 && var.varId == 2) newMebxPassword = &var;
            if (var.moduleId == 2 && var.varId == 7) {
                sawUserDefinedCertConfig = true;
                if (var.value.size() == 1) {
                    userDefinedCertConfigValue = static_cast<std::uint8_t>(var.value.at(0));
                }
            }
            if (var.moduleId == 2 && var.varId == 8) userDefinedCertAdd = &var;
        }

        // v2 specifically: if "Current MEBx Password" is the factory
        // default "admin", a "New MEBx Password" must follow in the same
        // record (BIOS rejects the "admin" sentinel without the rotation).
        if (file.version == 2 && currentMebxPassword != nullptr) {
            const QByteArray current(currentMebxPassword->value);
            if (current == "admin" && newMebxPassword == nullptr) {
                return QStringLiteral(
                    "v2 setup.bin: \"Current MEBx Password\" = \"admin\" "
                    "requires \"New MEBx Password\" in the same record.");
            }
        }

        // Within-record presence guard for cert-add: 2/8 requires 2/7,
        // and 2/7 must not be set to "Delete" (value 2).
        if (userDefinedCertAdd != nullptr) {
            if (!sawUserDefinedCertConfig) {
                return QStringLiteral(
                    "\"User Defined Certificate Addition\" (2/8) requires "
                    "\"User Defined Certificate Configuration\" (2/7) to be set first.");
            }
            if (userDefinedCertConfigValue == 2) {
                return QStringLiteral(
                    "\"User Defined Certificate Addition\" (2/8) is incompatible "
                    "with \"User Defined Certificate Configuration\" = Delete.");
            }
        }
    }

    return {};
}

} // namespace qumesh::wsman
