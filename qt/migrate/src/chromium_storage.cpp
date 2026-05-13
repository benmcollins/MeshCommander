// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "migrate/chromium_storage.h"

#include <leveldb/db.h>
#include <leveldb/iterator.h>
#include <leveldb/options.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

namespace qumesh::migrate {

namespace {

constexpr char kKeyPrefixByte = '_';
constexpr char kKeySeparator[] = {0x00, 0x01};

QString copyDirectoryRecursive(const QString &source, const QString &dest)
{
    QDir srcDir(source);
    if (!srcDir.exists()) return QStringLiteral("source directory does not exist: %1").arg(source);

    QDir destDir(dest);
    if (!destDir.exists() && !destDir.mkpath(QStringLiteral("."))) {
        return QStringLiteral("could not create destination directory: %1").arg(dest);
    }

    const auto entries = srcDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &fi : entries) {
        const QString from = fi.absoluteFilePath();
        const QString to = destDir.absoluteFilePath(fi.fileName());
        if (!QFile::copy(from, to)) {
            return QStringLiteral("could not copy %1 -> %2").arg(from, to);
        }
    }
    return QString();
}

} // namespace

ChromiumStorageReader::ChromiumStorageReader(QString leveldbPath)
    : m_sourcePath(std::move(leveldbPath))
{
}

ChromiumStorageReader::~ChromiumStorageReader()
{
    close();
}

bool ChromiumStorageReader::open()
{
    if (m_db != nullptr) return true;

    const QString tempBase = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .absoluteFilePath(QStringLiteral("qumesh-migrate-") +
                                                   QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_workPath = tempBase;

    if (const QString err = copyDirectoryRecursive(m_sourcePath, m_workPath); !err.isEmpty()) {
        m_error = err;
        return false;
    }

    // Skip the LOCK file the source may have copied over — leveldb will
    // create its own when opening.
    QFile::remove(QDir(m_workPath).absoluteFilePath(QStringLiteral("LOCK")));

    leveldb::Options opts;
    opts.create_if_missing = false;
    opts.paranoid_checks = true;

    leveldb::DB *db = nullptr;
    const leveldb::Status status =
        leveldb::DB::Open(opts, m_workPath.toStdString(), &db);
    if (!status.ok()) {
        m_error = QString::fromStdString(status.ToString());
        delete db;
        return false;
    }
    m_db = db;
    return true;
}

void ChromiumStorageReader::close()
{
    delete m_db;
    m_db = nullptr;

    if (!m_workPath.isEmpty()) {
        QDir(m_workPath).removeRecursively();
        m_workPath.clear();
    }
}

QList<ChromiumEntry> ChromiumStorageReader::readOrigin(const QString &origin)
{
    QList<ChromiumEntry> out;
    if (m_db == nullptr) {
        m_error = QStringLiteral("database is not open");
        return out;
    }

    // Build the binary key prefix: '_' + origin + 0x00 + 0x01
    QByteArray prefix;
    prefix.append(kKeyPrefixByte);
    prefix.append(origin.toUtf8());
    prefix.append(kKeySeparator[0]);
    prefix.append(kKeySeparator[1]);

    leveldb::ReadOptions ropts;
    std::unique_ptr<leveldb::Iterator> it(m_db->NewIterator(ropts));
    for (it->Seek(leveldb::Slice(prefix.constData(), prefix.size())); it->Valid(); it->Next()) {
        const leveldb::Slice keySlice = it->key();
        if (keySlice.size() < static_cast<size_t>(prefix.size())
            || std::memcmp(keySlice.data(), prefix.constData(), prefix.size()) != 0) {
            break;
        }
        const leveldb::Slice valSlice = it->value();

        ChromiumEntry entry;
        entry.key = QString::fromUtf8(keySlice.data() + prefix.size(),
                                      static_cast<qsizetype>(keySlice.size() - prefix.size()));
        entry.value = decodeValue(QByteArray(valSlice.data(),
                                             static_cast<qsizetype>(valSlice.size())));
        out.push_back(std::move(entry));
    }

    if (!it->status().ok()) {
        m_error = QString::fromStdString(it->status().ToString());
    }
    return out;
}

QByteArray ChromiumStorageReader::decodeValue(QByteArray rawValueWithTag)
{
    if (rawValueWithTag.isEmpty()) return {};
    const char tag = rawValueWithTag.at(0);
    QByteArray body = rawValueWithTag.mid(1);
    switch (tag) {
    case 0x00: {
        // UTF-16LE payload. Re-encode as UTF-8.
        if (body.size() % 2 != 0) return {};
        const auto *u16 = reinterpret_cast<const char16_t *>(body.constData());
        const QString s = QString::fromUtf16(u16, body.size() / 2);
        return s.toUtf8();
    }
    case 0x01:
        // Latin1 / 1-byte-per-codepoint payload. Bytes are already a valid
        // 7-bit ASCII representation in MeshCommander's case (hex ciphertext
        // or short ASCII identifiers); pass through unchanged.
        return body;
    default:
        return {};
    }
}

} // namespace qumesh::migrate
