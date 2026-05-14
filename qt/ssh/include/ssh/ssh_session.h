// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace qumesh::ssh {

class SshSessionWorker;

/// One SSH connection to a jump host. Lives on the caller's thread but
/// dispatches all libssh work onto a private worker thread because
/// libssh is not safe to share between threads, and all channels opened
/// from a session must be operated from the same thread that opened
/// them. Channels are obtained via `SshTunnel` (see ssh_tunnel.h).
///
/// State machine:
///   Disconnected → Connecting → Authenticating → Connected
///                                              \→ NeedsHostKeyTrust
///                                              \→ Failed
///
/// `NeedsHostKeyTrust` is the trust-on-first-use prompt point. The
/// caller surfaces a dialog and calls `trustPendingHostKey()` on
/// accept, which retries the auth step.
class SshSession : public QObject
{
    Q_OBJECT
public:
    enum AuthMode {
        AuthPassword = 0,
        AuthKey      = 1,
    };
    Q_ENUM(AuthMode)

    enum State {
        Disconnected      = 0,
        Connecting        = 1,
        NeedsHostKeyTrust = 2,
        Authenticating    = 3,
        Connected         = 4,
        Failed            = 5,
    };
    Q_ENUM(State)

    struct Params
    {
        QString host;
        quint16 port = 22;
        QString user;
        AuthMode authMode = AuthPassword;
        QString password;             ///< When `authMode == AuthPassword`.
        QString privateKeyPath;       ///< When `authMode == AuthKey`.
        QString privateKeyPassphrase; ///< Optional passphrase for the key.
        QStringList trustedHostKeyFingerprints; ///< Pinned SHA-256 host keys.
        int connectTimeoutMs = 15000;
    };

    explicit SshSession(QObject *parent = nullptr);
    ~SshSession() override;

    /// Open the session. Asynchronous — listen on `stateChanged` /
    /// `errorOccurred` / `hostKeyPromptRequired` for completion.
    void open(Params params);

    /// Close the session. Safe to call from any state.
    void close();

    /// Accept the host key captured during the last
    /// `hostKeyPromptRequired` and continue authenticating. No-op when
    /// no host key is pending.
    void trustPendingHostKey();

    [[nodiscard]] State state() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString pendingHostKeyFingerprint() const;
    [[nodiscard]] QString pendingHostKeyType() const;

    /// Opaque worker handle used by `SshTunnel` to open channels off
    /// the same authenticated session. Returns nullptr until the
    /// session reaches the `Connected` state.
    SshSessionWorker *worker() const;

signals:
    void stateChanged(qumesh::ssh::SshSession::State s);
    void errorOccurred(const QString &error);

    /// Emitted when the server's host key is not in the trusted list.
    /// The session is paused in `NeedsHostKeyTrust`; UI should surface
    /// a confirmation and either call `trustPendingHostKey()` or
    /// `close()`.
    void hostKeyPromptRequired(const QString &fingerprintSha256,
                               const QString &keyType);

    /// Emitted when the server's host key matched a pinned fingerprint.
    void hostKeyVerifiedByPin(const QString &fingerprintSha256);

    /// Emitted on each successful host-key trust acceptance. The caller
    /// is responsible for persisting the new fingerprint to the
    /// per-machine config.
    void hostKeyTrusted(const QString &fingerprintSha256);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace qumesh::ssh
