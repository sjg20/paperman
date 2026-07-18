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

#include "backend.h"
#include "cachedfile.h"
#include "localbackend.h"
#include "tokenstore.h"
#include "userstore.h"

#include <memory>

#include <QFileInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QStringList>
#include <QBitArray>
#include <QHash>
#include <QMap>
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcess>

#include "serverlog.h"

/**
 * Tracks an in-flight Ghostscript page extraction.
 *
 * When a client requests a single page from a multi-page PDF,
 * the server spawns a Ghostscript process to extract it.  A
 * PendingExtraction records everything needed to deliver the
 * result once the process finishes.
 *
 * Multiple clients may request the same page while an
 * extraction is already running.  Rather than spawning a
 * duplicate process, subsequent clients are appended to
 * @c waiters so they all receive the response when the single
 * extraction completes.  If a client disconnects while
 * waiting, it is removed from @c waiters (the extraction
 * itself continues so the result is cached).
 *
 * Instances are stored in SearchServer::_pendingExtractions,
 * keyed by the cache path (@c outputPath) of the extracted
 * page.
 */
struct PendingExtraction {
    QProcess *process;          //!< Ghostscript process extracting the page
    QString outputPath;         //!< Dest path for extracted single-page PDF
    QString filePath;           //!< Source document (for logging)
    int page;                   //!< 1-based page number (for logging)
    QList<QTcpSocket *> waiters; //!< Clients waiting
};

/**
 * Versioned HTTP API: clients should check /v1/status to see what the
 * server supports before relying on any v1 endpoint. Bump this when the
 * wire format changes.
 */
