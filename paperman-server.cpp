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

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <iostream>

void printUsage(const char *progName)
{
    std::cout << "Usage: " << progName << " [options] <repository-path>\n"
              << "\n"
              << "Options:\n"
              << "  -p, --port <port>    Port to listen on (default: 8080)\n"
              << "  -C, --no-cache       Skip building file cache at startup\n"
              << "  -h, --help           Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << progName << " -p 9000 /home/user/Documents\n"
              << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Parse command line arguments
    QStringList args = app.arguments();
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
