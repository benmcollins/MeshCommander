// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

#include <utility>

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

/// Build a WS-Enumeration `Enumerate` envelope. AMT replies with an
/// `<EnumerationContext>` token the caller feeds back to
/// `buildPullEnvelope` (potentially many times) to walk the collection.
[[nodiscard]] QByteArray buildEnumerateEnvelope(const QString &resourceUri,
                                                 const QString &to,
                                                 const QString &messageId);

/// Build a WS-Enumeration `Pull` envelope. `enumerationContext` is the
/// token returned by the previous Enumerate / Pull response;
/// `maxElements` caps the page size (64 is a sensible default for AMT).
[[nodiscard]] QByteArray buildPullEnvelope(const QString &resourceUri,
                                            const QString &enumerationContext,
                                            int maxElements,
                                            const QString &to,
                                            const QString &messageId);

/// Output of `parsePullResponse`. `items` is a list of inner-XML
/// strings — one per row returned by AMT — that the caller can feed
/// back through `findScalar` to pluck out specific field values.
struct PullChunk
{
    QList<QByteArray> items;
    QString enumerationContext;
    bool endOfSequence = false;
};

/// Extract the `EnumerationContext` from an `EnumerateResponse` body.
[[nodiscard]] QString parseEnumerateContext(const QByteArray &bodyXml);

/// Pull out the per-item XML bodies, the (possibly empty) continuation
/// context, and the EndOfSequence flag from a `PullResponse` body.
[[nodiscard]] PullChunk parsePullResponse(const QByteArray &bodyXml);

/// Build a WS-Transfer `Delete` envelope. Same shape as Get — the
/// addressing header carries the action, the resource URI, and the
/// `SelectorSet` that identifies which instance to remove — and the
/// body is empty. AMT applies the delete to the (single) instance
/// whose CIM keys match the supplied selectors.
[[nodiscard]] QByteArray buildDeleteEnvelope(const QString &resourceUri,
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

/// Ordered variant of `buildInvokeEnvelope`. Use when (a) the parameter
/// order matters to the responder, or (b) the same key needs to appear
/// multiple times — e.g. `AMT_AuthorizationService.AddUserAclEntryEx`
/// emits one `<Realms>N</Realms>` element per realm-bit the new user
/// is granted. The `QHash` overload collapses duplicate keys; this one
/// preserves them.
[[nodiscard]] QByteArray buildInvokeEnvelopeOrdered(const QString &resourceUri,
                                                     const QString &methodName,
                                                     const QHash<QString, QString> &selectors,
                                                     const QList<std::pair<QString, QString>> &parameters,
                                                     const QString &to,
                                                     const QString &messageId);

/// Build a WS-Transfer `Put` envelope. `className` is the leaf XML tag
/// (e.g. `AMT_BootSettingData`); `properties` becomes child elements in
/// the resource namespace.
[[nodiscard]] QByteArray buildPutEnvelope(const QString &resourceUri,
                                           const QString &className,
                                           const QHash<QString, QString> &selectors,
                                           const QHash<QString, QString> &properties,
                                           const QString &to,
                                           const QString &messageId);

/// `ChangeBootOrder` accepts a `Source` parameter that is itself an
/// endpoint reference (EPR) pointing at a `CIM_BootSourceSetting`
/// instance. This helper wraps the InstanceID for a given AMT boot
/// source (e.g. "Force PXE Boot") into the right EPR shape. `instanceId`
/// is the bit after the "Intel(r) AMT: " prefix — pass empty to clear
/// the boot order (Source omitted).
[[nodiscard]] QByteArray buildChangeBootOrderEnvelope(const QString &amtBootSourceInstanceId,
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
