// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace qumesh::wsman {
class WsmanClient;
}

namespace qumesh::app {

/// QML-creatable controller for one Machine Details window. Owns a
/// WSMAN client, exposes per-category fetch methods, and surfaces the
/// results as Q_PROPERTYs the QML side binds to. No background polling
/// — every fetch is user-triggered (or kicked off when a page becomes
/// visible).
class MachineDetailsController : public QObject
{
    Q_OBJECT

    // Connection inputs (set by the QML window from the saved machine).
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool tls READ tls WRITE setTls NOTIFY tlsChanged)

    // Overview / power.
    Q_PROPERTY(int powerState READ powerState NOTIFY powerStateChanged)
    Q_PROPERTY(QString powerStateLabel READ powerStateLabel NOTIFY powerStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // Identify.
    Q_PROPERTY(QString amtProtocolVersion READ amtProtocolVersion NOTIFY identifyChanged)
    Q_PROPERTY(QString amtVendor READ amtVendor NOTIFY identifyChanged)
    Q_PROPERTY(QString amtVersion READ amtVersion NOTIFY identifyChanged)

    // General settings.
    Q_PROPERTY(QString hostName READ hostName NOTIFY generalSettingsChanged)
    Q_PROPERTY(QString domainName READ domainName NOTIFY generalSettingsChanged)
    Q_PROPERTY(QString digestRealm READ digestRealm NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool networkInterfaceEnabled READ networkInterfaceEnabled NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool rmcpPingResponseEnabled READ rmcpPingResponseEnabled NOTIFY generalSettingsChanged)

    // Computer system.
    Q_PROPERTY(QString systemName READ systemName NOTIFY computerSystemChanged)
    Q_PROPERTY(QString systemElementName READ systemElementName NOTIFY computerSystemChanged)
    Q_PROPERTY(QString systemUuid READ systemUuid NOTIFY computerSystemChanged)

    // Ethernet settings.
    Q_PROPERTY(QString macAddress READ macAddress NOTIFY ethernetChanged)
    Q_PROPERTY(bool dhcpEnabled READ dhcpEnabled NOTIFY ethernetChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ethernetChanged)
    Q_PROPERTY(QString subnetMask READ subnetMask NOTIFY ethernetChanged)
    Q_PROPERTY(QString defaultGateway READ defaultGateway NOTIFY ethernetChanged)
    Q_PROPERTY(QString primaryDns READ primaryDns NOTIFY ethernetChanged)
    Q_PROPERTY(QString secondaryDns READ secondaryDns NOTIFY ethernetChanged)

    // Time.
    Q_PROPERTY(qint64 amtEpoch READ amtEpoch NOTIFY timeChanged)

public:
    explicit MachineDetailsController(QObject *parent = nullptr);
    ~MachineDetailsController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] bool    tls()  const { return m_tls; }
    void setHost(const QString &v);
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setTls(bool v);

    [[nodiscard]] int powerState() const { return m_powerState; }
    [[nodiscard]] QString powerStateLabel() const;
    [[nodiscard]] bool busy() const { return m_inflight > 0; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

    [[nodiscard]] QString amtProtocolVersion() const { return m_amtProtocolVersion; }
    [[nodiscard]] QString amtVendor() const { return m_amtVendor; }
    [[nodiscard]] QString amtVersion() const { return m_amtVersion; }

    [[nodiscard]] QString hostName() const { return m_hostName; }
    [[nodiscard]] QString domainName() const { return m_domainName; }
    [[nodiscard]] QString digestRealm() const { return m_digestRealm; }
    [[nodiscard]] bool networkInterfaceEnabled() const { return m_networkInterfaceEnabled; }
    [[nodiscard]] bool rmcpPingResponseEnabled() const { return m_rmcpPingResponseEnabled; }

    [[nodiscard]] QString systemName() const { return m_systemName; }
    [[nodiscard]] QString systemElementName() const { return m_systemElementName; }
    [[nodiscard]] QString systemUuid() const { return m_systemUuid; }

    [[nodiscard]] QString macAddress() const { return m_macAddress; }
    [[nodiscard]] bool dhcpEnabled() const { return m_dhcpEnabled; }
    [[nodiscard]] QString ipAddress() const { return m_ipAddress; }
    [[nodiscard]] QString subnetMask() const { return m_subnetMask; }
    [[nodiscard]] QString defaultGateway() const { return m_defaultGateway; }
    [[nodiscard]] QString primaryDns() const { return m_primaryDns; }
    [[nodiscard]] QString secondaryDns() const { return m_secondaryDns; }

    [[nodiscard]] qint64 amtEpoch() const { return m_amtEpoch; }

    /// Fetch the overview bundle (identify + general settings + system +
    /// power state). Each completes independently; the QML side just
    /// reacts to whichever property changes.
    Q_INVOKABLE void refreshOverview();

    /// Refresh individual categories.
    Q_INVOKABLE void refreshNetwork();
    Q_INVOKABLE void refreshTime();
    Q_INVOKABLE void refreshPower();

    /// CIM power-state codes:
    ///   2  = Power On
    ///   5  = Master Bus Reset
    ///   8  = Power Off (Hard)
    ///   10 = Master Bus Reset Graceful
    ///   12 = Power Off (Soft Graceful)
    Q_INVOKABLE void powerOn()           { changePowerState(2); }
    Q_INVOKABLE void powerOffHard()      { changePowerState(8); }
    Q_INVOKABLE void powerReset()        { changePowerState(5); }
    Q_INVOKABLE void powerResetGraceful(){ changePowerState(10); }
    Q_INVOKABLE void powerOffSoft()      { changePowerState(12); }

signals:
    void hostChanged();
    void userChanged();
    void passwordChanged();
    void tlsChanged();
    void busyChanged();
    void lastErrorChanged();
    void powerStateChanged();
    void identifyChanged();
    void generalSettingsChanged();
    void computerSystemChanged();
    void ethernetChanged();
    void timeChanged();
    void powerChangeRequested(int state);
    void powerChangeCompleted(int state, bool ok, const QString &error);

private:
    void rebuildEndpoint();
    void changePowerState(int code);
    void incInflight();
    void decInflight();
    void setLastError(const QString &e);

    qumesh::wsman::WsmanClient *m_client = nullptr;
    QString m_host;
    QString m_user;
    QString m_password;
    bool m_tls = false;

    int m_powerState = 0;
    int m_inflight = 0;
    QString m_lastError;

    QString m_amtProtocolVersion;
    QString m_amtVendor;
    QString m_amtVersion;

    QString m_hostName;
    QString m_domainName;
    QString m_digestRealm;
    bool m_networkInterfaceEnabled = false;
    bool m_rmcpPingResponseEnabled = false;

    QString m_systemName;
    QString m_systemElementName;
    QString m_systemUuid;

    QString m_macAddress;
    bool m_dhcpEnabled = false;
    QString m_ipAddress;
    QString m_subnetMask;
    QString m_defaultGateway;
    QString m_primaryDns;
    QString m_secondaryDns;

    qint64 m_amtEpoch = 0;
};

} // namespace qumesh::app
