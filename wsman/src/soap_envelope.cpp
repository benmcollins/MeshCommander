// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/soap_envelope.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <memory>

namespace qumesh::wsman {

namespace {

constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
constexpr char kNsTransfer[] = "http://schemas.xmlsoap.org/ws/2004/09/transfer";
constexpr char kNsIdentify[] =
    "http://schemas.dmtf.org/wbem/wsman/identity/1/wsmanidentity.xsd";

constexpr char kAnonymousReplyTo[] =
    "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";

constexpr char kActionGet[] = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Get";
constexpr char kActionPut[] = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Put";
constexpr char kActionDelete[] = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Delete";
constexpr char kActionEnumerate[] =
    "http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate";
constexpr char kActionPull[] =
    "http://schemas.xmlsoap.org/ws/2004/09/enumeration/Pull";
constexpr char kNsEnumeration[] = "http://schemas.xmlsoap.org/ws/2004/09/enumeration";

constexpr char kBootConfigResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_BootConfigSetting";
constexpr char kBootSourceResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_BootSourceSetting";

void writeAddressingHeader(QXmlStreamWriter &w, const QString &action,
                           const QString &to, const QString &messageId,
                           const QString &resourceUri,
                           const QHash<QString, QString> &selectors)
{
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));

    w.writeTextElement(QString::fromLatin1(kNsAddressing), QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing), QStringLiteral("To"), to);

    if (!resourceUri.isEmpty()) {
        w.writeTextElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("ResourceURI"), resourceUri);
    }

    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);

    w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QString::fromLatin1(kAnonymousReplyTo));
    w.writeEndElement(); // ReplyTo

    if (!selectors.isEmpty()) {
        w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("SelectorSet"));
        for (auto it = selectors.constBegin(); it != selectors.constEnd(); ++it) {
            w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("Selector"));
            w.writeAttribute(QStringLiteral("Name"), it.key());
            w.writeCharacters(it.value());
            w.writeEndElement(); // Selector
        }
        w.writeEndElement(); // SelectorSet
    }

    w.writeEndElement(); // Header
}

} // namespace

