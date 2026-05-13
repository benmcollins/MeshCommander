#include "wsman/operations.h"
#include "wsman/wsman_client.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QUrl>

using namespace meshcommander::wsman;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("meshcommander-wsman"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "WSMAN client for Intel AMT devices. Useful for connection sanity checks "
        "and bug reports."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("operation"),
                                 QStringLiteral("identify | power-state"));
    QCommandLineOption hostOpt({QStringLiteral("host"), QStringLiteral("H")},
                               QStringLiteral("AMT device host"),
                               QStringLiteral("host"));
    QCommandLineOption portOpt({QStringLiteral("port"), QStringLiteral("p")},
                               QStringLiteral("AMT WSMAN port (default 16992 or 16993 with --tls)"),
                               QStringLiteral("port"));
    QCommandLineOption tlsOpt({QStringLiteral("tls")},
                              QStringLiteral("Use HTTPS (port 16993)"));
    QCommandLineOption userOpt({QStringLiteral("user"), QStringLiteral("u")},
                               QStringLiteral("AMT user (typically 'admin')"),
                               QStringLiteral("user"));
    QCommandLineOption passOpt({QStringLiteral("pass"), QStringLiteral("P")},
                               QStringLiteral("AMT password"), QStringLiteral("pass"));
    parser.addOptions({hostOpt, portOpt, tlsOpt, userOpt, passOpt});
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (positional.isEmpty()) {
        err << "missing operation; one of: identify, power-state" << Qt::endl;
        return 2;
    }
    if (!parser.isSet(hostOpt)) {
        err << "--host is required" << Qt::endl;
        return 2;
    }

    const bool tls = parser.isSet(tlsOpt);
    const int port = parser.isSet(portOpt) ? parser.value(portOpt).toInt() : (tls ? 16993 : 16992);
    QUrl url;
    url.setScheme(tls ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(parser.value(hostOpt));
    url.setPort(port);
    url.setPath(QStringLiteral("/wsman"));

    WsmanClient client;
    client.setEndpoint(url);
    if (parser.isSet(userOpt)) {
        client.setCredentials(parser.value(userOpt), parser.value(passOpt));
    }

    const QString op = positional.first();
    if (op == QStringLiteral("identify")) {
        identify(&client, [&](IdentifyResult r) {
            if (!r.ok) {
                err << "identify failed: " << r.error << Qt::endl;
                QCoreApplication::exit(1);
                return;
            }
            out << "Vendor: " << r.productVendor << Qt::endl;
            out << "Version: " << r.productVersion << Qt::endl;
            out << "Protocol: " << r.protocolVersion << Qt::endl;
            QCoreApplication::exit(0);
        });
    } else if (op == QStringLiteral("power-state")) {
        getPowerState(&client, [&](PowerStateResult r) {
            if (!r.ok) {
                err << "power-state failed: " << r.error << Qt::endl;
                QCoreApplication::exit(1);
                return;
            }
            out << "PowerState: " << r.powerState << Qt::endl;
            QCoreApplication::exit(0);
        });
    } else {
        err << "unknown operation: " << op << Qt::endl;
        return 2;
    }

    return app.exec();
}
