// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "certs/cert_entry.h"

namespace meshcommander::certs {

QJsonObject CertEntry::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("subjectCN"), subjectCommonName);
    obj.insert(QStringLiteral("issuerCN"), issuerCommonName);
    obj.insert(QStringLiteral("serial"), serial);
    obj.insert(QStringLiteral("notBefore"), notBefore.toString(Qt::ISODate));
    obj.insert(QStringLiteral("notAfter"), notAfter.toString(Qt::ISODate));
    obj.insert(QStringLiteral("fingerprintSha256"), fingerprintSha256);
    obj.insert(QStringLiteral("hasPrivateKey"), hasPrivateKey);
    obj.insert(QStringLiteral("certbin"),
                QString::fromLatin1(certDer.toBase64()));
    if (!privateKeyPem.isEmpty()) {
        obj.insert(QStringLiteral("privateKeyBin"), privateKeyPem);
    }
    return obj;
}

CertEntry CertEntry::fromJson(const QJsonObject &obj)
{
    CertEntry e;
    e.id = obj.value(QStringLiteral("id")).toString();
    e.subjectCommonName = obj.value(QStringLiteral("subjectCN")).toString();
    e.issuerCommonName = obj.value(QStringLiteral("issuerCN")).toString();
    e.serial = obj.value(QStringLiteral("serial")).toString();
    e.notBefore = QDateTime::fromString(obj.value(QStringLiteral("notBefore")).toString(),
                                          Qt::ISODate);
    e.notAfter = QDateTime::fromString(obj.value(QStringLiteral("notAfter")).toString(),
                                         Qt::ISODate);
    e.fingerprintSha256 = obj.value(QStringLiteral("fingerprintSha256")).toString();
    e.hasPrivateKey = obj.value(QStringLiteral("hasPrivateKey")).toBool();
    e.certDer = QByteArray::fromBase64(
        obj.value(QStringLiteral("certbin")).toString().toLatin1());
    e.privateKeyPem = obj.value(QStringLiteral("privateKeyBin")).toString();
    return e;
}

} // namespace meshcommander::certs
