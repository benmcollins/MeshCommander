// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>

namespace qumesh::rmcp {

/// Expand a user-typed target spec into a concrete list of IPv4
/// addresses to scan. Accepts:
///   - single IP                 e.g. `10.0.0.5`
///   - inclusive range           e.g. `10.0.0.5-10.0.0.99`
///   - CIDR                      e.g. `10.0.0.0/24`
/// CIDR masks below /16 are rejected to bound the scan size (matches the
/// legacy MeshCommander scanner's safety check — at /16 the list is
/// already 65 536 hosts).
///
/// On parse failure returns an empty list and, when `errorOut` is
/// non-null, fills it with a human-readable explanation.
[[nodiscard]] QList<QHostAddress> parseTargets(const QString &spec,
                                               QString *errorOut = nullptr);

} // namespace qumesh::rmcp
