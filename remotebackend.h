/*
License: GPL-2
*/

#ifndef REMOTEBACKEND_H
#define REMOTEBACKEND_H

#include "backend.h"

#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;


/** A single entry in a directory listing: either a file or a
 *  subdirectory.  Mirrors the JSON shape /browse returns. */
struct DirectoryEntry
{
    QString name;
    QString path;        //!< Repo-relative path
    qint64 size = 0;     //!< 0 for directories
    bool isDir = false;
    QString modified;    //!< ISO-8601 (files only)
};

struct DirectoryListing
{
    QList<DirectoryEntry> entries;
};


/**
 * Backend that talks to a paperman-server over HTTP.  Symmetric with
 * LocalBackend but the data comes from the network instead of the
 * local filesystem, so the GUI can transparently use either one.
 *
 * Calls are synchronous: each method spins a nested QEventLoop until
 * the QNetworkReply finishes.  That matches the existing GUI's
 * blocking call style and keeps tests straightforward; an async
 * variant can come later.
 *
 * RemoteBackend manages a bearer token internally: after a successful
 * login() the token is attached as `Authorization: Bearer ...` to all
 * subsequent requests.
 */
class RemoteBackend : public Backend
{
public:
    explicit RemoteBackend(const QUrl &baseUrl);
    ~RemoteBackend() override;

    /** Exchange credentials for a bearer token.  Returns true on
     *  success; on failure, lastError() carries detail. */
    bool login(const QString &user, const QString &password);

    /** Fetch /v1/status and return the server's stable UUID.  Empty
     *  string on failure (lastError() set).  Result is cached. */
    QString serverId();

    bool isAuthenticated() const { return !_token.isEmpty(); }

    /** Inject a token directly (e.g. one loaded from a config file). */
    void setBearerToken(const QString &token) { _token = token; }

    /** The bearer token currently in use (empty if not authenticated). */
    QString bearerToken() const { return _token; }

    /** Last network or server-side error, if any. */
    QString lastError() const { return _lastError; }

    // Backend
    QList<RepositoryInfo> listRepositories() override;

    /** List the contents of `dir` (relative to the named repository).
     *  Empty `dir` lists the repo root.  Directories come first in the
     *  returned list. */
    DirectoryListing browseDirectory(const QString &repo,
                                     const QString &dir);

private:
    QByteArray getRequest(const QString &pathAndQuery);
    QByteArray postRequest(const QString &path, const QByteArray &body);
    QByteArray waitForReply(QNetworkReply *reply);

    QUrl _baseUrl;
    QString _token;
    QString _serverId;
    QString _lastError;
    QNetworkAccessManager *_nam;
};

#endif // REMOTEBACKEND_H
