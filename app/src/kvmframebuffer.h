// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QImage>
#include <QObject>
#include <QRect>

namespace qumesh::app {

/// Backing framebuffer for the KVM viewer. The session pushes tiles
/// here as `QImage`s; the QML viewer queries the current frame via
/// `image()` whenever it needs to repaint.
///
/// The framebuffer is owned by the `KvmController` and exposed to QML
/// as a property. The viewer treats it as opaque; tile composition
/// happens here, not in the viewer.
class KvmFramebuffer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int width READ width NOTIFY resized)
    Q_PROPERTY(int height READ height NOTIFY resized)
    Q_PROPERTY(int version READ version NOTIFY tileApplied)

public:
    explicit KvmFramebuffer(QObject *parent = nullptr);

    [[nodiscard]] int width() const { return m_image.width(); }
    [[nodiscard]] int height() const { return m_image.height(); }
    [[nodiscard]] int version() const { return m_version; }
    [[nodiscard]] const QImage &image() const { return m_image; }

    void resize(int w, int h);
    void applyTile(int x, int y, const QImage &tile);
    void clear();

signals:
    void resized();
    void tileApplied(QRect rect);

private:
    QImage m_image;
    int m_version = 0;
};

} // namespace qumesh::app
