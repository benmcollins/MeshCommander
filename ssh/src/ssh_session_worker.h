// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "ssh/ssh_session.h"

#include <QMutex>
#include <QObject>
#include <QString>

#include <libssh/libssh.h>

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
    QMutex &sessionMutex() { return m_sessionMutex; }

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

    ssh_session m_session = nullptr;
    QMutex m_sessionMutex;
    SshSession::Params m_params;
    bool m_awaitingHostKeyTrust = false;
    QString m_pendingHostKeyFingerprint;
    QString m_pendingHostKeyType;
};

} // namespace qumesh::ssh
