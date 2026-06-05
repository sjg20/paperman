/*
License: GPL-2
*/

#ifndef REMOTEBACKEND_H
#define REMOTEBACKEND_H

#include "backend.h"

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;


/**
 * Backend that talks to a paperman-server over HTTP.  Symmetric with
 * LocalBackend but the data comes from the network instead of the
 * local filesystem, so the GUI can transparently use either one.
 *
 * Two call styles:
 *  - The sync methods (listRepositories, browseDirectory, readFile,
 *    login) spin a nested QEventLoop until the reply arrives; that
 *    matches the existing GUI's blocking call style and keeps tests
 *    straightforward.
 *  - browseDirectoryAsync returns a token and emits
 *    browseDirectoryReady when the server responds, so the UI
 *    thread isn't blocked on each folder expansion.  Other endpoints
 *    will grow async variants as call sites demand them.
 *
 * RemoteBackend manages a bearer token internally: after a successful
 * login() the token is attached as `Authorization: Bearer ...` to all
 * subsequent requests.
 */
class RemoteBackend : public QObject, public Backend
{
    Q_OBJECT
public:
    explicit RemoteBackend(const QUrl &baseUrl, QObject *parent = nullptr);
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
    DirectoryListing browseDirectory(const QString &repo,
                                     const QString &dir) override;
    FileFetch readFile(const QString &repo,
                       const QString &path) override;

    /** Asynchronous version of browseDirectory.  Returns a
     *  monotonically-increasing token that identifies the request;
     *  when the server responds (or the request fails/times out) the
     *  browseDirectoryReady() signal is emitted with the same token
     *  and the parsed DirectoryListing.  The caller can correlate
     *  tokens to its own state (e.g. the DirNode being expanded). */
    quint64 browseDirectoryAsync(const QString &repo, const QString &dir);

    /** Fetch a thumbnail (JPEG bytes) for a single page of a file.
     *  @p size is "small", "medium", or "large" (matches the server's
     *  /thumbnail vocabulary).  Sync: blocks until the server
     *  responds.  Returns empty QByteArray on failure (lastError set). */
    QByteArray fetchThumbnail(const QString &repo, const QString &path,
                              int page = 1,
                              const QString &size = QStringLiteral("medium"));

    /** Async counterpart to fetchThumbnail.  Returns a token; emits
     *  thumbnailReady(token, bytes) when the reply arrives.  Empty
     *  bytes mean the request failed. */
    quint64 fetchThumbnailAsync(const QString &repo, const QString &path,
                                int page = 1,
                                const QString &size
                                    = QStringLiteral("medium"));

signals:
    void browseDirectoryReady(quint64 token,
                              const DirectoryListing &listing);
    void thumbnailReady(quint64 token, const QByteArray &jpegBytes);

private:
    QByteArray getRequest(const QString &pathAndQuery);
    QByteArray postRequest(const QString &path, const QByteArray &body);
    QByteArray waitForReply(QNetworkReply *reply);

    /** Construct an async GET against pathAndQuery.  Caller takes
     *  responsibility for the returned reply's signals; reply will
     *  call deleteLater on itself once consumed. */
    QNetworkReply *startGet(const QString &pathAndQuery);

    /** Translate a /browse reply (body bytes + reply error state)
     *  into a typed DirectoryListing.  Shared by sync and async
     *  paths. */
    DirectoryListing parseBrowseReply(QNetworkReply *reply);

    QUrl _baseUrl;
    QString _token;
    QString _serverId;
    QString _lastError;
    QNetworkAccessManager *_nam;
    quint64 _nextAsyncToken = 1;
};

#endif // REMOTEBACKEND_H
