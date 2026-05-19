// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "appinfo.h"
#include "appinfo_config.h"

#include <QFile>
#include <QStringLiteral>
#include <QTextDocument>

namespace qumesh::app {

AppInfo::AppInfo(QObject *parent) : QObject(parent) {}

QString AppInfo::version() const
{
    return QStringLiteral(QUMESH_VERSION);
}

QString AppInfo::buildDate() const
{
    return QStringLiteral(QUMESH_BUILD_DATE);
}

QString AppInfo::author() const
{
    return QStringLiteral("Ben Collins <ben@ironrocketsmc.org>");
}

QString AppInfo::licenseText() const
{
    QFile f(QStringLiteral(":/QuMesh/LICENSE.md"));
    if (!f.open(QIODevice::ReadOnly)) {
        // Resource registration regressed somehow — fall back to a
        // pointer at the on-disk file so users can still find the
        // license rather than seeing a blank dialog.
        return QStringLiteral("LICENSE.md not bundled. See https://github.com/benmcollins/QuMesh/blob/main/LICENSE.md");
    }

    // Convert the Markdown source to HTML via `QTextDocument` so we
    // can hand the result to `TextEdit.RichText` instead of relying
    // on the AboutDialog's body font scaling. Qt's `MarkdownText`
    // mode honoured the inherited (small) pixelSize and ended up
    // rendering the document flat — headings barely distinguishable
    // from the body. Forcing an explicit stylesheet here pins the
    // hierarchy regardless of the QML font settings.
    QTextDocument doc;
    doc.setMarkdown(QString::fromUtf8(f.readAll()));
    static const char *kStylesheet =
        "h1 { font-size: 18pt; font-weight: 600; margin: 14px 0 8px 0; }"
        "h2 { font-size: 14pt; font-weight: 600; margin: 12px 0 6px 0; }"
        "h3 { font-size: 12pt; font-weight: 600; margin: 10px 0 4px 0; }"
        "h4 { font-size: 11pt; font-weight: 600; margin: 10px 0 4px 0; }"
        "p  { margin: 6px 0; line-height: 1.4; }"
        "li { margin: 3px 0; }"
        "code { font-family: monospace; }"
        "pre { font-family: monospace; }";
    return QStringLiteral("<style>%1</style>%2")
            .arg(QString::fromLatin1(kStylesheet), doc.toHtml());
}

} // namespace qumesh::app
