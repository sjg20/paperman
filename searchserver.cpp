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

#include <QCoreApplication>
#include <QTcpSocket>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QUrl>
#include <QDebug>
#include <QProcess>
#include <QTemporaryDir>
#include <QDirIterator>

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

    // Load API key from environment variable
    _apiKey = QString::fromUtf8(qgetenv("PAPERMAN_API_KEY"));
    if (!_apiKey.isEmpty()) {
        qDebug() << "SearchServer: API key authentication enabled";
    }

    // Build file cache for the repository
    qDebug() << "SearchServer: Building file cache for" << _rootPath;
    QList<CachedFile> &fileList = _fileCache[_rootPath];
    scanDirectory(_rootPath, "", fileList);
    qDebug() << "SearchServer: File cache built with" << fileList.size() << "files";
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

    // Load API key from environment variable
    _apiKey = QString::fromUtf8(qgetenv("PAPERMAN_API_KEY"));
    if (!_apiKey.isEmpty()) {
        qDebug() << "SearchServer: API key authentication enabled";
    }

    // Build file cache for each repository
    foreach (const QString &path, _rootPaths) {
        qDebug() << "SearchServer: Building file cache for" << path;
        QList<CachedFile> &fileList = _fileCache[path];
        scanDirectory(path, "", fileList);
        qDebug() << "SearchServer: File cache built with" << fileList.size() << "files";
    }
}

SearchServer::~SearchServer()
{
    stop();
    // File cache cleanup is automatic (QHash and QList)
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
    QByteArray response = handleRequest(method, path, params);

    // Write binary response directly
    client->write(response);
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

    // Parse headers (look for X-API-Key)
    for (int i = 1; i < lines.size(); i++) {
        QString line = lines[i];
        if (line.isEmpty())
            break;  // End of headers

        int colonPos = line.indexOf(':');
        if (colonPos > 0) {
            QString headerName = line.left(colonPos).trimmed();
            QString headerValue = line.mid(colonPos + 1).trimmed();

            // Store X-API-Key header for authentication
            if (headerName.toLower() == "x-api-key") {
                params["__api_key__"] = headerValue;
            }
        }
    }
}

