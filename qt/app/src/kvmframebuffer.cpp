// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvmframebuffer.h"

#include <QPainter>

namespace meshcommander::app {

KvmFramebuffer::KvmFramebuffer(QObject *parent) : QObject(parent) {}

void KvmFramebuffer::resize(int w, int h)
{
    if (w == m_image.width() && h == m_image.height()) return;
    m_image = QImage(w, h, QImage::Format_ARGB32);
    m_image.fill(Qt::black);
    ++m_version;
    emit resized();
    emit tileApplied(QRect(0, 0, w, h));
}

void KvmFramebuffer::applyTile(int x, int y, const QImage &tile)
{
    if (m_image.isNull() || tile.isNull()) return;
    QPainter p(&m_image);
    p.drawImage(x, y, tile);
    ++m_version;
    emit tileApplied(QRect(x, y, tile.width(), tile.height()));
}

void KvmFramebuffer::clear()
{
    if (m_image.isNull()) return;
    m_image.fill(Qt::black);
    ++m_version;
    emit tileApplied(QRect(0, 0, m_image.width(), m_image.height()));
}

} // namespace meshcommander::app
