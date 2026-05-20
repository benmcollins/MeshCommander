// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace qumesh::ssh { class SshSession; }

namespace qumesh::app {

/// Small per-machine helper that owns an `SshSession` and translates
/// the saved-machine SSH config (a `QVariantMap` produced by
/// `ComputerModel::sshConfigFor`) into the session's connect params.
///
/// Each Sol/Kvm/Ider controller embeds one of these so it can route
/// its `RedirectionClient` through a tunnel without duplicating the
/// connect / auth-mode / TOFU plumbing from `MachineDetailsController`.
class SshTunnelHost : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool awaitingHostKeyTrust READ awaitingHostKeyTrust
                   NOTIFY awaitingHostKeyTrustChanged)
    Q_PROPERTY(QString pendingHostKeyFingerprint READ pendingHostKeyFingerprint
                   NOTIFY pendingHostKeyChanged)
    Q_PROPERTY(QString pendingHostKeyType READ pendingHostKeyType
                   NOTIFY pendingHostKeyChanged)
public:
    explicit SshTunnelHost(QObject *parent = nullptr);
    ~SshTunnelHost() override;

    /// Apply a per-machine SSH config. `cfg["enabled"] == true` creates
    /// (or reuses) the underlying `SshSession` and starts its open
    /// flow; `false` (or a missing key) tears any existing session
    /// down. When the remote presents an unpinned host key the session
    /// pauses in `NeedsHostKeyTrust` and emits `hostKeyPromptRequired`
    /// — callers must surface a dialog and either call
    /// `trustPendingHostKey(persist)` or cancel by passing
    /// `enabled=false` (or invoking `close()` indirectly via setConfig).
    void setConfig(const QVariantMap &cfg);

    /// `true` once the SSH session has reached `Connected`.
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] qumesh::ssh::SshSession *session() const { return m_session; }
    [[nodiscard]] QString status() const { return m_status; }
    [[nodiscard]] bool awaitingHostKeyTrust() const { return m_awaitingHostKeyTrust; }
    [[nodiscard]] QString pendingHostKeyFingerprint() const { return m_pendingHostKeyFingerprint; }
    [[nodiscard]] QString pendingHostKeyType() const { return m_pendingHostKeyType; }

    /// Called by the QML prompt when the user accepts the presented
    /// host key. Resumes the paused SSH session. When `persist` is
    /// true, also emits `trustedHostKeyAdded` so the controller can
    /// forward the new fingerprint into ComputerModel for verification
    /// on subsequent connects. No-op if no prompt is pending.
    Q_INVOKABLE void trustPendingHostKey(bool persist);

signals:
    void statusChanged();
    void connectedChanged();
    void awaitingHostKeyTrustChanged();
    void pendingHostKeyChanged();
    /// Emitted when the remote SSH server's host key is not in the
    /// machine's `trustedHostKeyFingerprints` list. The session is
    /// paused — call `trustPendingHostKey(persist)` to accept or
    /// reapply `setConfig` with `enabled=false` to abandon.
    void hostKeyPromptRequired(const QString &fingerprint,
                                const QString &keyType);
    /// Emitted after `trustPendingHostKey(true)` so the owning
    /// controller can persist the new fingerprint into ComputerModel.
    void trustedHostKeyAdded(const QString &fingerprint);

private:
    qumesh::ssh::SshSession *m_session = nullptr;
    bool m_enabled = false;
    QString m_status;
    QString m_jumpHost;  ///< Cached at setConfig() so status strings can name it.
    bool m_awaitingHostKeyTrust = false;
    QString m_pendingHostKeyFingerprint;
    QString m_pendingHostKeyType;
};

} // namespace qumesh::app
