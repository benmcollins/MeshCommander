// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/local_amt_probe.h"

#include <QPointer>
#include <QTcpSocket>
#include <QTimer>

namespace qumesh::wsman {

namespace {
constexpr int kConnectTimeoutMs = 500;
constexpr quint16 kPlainPort = 16992;
constexpr quint16 kTlsPort   = 16993;
} // namespace

LocalAmtProbe::LocalAmtProbe(QObject *parent)
    : QObject(parent)
{
}

LocalAmtProbe::~LocalAmtProbe() = default;

void LocalAmtProbe::start()
{
    // Cancel anything in flight. Re-arm both flags to false so the
    // QML rebinds — re-probing means "the last answer is now stale".
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_available = false;
    m_tlsAvailable = false;
    setProbing(true);
    m_phase = Phase::Plain;
    kickConnect(kPlainPort);
}

void LocalAmtProbe::kickConnect(quint16 port)
{
    m_socket = new QTcpSocket(this);
    // QTcpSocket emits errorOccurred (not error/) on connect failures;
    // both connected and errorOccurred bring us out of the wait state.
    connect(m_socket, &QTcpSocket::connected, this, &LocalAmtProbe::onConnected);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &LocalAmtProbe::onError);
    // Belt-and-braces timeout — on an unreachable port the kernel
    // takes a few seconds to give up. Cap to a UX-tolerable window.
    //
    // Capture the socket pointer explicitly + check identity in the
    // lambda. If start() is called again before the timer fires, the
    // old socket is deleteLater()'d (queued) and m_socket is
    // reassigned to a new socket. Without the identity check the
    // timer would test the *new* socket's state and could fire
    // onTimeout against the second probe phase — collapsing two
    // probes into one. See #286.
    QPointer<QAbstractSocket> sock = m_socket;
    QTimer::singleShot(kConnectTimeoutMs, this, [this, sock]() {
        if (sock && m_socket == sock
            && sock->state() != QAbstractSocket::ConnectedState) {
            onTimeout();
        }
    });
    m_socket->connectToHost(QStringLiteral("127.0.0.1"), port);
}

void LocalAmtProbe::onConnected()
{
    // LMS proxied the loopback port through to AMT. Record the hit
    // (and TLS capability) and either chase the second port or
    // finish.
    if (m_phase == Phase::Plain) {
        m_available = true;
        // Tear down and chase the TLS port too — the UI's pre-checked
        // TLS toggle wants to know.
        if (m_socket) {
            m_socket->abort();
            m_socket->deleteLater();
            m_socket = nullptr;
        }
        m_phase = Phase::Tls;
        kickConnect(kTlsPort);
        return;
    }
    if (m_phase == Phase::Tls) {
        m_available = true;
        m_tlsAvailable = true;
        m_phase = Phase::Done;
        publishResult();
        return;
    }
}

void LocalAmtProbe::onError()
{
    // Connection failed (refused / unreachable / etc.). Move on.
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_phase == Phase::Plain) {
        // Try TLS — some hardened LMS configs hide 16992 but expose
        // 16993.
        m_phase = Phase::Tls;
        kickConnect(kTlsPort);
        return;
    }
    if (m_phase == Phase::Tls) {
        m_phase = Phase::Done;
        publishResult();
    }
}

void LocalAmtProbe::onTimeout()
{
    onError();
}

void LocalAmtProbe::publishResult()
{
    setProbing(false);
    emit finished();
}

void LocalAmtProbe::setProbing(bool p)
{
    if (m_probing == p)
        return;
    m_probing = p;
    emit probingChanged();
}

} // namespace qumesh::wsman
