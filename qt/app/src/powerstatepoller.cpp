// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "powerstatepoller.h"

#include "wsman/operations.h"
#include "wsman/wsman_client.h"

#include <QTimer>

namespace qumesh::app {

using qumesh::wsman::WsmanClient;

PowerStatePoller::PowerStatePoller(QObject *parent)
    : QObject(parent),
      m_client(new WsmanClient(this)),
      m_timer(new QTimer(this))
{
    m_timer->setInterval(m_intervalMs);
    connect(m_timer, &QTimer::timeout, this, &PowerStatePoller::poll);
}

PowerStatePoller::~PowerStatePoller() = default;

void PowerStatePoller::setHost(const QString &host)
{
    if (host == m_host) return;
    m_host = host;
    rebuildEndpoint();
}

void PowerStatePoller::setPort(quint16 port)
{
    if (port == m_port) return;
    m_port = port;
    rebuildEndpoint();
}

void PowerStatePoller::setTls(bool tls)
{
    if (tls == m_tls) return;
    m_tls = tls;
    rebuildEndpoint();
}

void PowerStatePoller::setCredentials(const QString &user, const QString &pass)
{
    m_user = user;
    m_pass = pass;
    m_client->setCredentials(user, pass);
}

void PowerStatePoller::setIntervalMs(int ms)
{
    if (ms <= 0 || ms == m_intervalMs) return;
    m_intervalMs = ms;
    m_timer->setInterval(ms);
    emit intervalMsChanged();
}

void PowerStatePoller::start()
{
    if (m_host.isEmpty()) {
        setState(State::Unknown);
        return;
    }
    rebuildEndpoint();
    m_client->setCredentials(m_user, m_pass);
    poll();                       // immediate first request
    m_timer->start();
}

void PowerStatePoller::stop()
{
    m_timer->stop();
    setState(State::Unknown);
    setLastError({});
}

void PowerStatePoller::rebuildEndpoint()
{
    if (m_host.isEmpty()) return;
    QUrl url;
    url.setScheme(m_tls ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(QStringLiteral("/wsman"));
    m_client->setEndpoint(url);
}

void PowerStatePoller::poll()
{
    qumesh::wsman::getPowerState(m_client,
        [this](const qumesh::wsman::PowerStateResult &r) {
            if (!r.ok) {
                setLastError(r.error);
                setState(State::Unreachable);
                return;
            }
            setLastError({});
            applyCimState(r.powerState);
        });
}

void PowerStatePoller::applyCimState(int cimPowerState)
{
    // CIM_PowerState mapping. Anything we don't recognize gets
    // bucketed as Unknown rather than Unreachable since the device
    // did respond.
    switch (cimPowerState) {
    case 2:  setState(State::On); break;
    case 3:  setState(State::Standby); break; // Sleep — Light
    case 4:  setState(State::Standby); break; // Sleep — Deep
    case 6:                                   // Off — Soft
    case 8:  setState(State::Off); break;     // Off — Hard
    case 7:  setState(State::Hibernate); break;
    case 13: setState(State::Standby); break;
    case 14: setState(State::Standby); break;
    default: setState(State::Unknown); break;
    }
}

void PowerStatePoller::setState(State s)
{
    if (s == m_state) return;
    m_state = s;
    emit stateChanged(s);
}

void PowerStatePoller::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

} // namespace qumesh::app
