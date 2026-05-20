// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "ssh/ssh_session.h"

#include <QMutex>
#include <QObject>
#include <QString>

#include <libssh/libssh.h>

#include <atomic>
#include <functional>

namespace qumesh::ssh {

/// Internal worker owned by `SshSession`. Lives on a private QThread so
/// libssh is only ever entered from a single thread. Exposed here only
/// so `SshTunnel` can post `openForwardChannel` calls onto it.
class SshSessionWorker : public QObject
{
    Q_OBJECT
public:
    explicit SshSessionWorker(QObject *parent = nullptr);
    ~SshSessionWorker() override;

    ssh_session rawSession() const { return m_session; }

    /// Cancel-flag for the non-blocking libssh poll loop. Safe to call
    /// from any thread (it's a plain atomic). `~SshSession` flips this
    /// before posting `close()` so any in-flight `ssh_connect` /
    /// `ssh_userauth_*` / `ssh_channel_open_forward` exits at its next
    /// poll tick (~50 ms cap) instead of running to libssh's full
    /// timeout. See #233.
    void requestStop() { m_stopRequested.store(true, std::memory_order_release); }

    /// Snapshot of pending host-key state for `SshSession`'s public API.
    QString pendingFingerprint() const;
    QString pendingKeyType() const;

    /// Open a direct-tcpip channel. Must be invoked on the worker's
    /// thread (e.g. via `QMetaObject::invokeMethod`).
    ssh_channel openForwardChannel(const QString &remoteHost, quint16 remotePort);

signals:
    void stateChanged(qumesh::ssh::SshSession::State s);
    void errorOccurred(const QString &error);
    void hostKeyPromptRequired(const QString &fp, const QString &keyType);
    void hostKeyVerifiedByPin(const QString &fp);
    void hostKeyTrusted(const QString &fp);

public slots:
    void open(qumesh::ssh::SshSession::Params p);
    void resumeAfterHostKeyTrust();
    void close();

private:
    void fail(const QString &msg);
    bool verifyHostKey();
    void proceedWithAuth();

    /// Drive a libssh non-blocking call until it returns something
    /// other than `SSH_AGAIN` / `SSH_AUTH_AGAIN`, or until
    /// `m_stopRequested` flips, or `timeoutMs` elapses. Polls the libssh
    /// session fd in the direction(s) libssh requests
    /// (`ssh_get_poll_flags`) with a 50 ms tick so cancellation latency
    /// is bounded. `timeoutMs <= 0` falls back to
    /// `m_params.connectTimeoutMs`. Returns the libssh rc, `SSH_ABORTED`
    /// when cancellation won, or `SSH_ERROR` on timeout.
    ///
    /// We enforce our own deadline because libssh honours
    /// `SSH_OPTIONS_TIMEOUT` only in blocking mode; non-blocking mode
    /// (which `~SshSession` requires for cancellability — see #233)
    /// leaves cancellation to the caller.
    static constexpr int SSH_ABORTED = -100; // distinct from SSH_AGAIN(-2)/SSH_ERROR(-1)
    int waitForLibssh(const std::function<int()> &call, int timeoutMs = 0);

    ssh_session m_session = nullptr;
    SshSession::Params m_params;
    bool m_awaitingHostKeyTrust = false;
    QString m_pendingHostKeyFingerprint;
    QString m_pendingHostKeyType;
    /// Narrow mutex around `m_pendingHostKeyFingerprint` and
    /// `m_pendingHostKeyType` only — the rest of the worker's state
    /// is worker-thread-local. `pendingFingerprint()` /
    /// `pendingKeyType()` are direct method calls from the UI thread
    /// and need synchronization to coexist with the worker's writes
    /// in `verifyHostKey()`. Pre-#272 a coarser `m_sessionMutex`
    /// covered the entire `open()` body — including the multi-second
    /// libssh poll loop — and `pendingFingerprint()` from QML would
    /// block until the connect attempt finished. See #272.
    mutable QMutex m_pendingHostKeyMutex;
    std::atomic<bool> m_stopRequested{false};
};

} // namespace qumesh::ssh
