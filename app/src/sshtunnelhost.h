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
public:
    explicit SshTunnelHost(QObject *parent = nullptr);
    ~SshTunnelHost() override;

    /// Apply a per-machine SSH config. `cfg["enabled"] == true` creates
    /// (or reuses) the underlying `SshSession` and starts its open
    /// flow; `false` (or a missing key) tears any existing session
    /// down. On first connect the host auto-pins the remote key
    /// (TOFU) and emits `trustedHostKeyAdded` so the controller can
    /// forward the new fingerprint up to `ComputerModel`.
    void setConfig(const QVariantMap &cfg);

    /// `true` once the SSH session has reached `Connected`.
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] qumesh::ssh::SshSession *session() const { return m_session; }
    [[nodiscard]] QString status() const { return m_status; }

signals:
    void statusChanged();
    void connectedChanged();
    /// Emitted on a fresh first-connect with the remote key auto-pinned.
    /// The controller forwards this so `ComputerModel` can persist the
    /// fingerprint for verification on subsequent connects.
    void trustedHostKeyAdded(const QString &fingerprint);

private:
    qumesh::ssh::SshSession *m_session = nullptr;
    bool m_enabled = false;
    QString m_status;
    QString m_jumpHost;  ///< Cached at setConfig() so status strings can name it.
};

} // namespace qumesh::app