QByteArray buildIdentifyEnvelope()
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsIdentify), QStringLiteral("wsmid"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));
    w.writeEmptyElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeEmptyElement(QString::fromLatin1(kNsIdentify), QStringLiteral("Identify"));
    w.writeEndElement(); // Body
    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QByteArray buildGetEnvelope(const QString &resourceUri,
                            const QHash<QString, QString> &selectors, const QString &to,
                            const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(QString::fromLatin1(kNsTransfer), QStringLiteral("t"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    writeAddressingHeader(w, QString::fromLatin1(kActionGet), to, messageId, resourceUri,
                            selectors);

    w.writeEmptyElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QByteArray buildDeleteEnvelope(const QString &resourceUri,
                                const QHash<QString, QString> &selectors,
                                const QString &to,
                                const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(QString::fromLatin1(kNsTransfer), QStringLiteral("t"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    writeAddressingHeader(w, QString::fromLatin1(kActionDelete), to, messageId,
                            resourceUri, selectors);

    w.writeEmptyElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QByteArray buildPutEnvelope(const QString &resourceUri,
                             const QString &className,
                             const QHash<QString, QString> &selectors,
                             const QHash<QString, QString> &properties,
                             const QString &to,
                             const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(resourceUri, QStringLiteral("r"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    writeAddressingHeader(w, QString::fromLatin1(kActionPut), to, messageId, resourceUri,
                            selectors);

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resourceUri, className);
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        w.writeTextElement(resourceUri, it.key(), it.value());
    }
    w.writeEndElement(); // <className>
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QByteArray buildChangeBootOrderEnvelope(const QString &amtBootSourceInstanceId,
                                         const QString &to,
                                         const QString &messageId)
{
    // CIM_BootConfigSetting.ChangeBootOrder accepts a `Source` parameter
    // that is a full WS-Addressing EPR pointing at a specific
    // CIM_BootSourceSetting. Empty `amtBootSourceInstanceId` → omit
    // Source entirely (clears the boot-order override).
    const QString configUri = QString::fromLatin1(kBootConfigResource);
    const QString sourceUri = QString::fromLatin1(kBootSourceResource);

    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"),
                     QStringLiteral("Intel(r) AMT: Boot Configuration 0"));

    const QString action = configUri + QLatin1String("/ChangeBootOrder");

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(configUri, QStringLiteral("r"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));
    writeAddressingHeader(w, action, to, messageId, configUri, selectors);
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(configUri, QStringLiteral("ChangeBootOrder_INPUT"));

    if (!amtBootSourceInstanceId.isEmpty()) {
        w.writeStartElement(configUri, QStringLiteral("Source"));

        // The EPR is the standard {Address, ReferenceParameters} pair.
        w.writeTextElement(QString::fromLatin1(kNsAddressing),
                            QStringLiteral("Address"),
                            QString::fromLatin1(
                                "http://schemas.xmlsoap.org/ws/2004/08/addressing"));

        w.writeStartElement(QString::fromLatin1(kNsAddressing),
                            QStringLiteral("ReferenceParameters"));
        w.writeTextElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("ResourceURI"), sourceUri);
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("SelectorSet"));
        w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), QStringLiteral("InstanceID"));
        w.writeCharacters(QStringLiteral("Intel(r) AMT: ") + amtBootSourceInstanceId);
        w.writeEndElement(); // Selector
        w.writeEndElement(); // SelectorSet
        w.writeEndElement(); // ReferenceParameters

        w.writeEndElement(); // Source
    }

    w.writeEndElement(); // ChangeBootOrder_INPUT
    w.writeEndElement(); // Body
    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QByteArray buildEnumerateEnvelope(const QString &resourceUri, const QString &to,
                                   const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(QString::fromLatin1(kNsEnumeration), QStringLiteral("e"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));
    writeAddressingHeader(w, QString::fromLatin1(kActionEnumerate), to, messageId,
                            resourceUri, {});
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeEmptyElement(QString::fromLatin1(kNsEnumeration),
                         QStringLiteral("Enumerate"));
    w.writeEndElement(); // Body
    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QByteArray buildPullEnvelope(const QString &resourceUri,
                              const QString &enumerationContext, int maxElements,
                              const QString &to, const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(QString::fromLatin1(kNsEnumeration), QStringLiteral("e"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));
    writeAddressingHeader(w, QString::fromLatin1(kActionPull), to, messageId,
                            resourceUri, {});
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(QString::fromLatin1(kNsEnumeration), QStringLiteral("Pull"));
    w.writeTextElement(QString::fromLatin1(kNsEnumeration),
                        QStringLiteral("EnumerationContext"), enumerationContext);
    w.writeTextElement(QString::fromLatin1(kNsEnumeration),
                        QStringLiteral("MaxElements"),
                        QString::number(maxElements > 0 ? maxElements : 64));
    w.writeEndElement(); // Pull
    w.writeEndElement(); // Body
    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

QString parseEnumerateContext(const QByteArray &bodyXml)
{
    return findScalar(bodyXml, QStringLiteral("EnumerationContext"));
}

PullChunk parsePullResponse(const QByteArray &bodyXml)
{
    PullChunk out;
    QXmlStreamReader r(bodyXml);
    int depth = 0;
    int itemsDepth = -1; // Depth at which we're inside <Items>.
    int itemDepth = -1;  // Depth at which we're capturing a single item.

    QByteArray itemBuf;
    // QXmlStreamWriter is non-copyable / non-assignable; re-create
    // through a smart-pointer for each item so we can target a fresh
    // QByteArray without juggling lifetimes.
    std::unique_ptr<QXmlStreamWriter> itemWriter;

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            ++depth;
            const QStringView ns = r.namespaceUri();
            const QStringView ln = r.name();

            if (itemsDepth < 0 && ns == QString::fromLatin1(kNsEnumeration)
                && ln == QStringLiteral("Items")) {
                itemsDepth = depth;
                continue;
            }
            if (ns == QString::fromLatin1(kNsEnumeration)
                && ln == QStringLiteral("EnumerationContext")) {
                out.enumerationContext = r.readElementText();
                --depth;
                continue;
            }
            if (ns == QString::fromLatin1(kNsEnumeration)
                && ln == QStringLiteral("EndOfSequence")) {
                out.endOfSequence = true;
                // self-closing; don't expect content
                continue;
            }
            if (itemsDepth > 0 && depth == itemsDepth + 1) {
                // Starting a new item. Capture under a fresh writer.
                itemBuf.clear();
                itemWriter = std::make_unique<QXmlStreamWriter>(&itemBuf);
                itemWriter->setAutoFormatting(false);
                itemDepth = depth;
                itemWriter->writeStartElement(ns.toString(), ln.toString());
                const auto attrs = r.attributes();
                for (const auto &a : attrs) {
                    if (a.namespaceUri().isEmpty()) {
                        itemWriter->writeAttribute(a.name().toString(), a.value().toString());
                    } else {
                        itemWriter->writeAttribute(a.namespaceUri().toString(),
                                                    a.name().toString(),
                                                    a.value().toString());
                    }
                }
                continue;
            }
            if (itemWriter && itemDepth > 0 && depth > itemDepth) {
                itemWriter->writeStartElement(ns.toString(), ln.toString());
                const auto attrs = r.attributes();
                for (const auto &a : attrs) {
                    if (a.namespaceUri().isEmpty()) {
                        itemWriter->writeAttribute(a.name().toString(), a.value().toString());
                    } else {
                        itemWriter->writeAttribute(a.namespaceUri().toString(),
                                                    a.name().toString(),
                                                    a.value().toString());
                    }
                }
            }
        } else if (r.isCharacters() && itemWriter && itemDepth > 0 && depth >= itemDepth) {
            itemWriter->writeCharacters(r.text().toString());
        } else if (r.isEndElement()) {
            if (itemWriter && itemDepth > 0 && depth > itemDepth) {
                itemWriter->writeEndElement();
            } else if (itemWriter && itemDepth > 0 && depth == itemDepth) {
                itemWriter->writeEndElement();
                itemWriter.reset();
                out.items.append(itemBuf);
                itemBuf.clear();
                itemDepth = -1;
            }
            if (itemsDepth > 0 && depth == itemsDepth) itemsDepth = -1;
            --depth;
        }
    }
    return out;
}

QByteArray buildInvokeEnvelope(const QString &resourceUri,
                                const QString &methodName,
                                const QHash<QString, QString> &selectors,
                                const QHash<QString, QString> &parameters,
                                const QString &to,
                                const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    const QString action = resourceUri + QLatin1Char('/') + methodName;

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap), QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman), QStringLiteral("w"));
    w.writeNamespace(resourceUri, QStringLiteral("r"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    writeAddressingHeader(w, action, to, messageId, resourceUri, selectors);

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resourceUri, methodName + QStringLiteral("_INPUT"));
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        w.writeTextElement(resourceUri, it.key(), it.value());
    }
    w.writeEndElement(); // <methodName>_INPUT
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

SoapResponse parseResponse(const QByteArray &xml)
{
    SoapResponse out;
    QXmlStreamReader r(xml);

    int depth = 0;
    int bodyDepth = -1;
    QByteArray bodyXml;
    QXmlStreamWriter bodyWriter(&bodyXml);

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            ++depth;
            const QStringView ns = r.namespaceUri();
            const QStringView ln = r.name();
            if (depth == 2 && ns == QString::fromLatin1(kNsSoap)
                && ln == QStringLiteral("Body")) {
                bodyDepth = depth;
                continue;
            }
            if (depth == 3 && bodyDepth == 2 && ns == QString::fromLatin1(kNsSoap)
                && ln == QStringLiteral("Fault")) {
                // readElementText consumes through the closing tag, so we
                // must compensate for the end-element our loop will miss.
                out.fault = r.readElementText(QXmlStreamReader::IncludeChildElements);
                --depth;
                continue;
            }
            if (depth == 3 && ns == QString::fromLatin1(kNsAddressing)
                && ln == QStringLiteral("Action")) {
                out.headerAction = r.readElementText();
                --depth;
                continue;
            }
            if (depth == 3 && ns == QString::fromLatin1(kNsAddressing)
                && ln == QStringLiteral("RelatesTo")) {
                out.headerRelatesTo = r.readElementText();
                --depth;
                continue;
            }
            if (bodyDepth > 0 && depth > bodyDepth) {
                bodyWriter.writeStartElement(ns.toString(), ln.toString());
                const auto attrs = r.attributes();
                for (const auto &a : attrs) {
                    if (a.namespaceUri().isEmpty()) {
                        bodyWriter.writeAttribute(a.name().toString(), a.value().toString());
                    } else {
                        bodyWriter.writeAttribute(a.namespaceUri().toString(),
                                                    a.name().toString(),
                                                    a.value().toString());
                    }
                }
            }
        } else if (r.isCharacters() && bodyDepth > 0 && depth > bodyDepth) {
            bodyWriter.writeCharacters(r.text().toString());
        } else if (r.isEndElement()) {
            if (bodyDepth > 0 && depth > bodyDepth) {
                bodyWriter.writeEndElement();
            }
            if (depth == bodyDepth) bodyDepth = -1;
            --depth;
        }
    }

    out.bodyXml = bodyXml;
    return out;
}

QString findScalar(const QByteArray &bodyXml, const QString &localName)
{
    QXmlStreamReader r(bodyXml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == localName) {
            return r.readElementText();
        }
    }
    return {};
}

} // namespace qumesh::wsman
