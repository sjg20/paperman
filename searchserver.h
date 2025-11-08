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
   File:       searchserver.h
   Started:    2025

   This file implements a simple HTTP server that provides search functionality
   for the paper repository. It exposes REST endpoints to search for files/stacks.
*/

#ifndef __searchserver_h
#define __searchserver_h

#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QStringList>
#include <QHash>

/**
 * Simple HTTP server for searching paper repository
 */
class SearchServer : public QTcpServer
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param rootPath  Root path of the paper repository (single path)
     * @param port      Port to listen on (default 8080)
     * @param parent    Parent QObject
     */
    explicit SearchServer(const QString &rootPath, quint16 port = 8080,
                         QObject *parent = nullptr);

    /**
     * Constructor for multiple repositories
     * @param rootPaths List of root paths to paper repositories
     * @param port      Port to listen on (default 8080)
     * @param parent    Parent QObject
     */
    explicit SearchServer(const QStringList &rootPaths, quint16 port = 8080,
                         QObject *parent = nullptr);

    virtual ~SearchServer();

    /**
     * Start the server
     * @return true if started successfully, false otherwise
     */
    bool start();

    /**
     * Stop the server
     */
    void stop();

    /**
     * Get the port the server is listening on
     */
    quint16 port() const { return _port; }

    /**
     * Check if server is running
     */
    bool isRunning() const { return isListening(); }

    /**
     * Get list of repository paths
     */
    QStringList repositories() const { return _rootPaths; }

protected:
    /**
     * Handle incoming connections
     */
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    /**
     * Handle client disconnection
     */
    void onClientDisconnected();

    /**
     * Read data from client
     */
    void onReadyRead();

private:
    /**
     * Parse HTTP request
     * @param request  Raw HTTP request
     * @param method   Returns HTTP method (GET, POST, etc.)
     * @param path     Returns request path
     * @param params   Returns query parameters
     */
    void parseRequest(const QString &request, QString &method,
                     QString &path, QHash<QString, QString> &params);

    /**
     * Handle a request
     * @param method   HTTP method
     * @param path     Request path
     * @param params   Query parameters
     * @return HTTP response
     */
    QString handleRequest(const QString &method, const QString &path,
                         const QHash<QString, QString> &params);

    /**
     * Search for files matching a pattern
     * @param repoPath    Repository root path
     * @param searchPath  Directory to search in (relative to root)
     * @param pattern     Search pattern (partial filename match)
     * @param recursive   Search subdirectories
     * @return JSON response with results
     */
    QString searchFiles(const QString &repoPath, const QString &searchPath,
                       const QString &pattern, bool recursive);

    /**
     * Get file list for a directory
     * @param dirPath  Directory path (relative to root)
     * @return JSON response with file list
     */
    QString listFiles(const QString &dirPath);

    /**
     * Get list of all repositories
     * @return JSON response with repository list
     */
    QString listRepositories();

    /**
     * Get file content
     * @param repoPath   Repository root path
     * @param filePath   File path (relative to repository root)
     * @param type       Output type ("original" or "pdf")
     * @return HTTP response with file content
     */
    QString getFile(const QString &repoPath, const QString &filePath, const QString &type = "original");

    /**
     * Build JSON response
     * @param success  Whether the operation was successful
     * @param data     Data to include in response
     * @param error    Error message (if any)
     * @return JSON string
     */
    QString buildJsonResponse(bool success, const QString &data,
                             const QString &error = QString());

    /**
     * Build HTTP response
     * @param statusCode  HTTP status code
     * @param statusText  HTTP status text
     * @param contentType Content type
     * @param body        Response body
     * @return Full HTTP response
     */
    QString buildHttpResponse(int statusCode, const QString &statusText,
                             const QString &contentType, const QString &body);

    /**
     * Build HTTP response with binary body
     * @param statusCode  HTTP status code
     * @param statusText  HTTP status text
     * @param contentType Content type
     * @param body        Response body (binary)
     * @return Full HTTP response
     */
    QString buildHttpResponse(int statusCode, const QString &statusText,
                             const QString &contentType, const QByteArray &body);

    /**
     * URL decode a string
     */
    QString urlDecode(const QString &str);

private:
    QString _rootPath;      //!< Root path of paper repository (deprecated, use _rootPaths)
    QStringList _rootPaths; //!< List of root paths of paper repositories
    quint16 _port;          //!< Port to listen on
    QList<QTcpSocket*> _clients;  //!< Connected clients
};

#endif // __searchserver_h
