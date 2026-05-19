// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "setupbincontroller.h"

#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <algorithm>

namespace qumesh::app {

using qumesh::wsman::SetupBinFile;
using qumesh::wsman::SetupBinRecord;
using qumesh::wsman::SetupBinVariable;
using qumesh::wsman::SetupBinVarSpec;
using qumesh::wsman::SetupBinVarType;

namespace {

bool indexInRange(int i, int size) { return i >= 0 && i < size; }

QByteArray u8Bytes(unsigned v) { return QByteArray(1, static_cast<char>(v & 0xFF)); }

QByteArray u16Bytes(unsigned v)
{
    QByteArray b(2, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    return b;
}

QByteArray u32Bytes(unsigned v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    return b;
}

QByteArray emptyDefault(SetupBinVarType type)
{
    switch (type) {
    case SetupBinVarType::U8:     return QByteArray(1, '\0');
    case SetupBinVarType::U16:    return QByteArray(2, '\0');
    case SetupBinVarType::U32:    return QByteArray(4, '\0');
    case SetupBinVarType::Guid:   return QByteArray(16, '\0');
    case SetupBinVarType::Binary: return {};
    }
    return {};
}

} // namespace

SetupBinController::SetupBinController(QObject *parent)
    : QObject(parent)
{
    newFile(2);
}

void SetupBinController::setVersion(int v)
{
    if (v < 1 || v > 4 || v == m_file.version) return;
    m_file.version = v;
    markDirty();
    emit fileChanged();
}

void SetupBinController::setDoNotConsumeRecords(bool on)
{
    const std::uint16_t newFlags = on
        ? static_cast<std::uint16_t>(m_file.flags | 0x1)
        : static_cast<std::uint16_t>(m_file.flags & ~0x1);
    if (newFlags == m_file.flags) return;
    m_file.flags = newFlags;
    markDirty();
    emit fileChanged();
}

void SetupBinController::newFile(int version)
{
    if (version < 1 || version > 4) version = 2;
    m_file = SetupBinFile{};
    m_file.version = version;
    m_currentPath.clear();
    m_lastError.clear();
    m_dirty = false;
    emit fileChanged();
    emit currentPathChanged();
    emit lastErrorChanged();
    emit dirtyChanged();
}

bool SetupBinController::openFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setLastError(tr("Couldn't open \"%1\": %2").arg(path, f.errorString()));
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    bool ok = false;
    auto decoded = qumesh::wsman::decodeSetupBin(bytes, &ok);
    if (!ok) {
        setLastError(tr("\"%1\" is not a valid setup.bin file.").arg(path));
        return false;
    }

