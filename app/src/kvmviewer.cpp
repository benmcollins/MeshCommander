// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvmviewer.h"

#include "kvmcontroller.h"
#include "kvmframebuffer.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QRect>
#include <QWheelEvent>

#include <algorithm>

namespace qumesh::app {

namespace {

constexpr quint32 kXkBackspace = 0xFF08;
constexpr quint32 kXkTab       = 0xFF09;
constexpr quint32 kXkReturn    = 0xFF0D;
constexpr quint32 kXkEscape    = 0xFF1B;
constexpr quint32 kXkDelete    = 0xFFFF;
constexpr quint32 kXkHome      = 0xFF50;
constexpr quint32 kXkLeft      = 0xFF51;
constexpr quint32 kXkUp        = 0xFF52;
constexpr quint32 kXkRight     = 0xFF53;
constexpr quint32 kXkDown      = 0xFF54;
constexpr quint32 kXkPageUp    = 0xFF55;
constexpr quint32 kXkPageDown  = 0xFF56;
constexpr quint32 kXkEnd       = 0xFF57;
constexpr quint32 kXkInsert    = 0xFF63;
constexpr quint32 kXkF1Base    = 0xFFBE;
constexpr quint32 kXkShiftL    = 0xFFE1;
constexpr quint32 kXkControlL  = 0xFFE3;
constexpr quint32 kXkAltL      = 0xFFE9;
constexpr quint32 kXkMetaL     = 0xFFE7;

} // namespace

KvmViewer::KvmViewer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    // QQuickPaintedItem already sets ItemHasContents in its base
    // ctor — the explicit setFlag here was redundant (#295).
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFillColor(Qt::black);
    setFocus(true);
}

void KvmViewer::setController(KvmController *controller)
{
    if (controller == m_controller) return;
    if (m_framebuffer) m_framebuffer->disconnect(this);
    m_controller = controller;
    m_framebuffer = controller ? controller->framebuffer() : nullptr;
    if (m_framebuffer) {
        connect(m_framebuffer, &KvmFramebuffer::resized, this, [this]() { update(); });
        connect(m_framebuffer, &KvmFramebuffer::tileApplied, this,
                [this](QRect) { update(); });
    }
    emit controllerChanged();
    update();
}

void KvmViewer::paint(QPainter *painter)
{
    painter->fillRect(0, 0, static_cast<int>(width()), static_cast<int>(height()),
                       Qt::black);
    if (!m_framebuffer || m_framebuffer->image().isNull()) return;

    const QImage &img = m_framebuffer->image();
    const qreal sx = width() / static_cast<qreal>(img.width());
    const qreal sy = height() / static_cast<qreal>(img.height());
    const qreal s = (std::min)(sx, sy);
    const qreal w = img.width() * s;
    const qreal h = img.height() * s;
    const qreal ox = (width() - w) * 0.5;
    const qreal oy = (height() - h) * 0.5;
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(QRectF(ox, oy, w, h), img);
}

QPoint KvmViewer::toFramebufferCoords(const QPointF &local) const
{
    if (!m_framebuffer || m_framebuffer->image().isNull()) return {};
    const QImage &img = m_framebuffer->image();
    const qreal sx = width() / static_cast<qreal>(img.width());
    const qreal sy = height() / static_cast<qreal>(img.height());
    const qreal s = (std::min)(sx, sy);
    const qreal w = img.width() * s;
    const qreal h = img.height() * s;
    const qreal ox = (width() - w) * 0.5;
    const qreal oy = (height() - h) * 0.5;
    const qreal fx = (local.x() - ox) / s;
    const qreal fy = (local.y() - oy) / s;
    return QPoint(std::clamp(static_cast<int>(fx), 0, img.width() - 1),
                   std::clamp(static_cast<int>(fy), 0, img.height() - 1));
}

quint8 KvmViewer::mouseButtons(Qt::MouseButtons buttons) const
{
    quint8 mask = 0;
    if (buttons & Qt::LeftButton)   mask |= 0x01;
    if (buttons & Qt::MiddleButton) mask |= 0x02;
    if (buttons & Qt::RightButton)  mask |= 0x04;
    return mask;
}

