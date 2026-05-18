// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "rmcp/target_range.h"

#include <QStringList>

namespace qumesh::rmcp {

namespace {

bool parseIPv4(const QString &s, quint32 *out)
{
    QHostAddress addr;
    if (!addr.setAddress(s.trimmed()))
        return false;
    bool ok = false;
    const quint32 v = addr.toIPv4Address(&ok);
    if (!ok)
        return false;
    *out = v;
    return true;
}

void setError(QString *errorOut, const QString &msg)
{
    if (errorOut)
        *errorOut = msg;
}

} // namespace

QList<QHostAddress> parseTargets(const QString &spec, QString *errorOut)
{
    const QString s = spec.trimmed();
    if (s.isEmpty()) {
        setError(errorOut, QStringLiteral("Target is empty."));
        return {};
    }

    // CIDR
    if (s.contains(QLatin1Char('/'))) {
        const auto parts = s.split(QLatin1Char('/'));
        if (parts.size() != 2) {
            setError(errorOut, QStringLiteral("CIDR must be address/mask."));
            return {};
        }
        quint32 base = 0;
        if (!parseIPv4(parts[0], &base)) {
            setError(errorOut, QStringLiteral("CIDR base address is not a valid IPv4."));
            return {};
        }
        bool ok = false;
        const int mask = parts[1].toInt(&ok);
        if (!ok || mask < 0 || mask > 32) {
            setError(errorOut, QStringLiteral("CIDR mask must be 0–32."));
            return {};
        }
        if (mask < 16) {
            setError(errorOut, QStringLiteral(
                "CIDR mask /%1 is too broad — use /16 or narrower.").arg(mask));
            return {};
        }
        const quint32 hostBits = 32 - mask;
        const quint32 netmask = (mask == 0) ? 0u : (~quint32(0) << hostBits);
        const quint32 network = base & netmask;
        const quint64 count = (quint64(1) << hostBits);
        QList<QHostAddress> out;
        out.reserve(static_cast<int>(count));
        for (quint64 i = 0; i < count; ++i)
            out.append(QHostAddress(static_cast<quint32>(network + i)));
        return out;
    }

    // Range
    if (s.contains(QLatin1Char('-'))) {
        const auto parts = s.split(QLatin1Char('-'));
        if (parts.size() != 2) {
            setError(errorOut, QStringLiteral("Range must be start-end."));
            return {};
        }
        quint32 lo = 0, hi = 0;
        if (!parseIPv4(parts[0], &lo) || !parseIPv4(parts[1], &hi)) {
            setError(errorOut, QStringLiteral("Range endpoints must be IPv4."));
            return {};
        }
        if (hi < lo) {
            setError(errorOut, QStringLiteral("Range end is before start."));
            return {};
        }
        const quint64 count = quint64(hi) - quint64(lo) + 1;
        // Cap range size at the /16-equivalent ceiling so a typo like
        // `10.0.0.0-10.1.0.0` doesn't expand to 65 537+ probes.
        if (count > 65536) {
            setError(errorOut, QStringLiteral(
                "Range spans %1 addresses — limit is 65536.").arg(count));
            return {};
        }
        QList<QHostAddress> out;
        out.reserve(static_cast<int>(count));
        for (quint64 v = lo; v <= hi; ++v)
            out.append(QHostAddress(static_cast<quint32>(v)));
        return out;
    }

    // Single IP
    quint32 only = 0;
    if (!parseIPv4(s, &only)) {
        setError(errorOut, QStringLiteral("Not a valid IPv4 address, range, or CIDR."));
        return {};
    }
    return { QHostAddress(only) };
}

} // namespace qumesh::rmcp
