// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/soap_envelope.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

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
