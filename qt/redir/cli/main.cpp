#include "redir/redir_client.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

using namespace meshcommander::redir;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("meshcommander-redir"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "AMT Redirection transport handshake (TCP only, no auth yet). "
        "Confirms a device accepts a redirection session for sol|kvm|ider."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption hostOpt({QStringLiteral("host"), QStringLiteral("H")},
                               QStringLiteral("AMT device host"),
                               QStringLiteral("host"));
    QCommandLineOption portOpt({QStringLiteral("port"), QStringLiteral("p")},
                               QStringLiteral("AMT redirection port (default 16994)"),
                               QStringLiteral("port"), QStringLiteral("16994"));
    QCommandLineOption protoOpt({QStringLiteral("protocol")},
                                QStringLiteral("sol | kvm | ider (default sol)"),
                                QStringLiteral("protocol"), QStringLiteral("sol"));
    QCommandLineOption userOpt({QStringLiteral("user"), QStringLiteral("u")},
                               QStringLiteral("AMT username (enables digest auth)"),
                               QStringLiteral("user"));
    QCommandLineOption passOpt({QStringLiteral("pass"), QStringLiteral("P")},
                               QStringLiteral("AMT password"),
                               QStringLiteral("pass"));
    parser.addOptions({hostOpt, portOpt, protoOpt, userOpt, passOpt});
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);

    if (!parser.isSet(hostOpt)) {
        err << "--host is required" << Qt::endl;
        return 2;
    }

    Protocol proto = Protocol::Sol;
    const QString protoStr = parser.value(protoOpt).toLower();
    if (protoStr == QStringLiteral("sol")) proto = Protocol::Sol;
    else if (protoStr == QStringLiteral("kvm")) proto = Protocol::Kvm;
    else if (protoStr == QStringLiteral("ider")) proto = Protocol::Ider;
    else {
        err << "unknown protocol: " << protoStr << Qt::endl;
        return 2;
    }

    RedirectionClient client;
    client.setProtocol(proto);
    const bool wantAuth = parser.isSet(userOpt);
    if (wantAuth) {
        client.setCredentials(parser.value(userOpt), parser.value(passOpt));
    }

    QObject::connect(&client, &RedirectionClient::sessionOpened, &app, [&]() {
        out << "Session opened. OEM data length: " << client.oemData().size() << Qt::endl;
        if (!wantAuth) QCoreApplication::exit(0);
    });
    QObject::connect(&client, &RedirectionClient::authenticated, &app, [&]() {
        out << "Authenticated." << Qt::endl;
        QCoreApplication::exit(0);
    });
    QObject::connect(&client, &RedirectionClient::failed, &app,
                     [&](const QString &msg) {
                         err << "redirection failed: " << msg << Qt::endl;
                         QCoreApplication::exit(1);
                     });

    client.connectTo(parser.value(hostOpt), parser.value(portOpt).toUShort());
    return app.exec();
}
