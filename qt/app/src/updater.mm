// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "updater.h"

#import <Sparkle/Sparkle.h>

namespace qumesh::app {

struct Updater::Private
{
    SPUStandardUpdaterController *controller = nil;
};

Updater::Updater(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    // `startingUpdater:YES` kicks off Sparkle's scheduled-check
    // machinery using the Info.plist-supplied feed URL + cadence.
    // Passing nil for the user driver delegate uses Sparkle's stock
    // UI; passing nil for the updater delegate keeps the default
    // behaviour (find feed via Info.plist, install via Sparkle's
    // installer XPC).
    d->controller =
        [[SPUStandardUpdaterController alloc] initWithStartingUpdater:YES
                                                      updaterDelegate:nil
                                                   userDriverDelegate:nil];
}

Updater::~Updater()
{
    // ARC handles release; nil out the strong ref so any pending
    // callbacks see a clean state.
    d->controller = nil;
}

bool Updater::available() const
{
    return d->controller != nil;
}

void Updater::checkForUpdates()
{
    if (d->controller == nil) return;
    // `checkForUpdates:` is the standard menu-action selector on
    // SPUStandardUpdaterController. Pass nil sender — there's no
    // NSMenuItem driving this from the QML side.
    [d->controller checkForUpdates:nil];
}

} // namespace qumesh::app
