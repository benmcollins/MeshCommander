// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/redir_client.h"
#include "redir/sol_session.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QSocketNotifier>
#include <QTextStream>

using namespace qumesh::redir;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qumesh-redir"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "AMT Redirection transport CLI. Without --user runs the bare TCP "
        "handshake; with --user/--pass completes digest auth; with --sol "
        "drops into an interactive SOL terminal session."));
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
    QCommandLineOption solOpt({QStringLiteral("sol")},
                              QStringLiteral("After auth, run an interactive SOL session "
                                             "(stdin → AMT, AMT → stdout)."));
    parser.addOptions({hostOpt, portOpt, protoOpt, userOpt, passOpt, solOpt});
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
    const bool wantSol = parser.isSet(solOpt);
    if (wantAuth) {
        client.setCredentials(parser.value(userOpt), parser.value(passOpt));
    }

    SolSession sol(&client);

    QObject::connect(&client, &RedirectionClient::sessionOpened, &app, [&]() {
        out << "Session opened. OEM data length: " << client.oemData().size() << Qt::endl;
        if (!wantAuth) QCoreApplication::exit(0);
    });
    QObject::connect(&client, &RedirectionClient::authenticated, &app, [&]() {
        out << "Authenticated." << Qt::endl;
        if (wantSol) {
            sol.start();
        } else {
            QCoreApplication::exit(0);
        }
    });
    QObject::connect(&client, &RedirectionClient::failed, &app,
                     [&](const QString &msg) {
                         err << "redirection failed: " << msg << Qt::endl;
                         QCoreApplication::exit(1);
                     });

    if (wantSol) {
        QObject::connect(&sol, &SolSession::sessionOpened, &app, [&]() {
            err << "SOL session open. ^C to exit." << Qt::endl;
        });
        QObject::connect(&sol, &SolSession::data, &app, [&](const QByteArray &b) {
            QFile stdoutFile;
            stdoutFile.open(stdout, QIODevice::WriteOnly);
            stdoutFile.write(b);
            stdoutFile.flush();
        });
        QObject::connect(&sol, &SolSession::closed, &app, [&](const QString &reason) {
            err << "SOL closed: " << reason << Qt::endl;
            QCoreApplication::exit(1);
        });

        // Forward stdin to the SOL session line-buffered. QSocketNotifier on
        // fd 0 works on macOS/Linux; Windows users typing into the CLI will
        // need a separate path (out of scope for this PR).
        auto *stdinNotifier = new QSocketNotifier(0, QSocketNotifier::Read, &app);
        QObject::connect(stdinNotifier, &QSocketNotifier::activated, &app, [&]() {
            QFile stdinFile;
            if (stdinFile.open(stdin, QIODevice::ReadOnly)) {
                const QByteArray chunk = stdinFile.read(4096);
                if (!chunk.isEmpty()) sol.sendInput(chunk);
            }
        });
    }

    client.connectTo(parser.value(hostOpt), parser.value(portOpt).toUShort());
    return app.exec();
}
