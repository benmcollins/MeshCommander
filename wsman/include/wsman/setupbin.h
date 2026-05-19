// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <cstdint>

namespace qumesh::wsman {

/// Encoder / decoder for the Intel AMT `setup.bin` USB-key provisioning
/// file. This is the file BIOS reads off a USB stick during initial
/// AMT provisioning; MEBx applies the records and the machine comes up
/// with admin password, certificates, network config, etc. The format
/// is fully out-of-band — there's no AMT connection involved — so the
/// code below has no Qt networking dependency.
///
/// Four file versions exist (v1..v4), identified by a 16-byte UUID at
/// offset 0. Bodies share the same record structure. The legacy NW.js
/// MeshCommander shipped this parser as `amt-setupbin-0.1.0.js`; this
/// is a port of that, matched byte-for-byte so a round-trip through
/// `decode → encode` preserves a known-good file.

/// Variable value encoding. Indexed by the variable-ID table per module.
enum class SetupBinVarType : std::uint8_t
{
    /// Raw bytes — strings, certs, network blobs.
    Binary = 0,
    /// Single unsigned byte. Most enum-valued flags use this.
    U8 = 1,
    /// 16-bit unsigned LE.
    U16 = 2,
    /// 32-bit unsigned LE.
    U32 = 3,
    /// 16-byte GUID, stored raw (little-endian first three groups per
    /// Microsoft GUID encoding, matching how AMT emits them on the wire).
    Guid = 4,
};

/// Catalog entry for a single (moduleId, varId) tuple.
struct SetupBinVarSpec
{
    int moduleId;
    int varId;
    SetupBinVarType type;
    const char *name;

    /// Enum value labels — `enumValues[k]` is the human-readable label
    /// for u8/u16/u32 value `k`; absent (`nullptr`) when the variable is
    /// free-form. Sorted by value, ends with `{-1, nullptr}`.
    const struct SetupBinEnumLabel *enumValues;
};

struct SetupBinEnumLabel
{
    int value;
    const char *label;
};

/// One variable inside a data record.
struct SetupBinVariable
{
    int moduleId = 0;
    int varId = 0;
    /// Raw value bytes. For U8 / U16 / U32 the bytes are the LE encoding
    /// of the integer; for GUID, the 16 raw bytes; for Binary, opaque.
    /// Always exactly the on-wire length.
    QByteArray value;
};

/// One data record in a setup.bin. A record holds an ordered (well,
/// sorted-at-encode) list of variables. Each record is exactly 512
/// bytes on disk: a 24-byte header plus up to 488 bytes of TLV body.
struct SetupBinRecord
{
    /// Bit 0 = valid (always set for records we write), bit 1 =
    /// scrambled body. The remaining bits are reserved.
    std::uint32_t flags = 1;
    QList<SetupBinVariable> variables;
};

/// Decoded contents of a setup.bin file.
struct SetupBinFile
{
    /// 1..4. Determines the UUID prefix in the header.
    int version = 2;
    /// File-header flags. Bit 0 = "do not consume records" — when set,
    /// BIOS applies records without advancing the consumed counter, so
    /// the same stick can be reused on multiple machines.
    std::uint16_t flags = 0;
    /// Persisted verbatim; only BIOS bumps this. Defaults to 0 for files
    /// we create.
    std::uint32_t dataRecordsConsumed = 0;
    QList<SetupBinRecord> records;
};

/// Look up the spec for (moduleId, varId). Returns `nullptr` if the
/// tuple isn't in the known catalog; the decoder uses this to skip
/// unknown variables (matching legacy behavior).
[[nodiscard]] const SetupBinVarSpec *findSetupBinVarSpec(int moduleId, int varId);

/// Walk the full catalog. Useful for the editor to populate dropdowns.
[[nodiscard]] QList<const SetupBinVarSpec *> allSetupBinVarSpecs();

/// Decode `bytes` into a `SetupBinFile`. `ok` is set to `false` on:
///   - unknown / invalid UUID,
///   - declared record count != records actually present in the file,
///   - file shorter than the 512-byte header.
[[nodiscard]] SetupBinFile decodeSetupBin(const QByteArray &bytes, bool *ok = nullptr);

/// Encode a `SetupBinFile` to its on-disk representation. Variables
/// inside each record are sorted by `(moduleId, varId)` before encoding
/// (this matches the legacy behavior and happens to satisfy the
/// "Manageability Feature Selection cannot follow CM settings",
/// "2/7 precedes 2/8", "2/28 follows 2/3, 2/4, 2/17" ordering rules
/// the AMT format imposes — they all reduce to the sort).
[[nodiscard]] QByteArray encodeSetupBin(const SetupBinFile &file);

/// Validate a `SetupBinFile` against the content rules the AMT format
/// imposes (presence / length / type-width / duplicate guards). Returns
/// an empty string when valid; otherwise the first failing rule as a
/// human-readable message suitable for surfacing in the editor.
[[nodiscard]] QString validateSetupBin(const SetupBinFile &file);

/// Apply or reverse the "scramble" transform used on record bodies
/// when `SetupBinRecord::flags & 2` is set. The forward transform adds
/// 0x11 modulo 256 to every byte; the reverse adds 0xEF (i.e. -0x11).
/// Exposed for unit testing — `encodeSetupBin` applies it internally.
[[nodiscard]] QByteArray scrambleSetupBinBody(const QByteArray &body);
[[nodiscard]] QByteArray descrambleSetupBinBody(const QByteArray &body);

} // namespace qumesh::wsman
