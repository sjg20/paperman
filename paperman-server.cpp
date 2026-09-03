/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/
/*
   Project:    Paperman
   File:       paperman-server.cpp

   Main entry point for the Paperman search server.
*/

#include "builddate.h"
#include "config.h"
#include "searchserver.h"
#include "userstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <cstdio>
#include <cstring>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#else
#include <termios.h>
#include <unistd.h>
#endif

void printUsage(const char *progName)
{
    std::cout << "Usage: " << progName << " [options] <repository-path>\n"
              << "       " << progName << " useradd <name>\n"
              << "       " << progName << " passwd <name>\n"
              << "       " << progName << " userdel <name>\n"
              << "       " << progName << " usermod <name> [--repos R1,R2,..|all]\n"
              << "       " << progName << " userlist\n"
              << "\n"
              << "Options:\n"
              << "  -p, --port <port>    Port to listen on (default: 8080)\n"
              << "  -C, --no-cache       Skip building file cache at startup\n"
              << "  -h, --help           Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << progName << " -p 9000 /home/user/Documents\n"
              << "  " << progName << " useradd alice\n"
              << std::endl;
}


/* Read a line from stdin with terminal echo disabled (interactive
 * password entry).  Returns the line without the trailing newline.
 * Empty on EOF. */
static QString readPasswordHidden(const QString &prompt)
{
    std::cout << prompt.toStdString() << std::flush;

    bool echoToggled = false;
#ifdef Q_OS_WIN
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD oldMode = 0;
    if (isatty(fileno(stdin)) && GetConsoleMode(hin, &oldMode)) {
        if (SetConsoleMode(hin, oldMode & ~ENABLE_ECHO_INPUT))
            echoToggled = true;
    }
#else
    termios oldt;
    if (isatty(fileno(stdin)) && tcgetattr(fileno(stdin), &oldt) == 0) {
        termios newt = oldt;
        newt.c_lflag &= ~(tcflag_t)ECHO;
        if (tcsetattr(fileno(stdin), TCSANOW, &newt) == 0)
            echoToggled = true;
    }
#endif

    char buf[256];
    QString line;
    if (fgets(buf, sizeof(buf), stdin)) {
        line = QString::fromLocal8Bit(buf);
        if (line.endsWith('\n'))
            line.chop(1);
    }

    if (echoToggled) {
#ifdef Q_OS_WIN
        SetConsoleMode(hin, oldMode);
#else
        tcsetattr(fileno(stdin), TCSANOW, &oldt);
#endif
        std::cout << "\n";
    }
    return line;
}


/* Prompt twice and confirm.  Empty input or mismatch aborts. */
static bool promptNewPassword(QString &out)
{
    QString first = readPasswordHidden("New password: ");
    if (first.isEmpty()) {
        std::cerr << "Aborted: empty password\n";
        return false;
    }
    QString second = readPasswordHidden("Confirm password: ");
    if (first != second) {
        std::cerr << "Aborted: passwords do not match\n";
        return false;
    }
    out = first;
    return true;
}


static int cmdUserAdd(const QString &name)
{
    UserStore store;
    if (store.hasUser(name)) {
        std::cerr << "User '" << name.toStdString() << "' already exists\n";
        return 1;
    }
    QString password;
    if (!promptNewPassword(password))
        return 1;
    if (!store.addUser(name, password)) {
        std::cerr << "Failed to add user\n";
        return 1;
    }
    std::cout << "Added user '" << name.toStdString() << "' to "
              << store.filePath().toStdString() << "\n";
    return 0;
}


static int cmdPasswd(const QString &name)
{
    UserStore store;
    if (!store.hasUser(name)) {
        std::cerr << "User '" << name.toStdString() << "' not found\n";
        return 1;
    }
    QString password;
    if (!promptNewPassword(password))
        return 1;
    if (!store.setPassword(name, password)) {
        std::cerr << "Failed to set password\n";
        return 1;
    }
    std::cout << "Password updated for '" << name.toStdString() << "'\n";
    return 0;
}


static int cmdUserDel(const QString &name)
{
    UserStore store;
    if (!store.delUser(name)) {
        std::cerr << "User '" << name.toStdString() << "' not found\n";
        return 1;
    }
    std::cout << "Removed user '" << name.toStdString() << "'\n";
    return 0;
}


