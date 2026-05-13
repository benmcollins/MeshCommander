// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QPainter>
#include <QPointer>
#include <QQuickPaintedItem>

namespace meshcommander::app {

class KvmController;
class KvmFramebuffer;

/// Custom QQuickItem that paints the framebuffer from `KvmFramebuffer`
/// scaled to fit (preserving aspect ratio). Mouse and keyboard input
/// are forwarded to `KvmController` after translating viewport
/// coordinates into framebuffer space.
class KvmViewer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(meshcommander::app::KvmController *controller
                   READ controller WRITE setController NOTIFY controllerChanged)

public:
    explicit KvmViewer(QQuickItem *parent = nullptr);

    [[nodiscard]] KvmController *controller() const { return m_controller; }
    void setController(KvmController *controller);

    void paint(QPainter *painter) override;

signals:
    void controllerChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QPoint toFramebufferCoords(const QPointF &local) const;
    quint8 mouseButtons(Qt::MouseButtons buttons) const;
    static quint32 qtKeyToKeysym(int key, const QString &text);
    void sendPointer(Qt::MouseButtons buttons);

    QPointer<KvmController> m_controller;
    QPointer<KvmFramebuffer> m_framebuffer;
    QPoint m_lastFbPos;
    QPoint m_lastSentFbPos{-1, -1};
    quint8 m_lastSentMask = 0;
};

} // namespace meshcommander::app