    m_file = std::move(decoded);
    setCurrentPath(path);
    m_dirty = false;
    setLastError({});
    emit fileChanged();
    emit dirtyChanged();
    return true;
}

QString SetupBinController::validate() const
{
    return qumesh::wsman::validateSetupBin(m_file);
}

bool SetupBinController::saveToFile(const QString &path)
{
    const QString err = validate();
    if (!err.isEmpty()) {
        setLastError(err);
        return false;
    }

    const QByteArray bytes = qumesh::wsman::encodeSetupBin(m_file);
    if (bytes.isEmpty()) {
        setLastError(tr("Couldn't encode the file (invalid version?)."));
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(tr("Couldn't write to \"%1\": %2").arg(path, f.errorString()));
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        setLastError(tr("Short write to \"%1\": %2").arg(path, f.errorString()));
        return false;
    }
    f.close();

    setCurrentPath(path);
    m_dirty = false;
    setLastError({});
    emit dirtyChanged();
    return true;
}

int SetupBinController::addRecord()
{
    SetupBinRecord rec;
    rec.flags = 0x1; // valid
    m_file.records.append(rec);
    markDirty();
    emit fileChanged();
    return m_file.records.size() - 1;
}

bool SetupBinController::removeRecord(int recordIndex)
{
    if (!indexInRange(recordIndex, m_file.records.size())) return false;
    m_file.records.removeAt(recordIndex);
    markDirty();
    emit fileChanged();
    return true;
}

bool SetupBinController::setRecordScrambled(int recordIndex, bool scrambled)
{
    if (!indexInRange(recordIndex, m_file.records.size())) return false;
    auto &rec = m_file.records[recordIndex];
    const std::uint32_t newFlags = scrambled
        ? (rec.flags | 0x2)
        : (rec.flags & ~0x2u);
    if (newFlags == rec.flags) return true;
    rec.flags = newFlags;
    markDirty();
    emit fileChanged();
    return true;
}

int SetupBinController::addVariable(int recordIndex, int moduleId, int varId)
{
    if (!indexInRange(recordIndex, m_file.records.size())) return -1;
    const auto *spec = qumesh::wsman::findSetupBinVarSpec(moduleId, varId);
    if (spec == nullptr) return -1;

    SetupBinVariable var;
    var.moduleId = moduleId;
    var.varId = varId;
    var.value = emptyDefault(spec->type);
    m_file.records[recordIndex].variables.append(var);
    markDirty();
    emit fileChanged();
    return m_file.records[recordIndex].variables.size() - 1;
}

bool SetupBinController::removeVariable(int recordIndex, int varIndex)
{
    if (!indexInRange(recordIndex, m_file.records.size())) return false;
    auto &rec = m_file.records[recordIndex];
    if (!indexInRange(varIndex, rec.variables.size())) return false;
    rec.variables.removeAt(varIndex);
    markDirty();
    emit fileChanged();
    return true;
}

bool SetupBinController::setVariableValue(int recordIndex, int varIndex,
                                           const QVariant &value)
{
    if (!indexInRange(recordIndex, m_file.records.size())) return false;
    auto &rec = m_file.records[recordIndex];
    if (!indexInRange(varIndex, rec.variables.size())) return false;
    auto &var = rec.variables[varIndex];

    const auto *spec = qumesh::wsman::findSetupBinVarSpec(var.moduleId, var.varId);
    if (spec == nullptr) {
        setLastError(tr("Variable %1/%2 isn't in the catalog.")
                         .arg(var.moduleId).arg(var.varId));
        return false;
    }

    switch (spec->type) {
    case SetupBinVarType::Binary: {
        if (value.canConvert<QByteArray>() && value.metaType().id() == QMetaType::QByteArray) {
            var.value = value.toByteArray();
        } else {
            var.value = value.toString().toUtf8();
        }
        break;
    }
    case SetupBinVarType::U8: {
        bool ok = false;
        const auto n = value.toUInt(&ok);
        if (!ok || n > 0xFF) {
            setLastError(tr("Value must be 0..255."));
            return false;
        }
        var.value = u8Bytes(n);
        break;
    }
    case SetupBinVarType::U16: {
        bool ok = false;
        const auto n = value.toUInt(&ok);
        if (!ok || n > 0xFFFF) {
            setLastError(tr("Value must be 0..65535."));
            return false;
        }
        var.value = u16Bytes(n);
        break;
    }
    case SetupBinVarType::U32: {
        bool ok = false;
        const auto n = value.toULongLong(&ok);
        if (!ok || n > 0xFFFFFFFFULL) {
            setLastError(tr("Value must be 0..4294967295."));
            return false;
        }
        var.value = u32Bytes(static_cast<unsigned>(n));
        break;
    }
    case SetupBinVarType::Guid: {
        QByteArray bytes;
        if (value.metaType().id() == QMetaType::QByteArray) {
            bytes = value.toByteArray();
        } else {
            const QUuid uuid(value.toString());
            if (uuid.isNull()) {
                setLastError(tr("GUID must be a valid UUID string or 16 raw bytes."));
                return false;
            }
            bytes = uuid.toRfc4122();
        }
        if (bytes.size() != 16) {
            setLastError(tr("GUID must be exactly 16 bytes."));
            return false;
        }
        var.value = bytes;
        break;
    }
    }

    setLastError({});
    markDirty();
    emit fileChanged();
    return true;
}

bool SetupBinController::setVariableValueFromFile(int recordIndex, int varIndex,
                                                  const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setLastError(tr("Couldn't read \"%1\": %2").arg(path, f.errorString()));
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();
    return setVariableValue(recordIndex, varIndex, QVariant(bytes));
}

QVariantMap SetupBinController::fileSnapshot() const
{
    QVariantMap out;
    out["version"] = m_file.version;
    out["doNotConsumeRecords"] = doNotConsumeRecords();

    QVariantList records;
    records.reserve(m_file.records.size());
    for (const auto &rec : m_file.records) {
        QVariantMap r;
        r["scrambled"] = (rec.flags & 0x2u) != 0;
        QVariantList vars;
        vars.reserve(rec.variables.size());
        for (const auto &var : rec.variables) {
            vars.append(variableSnapshot(var));
        }
        r["variables"] = vars;
        records.append(r);
    }
    out["records"] = records;
    return out;
}

QVariantList SetupBinController::catalog() const
{
    QVariantList out;
    for (const auto *spec : qumesh::wsman::allSetupBinVarSpecs()) {
        QVariantMap entry;
        entry["moduleId"] = spec->moduleId;
        entry["varId"] = spec->varId;
        entry["name"] = QString::fromLatin1(spec->name);
        entry["typeCode"] = static_cast<int>(spec->type);
        if (spec->enumValues != nullptr) {
            QVariantList values;
            for (const auto *e = spec->enumValues; e->label != nullptr; ++e) {
                QVariantMap v;
                v["value"] = e->value;
                v["label"] = QString::fromLatin1(e->label);
                values.append(v);
            }
            entry["enumValues"] = values;
        }
        out.append(entry);
    }
    return out;
}

QVariantMap SetupBinController::variableSnapshot(const SetupBinVariable &var) const
{
    QVariantMap m;
    m["moduleId"] = var.moduleId;
    m["varId"] = var.varId;
    m["rawValue"] = var.value;

    const auto *spec = qumesh::wsman::findSetupBinVarSpec(var.moduleId, var.varId);
    if (spec != nullptr) {
        m["name"] = QString::fromLatin1(spec->name);
        m["typeCode"] = static_cast<int>(spec->type);
    } else {
        m["name"] = QString("Unknown %1/%2").arg(var.moduleId).arg(var.varId);
        m["typeCode"] = static_cast<int>(SetupBinVarType::Binary);
    }
    m["valueDisplay"] = decodeForDisplay(var);
    return m;
}

QString SetupBinController::decodeForDisplay(const SetupBinVariable &var) const
{
    const auto *spec = qumesh::wsman::findSetupBinVarSpec(var.moduleId, var.varId);
    if (spec == nullptr) {
        return QString::fromLatin1(var.value.toHex());
    }

    switch (spec->type) {
    case SetupBinVarType::Binary:
        // Render printable ASCII as-is, otherwise hex. Hostnames /
        // passwords / FQDNs are almost always ASCII; cert blobs are
        // not.
        for (char c : var.value) {
            const auto b = static_cast<std::uint8_t>(c);
            if (b < 0x20 || b >= 0x7F) {
                return QStringLiteral("0x") + QString::fromLatin1(var.value.toHex());
            }
        }
        return QString::fromUtf8(var.value);

    case SetupBinVarType::U8: {
        if (var.value.size() != 1) return QStringLiteral("(bad length)");
        const auto n = static_cast<std::uint8_t>(var.value.at(0));
        for (const auto *e = spec->enumValues; e != nullptr && e->label != nullptr; ++e) {
            if (e->value == n) return QString::fromLatin1(e->label);
        }
        return QString::number(n);
    }
    case SetupBinVarType::U16: {
        if (var.value.size() != 2) return QStringLiteral("(bad length)");
        const auto lo = static_cast<std::uint8_t>(var.value.at(0));
        const auto hi = static_cast<std::uint8_t>(var.value.at(1));
        return QString::number(lo | (hi << 8));
    }
    case SetupBinVarType::U32: {
        if (var.value.size() != 4) return QStringLiteral("(bad length)");
        std::uint32_t n = 0;
        for (int i = 0; i < 4; ++i) {
            n |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(var.value.at(i))) << (i * 8);
        }
        return QString::number(n);
    }
    case SetupBinVarType::Guid:
        if (var.value.size() != 16) return QStringLiteral("(bad length)");
        return QUuid::fromRfc4122(var.value).toString(QUuid::WithoutBraces);
    }
    return {};
}

void SetupBinController::markDirty()
{
    if (m_dirty) return;
    m_dirty = true;
    emit dirtyChanged();
}

void SetupBinController::setLastError(const QString &err)
{
    if (m_lastError == err) return;
    m_lastError = err;
    emit lastErrorChanged();
}

void SetupBinController::setCurrentPath(const QString &path)
{
    if (m_currentPath == path) return;
    m_currentPath = path;
    emit currentPathChanged();
}

} // namespace qumesh::app
