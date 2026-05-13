#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// Forward-declare leveldb to keep this header free of leveldb includes.
namespace leveldb {
class DB;
}

namespace meshcommander::migrate {

/// A single decoded entry from Chromium's DOMStorage leveldb. `value` has
/// already had its 1-byte Chromium type prefix stripped, and any UTF-16LE
/// payload re-encoded as UTF-8.
struct ChromiumEntry
{
    QString key;
    QByteArray value;
};

/// Read-only reader for the leveldb directory that Chromium uses to back
/// `localStorage` (`Default/Local Storage/leveldb/`).
///
/// The reader copies the source directory to a temporary location before
/// opening so that a running NW.js / Chromium instance holding the original
/// `LOCK` file does not block the migration. The copy is deleted in the
/// destructor.
class ChromiumStorageReader
{
public:
    explicit ChromiumStorageReader(QString leveldbPath);
    ChromiumStorageReader(const ChromiumStorageReader &) = delete;
    ChromiumStorageReader &operator=(const ChromiumStorageReader &) = delete;
    ~ChromiumStorageReader();

    [[nodiscard]] bool open();
    void close();
    [[nodiscard]] bool isOpen() const { return m_db != nullptr; }
    [[nodiscard]] QString lastError() const { return m_error; }

    /// Return every localStorage entry stored under `origin`. The origin
    /// must include the scheme — e.g.
    /// `"chrome-extension://gkmjdlhkgdlflppihloafibfdkhfnfco"`.
    [[nodiscard]] QList<ChromiumEntry> readOrigin(const QString &origin);

    /// Decode a raw localStorage value byte sequence as Chromium would have
    /// produced it: the first byte is a type tag (`\x00`=UTF-16LE,
    /// `\x01`=Latin1). Returns an empty QByteArray on unrecognised tags.
    /// Public for unit testing.
    [[nodiscard]] static QByteArray decodeValue(QByteArray rawValueWithTag);

private:
    QString m_sourcePath;
    QString m_workPath;
    QString m_error;
    leveldb::DB *m_db = nullptr;
};

} // namespace meshcommander::migrate