QByteArray SearchServer::handleRequest(const QString &method, const QString &path,
                                      const QHash<QString, QString> &params)
{
    qDebug() << "SearchServer: Handling" << method << path;

    // Only support GET requests
    if (method != "GET") {
        return buildHttpResponse(405, "Method Not Allowed", "text/plain",
                                QString("Only GET requests are supported"));
    }

    // Check authentication (except for /status endpoint)
    if (isAuthEnabled() && path != "/status") {
        QString providedKey = params.value("__api_key__");
        if (!validateApiKey(providedKey)) {
            qWarning() << "SearchServer: Authentication failed for" << path;
            return buildHttpResponse(401, "Unauthorized", "application/json",
                                   buildJsonResponse(false, "", "Invalid or missing API key. "
                                       "Please provide X-API-Key header."));
        }
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
    else if (path == "/browse") {
        QString dirPath = params.value("path", "");
        QString repoName = params.value("repo", "");

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

        QString result = browseDirectory(repoPath, dirPath);

        // Check for error markers from browseDirectory
        if (result.isEmpty()) {
            // Invalid path (400 Bad Request)
            return buildHttpResponse(400, "Bad Request", "application/json",
                                   buildJsonResponse(false, "", "Invalid path"));
        } else if (result == "NOT_FOUND") {
            // Directory not found (404 Not Found)
            return buildHttpResponse(404, "Not Found", "application/json",
                                   buildJsonResponse(false, "", "Directory not found"));
        }

        // Success
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
    // Get file cache for this repository
    const QList<CachedFile> &fileList = _fileCache.value(repoPath);
    if (fileList.isEmpty()) {
        qWarning() << "SearchServer: No file cache found for" << repoPath;
        return buildJsonResponse(false, "", "Repository not found in cache");
    }

    qDebug() << "SearchServer: Searching" << fileList.size() << "files for pattern:" << pattern;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Build JSON array with Qt 5
    QJsonArray jsonArray;
    int matchCount = 0;

    // Iterate through all files in the cache
    foreach (const CachedFile &file, fileList) {
        // Skip if searchPath is specified and file is not in that path
        if (!searchPath.isEmpty()) {
            if (recursive) {
                // For recursive search, check if file is under the searchPath
                if (!file.path.startsWith(searchPath + "/") && file.path != searchPath)
                    continue;
            } else {
                // For non-recursive, check if file is directly in searchPath
                QString fileDir = QFileInfo(file.path).path();
                if (fileDir != searchPath && fileDir != ".")
                    continue;
            }
        }

        // Check if filename matches pattern (case-insensitive)
        if (file.name.contains(pattern, Qt::CaseInsensitive)) {
            QJsonObject fileObj;
            fileObj["path"] = file.path;
            fileObj["name"] = file.name;
            fileObj["size"] = file.size;
            fileObj["modified"] = file.modified.toString(Qt::ISODate);

            jsonArray.append(fileObj);
            matchCount++;
        }
    }

    qDebug() << "SearchServer: Found" << matchCount << "matches";

    QJsonObject responseObj;
    responseObj["success"] = true;
    responseObj["count"] = matchCount;
    responseObj["results"] = jsonArray;

    QJsonDocument doc(responseObj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
#else
    // Fallback for Qt 4 - simple JSON string building
    QString json = "{\"success\":true,\"results\":[";
    int matchCount = 0;

    foreach (const CachedFile &file, fileList) {
        // Skip if searchPath is specified and file is not in that path
        if (!searchPath.isEmpty()) {
            if (recursive) {
                if (!file.path.startsWith(searchPath + "/") && file.path != searchPath)
                    continue;
            } else {
                QString fileDir = QFileInfo(file.path).path();
                if (fileDir != searchPath && fileDir != ".")
                    continue;
            }
        }

        // Check if filename matches pattern
        if (file.name.contains(pattern, Qt::CaseInsensitive)) {
            if (matchCount > 0) json += ",";

            json += "{\"path\":\"" + file.path + "\","
                    "\"name\":\"" + file.name + "\","
                    "\"size\":" + QString::number(file.size) + ","
                    "\"modified\":\"" + file.modified.toString(Qt::ISODate) + "\"}";
            matchCount++;
        }
    }

    json += "],\"count\":" + QString::number(matchCount) + "}";
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

QByteArray SearchServer::getFile(const QString &repoPath, const QString &filePath, const QString &type)
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
    if (type == "pdf" && ext != "pdf") {
        // Convert to PDF using maxview command-line tool
        QTemporaryDir tmpDir;
        if (!tmpDir.isValid()) {
            return buildHttpResponse(500, "Internal Server Error", "application/json",
                                   buildJsonResponse(false, "", "Failed to create temporary directory"));
        }

        // Create output PDF path in temporary directory
        QString baseName = fileInfo.completeBaseName();
        QString outputPdf = tmpDir.path() + "/" + baseName + ".pdf";

        // Set up environment for maxview (needs DISPLAY for Qt, but we use offscreen)
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen");

        // Run maxview to convert the file
        QProcess process;
        process.setProcessEnvironment(env);
        process.setWorkingDirectory(tmpDir.path());

        // Find paperman executable - use same directory as server executable
        QString papermanPath = QCoreApplication::applicationDirPath() + "/paperman";
        if (!QFile::exists(papermanPath)) {
            // Fallback to current directory
            papermanPath = "paperman";
        }

        QStringList args;
        args << "-p" << fullPath;

        qDebug() << "SearchServer: Converting" << fullPath << "to PDF using" << papermanPath;
        process.start(papermanPath, args);

        if (!process.waitForStarted(5000)) {
            qWarning() << "SearchServer: Failed to start paperman - falling back to original file";
            // Fallback: return original file instead of failing
            QFile file(fullPath);
            if (!file.open(QIODevice::ReadOnly)) {
                return buildHttpResponse(500, "Internal Server Error", "application/json",
                                       buildJsonResponse(false, "", "PDF conversion not available and cannot read original file"));
            }
            QByteArray fileContent = file.readAll();
            file.close();

            // Determine content type based on extension
            QString contentType = "application/octet-stream";
            if (ext == "max") contentType = "application/x-max";
            else if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
            else if (ext == "tiff" || ext == "tif") contentType = "image/tiff";

            return buildHttpResponse(200, "OK", contentType, fileContent);
        }

        if (!process.waitForFinished(30000)) {  // 30 second timeout
            process.kill();
            return buildHttpResponse(500, "Internal Server Error", "application/json",
                                   buildJsonResponse(false, "", "PDF conversion timed out (30s limit)"));
        }

        int exitCode = process.exitCode();
        QString stdout_output = QString::fromUtf8(process.readAllStandardOutput());
        QString stderr_output = QString::fromUtf8(process.readAllStandardError());

        qDebug() << "SearchServer: Conversion process exit code:" << exitCode;
        qDebug() << "SearchServer: stdout:" << stdout_output;
        qDebug() << "SearchServer: stderr:" << stderr_output;

        if (exitCode != 0) {
            qWarning() << "SearchServer: Conversion failed with exit code" << exitCode;
            return buildHttpResponse(500, "Internal Server Error", "application/json",
                                   buildJsonResponse(false, "", "PDF conversion failed: " + stderr_output));
        }

        // Find the generated PDF file (maxview generates it with various names)
        QDir tmpDirObj(tmpDir.path());
        QStringList allFiles = tmpDirObj.entryList(QDir::Files);
        QStringList pdfFiles = tmpDirObj.entryList(QStringList() << "*.pdf", QDir::Files);

        qDebug() << "SearchServer: Temp directory:" << tmpDir.path();
        qDebug() << "SearchServer: All files in temp dir:" << allFiles;
        qDebug() << "SearchServer: PDF files found:" << pdfFiles;

        if (pdfFiles.isEmpty()) {
            return buildHttpResponse(500, "Internal Server Error", "application/json",
                                   buildJsonResponse(false, "", "PDF file was not generated in " + tmpDir.path()));
        }

        // Read the generated PDF
        QString pdfPath = tmpDir.path() + "/" + pdfFiles.first();
        QFile pdfFile(pdfPath);
        if (!pdfFile.open(QIODevice::ReadOnly)) {
            return buildHttpResponse(500, "Internal Server Error", "application/json",
                                   buildJsonResponse(false, "", "Failed to read converted PDF"));
        }

        QByteArray pdfContent = pdfFile.readAll();
        pdfFile.close();

        qDebug() << "SearchServer: Successfully converted to PDF (" << pdfContent.size() << "bytes)";

        // Return the PDF
        return buildHttpResponse(200, "OK", "application/pdf", pdfContent);
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

QByteArray SearchServer::buildHttpResponse(int statusCode, const QString &statusText,
                                          const QString &contentType, const QString &body)
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText.toUtf8() + "\r\n";
    response += "Content-Type: " + contentType.toUtf8() + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.toUtf8().size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";  // Enable CORS
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body.toUtf8();
    return response;
}

QByteArray SearchServer::buildHttpResponse(int statusCode, const QString &statusText,
                                          const QString &contentType, const QByteArray &body)
{
    // Build response as QByteArray to preserve binary data
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText.toUtf8() + "\r\n";
    response += "Content-Type: " + contentType.toUtf8() + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";  // Enable CORS
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;  // Append binary body directly
    return response;
}

QString SearchServer::urlDecode(const QString &str)
{
    // Replace + with space (URL query string encoding)
    QString decoded = str;
    decoded.replace('+', ' ');

    // Then handle percent-encoded characters
    QByteArray bytes = QByteArray::fromPercentEncoding(decoded.toUtf8());
    return QString::fromUtf8(bytes);
}

bool SearchServer::validateApiKey(const QString &token)
{
    // If no API key is configured, authentication is disabled
    if (_apiKey.isEmpty()) {
        return true;
    }

    // Compare provided token with configured API key
    return token == _apiKey;
}

bool SearchServer::isAuthEnabled()
{
    return !_apiKey.isEmpty();
}

QString SearchServer::browseDirectory(const QString &repoPath, const QString &dirPath)
{
    // Security: Prevent directory traversal
    if (dirPath.contains("..") || dirPath.startsWith("/")) {
        // Invalid path - this is a client error (400)
        // We return an empty string to signal the caller to send a 400 response
        return "";  // Will be handled by caller with 400 error
    }

    // Build full path
    QString fullPath = repoPath;
    if (!dirPath.isEmpty()) {
        fullPath += "/" + dirPath;
    }

    QDir dir(fullPath);
    if (!dir.exists()) {
        // Directory not found - return special marker for 404
        return "NOT_FOUND";  // Will be handled by caller with 404 error
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonArray filesArray;
    QJsonArray dirsArray;

    // File extensions to count
    QStringList nameFilters;
    nameFilters << "*.max" << "*.pdf" << "*.jpg" << "*.jpeg" << "*.tiff" << "*.tif";

    // Get subdirectories
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    foreach (const QFileInfo &subdir, subdirs) {
        // Skip hidden directories
        if (subdir.fileName().startsWith('.'))
            continue;

        QJsonObject dirObj;
        dirObj["name"] = subdir.fileName();

        // Build relative path
        QString relativePath = dirPath.isEmpty()
                              ? subdir.fileName()
                              : dirPath + "/" + subdir.fileName();
        dirObj["path"] = relativePath;

        // Count files in this directory recursively
        QString subdirFullPath = fullPath + "/" + subdir.fileName();
        int fileCount = countFilesRecursive(subdirFullPath, nameFilters);
        dirObj["count"] = fileCount;

        dirsArray.append(dirObj);
    }

    // Get files (only supported file types)
    QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    foreach (const QFileInfo &file, files) {
        QJsonObject fileObj;
        fileObj["name"] = file.fileName();
        fileObj["size"] = (qint64)file.size();
        fileObj["modified"] = file.lastModified().toString(Qt::ISODate);

        // Build relative path
        QString relativePath = dirPath.isEmpty()
                              ? file.fileName()
                              : dirPath + "/" + file.fileName();
        fileObj["path"] = relativePath;

        filesArray.append(fileObj);
    }

    QJsonObject responseObj;
    responseObj["success"] = true;
    responseObj["path"] = dirPath;
    responseObj["count"] = dirsArray.size() + filesArray.size();
    responseObj["directories"] = dirsArray;
    responseObj["files"] = filesArray;

    QJsonDocument doc(responseObj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
#else
    // Qt 4 fallback
    QStringList nameFilters;
    nameFilters << "*.max" << "*.pdf" << "*.jpg" << "*.jpeg" << "*.tiff" << "*.tif";

    // Count items
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

    // Filter hidden subdirs
    QStringList visibleSubdirs;
    foreach (const QString &subdir, subdirs) {
        if (!subdir.startsWith('.'))
            visibleSubdirs.append(subdir);
    }

    QString json = "{\"success\":true,\"path\":\"" + dirPath + "\",";
    json += "\"count\":" + QString::number(visibleSubdirs.size() + files.size()) + ",";

    json += "\"directories\":[";
    for (int i = 0; i < visibleSubdirs.size(); i++) {
        if (i > 0) json += ",";
        QString relativePath = dirPath.isEmpty() ? visibleSubdirs[i] : dirPath + "/" + visibleSubdirs[i];
        QString subdirFullPath = fullPath + "/" + visibleSubdirs[i];
        int fileCount = countFilesRecursive(subdirFullPath, nameFilters);
        json += "{\"name\":\"" + visibleSubdirs[i] + "\",\"path\":\"" + relativePath
              + "\",\"count\":" + QString::number(fileCount) + "}";
    }
    json += "],\"files\":[";
    for (int i = 0; i < files.size(); i++) {
        if (i > 0) json += ",";
        QFileInfo info(fullPath + "/" + files[i]);
        QString relativePath = dirPath.isEmpty() ? files[i] : dirPath + "/" + files[i];
        json += "{\"name\":\"" + files[i] + "\","
                "\"size\":" + QString::number(info.size()) + ","
                "\"modified\":\"" + info.lastModified().toString(Qt::ISODate) + "\","
                "\"path\":\"" + relativePath + "\"}";
    }
    json += "]}";
    return json;
#endif
}

int SearchServer::countFilesRecursive(const QString &dirPath, const QStringList &nameFilters)
{
    int count = 0;
    QDirIterator it(dirPath, nameFilters,
                    QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        count++;
    }

    return count;
}

void SearchServer::scanDirectory(const QString &repoPath, const QString &dirPath,
                                 QList<CachedFile> &fileList)
{
    QString fullPath = repoPath;
    if (!dirPath.isEmpty())
        fullPath += "/" + dirPath;

    // File extensions to cache
    QStringList nameFilters;
    nameFilters << "*.max" << "*.pdf" << "*.jpg" << "*.jpeg" << "*.tiff" << "*.tif";

    // Use QDirIterator for efficient recursive scanning
    QDirIterator it(fullPath, nameFilters,
                    QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo info(filePath);

        // Calculate relative path from repository root
        QString relativePath = filePath;
        if (relativePath.startsWith(repoPath + "/"))
            relativePath = relativePath.mid(repoPath.length() + 1);
        else if (relativePath.startsWith(repoPath))
            relativePath = relativePath.mid(repoPath.length());

        // Add to cache
        CachedFile cached;
        cached.path = relativePath;
        cached.name = info.fileName();
        cached.size = info.size();
        cached.modified = info.lastModified();

        fileList.append(cached);
    }
}
