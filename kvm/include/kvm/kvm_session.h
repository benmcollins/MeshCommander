// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>

#include "kvm/kvm_codec.h"

namespace qumesh::redir {
class RedirectionClient;
}

namespace qumesh::kvm {

/// Drives the AMT KVM (HW remote desktop) protocol on top of an
/// authenticated `RedirectionClient`.
///
/// State machine after `start()`:
///   Version   — waiting for "RFB 003.xxx".
///   Security  — waiting for security types list.
///   AuthResult — waiting for the 4-byte security result.
///   ServerInit — waiting for the desktop dimensions + name.
///   FrameLoop  — running, framebuffer updates flow as tile signals.
class KvmSession : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Version,
        Security,
        AuthResult,
        ServerInit,
        FrameLoop,
        Failed,
    };
    Q_ENUM(State)

    explicit KvmSession(qumesh::redir::RedirectionClient *client,
                         QObject *parent = nullptr);
    ~KvmSession() override;

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] quint16 desktopWidth() const { return m_width; }
    [[nodiscard]] quint16 desktopHeight() const { return m_height; }

    /// Begin the handshake. Only legal once the underlying client is
    /// `Authenticated`.
    void start();

    /// Send a key press / release using an X11 keysym.
    void sendKey(quint32 keysym, bool down);

    /// Send a pointer event in framebuffer coordinates. `buttonMask`
    /// is a bitmask of currently-held buttons.
    void sendPointer(quint8 buttonMask, quint16 x, quint16 y);

    /// Convenience: emit Ctrl+Alt+Del as a single keystroke burst.
    void sendCtrlAltDel();

    /// Convenience: request the entire desktop be refreshed
    /// (non-incremental). Call after a settings change.
    void requestFullRefresh();

signals:
    void stateChanged(State state);
    void desktopResized(int width, int height);
    void tileUpdated(int x, int y, const QImage &tile);
    void bell();
    void closed(const QString &reason);

private:
    void onRawBytes(const QByteArray &chunk);
    void drainInbox();
    bool stepHandshake();
    bool stepFrameLoop();
    void requestIncremental();
    void setState(State s);
    void fail(QString reason);
    void writeFrame(const QByteArray &frame);

    qumesh::redir::RedirectionClient *m_client;
    QByteArray m_inbox;
    State m_state = State::Idle;
    QString m_lastError;

    quint16 m_width = 0;
    quint16 m_height = 0;
    int m_pendingRects = 0;
    InflateStream m_inflate;     ///< Persists across all RLE blocks.

    /// Cached at `start()` from `QUMESH_KVM_COMPRESSION`. When `true`,
    /// the handshake emits `KvmExtCmd(4, 1)` instead of the default
    /// `(4, 0)` and AMT may send zlib-compressed RLE blocks. See
    /// issue #237 — gated on env var while we validate the decoder
    /// against real firmware.
    bool m_compressionEnabled = false;
};

} // namespace qumesh::kvm