#define PAPERMAN_API_VERSION "1"

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
     * Handle completion of an async Ghostscript page extraction
     */
    void onExtractionFinished(int exitCode, QProcess::ExitStatus exitStatus);

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
     * @param client   Client socket (needed for async responses)
     * @return HTTP response, or empty QByteArray if the response is deferred
     */
    QByteArray handleRequest(const QString &method, const QString &path,
                            const QHash<QString, QString> &params,
                            QTcpSocket *client = nullptr);

    /**
     * Handle POST /v1/auth/login.  Body is JSON {user, password}.
     * Returns 200 with {token, expiry, user} on success, 401/400
     * otherwise.
     */
    QByteArray handleAuthLogin(const QHash<QString, QString> &params);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/transform
     *
     * Rotates or flips a page (or every page) of a stack on disk by
     * calling File::transformPage().  The body is JSON {page, op} where
     * page is 1-based (a value below 1 means every page) and op is one
     * of rotate90/rotate180/rotate270/hflip/vflip.
     *
     * @param path        The full request path (carries repo and stack)
     * @param params      Parsed request parameters (incl. __body__)
     * @param authedUser  The authenticated user, or empty for API-key auth
     * @return HTTP response, 200 with {success:true} on success
     */
    QByteArray handleTransform(const QString &path,
                               const QHash<QString, QString> &params,
                               const QString &authedUser);

    /** Target of a per-stack mutation URL. */
    struct StackTarget {
        QString repoName;   //!< repository basename
        QString filePath;   //!< path relative to the repository root
        QString repoPath;   //!< absolute repository root
        QString fullPath;   //!< absolute path of the stack's file
    };

    /**
     * Split a /v1/repos/{repo}/stacks/{path}{verb} URL into repo and
     * stack path.  @p verb is the trailing part including its leading
     * slash (e.g. "/rename").  Returns false if the URL is malformed.
     */
    static bool splitStackUrl(const QString &path, const QString &verb,
                              QString *repoName, QString *filePath);

    /**
     * Validate a stack target: path traversal, the per-user repo
     * allowlist and repository existence; when @p mustExist is true
     * the file itself must exist too.
     *
     * @return empty QByteArray on success (out filled in), or the
     * HTTP error response to send
     */
    QByteArray resolveStackTarget(const QString &repoName,
                                  const QString &filePath,
                                  const QString &authedUser,
                                  StackTarget &out, bool mustExist = true);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/rename
     *
     * Body {newName, autoRename}.  Renames the file within its
     * directory; with autoRename a colliding name gets _1, _2...
     * appended.  Returns {success, name} with the final name.
     */
    QByteArray handleRename(const QString &path,
                            const QHash<QString, QString> &params,
                            const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/move
     *
     * Body {destDir, copy}.  Moves (or copies) the file to another
     * directory in the same repository; destDir is repo-relative and
     * "" means the root.  The trash directory is created on demand;
     * any other destination must exist.  A name collision appends
     * _move_1 etc.  Returns {success, name} with the final name.
     */
    QByteArray handleMove(const QString &path,
                          const QHash<QString, QString> &params,
                          const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/delete
     *
     * Removes the file outright (no trash).
     */
    QByteArray handleDelete(const QString &path,
                            const QHash<QString, QString> &params,
                            const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/pages/{n}/rename
     *
     * Body {newName}.  Renames a page within the stack via the File
     * classes; n is 1-based.
     */
    QByteArray handleRenamePage(const QString &path,
                                const QHash<QString, QString> &params,
                                const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/annotations
     *
     * Body is a JSON object with any subset of author/title/keywords/
     * notes/ocr; the named fields are written into the stack's
     * annotation block via the File classes.
     */
    QByteArray handleUpdateAnnot(const QString &path,
                                 const QHash<QString, QString> &params,
                                 const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/pages/delete
     *
     * Body {pages:[n,...]} with 1-based page numbers.  Removes the
     * pages from the stack and stores the recovery data server-side,
     * returning {undoId} as an opaque token for pages/undelete.
     */
    QByteArray handleDeletePages(const QString &path,
                                 const QHash<QString, QString> &params,
                                 const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/pages/undelete
     *
     * Body {undoId}.  Restores the pages removed by the matching
     * pages/delete call.  An unknown or already-redeemed id is 410
     * Gone; the recovery data does not survive a server restart.
     */
    QByteArray handleUndeletePages(const QString &path,
                                   const QHash<QString, QString> &params,
                                   const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/unstack
     *
     * Body {pagenum, pagecount, remove, newName}.  Copies @c pagecount
     * pages starting at 1-based @c pagenum into a new stack in the
     * same directory (named newName, made unique), removing them from
     * the source when @c remove is set.  Returns {name}.
     */
    QByteArray handleUnstack(const QString &path,
                             const QHash<QString, QString> &params,
                             const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/stack
     *
     * Body {sources:[path,...], insertPage}.  Appends the pages of
     * each source stack into the target at 1-based insertPage (or the
     * end), deleting the source files.  All stacks must be the same
     * type.
     */
    QByteArray handleStack(const QString &path,
                           const QHash<QString, QString> &params,
                           const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/upload
     *
     * The raw request body is the whole file's bytes (e.g. a stack
     * scanned on a client).  Writes it at {path}; a name clash gets a
     * fresh name.  Returns {name, etag} so the caller can label its
     * cached copy as current.
     */
    QByteArray handleUpload(const QString &path,
                            const QHash<QString, QString> &params,
                            const QString &authedUser);

    /**
     * Handle POST /v1/repos/{repo}/stacks/{path}/duplicate
     *
     * Body {}.  Copies the stack's file within its directory under a
     * fresh name and returns {name}.  Format conversion is not
     * supported here yet.
     */
    QByteArray handleDuplicate(const QString &path,
                               const QHash<QString, QString> &params,
                               const QString &authedUser);

    /** Name of the shared trash directory within a repository. */
    static const char *trashDirName() { return ".maxview-trash"; }

    /** Find a filename not present in @p dir by appending _1, _2...
     *  to @p base.  Returns base+ext unchanged if that is free. */
    static QString uniqueNameIn(const QString &dir, const QString &base,
                                const QString &ext);

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
     * Get list of all repositories.
     *
     * @param user  If non-empty, restrict the list to repositories the
     *              named user is permitted to see (per the UserStore
     *              allowlist).  Empty means "no per-user filter", as
     *              used for API-key callers and pre-auth callers.
     * @return JSON response with repository list
     */
    QString listRepositories(const QString &user = QString());

    /**
     * Get file content
     * @param repoPath       Repository root path
     * @param filePath       File path (relative to repository root)
     * @param type           Output type ("original" or "pdf")
     * @param page           Extract single page (0 = return whole file)
     * @param wantPageCount  Return page count as JSON instead of file data
     * @param client         Client socket (needed for async PDF extraction)
     * @param ifNoneMatch    Client's If-None-Match header; when it matches
     *                       the file's current ETag a whole-file request
     *                       returns 304 with no body
     * @return HTTP response, or empty QByteArray if the response is deferred
     */
    QByteArray getFile(const QString &repoPath, const QString &filePath,
                       const QString &type = "original", int page = 0,
                       bool wantPageCount = false,
                       QTcpSocket *client = nullptr,
                       const QString &ifNoneMatch = QString());

    /** ETag for a file: its size and mtime, quoted.  Any change the
     *  server can observe changes the tag. */
    static QString fileEtag(const QFileInfo &info);

    /**
     * Convert a non-PDF file to PDF, caching the result
     * @param fullPath Absolute path to the source file
     * @param client If non-null, abort when this client disconnects
     * @return Path to cached PDF, or empty string on failure
     */
    QString convertToPdf(const QString &fullPath,
                         QTcpSocket *client = nullptr);

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

    /**
     * Get page count of a PDF file using pdfinfo
     * @param pdfPath Path to PDF file
     * @return Number of pages, or -1 on failure
     */
    int getPdfPageCount(const QString &pdfPath);

    /**
     * Extract a single page from a PDF using pdftocairo (synchronous)
     * @param pdfPath    Path to source PDF file
     * @param page       Page number (1-based)
     * @param outputPath Output path for single-page PDF
     * @return true if successful
     */
    bool extractPdfPage(const QString &pdfPath, int page,
                        const QString &outputPath);

    /**
     * Launch an async Ghostscript extraction for a PDF page
     * @param pdfPath    Absolute path to source PDF
     * @param page       Page number (1-based)
     * @param outputPath Cache path for extracted page
     * @param filePath   Relative file path (for logging)
     * @param client     Client socket awaiting the result
     */
    void startAsyncExtraction(const QString &pdfPath, int page,
                              const QString &outputPath,
                              const QString &filePath,
                              QTcpSocket *client);

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
     * Stream a file to a client socket with HTTP headers.
     *
     * Writes headers first, then streams the file in 512 KB chunks
     * to avoid buffering the entire file in memory.
     *
     * @param filePath    Path to the file to send
     * @param contentType MIME type for the Content-Type header
     * @param client      Client socket to write to
     * @param etag        ETag header value to send (omitted if empty)
     */
    void streamFile(const QString &filePath,
                    const QString &contentType,
                    QTcpSocket *client,
                    const QString &etag = QString());

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

public:
    ServerLog _log;         //!< Request log

private:
    /**
     * Load the persistent server ID (a UUID) from disk, or create one on
     * first run. The ID lives in ~/.config/paperman-server/server-id and is
     * returned to clients via /v1/status so they can key their local caches
     * on it.
     */
    QString loadOrCreateServerId();

    QString _rootPath;      //!< Root path of paper repository (deprecated, use _rootPaths)
    QStringList _rootPaths; //!< List of root paths of paper repositories
    quint16 _port;          //!< Port to listen on
    QString _serverId;      //!< Stable per-server UUID, persisted across runs
    QList<QTcpSocket*> _clients;  //!< Connected clients
    QString _apiKey;        //!< API key for authentication (from PAPERMAN_API_KEY env var)
    UserStore _users;       //!< Per-user account store
    TokenStore _tokens;     //!< In-memory bearer tokens
    std::unique_ptr<LocalBackend> _backend;  //!< Data-source (owns the file cache)
    QFileSystemWatcher *_fsWatcher;  //!< File system watcher for automatic cache updates
    QMap<QString, PendingExtraction> _pendingExtractions;  //!< In-flight gs extractions keyed by cache path

    /** Tracks progress of an in-flight PDF conversion */
    struct ConvertProgress {
        int currentPage;  //!< Last completed page (0 = not started)
        int totalPages;   //!< Total pages in the source file
    };
    QHash<QString, ConvertProgress> _convertProgress;  //!< Keyed by source path

    /** Recovery data held for a pages/delete call until its undoId is
     *  redeemed (or the server restarts) */
    struct PageUndo {
        QString fullPath;   //!< Absolute path of the mutated file
        QBitArray pages;    //!< Which pages were removed
        QByteArray delInfo; //!< Opaque recovery blob from removePages()
        int count;          //!< Number of removed pages
    };
    QHash<QString, PageUndo> _pageUndos;  //!< Keyed by undoId
};

#endif // __searchserver_h
