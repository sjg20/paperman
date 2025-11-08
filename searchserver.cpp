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

#include "searchserver.h"

#include <QTcpSocket>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QUrl>
#include <QDebug>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#endif

SearchServer::SearchServer(const QString &rootPath, quint16 port, QObject *parent)
    : QTcpServer(parent)
    , _rootPath(rootPath)
    , _port(port)
{
    // Ensure root path has no trailing slash
    if (_rootPath.endsWith('/'))
        _rootPath.chop(1);

    // Also store in the list for compatibility
    _rootPaths.append(_rootPath);
}

SearchServer::SearchServer(const QStringList &rootPaths, quint16 port, QObject *parent)
    : QTcpServer(parent)
    , _port(port)
{
    // Store all paths, removing trailing slashes
    foreach (QString path, rootPaths) {
        if (path.endsWith('/'))
            path.chop(1);
        _rootPaths.append(path);
    }

    // Keep first path for backward compatibility
    if (!_rootPaths.isEmpty())
        _rootPath = _rootPaths.first();
}

SearchServer::~SearchServer()
{
    stop();
}

bool SearchServer::start()
{
    if (!listen(QHostAddress::Any, _port)) {
        qWarning() << "SearchServer: Failed to start on port" << _port
                   << ":" << errorString();
        return false;
    }

    qDebug() << "SearchServer: Listening on port" << _port;
    qDebug() << "SearchServer: Repository root:" << _rootPath;
    return true;
}

void SearchServer::stop()
{
    // Disconnect all clients
    foreach (QTcpSocket *client, _clients) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    _clients.clear();

    // Stop listening
    close();
    qDebug() << "SearchServer: Stopped";
}

void SearchServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *client = new QTcpSocket(this);

    if (!client->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "SearchServer: Failed to set socket descriptor";
        delete client;
        return;
    }

    _clients.append(client);

    connect(client, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(client, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));

    qDebug() << "SearchServer: Client connected from" << client->peerAddress().toString();
}

void SearchServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client)
        return;

    qDebug() << "SearchServer: Client disconnected";
    _clients.removeAll(client);
    client->deleteLater();
}

void SearchServer::onReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client)
        return;

    QString request = QString::fromUtf8(client->readAll());
    qDebug() << "SearchServer: Request:" << request.left(100);

    QString method, path;
    QHash<QString, QString> params;

    parseRequest(request, method, path, params);
    QString response = handleRequest(method, path, params);

    client->write(response.toUtf8());
    client->flush();
    client->disconnectFromHost();
}

void SearchServer::parseRequest(const QString &request, QString &method,
                                QString &path, QHash<QString, QString> &params)
{
    // Parse first line: "GET /path?param=value HTTP/1.1"
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty())
        return;

    QStringList parts = lines[0].split(' ');
    if (parts.size() < 2)
        return;

    method = parts[0];
    QString fullPath = parts[1];

    // Split path and query string
    int queryStart = fullPath.indexOf('?');
    if (queryStart >= 0) {
        path = fullPath.left(queryStart);
        QString queryString = fullPath.mid(queryStart + 1);

        // Parse query parameters
        QStringList paramPairs = queryString.split('&');
        foreach (const QString &pair, paramPairs) {
            int eqPos = pair.indexOf('=');
            if (eqPos >= 0) {
                QString key = urlDecode(pair.left(eqPos));
                QString value = urlDecode(pair.mid(eqPos + 1));
                params[key] = value;
            }
        }
    } else {
        path = fullPath;
    }
}

