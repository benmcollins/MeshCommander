// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>

class QTcpSocket;

namespace qumesh::wsman {

/// Probe `127.0.0.1:16992` (plain WSMAN) and `127.0.0.1:16993` (TLS)
/// to decide whether Intel's Local Manageability Service is running
/// and proxying the loopback ports to the AMT firmware on the host.
///
/// Result is published on two Q_PROPERTYs so QML can hide / show the
/// "This PC" sidebar entry without re-polling: `available` flips true
/// when either port answers, `tlsAvailable` when 16993 answers
/// specifically (so the QML can pre-fill the TLS toggle on the add
/// prompt).
class LocalAmtProbe : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available    READ available    NOTIFY finished)
    Q_PROPERTY(bool tlsAvailable READ tlsAvailable NOTIFY finished)
    Q_PROPERTY(bool probing      READ probing      NOTIFY probingChanged)
public:
    explicit LocalAmtProbe(QObject *parent = nullptr);
    ~LocalAmtProbe() override;

    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] bool tlsAvailable() const { return m_tlsAvailable; }
    [[nodiscard]] bool probing() const { return m_probing; }

    /// Kick a fresh probe. Safe to call repeatedly; an in-flight
    /// probe is cancelled and a new one started. `finished()` fires
    /// exactly once per `start()` call.
    Q_INVOKABLE void start();

signals:
    void finished();
    void probingChanged();

private:
    enum class Phase { Plain, Tls, Done };
    void kickConnect(quint16 port);
    void onConnected();
    void onError();
    void onTimeout();
    void setProbing(bool p);
    void publishResult();

    QTcpSocket *m_socket = nullptr;
    bool m_available = false;
    bool m_tlsAvailable = false;
    bool m_probing = false;
    Phase m_phase = Phase::Done;
};

} // namespace qumesh::wsman