void KvmViewer::sendPointer(Qt::MouseButtons buttons)
{
    if (!m_controller) return;
    const quint8 mask = mouseButtons(buttons);
    // Dedup: some platforms deliver both hoverMove and mouseMove for
    // the same physical move during a drag. Skip if neither position
    // nor button mask changed.
    if (m_lastFbPos == m_lastSentFbPos && mask == m_lastSentMask) return;
    m_lastSentFbPos = m_lastFbPos;
    m_lastSentMask = mask;
    m_controller->sendPointer(mask, m_lastFbPos.x(), m_lastFbPos.y());
}

void KvmViewer::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus();
    m_lastFbPos = toFramebufferCoords(event->position());
    sendPointer(event->buttons());
    event->accept();
}

void KvmViewer::mouseReleaseEvent(QMouseEvent *event)
{
    m_lastFbPos = toFramebufferCoords(event->position());
    sendPointer(event->buttons());
    event->accept();
}

void KvmViewer::mouseMoveEvent(QMouseEvent *event)
{
    m_lastFbPos = toFramebufferCoords(event->position());
    sendPointer(event->buttons());
    event->accept();
}

void KvmViewer::hoverMoveEvent(QHoverEvent *event)
{
    m_lastFbPos = toFramebufferCoords(event->position());
    sendPointer(Qt::NoButton);
    event->accept();
}

void KvmViewer::wheelEvent(QWheelEvent *event)
{
    if (!m_controller) return;
    const int dy = event->angleDelta().y();
    if (dy == 0) return;
    const quint8 base = mouseButtons(event->buttons());
    const quint8 wheelBit = (dy > 0) ? 0x08 : 0x10;
    // Wheel events look like a momentary button press at the current
    // location with the wheel bit set, then a release.
    m_controller->sendPointer(base | wheelBit, m_lastFbPos.x(), m_lastFbPos.y());
    m_controller->sendPointer(base,            m_lastFbPos.x(), m_lastFbPos.y());
    event->accept();
}

quint32 KvmViewer::qtKeyToKeysym(int key, const QString &text)
{
    switch (key) {
    case Qt::Key_Backspace:   return kXkBackspace;
    case Qt::Key_Tab:         return kXkTab;
    case Qt::Key_Return:
    case Qt::Key_Enter:       return kXkReturn;
    case Qt::Key_Escape:      return kXkEscape;
    case Qt::Key_Delete:      return kXkDelete;
    case Qt::Key_Home:        return kXkHome;
    case Qt::Key_Left:        return kXkLeft;
    case Qt::Key_Up:          return kXkUp;
    case Qt::Key_Right:       return kXkRight;
    case Qt::Key_Down:        return kXkDown;
    case Qt::Key_PageUp:      return kXkPageUp;
    case Qt::Key_PageDown:    return kXkPageDown;
    case Qt::Key_End:         return kXkEnd;
    case Qt::Key_Insert:      return kXkInsert;
    case Qt::Key_Shift:       return kXkShiftL;
    case Qt::Key_Control:     return kXkControlL;
    case Qt::Key_Alt:         return kXkAltL;
    case Qt::Key_Meta:        return kXkMetaL;
    default: break;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return kXkF1Base + (key - Qt::Key_F1);
    }
    if (!text.isEmpty()) {
        const char16_t u = text.at(0).unicode();
        if (u >= 0x20 && u <= 0x7E) return u;
        if (u >= 0xA0 && u <= 0xFF) return u; // Latin-1 maps to X11 keysym 1:1
    }
    // Fall through when `text` is empty or a control byte (e.g.
    // Ctrl+letter delivers "\x03" but the remote needs the base
    // letter — it'll apply the Ctrl-down we already sent on its own).
    // Map the physical key to its lowercase ASCII keysym for letters
    // and digits.
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<quint32>(key - Qt::Key_A + 0x61);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<quint32>(key - Qt::Key_0 + 0x30);
    }
    return 0;
}

void KvmViewer::keyPressEvent(QKeyEvent *event)
{
    const quint32 sym = qtKeyToKeysym(event->key(), event->text());
    if (sym == 0 || !m_controller) {
        event->ignore();
        return;
    }
    m_controller->sendKey(sym, true);
    event->accept();
}

void KvmViewer::keyReleaseEvent(QKeyEvent *event)
{
    const quint32 sym = qtKeyToKeysym(event->key(), event->text());
    if (sym == 0 || !m_controller) {
        event->ignore();
        return;
    }
    m_controller->sendKey(sym, false);
    event->accept();
}

} // namespace qumesh::app