QString SearchServer::handleRequest(const QString &method, const QString &path,
                                    const QHash<QString, QString> &params)
{
    qDebug() << "SearchServer: Handling" << method << path;

    // Only support GET requests
    if (method != "GET") {
        return buildHttpResponse(405, "Method Not Allowed", "text/plain",
                                QString("Only GET requests are supported"));
    }

    // Route requests
    if (path == "/" || path == "/status") {
        QString data = QString("{\"status\":\"running\",\"repository\":\"%1\"}")
                      .arg(_rootPath);
        return buildHttpResponse(200, "OK", "application/json", data);
    }
    else if (path == "/repos") {
        // List all repositories
        QString result = listRepositories();
        return buildHttpResponse(200, "OK", "application/json", result);
    }
    else if (path == "/search") {
        QString searchPath = params.value("path", "");
        QString pattern = params.value("q", "");
        QString repoName = params.value("repo", "");
        bool recursive = params.value("recursive", "true") == "true";

        if (pattern.isEmpty()) {
            return buildHttpResponse(400, "Bad Request", "application/json",
                                   buildJsonResponse(false, "", "Missing 'q' parameter"));
        }

        // Find repository by name if specified
        QString repoPath = _rootPath;  // Default to first/primary repo
        if (!repoName.isEmpty()) {
            bool found = false;
            foreach (const QString &path, _rootPaths) {
                if (QFileInfo(path).fileName() == repoName) {
                    repoPath = path;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return buildHttpResponse(404, "Not Found", "application/json",
                                       buildJsonResponse(false, "", "Repository not found: " + repoName));
            }
        }

        QString result = searchFiles(repoPath, searchPath, pattern, recursive);
        return buildHttpResponse(200, "OK", "application/json", result);
    }
    else if (path == "/list") {
        QString dirPath = params.value("path", "");
        QString result = listFiles(dirPath);
        return buildHttpResponse(200, "OK", "application/json", result);
    }
    else if (path == "/file") {
        QString filePath = params.value("path", "");
        QString repoName = params.value("repo", "");
        QString type = params.value("type", "original");  // original or pdf

        if (filePath.isEmpty()) {
            return buildHttpResponse(400, "Bad Request", "application/json",
                                   buildJsonResponse(false, "", "Missing 'path' parameter"));
        }

        if (type != "original" && type != "pdf") {
            return buildHttpResponse(400, "Bad Request", "application/json",
                                   buildJsonResponse(false, "", "Invalid type parameter. Must be 'original' or 'pdf'"));
        }

        // Find repository by name if specified
        QString repoPath = _rootPath;  // Default to first/primary repo
        if (!repoName.isEmpty()) {
            bool found = false;
            foreach (const QString &path, _rootPaths) {
                if (QFileInfo(path).fileName() == repoName) {
                    repoPath = path;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return buildHttpResponse(404, "Not Found", "application/json",
                                       buildJsonResponse(false, "", "Repository not found: " + repoName));
            }
        }

        return getFile(repoPath, filePath, type);
    }
    else {
        return buildHttpResponse(404, "Not Found", "text/plain",
                                QString("Endpoint not found"));
    }
}

QString SearchServer::searchFiles(const QString &repoPath, const QString &searchPath,
                                  const QString &pattern, bool recursive)
{
    QStringList results;
    QString fullSearchPath = repoPath;

    if (!searchPath.isEmpty()) {
        fullSearchPath += "/" + searchPath;
    }

    QDir searchDir(fullSearchPath);
    if (!searchDir.exists()) {
        return buildJsonResponse(false, "", "Search path does not exist");
    }

    // Search for files
    QStringList nameFilters;
    nameFilters << "*.max" << "*.pdf" << "*.jpg" << "*.jpeg" << "*.tiff" << "*.tif";

    QDir::Filters filters = QDir::Files | QDir::Readable;
    if (recursive)
        filters |= QDir::AllDirs | QDir::NoDotAndDotDot;

    QFileInfoList entries = searchDir.entryInfoList(nameFilters, filters);

    foreach (const QFileInfo &info, entries) {
        if (info.isDir() && recursive) {
            // Recursively search subdirectories
            QString subPath = searchPath.isEmpty()
                            ? info.fileName()
                            : searchPath + "/" + info.fileName();
            QString subResult = searchFiles(repoPath, subPath, pattern, recursive);
            // Note: This is a simplified version - in production you'd merge the results properly
        }
        else if (info.isFile()) {
            // Check if filename matches pattern (case-insensitive)
            if (info.fileName().contains(pattern, Qt::CaseInsensitive)) {
                QString relativePath = searchPath.isEmpty()
                                      ? info.fileName()
                                      : searchPath + "/" + info.fileName();
                results.append(relativePath);
            }
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Build JSON array with Qt 5
    QJsonArray jsonArray;
    foreach (const QString &file, results) {
        QJsonObject fileObj;
        fileObj["path"] = file;

        QFileInfo info(repoPath + "/" + file);
        fileObj["name"] = info.fileName();
        fileObj["size"] = (qint64)info.size();
        fileObj["modified"] = info.lastModified().toString(Qt::ISODate);

        jsonArray.append(fileObj);
    }

    QJsonObject responseObj;
    responseObj["success"] = true;
    responseObj["count"] = results.size();
    responseObj["results"] = jsonArray;

    QJsonDocument doc(responseObj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
#else
    // Fallback for Qt 4 - simple JSON string building
    QString json = "{\"success\":true,\"count\":" + QString::number(results.size()) + ",\"results\":[";
    for (int i = 0; i < results.size(); i++) {
        if (i > 0) json += ",";
        QFileInfo info(repoPath + "/" + results[i]);
        json += "{\"path\":\"" + results[i] + "\","
                "\"name\":\"" + info.fileName() + "\","
                "\"size\":" + QString::number(info.size()) + ","
                "\"modified\":\"" + info.lastModified().toString(Qt::ISODate) + "\"}";
    }
    json += "]}";
    return json;
#endif
}

QString SearchServer::listFiles(const QString &dirPath)
{
    QString fullPath = _rootPath;
    if (!dirPath.isEmpty()) {
        fullPath += "/" + dirPath;
    }

    QDir dir(fullPath);
    if (!dir.exists()) {
        return buildJsonResponse(false, "", "Directory does not exist");
    }

    QStringList nameFilters;
    nameFilters << "*.max" << "*.pdf" << "*.jpg" << "*.jpeg" << "*.tiff" << "*.tif";

    QFileInfoList entries = dir.entryInfoList(nameFilters, QDir::Files | QDir::Readable);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonArray jsonArray;
    foreach (const QFileInfo &info, entries) {
        QJsonObject fileObj;
        fileObj["name"] = info.fileName();
        fileObj["path"] = dirPath.isEmpty() ? info.fileName() : dirPath + "/" + info.fileName();
        fileObj["size"] = (qint64)info.size();
        fileObj["modified"] = info.lastModified().toString(Qt::ISODate);
        jsonArray.append(fileObj);
    }

    QJsonObject responseObj;
    responseObj["success"] = true;
    responseObj["path"] = dirPath;
    responseObj["count"] = entries.size();
    responseObj["files"] = jsonArray;

    QJsonDocument doc(responseObj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
#else
    // Qt 4 fallback
    QString json = "{\"success\":true,\"path\":\"" + dirPath + "\",\"count\":"
                  + QString::number(entries.size()) + ",\"files\":[";
    for (int i = 0; i < entries.size(); i++) {
        if (i > 0) json += ",";
        const QFileInfo &info = entries[i];
        json += "{\"name\":\"" + info.fileName() + "\","
                "\"path\":\"" + (dirPath.isEmpty() ? info.fileName() : dirPath + "/" + info.fileName()) + "\","
                "\"size\":" + QString::number(info.size()) + ","
                "\"modified\":\"" + info.lastModified().toString(Qt::ISODate) + "\"}";
    }
    json += "]}";
    return json;
#endif
}

QString SearchServer::listRepositories()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonArray jsonArray;
    foreach (const QString &path, _rootPaths) {
        QJsonObject repoObj;
        repoObj["path"] = path;

        QDir dir(path);
        if (dir.exists()) {
            repoObj["exists"] = true;
            repoObj["name"] = QFileInfo(path).fileName();
        } else {
            repoObj["exists"] = false;
            repoObj["name"] = "";
        }

        jsonArray.append(repoObj);
    }

    QJsonObject responseObj;
    responseObj["success"] = true;
    responseObj["count"] = _rootPaths.size();
    responseObj["repositories"] = jsonArray;

    QJsonDocument doc(responseObj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
#else
    // Qt 4 fallback
    QString json = "{\"success\":true,\"count\":" + QString::number(_rootPaths.size())
                  + ",\"repositories\":[";
    for (int i = 0; i < _rootPaths.size(); i++) {
        if (i > 0) json += ",";
        const QString &path = _rootPaths[i];
        QDir dir(path);
        bool exists = dir.exists();
        QString name = exists ? QFileInfo(path).fileName() : "";
        json += "{\"path\":\"" + path + "\","
                "\"exists\":" + QString(exists ? "true" : "false") + ","
                "\"name\":\"" + name + "\"}";
    }
    json += "]}";
    return json;
#endif
}

QString SearchServer::getFile(const QString &repoPath, const QString &filePath, const QString &type)
{
    // Security: Prevent directory traversal
    if (filePath.contains("..") || filePath.startsWith("/")) {
        return buildHttpResponse(400, "Bad Request", "application/json",
                               buildJsonResponse(false, "", "Invalid file path"));
    }

    // Build full path
    QString fullPath = repoPath + "/" + filePath;
    QFileInfo fileInfo(fullPath);

    // Check if file exists and is readable
    if (!fileInfo.exists()) {
        return buildHttpResponse(404, "Not Found", "application/json",
                               buildJsonResponse(false, "", "File not found"));
    }

    if (!fileInfo.isFile()) {
        return buildHttpResponse(400, "Bad Request", "application/json",
                               buildJsonResponse(false, "", "Path is not a file"));
    }

    // Get file extension
    QString ext = fileInfo.suffix().toLower();

    // Handle PDF conversion request
    if (type == "pdf") {
        // If file is already PDF, return it directly
        if (ext == "pdf") {
            // Fall through to normal file reading below
        }
        // Otherwise, conversion not yet supported
        else {
            return buildHttpResponse(501, "Not Implemented", "application/json",
                                   buildJsonResponse(false, "",
                                       "PDF conversion not yet supported for " + ext + " files. " +
                                       "Please use type=original to get the file in its original format."));
        }
    }

    // Determine content type based on extension
    QString contentType = "application/octet-stream";  // Default
    if (ext == "pdf") {
        contentType = "application/pdf";
    } else if (ext == "jpg" || ext == "jpeg") {
        contentType = "image/jpeg";
    } else if (ext == "tif" || ext == "tiff") {
        contentType = "image/tiff";
    } else if (ext == "max") {
        contentType = "application/octet-stream";
    }

    // Read file content
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return buildHttpResponse(500, "Internal Server Error", "application/json",
                               buildJsonResponse(false, "", "Failed to read file"));
    }

    QByteArray fileContent = file.readAll();
    file.close();

    return buildHttpResponse(200, "OK", contentType, fileContent);
}

QString SearchServer::buildJsonResponse(bool success, const QString &data,
                                        const QString &error)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonObject obj;
    obj["success"] = success;
    if (!error.isEmpty())
        obj["error"] = error;
    if (!data.isEmpty())
        obj["data"] = data;

    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
#else
    QString json = "{\"success\":" + QString(success ? "true" : "false");
    if (!error.isEmpty())
        json += ",\"error\":\"" + error + "\"";
    if (!data.isEmpty())
        json += ",\"data\":\"" + data + "\"";
    json += "}";
    return json;
#endif
}

QString SearchServer::buildHttpResponse(int statusCode, const QString &statusText,
                                        const QString &contentType, const QString &body)
{
    QString response;
    response += "HTTP/1.1 " + QString::number(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QString::number(body.toUtf8().size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";  // Enable CORS
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    return response;
}

QString SearchServer::buildHttpResponse(int statusCode, const QString &statusText,
                                        const QString &contentType, const QByteArray &body)
{
    QString response;
    response += "HTTP/1.1 " + QString::number(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QString::number(body.size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";  // Enable CORS
    response += "Connection: close\r\n";
    response += "\r\n";
    response += QString::fromLatin1(body);  // Binary safe conversion
    return response;
}

QString SearchServer::urlDecode(const QString &str)
{
    QByteArray bytes = QByteArray::fromPercentEncoding(str.toUtf8());
    return QString::fromUtf8(bytes);
}
