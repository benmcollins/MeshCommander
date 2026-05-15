// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "updater.h"
#include "updater_win_config.h"

#include <QCoreApplication>

#include <winsparkle.h>

namespace qumesh::app {

using updater_config::kAppcastFeedUrl;
using updater_config::kWinSparklePublicEdKey;

struct Updater::Private
{
    bool initialized = false;
};

Updater::Updater(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    if (kAppcastFeedUrl[0] == '\0') {
        // No feed URL baked in (e.g. -DQUMESH_AUTOUPDATE=OFF or the
        // CMake cache var was cleared). Stay uninitialized so
        // available() returns false and the QML button stays hidden.
        return;
    }

    win_sparkle_set_appcast_url(kAppcastFeedUrl);
    if (kWinSparklePublicEdKey[0] != '\0') {
        win_sparkle_set_eddsa_public_key(kWinSparklePublicEdKey);
    }

    // App-details metadata is shown in WinSparkle's update prompt
    // dialog. Use the live QCoreApplication values so localized /
    // configured names take effect.
    const QString org  = QCoreApplication::organizationName();
    const QString name = QCoreApplication::applicationName();
    const QString ver  = QCoreApplication::applicationVersion();
    win_sparkle_set_app_details(
        reinterpret_cast<const wchar_t *>(org.utf16()),
        reinterpret_cast<const wchar_t *>(name.utf16()),
        reinterpret_cast<const wchar_t *>(ver.utf16()));

    // Starts WinSparkle's background thread; it owns its own scheduled
    // check timer per the appcast's update interval.
    win_sparkle_init();
    d->initialized = true;
}

Updater::~Updater()
{
    if (d->initialized) {
        win_sparkle_cleanup();
        d->initialized = false;
    }
}

bool Updater::available() const
{
    return d->initialized;
}

void Updater::checkForUpdates()
{
    if (!d->initialized) return;
    // _with_ui = full progress dialog + release-notes view; the user
    // explicitly asked, so an in-your-face check is appropriate.
    win_sparkle_check_update_with_ui();
}

} // namespace qumesh::app
