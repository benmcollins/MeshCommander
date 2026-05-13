// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "solcontroller.h"

#include "redir/redir_client.h"
#include "redir/redir_codec.h"
#include "redir/sol_session.h"

namespace meshcommander::app {

using meshcommander::redir::RedirectionClient;
using meshcommander::redir::SolSession;
using meshcommander::terminal::TerminalScreen;

SolController::SolController(QObject *parent)
    : QObject(parent), m_screen(new TerminalScreen(this))
{
}

SolController::~SolController() { teardown(); }

void SolController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
}

void SolController::setPort(quint16 v)
{
    if (v == m_port) return;
    m_port = v;
    emit portChanged();
}

void SolController::setUser(const QString &v)
{
    if (v == m_user) return;
    m_user = v;
    emit userChanged();
}

void SolController::setPassword(const QString &v)
{
    if (v == m_password) return;
    m_password = v;
    emit passwordChanged();
}

void SolController::setState(State s)
{
    if (s == m_state) return;
    m_state = s;
    emit stateChanged();
}

void SolController::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void SolController::open()
{
    teardown();
    if (m_host.isEmpty()) {
        setLastError(tr("host is empty"));
        setState(State::Failed);
        return;
    }
    m_screen->clear();

    m_client = new RedirectionClient(this);
    m_client->setProtocol(meshcommander::redir::Protocol::Sol);
    m_client->setCredentials(m_user, m_password);

    m_session = new SolSession(m_client, this);

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
            m_session.data(), &SolSession::start);

    connect(m_client.data(), &RedirectionClient::failed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    connect(m_session.data(), &SolSession::sessionOpened, this, [this]() {
        setState(State::Connected);
    });
    connect(m_session.data(), &SolSession::data, this, [this](const QByteArray &bytes) {
        m_screen->feed(bytes);
    });
    connect(m_session.data(), &SolSession::closed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    setLastError({});
    setState(State::Connecting);
    m_client->connectTo(m_host, m_port);
}

void SolController::close()
{
    teardown();
    setState(State::Disconnected);
}

void SolController::sendText(const QString &text)
{
    sendBytes(text.toUtf8());
}

void SolController::sendBytes(const QByteArray &bytes)
{
    if (!m_session) return;
    m_session->sendInput(bytes);
}

void SolController::teardown()
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
