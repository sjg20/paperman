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

    /** Attach a BackendStats object for activity reporting.  Optional;
     *  if unset the backend doesn't report anywhere.  Non-owning. */
    void setStats(class BackendStats *stats) { _stats = stats; }

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

    /** Rotate or flip a page of a stack on the server.  @p page is
     *  1-based; a value below 1 transforms every page.  @p op is the
     *  wire name of the transform: one of "rotate90", "rotate180",
     *  "rotate270", "hflip", "vflip".  Sync: blocks until the server
     *  responds.  Returns true on success; on failure lastError() is
     *  set. */
    bool transformPage(const QString &repo, const QString &path,
                       int page, const QString &op);

    /** Rename a stack on the server.  On success @p newName holds the
     *  final name the server chose (with autoRename it may differ
     *  from the requested one).  Sync. */
    bool renameStack(const QString &repo, const QString &path,
                     QString &newName, bool autoRename = true);

    /** Rename a page of a stack on the server.  @p page is 1-based. */
    bool renamePage(const QString &repo, const QString &path,
                    int page, const QString &newName);

    /** Move (or copy) a stack to another directory in the same repo.
     *  @p destDir is repo-relative; "" means the root.  On success
     *  @p finalName holds the name in the destination, which differs
     *  from the original on a collision. */
    bool moveStack(const QString &repo, const QString &path,
                   const QString &destDir, bool copy, QString *finalName);

    /** Delete a stack outright (no trash). */
    bool deleteStack(const QString &repo, const QString &path);

    /** Write annotation fields into a stack on the server.  The
     *  object's keys are wire annotation names (author/title/
     *  keywords/notes/ocr) and the values the new text. */
    bool updateAnnotations(const QString &repo, const QString &path,
                           const class QJsonObject &updates);

    /** Delete pages (1-based) from a stack on the server.  On success
     *  @p undoId holds an opaque token which undeletePages() can
     *  redeem to restore them; the server may forget it after a
     *  while, so treat the undo as best-effort. */
    bool deletePages(const QString &repo, const QString &path,
                     const QList<int> &pages, QString *undoId);

    /** Restore the pages removed by the deletePages() call that
     *  returned @p undoId. */
    bool undeletePages(const QString &repo, const QString &path,
                       const QString &undoId);

    /** Split @p pagecount pages starting at 1-based @p pagenum out of
     *  a stack into a new stack in the same directory, removing them
     *  from the source when @p remove is set.  @p suggestedName may
     *  be empty; @p newName returns the name the server chose. */
    bool unstackStack(const QString &repo, const QString &path,
                      int pagenum, int pagecount, bool remove,
                      const QString &suggestedName, QString *newName);

    /** Append the pages of each source stack (repo-relative paths)
     *  into @p destPath at 1-based @p insertPage (0 or past the end
     *  appends), deleting the sources. */
    bool stackStacks(const QString &repo, const QString &destPath,
                     const QStringList &sources, int insertPage);

    /** Copy a stack's file within its directory under a fresh name,
     *  returned in @p newName. */
    bool duplicateStack(const QString &repo, const QString &path,
                        QString *newName);

    /** Upload a whole file's bytes to the server at @p path (e.g. a
     *  stack scanned into a remote desk).  The server never
     *  overwrites: on a name clash @p finalName differs from the
     *  requested one.  @p etag returns the stored file's validator,
     *  ready for the cache sidecar. */
    bool uploadFile(const QString &repo, const QString &path,
                    const QByteArray &bytes, QString *finalName,
                    QString *etag);

    /** Name of the shared per-repo trash directory on the server. */
    static QString trashDirName() { return QStringLiteral(".maxview-trash"); }

    /** Root of this server's on-disk file cache:
     *  <cache>/paperman/<serverId>.  Empty if the server id cannot be
     *  fetched (e.g. server unreachable and never seen before). */
    QString cacheRoot();

    /** Directory caching files under @p dirInRepo of @p repo, without
     *  a trailing slash.  Empty when cacheRoot() is empty.  The
     *  directory is not created. */
    QString cacheDirFor(const QString &repo, const QString &dirInRepo);

    /** Cache pathname for a single repo file. */
    QString cachePathFor(const QString &repo, const QString &relPath);

    /** Make sure the cache holds a current copy of @p relPath in
     *  @p repo: revalidate an existing copy with If-None-Match (a 304
     *  costs no transfer) or download the bytes.  If the server cannot
     *  be reached but a cached copy exists, that copy is returned so
     *  already-fetched stacks stay viewable.  Returns the cache
     *  pathname, or an empty string on failure (lastError() set).
     *  @p refreshed, if non-null, is set true when new bytes were
     *  written (so any parsed state is stale) and false when the
     *  existing copy was already current. */
    QString ensureCachedFile(const QString &repo, const QString &relPath,
                             bool *refreshed = nullptr);

    /** Forget any cached copy of @p relPath (bytes and validator), so
     *  the next ensureCachedFile() downloads afresh. */
    void invalidateCachedFile(const QString &repo, const QString &relPath);

signals:
    void browseDirectoryReady(quint64 token,
                              const DirectoryListing &listing);
    void thumbnailReady(quint64 token, const QByteArray &jpegBytes);

private:
    QByteArray getRequest(const QString &pathAndQuery);
    QByteArray postRequest(const QString &path, const QByteArray &body,
                           const QString &contentType
                               = QStringLiteral("application/json"));
    QByteArray waitForReply(QNetworkReply *reply);

    /** waitForReply() variant that also surfaces the HTTP status code
     *  and the response's ETag header (empty when absent).  A 304 is
     *  not an error. */
    QByteArray waitForReplyFull(QNetworkReply *reply, int *status,
                                QString *etag);

    /** POST a JSON body to /v1/repos/{repo}/stacks/{path}{verb} and
     *  parse the JSON reply into *out (optional).  Returns the reply's
     *  success flag; on failure lastError() is set. */
    bool postStackOp(const QString &repo, const QString &path,
                     const QString &verb, const class QJsonObject &body,
                     class QJsonObject *out);

    /** Construct an async GET against pathAndQuery.  Caller takes
     *  responsibility for the returned reply's signals; reply will
     *  call deleteLater on itself once consumed.  @p ifNoneMatch, when
     *  non-empty, is sent as an If-None-Match header. */
    QNetworkReply *startGet(const QString &pathAndQuery,
                            const QString &ifNoneMatch = QString());

    /** Translate a /browse reply (body bytes + reply error state)
     *  into a typed DirectoryListing.  Shared by sync and async
     *  paths. */
    DirectoryListing parseBrowseReply(QNetworkReply *reply);

    QUrl _baseUrl;
    QString _token;
    QString _serverId;
    QString _lastError;
    QNetworkAccessManager *_nam;
    class BackendStats *_stats = nullptr;
    quint64 _nextAsyncToken = 1;
};

#endif // REMOTEBACKEND_H
