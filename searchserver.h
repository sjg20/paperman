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
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemWatcher>

// Simple struct for cached file information
struct CachedFile {
    QString path;        // Relative path from repository root
    QString name;        // Filename only
    qint64 size;         // File size in bytes
    QDateTime modified;  // Last modification time
};

/**
 * Simple HTTP server for searching paper repository
 */
class SearchServer : public QTcpServer
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param rootPath   Root path of the paper repository (single path)
     * @param port       Port to listen on (default 8080)
     * @param parent     Parent QObject
     * @param skipCache  Skip building file cache at startup
     */
    explicit SearchServer(const QString &rootPath, quint16 port = 8080,
                         QObject *parent = nullptr,
                         bool skipCache = false);

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

    /**
     * Handle file system changes (files added/removed/modified)
     * @param path Path to the directory that changed
     */
    void onDirectoryChanged(const QString &path);

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
     * @return HTTP response as binary data
     */
    QByteArray handleRequest(const QString &method, const QString &path,
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
     * Browse directory contents (files and subdirectories)
     * @param repoPath   Repository root path
     * @param dirPath    Directory path (relative to repository root)
     * @return JSON response with files and directories
     */
    QString browseDirectory(const QString &repoPath, const QString &dirPath);

    /**
     * Get list of all repositories
     * @return JSON response with repository list
     */
    QString listRepositories();

    /**
     * Get file content
     * @param repoPath       Repository root path
     * @param filePath       File path (relative to repository root)
     * @param type           Output type ("original" or "pdf")
     * @param page           Extract single page (0 = return whole file)
     * @param wantPageCount  Return page count as JSON instead of file data
     * @return HTTP response with file content (binary safe)
     */
    QByteArray getFile(const QString &repoPath, const QString &filePath,
                       const QString &type = "original", int page = 0,
                       bool wantPageCount = false);

    /**
     * Convert a non-PDF file to PDF, caching the result
     * @param fullPath Absolute path to the source file
     * @return Path to cached PDF, or empty string on failure
     */
    QString convertToPdf(const QString &fullPath);

#ifndef QT_NO_GUI
    /**
     * Get page count of a non-PDF file using the File class
     * @param fullPath Absolute path to the source file
     * @return Number of pages, or -1 on failure
     */
    int getFilePageCount(const QString &fullPath);

    /**
     * Convert a single page of a non-PDF file to PDF using the File class
     * @param fullPath  Absolute path to the source file
     * @param page      Page number (1-based)
     * @param fileInfo  QFileInfo for the source file (used for mtime)
     * @return Path to cached single-page PDF, or empty string on failure
     */
    QString convertPageWithFile(const QString &fullPath, int page,
                                const QFileInfo &fileInfo);
#else
    /**
     * Convert a single page of a non-PDF file to PDF, caching the result
     * @param fullPath  Absolute path to the source file
     * @param page      Page number (1-based)
     * @param fileInfo  QFileInfo for the source file (used for mtime)
     * @return Path to cached single-page PDF, or empty string on failure
     */
    QString convertPageToPdf(const QString &fullPath, int page,
                             const QFileInfo &fileInfo);
#endif

    /**
     * Get page count of a PDF file using pdfinfo
     * @param pdfPath Path to PDF file
     * @return Number of pages, or -1 on failure
     */
    int getPdfPageCount(const QString &pdfPath);

    /**
     * Extract a single page from a PDF using pdftocairo
     * @param pdfPath    Path to source PDF file
     * @param page       Page number (1-based)
     * @param outputPath Output path for single-page PDF
     * @return true if successful
     */
    bool extractPdfPage(const QString &pdfPath, int page,
                        const QString &outputPath);

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
     * @return Full HTTP response (binary safe)
     */
    QByteArray buildHttpResponse(int statusCode, const QString &statusText,
                                const QString &contentType, const QString &body);

    /**
     * Build HTTP response with binary body
     * @param statusCode  HTTP status code
     * @param statusText  HTTP status text
     * @param contentType Content type
     * @param body        Response body (binary)
     * @return Full HTTP response (binary safe)
     */
    QByteArray buildHttpResponse(int statusCode, const QString &statusText,
                                const QString &contentType, const QByteArray &body);

    /**
     * URL decode a string
     */
    QString urlDecode(const QString &str);

    /**
     * Validate API key for authentication
     * @param token API key to validate
     * @return true if valid, false otherwise
     */
    bool validateApiKey(const QString &token);

    /**
     * Check if authentication is enabled
     * @return true if API key is configured
     */
    bool isAuthEnabled();

    /**
     * Generate thumbnail for a file
     * @param repoPath Repository root path
     * @param filePath File path relative to repository
     * @param page Page number (for multi-page documents)
     * @param size Thumbnail size (small, medium, large)
     * @return Path to generated thumbnail, or empty string on failure
     */
    QString generateThumbnail(const QString &repoPath, const QString &filePath,
                             int page, const QString &size);

    /**
     * Get thumbnail pixel size from size string
     * @param size Size string (small, medium, large)
     * @return Pixel dimension for thumbnail
     */
    int getThumbnailSize(const QString &size);

    /**
     * Extract thumbnail from PDF using pdftocairo
     * @param pdfPath Path to PDF file
     * @param page Page number
     * @param size Pixel size for thumbnail
     * @param outputPath Output path for thumbnail
     * @return true if successful
     */
    bool extractPdfThumbnail(const QString &pdfPath, int page, int size,
                            const QString &outputPath);

    /**
     * Clean old thumbnails from cache
     */
    void cleanThumbnailCache();

private:
    /**
     * Scan a directory recursively and build file cache
     * @param repoPath Repository root path
     * @param dirPath Current directory being scanned (relative to repo)
     * @param fileList Returns list of cached files
     */
    void scanDirectory(const QString &repoPath, const QString &dirPath,
                      QList<CachedFile> &fileList);

    /**
     * Count files recursively in a directory using cached data
     * @param repoPath Repository root path
     * @param dirPath Relative directory path within repository
     * @return Number of files found recursively in cache
     */
    int countFilesRecursive(const QString &repoPath, const QString &dirPath);

    /**
     * Load file cache from .papertree file
     * @param repoPath Repository root path
     * @param fileList Returns list of cached files
     * @return true if successfully loaded from papertree, false otherwise
     */
    bool loadFromPapertree(const QString &repoPath, QList<CachedFile> &fileList);

    QString _rootPath;      //!< Root path of paper repository (deprecated, use _rootPaths)
    QStringList _rootPaths; //!< List of root paths of paper repositories
    quint16 _port;          //!< Port to listen on
    QList<QTcpSocket*> _clients;  //!< Connected clients
    QString _apiKey;        //!< API key for authentication (from PAPERMAN_API_KEY env var)
    QHash<QString, QList<CachedFile>> _fileCache;  //!< Cached file list for each repository
    QFileSystemWatcher *_fsWatcher;  //!< File system watcher for automatic cache updates
};

#endif // __searchserver_h
