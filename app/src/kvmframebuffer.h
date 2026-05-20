// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QImage>
#include <QObject>
#include <QRect>
#include <QtQmlIntegration>

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
    QML_ELEMENT
    QML_UNCREATABLE("Owned by KvmController")
    Q_PROPERTY(int width READ width NOTIFY resized)
    Q_PROPERTY(int height READ height NOTIFY resized)
    // `version` is a monotonic frame counter — bumped on tileApplied,
    // resize, and clear. Pre-#287 it reused `tileApplied(QRect)` as
    // its NOTIFY signal; same fire-points worked in practice but the
    // signal name implied a different scope (tile arrival only) and
    // qmllint flagged the asymmetric arity (tileApplied takes a
    // QRect, NOTIFY signals for ints conventionally take none).
    Q_PROPERTY(int version READ version NOTIFY versionChanged)

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
    void versionChanged();

private:
    QImage m_image;
    int m_version = 0;
};

} // namespace qumesh::app
