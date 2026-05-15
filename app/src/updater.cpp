// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "updater.h"

namespace qumesh::app {

struct Updater::Private {};

Updater::Updater(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}
Updater::~Updater() = default;

bool Updater::available() const { return false; }
void Updater::checkForUpdates() {}

} // namespace qumesh::app
