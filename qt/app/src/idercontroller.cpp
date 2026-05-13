// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "idercontroller.h"

#include "ider/ider_session.h"
#include "redir/redir_client.h"
#include "redir/redir_codec.h"

namespace meshcommander::app {

using meshcommander::ider::IderSession;
using meshcommander::redir::RedirectionClient;

IderController::IderController(QObject *parent) : QObject(parent) {}
IderController::~IderController() { teardown(); }

void IderController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
}

void IderController::setPort(quint16 v)
{
    if (v == m_port) return;
    m_port = v;
    emit portChanged();
}

void IderController::setUser(const QString &v)
{
    if (v == m_user) return;
    m_user = v;
    emit userChanged();
}

void IderController::setPassword(const QString &v)
{
    if (v == m_password) return;
    m_password = v;
    emit passwordChanged();
}

void IderController::setIsoPath(const QString &v)
{
    if (v == m_isoPath) return;
    m_isoPath = v;
    emit isoPathChanged();
}

void IderController::setStartOption(StartOption v)
{
    if (v == m_startOption) return;
    m_startOption = v;
    emit startOptionChanged();
}

void IderController::setState(State s)
{
    if (s == m_state) return;
    m_state = s;
    emit stateChanged();
}

void IderController::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void IderController::open()
{
    teardown();
    if (m_host.isEmpty()) {
        setLastError(tr("host is empty"));
        setState(State::Failed);
        return;
    }
    if (m_isoPath.isEmpty()) {
        setLastError(tr("ISO path is required"));
        setState(State::Failed);
        return;
    }

    m_bytesSentToAmt = 0;
    m_bytesReceivedFromAmt = 0;
    emit statsChanged();
    m_deviceEnabled = false;
    emit deviceEnabledChanged();

    m_client = new RedirectionClient(this);
    m_client->setProtocol(meshcommander::redir::Protocol::Ider);
    m_client->setCredentials(m_user, m_password);

    m_session = new IderSession(m_client, this);
    m_session->setIsoPath(m_isoPath);

    using IderStart = IderSession::StartOption;
    switch (m_startOption) {
    case StartOption::OnReboot:  m_session->setStartOption(IderStart::OnReboot); break;
    case StartOption::Graceful:  m_session->setStartOption(IderStart::Graceful); break;
    case StartOption::Immediate: m_session->setStartOption(IderStart::Immediate); break;
    }

    connect(m_client.data(), &RedirectionClient::stateChanged, this, [this]() {
        if (!m_client) return;
        switch (m_client->state()) {
        case RedirectionClient::State::Disconnected:
            setState(State::Disconnected);
            break;
        case RedirectionClient::State::Connecting:
            setState(State::Connecting);
            break;
        case RedirectionClient::State::SelectorSent:
        case RedirectionClient::State::SessionOpened:
        case RedirectionClient::State::AuthQuerying:
        case RedirectionClient::State::AuthChallenging:
        case RedirectionClient::State::AuthResponding:
            setState(State::Authenticating);
            break;
        case RedirectionClient::State::Authenticated:
            setState(State::Opening);
            break;
        case RedirectionClient::State::Failed:
            setState(State::Failed);
            break;
        }
    });

    connect(m_client.data(), &RedirectionClient::authenticated,
            m_session.data(), &IderSession::start);

    connect(m_client.data(), &RedirectionClient::failed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    connect(m_session.data(), &IderSession::sessionOpened, this,
            [this](const meshcommander::ider::SessionInfo &) {
                setState(State::Running);
            });
    connect(m_session.data(), &IderSession::enabledChanged, this, [this](bool en) {
        if (en == m_deviceEnabled) return;
        m_deviceEnabled = en;
        emit deviceEnabledChanged();
    });
    connect(m_session.data(), &IderSession::statsChanged, this,
            [this](quint64 to, quint64 from) {
                m_bytesSentToAmt = to;
                m_bytesReceivedFromAmt = from;
                emit statsChanged();
            });
    connect(m_session.data(), &IderSession::closed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    setLastError({});
    setState(State::Connecting);
    m_client->connectTo(m_host, m_port);
}

void IderController::close()
{
    teardown();
    setState(State::Disconnected);
}

void IderController::teardown()
{
    if (m_session) {
        m_session->disconnect(this);
        m_session->deleteLater();
        m_session = nullptr;
    }
    if (m_client) {
        m_client->disconnect(this);
        m_client->deleteLater();
        m_client = nullptr;
    }
}

} // namespace meshcommander::app