static int cmdUserList()
{
    UserStore store;
    for (const QString &name : store.userNames()) {
        const UserStore::User *u = store.lookup(name);
        QString repos = u->repos.isEmpty()
                            ? QStringLiteral("(all)")
                            : u->repos.join(',');
        std::cout << name.toStdString() << "\trepos="
                  << repos.toStdString() << "\n";
    }
    return 0;
}


static int cmdUserMod(const QString &name, const QStringList &extraArgs)
{
    UserStore store;
    if (!store.hasUser(name)) {
        std::cerr << "User '" << name.toStdString() << "' not found\n";
        return 1;
    }
    for (int i = 0; i < extraArgs.size(); i++) {
        if (extraArgs[i] == "--repos" && i + 1 < extraArgs.size()) {
            QString value = extraArgs[++i];
            QStringList repos;
            if (value != "all")
                repos = value.split(',', Qt::SkipEmptyParts);
            if (!store.setRepos(name, repos)) {
                std::cerr << "Failed to update repos\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown usermod option: "
                      << extraArgs[i].toStdString() << "\n";
            return 1;
        }
    }
    std::cout << "Updated user '" << name.toStdString() << "'\n";
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Parse command line arguments
    QStringList args = app.arguments();

    // User-management subcommands run and exit without starting the
    // server.  They accept no port/repository options.
    if (args.size() >= 2) {
        QString sub = args[1];
        if (sub == "useradd" || sub == "passwd" || sub == "userdel"
            || sub == "usermod" || sub == "userlist") {
            if (sub == "userlist")
                return cmdUserList();
            if (args.size() < 3) {
                std::cerr << "Error: " << sub.toStdString()
                          << " requires a username\n";
                return 1;
            }
            QString name = args[2];
            if (sub == "useradd")  return cmdUserAdd(name);
            if (sub == "passwd")   return cmdPasswd(name);
            if (sub == "userdel")  return cmdUserDel(name);
            if (sub == "usermod")  return cmdUserMod(name, args.mid(3));
        }
    }

    QString repositoryPath;
    quint16 port = 8080;
    bool skipCache = false;

    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "-h" || args[i] == "--help") {
            printUsage(args[0].toUtf8().constData());
            return 0;
        }
        else if (args[i] == "-p" || args[i] == "--port") {
            if (i + 1 < args.size()) {
                bool ok;
                port = args[++i].toUShort(&ok);
                if (!ok || port == 0) {
                    std::cerr << "Error: Invalid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires an argument" << std::endl;
                return 1;
            }
        }
        else if (args[i] == "-C" || args[i] == "--no-cache") {
            skipCache = true;
        }
        else if (!args[i].startsWith('-')) {
            repositoryPath = args[i];
        }
        else {
            std::cerr << "Error: Unknown option: " << args[i].toStdString() << std::endl;
            printUsage(args[0].toUtf8().constData());
            return 1;
        }
    }

    // Validate repository path
    if (repositoryPath.isEmpty()) {
        std::cerr << "Error: Repository path is required" << std::endl;
        printUsage(args[0].toUtf8().constData());
        return 1;
    }

    QDir repoDir(repositoryPath);
    if (!repoDir.exists()) {
        std::cerr << "Error: Repository path does not exist: "
                  << repositoryPath.toStdString() << std::endl;
        return 1;
    }

    repositoryPath = repoDir.absolutePath();

    // Create and start server
    std::cout << "Starting Paperman Search Server v" CONFIG_version_str
              << " (built " SERVER_BUILD_DATE ")" << std::endl;
    std::cout << "Repository: " << repositoryPath.toStdString() << std::endl;
    std::cout << "Port: " << port << std::endl;

    SearchServer server(repositoryPath, port, nullptr, skipCache);
    if (!server.start()) {
        std::cerr << "Error: Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "\nServer is running!" << std::endl;
    std::cout << "Available endpoints:" << std::endl;
    std::cout << "  GET /status                      - Server status" << std::endl;
    std::cout << "  GET /repos                       - List all repositories" << std::endl;
    std::cout << "  GET /search?q=<pattern>          - Search for files" << std::endl;
    std::cout << "  GET /search?q=<pattern>&repo=<name>&path=<dir>&recursive=true" << std::endl;
    std::cout << "  GET /list?path=<dir>             - List files in directory" << std::endl;
    std::cout << "  GET /file?path=<file>&repo=<name>&type=<original|pdf> - Retrieve file content" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;

    return app.exec();
}
