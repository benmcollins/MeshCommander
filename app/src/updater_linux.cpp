// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "updater.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QVersionNumber>
#include <QXmlStreamReader>

// QUMESH_APPCAST_FEED_URL is set as a target_compile_definitions string
// in app/CMakeLists.txt — same URL Sparkle/WinSparkle hit on the other
// two platforms.
#ifndef QUMESH_APPCAST_FEED_URL
#define QUMESH_APPCAST_FEED_URL ""
#endif

namespace qumesh::app {

namespace {

// Pulls the project owner/repo out of the feed URL so we can build a
// "/releases/tag/v..." link without hard-coding the org in two places.
// Falls back to a sensible default if parsing fails.
QString releaseTagUrl(const QString &version)
{
    const QString feed = QStringLiteral(QUMESH_APPCAST_FEED_URL);
    static const QRegularExpression re(
        QStringLiteral(R"(github\.com/([^/]+/[^/]+)/releases/)"));
    const auto m = re.match(feed);
    const QString repo = m.hasMatch() ? m.captured(1)
                                      : QStringLiteral("benmcollins/QuMesh");
    return QStringLiteral("https://github.com/%1/releases/tag/v%2")
        .arg(repo, version);
}

} // namespace

struct Updater::Private
{
    QNetworkAccessManager nam;
    QPointer<QNetworkReply> inflight;
};

Updater::Updater(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}
Updater::~Updater() = default;

bool Updater::available() const { return true; }

void Updater::checkForUpdates()
{
    // If a previous check is still inflight, drop the new request
    // rather than racing two replies.
    if (d->inflight)
        return;

    const QUrl url(QStringLiteral(QUMESH_APPCAST_FEED_URL));
    if (!url.isValid()) {
        emit checkFailed(tr("Update feed URL is not configured."));
        return;
    }

    QNetworkRequest req(url);
    // The feed URL is `/releases/latest/download/appcast.xml`, which
    // 302s to the actual tag's asset path. Qt 6 follows redirects by
    // default, but pin the policy explicitly so a future Qt change
    // doesn't silently break us. NoLessSafeRedirectPolicy blocks
    // HTTPS→HTTP downgrades, which is what we want.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("QuMesh/%1 (Linux)")
                      .arg(QCoreApplication::applicationVersion()));

    auto *reply = d->nam.get(req);
    d->inflight = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        d->inflight.clear();

        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        // Pick the highest <sparkle:version> seen anywhere in the
        // feed. The macOS/Windows entries share the version across
        // platforms (releases ship simultaneously), so we don't need
        // a Linux-specific <item> — any item tells us "this is what
        // was released most recently". We also capture the
        // <description> from the same item so the dialog can show
        // accurate notes; descriptions are platform-shared.
        QXmlStreamReader xml(reply);
        QVersionNumber latest;
        QString latestNotes;
        QString itemNotes;
        QString itemVersion;
        bool inItem = false;

        const QString sparkleNs =
            QStringLiteral("http://www.andymatuschak.org/xml-namespaces/sparkle");

        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == QLatin1String("item")) {
                    inItem = true;
                    itemVersion.clear();
                    itemNotes.clear();
                } else if (inItem
                           && xml.name() == QLatin1String("description")) {
                    // <description> wraps CDATA HTML; readElementText
                    // (IncludeChildElements) returns the inner text
                    // even when CDATA-wrapped.
                    itemNotes = xml.readElementText(
                        QXmlStreamReader::IncludeChildElements);
                } else if (inItem
                           && xml.name() == QLatin1String("enclosure")) {
                    // The version lives on the <enclosure>'s
                    // sparkle:version attribute, namespaced. Qt
                    // resolves the prefix via QXmlStreamAttribute's
                    // namespaceUri/name pair.
                    for (const auto &attr : xml.attributes()) {
                        if (attr.namespaceUri() == sparkleNs
                            && attr.name() == QLatin1String("version")) {
                            itemVersion = attr.value().toString();
                            break;
                        }
                    }
                }
            } else if (xml.isEndElement()
                       && xml.name() == QLatin1String("item")) {
                inItem = false;
                if (!itemVersion.isEmpty()) {
                    const auto v = QVersionNumber::fromString(itemVersion);
                    if (!v.isNull() && v > latest) {
                        latest = v;
                        latestNotes = itemNotes;
                    }
                }
            }
        }

        if (xml.hasError()) {
            emit checkFailed(tr("Could not parse update feed: %1")
                                 .arg(xml.errorString()));
            return;
        }

        if (latest.isNull()) {
            emit checkFailed(tr("Update feed contained no version entries."));
            return;
        }

        const auto current = QVersionNumber::fromString(
            QCoreApplication::applicationVersion());

        if (latest > current) {
            emit updateAvailable(latest.toString(),
                                 latestNotes,
                                 releaseTagUrl(latest.toString()));
        } else {
            emit upToDate(QCoreApplication::applicationVersion());
        }
    });
}

} // namespace qumesh::app
