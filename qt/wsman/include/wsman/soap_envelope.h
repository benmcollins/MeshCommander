// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace qumesh::wsman {

/// Pure-function helpers for building and parsing the SOAP/WS-Management
/// envelopes that Intel AMT speaks. Kept transport-agnostic so the same
/// helpers can be unit-tested in isolation and reused by mocks.

/// Build a DMTF Identify envelope. The Identify request requires no
/// authentication and asks the device for its WSMAN protocol version,
/// vendor, and product info. A good first-contact handshake.
[[nodiscard]] QByteArray buildIdentifyEnvelope();

/// Build a WS-Transfer `Get` envelope.
///
/// - `resourceUri` is the CIM/AMT class URI, e.g.
///   `"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_AssociatedPowerManagementService"`.
/// - `selectors` lets you target a specific instance of that class via
///   selector key/value pairs (`InstanceID`, `Name`, etc.); empty means
///   "the (sole) default instance".
/// - `to` is the device URL the envelope addresses; this is the same URL
///   the request will be POSTed to.
/// - `messageId` is an opaque request-correlation token; the caller must
///   ensure uniqueness within a session (typically an auto-incrementing
///   counter or a UUID).
[[nodiscard]] QByteArray buildGetEnvelope(const QString &resourceUri,
                                          const QHash<QString, QString> &selectors,
                                          const QString &to,
                                          const QString &messageId);

/// Build a WS-Eventing/Transfer custom-action `Invoke` envelope. The body
/// contains the named method element with the provided parameter
/// key/value pairs. The full action URI is `<resourceUri>/<methodName>`,
/// which is what AMT expects.
[[nodiscard]] QByteArray buildInvokeEnvelope(const QString &resourceUri,
                                              const QString &methodName,
                                              const QHash<QString, QString> &selectors,
                                              const QHash<QString, QString> &parameters,
                                              const QString &to,
                                              const QString &messageId);

/// Parsed SOAP response. `bodyXml` is the raw `<s:Body>` content (without
/// the outer Envelope/Header); `fault` is non-empty if a `s:Fault` was
/// detected at the SOAP layer.
struct SoapResponse
{
    QByteArray bodyXml;
    QString fault;
    QString headerAction;
    QString headerRelatesTo;

    [[nodiscard]] bool isFault() const { return !fault.isEmpty(); }
};

[[nodiscard]] SoapResponse parseResponse(const QByteArray &xml);

/// Find the first element with the given local name (any namespace) under
/// the parsed body XML and return its text value. Returns an empty string
/// if absent. Useful for pulling a single scalar out of a flat response
/// like `CIM_AssociatedPowerManagementService.PowerState`.
[[nodiscard]] QString findScalar(const QByteArray &bodyXml, const QString &localName);

} // namespace qumesh::wsman
