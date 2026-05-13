// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ider/scsi.h"

namespace qumesh::ider::scsi {

namespace {

QByteArray fromBytes(const unsigned char *bytes, int n)
{
    return QByteArray(reinterpret_cast<const char *>(bytes), n);
}

} // namespace

const QByteArray &modeSenseCd1A()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x12, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x1A, 0x0A, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &modeSenseCd1D()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x12, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x0A, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &modeSenseCd2A()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x20, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x18, 0x00, 0x00,
        0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &modeSenseCd3F()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x28, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x00, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x2A, 0x18, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &modeSenseCdErrorRecovery()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x0E, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x00, 0xFF,
        0x00, 0x00, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configProfileList()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x00, 0x03, 0x04, 0x00, 0x08, 0x01, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configCore()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x01, 0x03, 0x04, 0x00, 0x00, 0x00, 0x02,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configMorphing()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configRemovable()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x03, 0x03, 0x04, 0x29, 0x00, 0x00, 0x02,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configRandom()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x10, 0x01, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configRead()
{
    static constexpr unsigned char kData[] = {
        0x00, 0x1E, 0x03, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configPowerManagement()
{
    static constexpr unsigned char kData[] = {
        0x01, 0x00, 0x03, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

const QByteArray &configTimeout()
{
    static constexpr unsigned char kData[] = {
        0x01, 0x05, 0x03, 0x00,
    };
    static const QByteArray kArray = fromBytes(kData, sizeof(kData));
    return kArray;
}

} // namespace qumesh::ider::scsi
