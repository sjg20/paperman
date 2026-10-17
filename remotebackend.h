/*
License: GPL-2
*/

#ifndef REMOTEBACKEND_H
#define REMOTEBACKEND_H

#include "backend.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

#include <functional>

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

    //! the server's URL, as given to the constructor
    QUrl baseUrl() const { return _baseUrl; }
    ~RemoteBackend() override;

    /** What a request to the server came back with */
    struct Result
    {
        bool ok = false;     //!< true if the request worked
        QString error;       //!< why not, if it did not
        int status = 0;      //!< the HTTP status; 0 if none came back
        QByteArray body;     //!< what the server sent

        //! the body as a JSON object; empty if it is not one
        QJsonObject json() const;
    };

    //! called with what a request came back with
    using Done = std::function<void (const Result &)>;

    /** Exchange credentials for a bearer token.  Returns true on
     *  success; on failure, lastError() carries detail. */
    bool login(const QString &user, const QString &password);

    /** Log in without waiting, keeping the token if it works.  @p done
     *  is called once the server answers, unless @p context has gone;
     *  the result's error says why a login failed. */
    void login(const QString &user, const QString &password,
               QObject *context, Done done);

    /** Fetch /v1/status without waiting, as serverId() does.  Once it
     *  has come serverId(), features(), apiVersion() and lacksV1Api()
     *  give what it said, and @p done is called, unless @p context has
     *  gone. */
    void fetchStatus(QObject *context, Done done);

    /** Fetch the list of repositories without waiting; read the result
     *  with parseRepositories() */
    void fetchRepositories(QObject *context, Done done);

    //! the repositories in the body of a reply to fetchRepositories()
    static QList<RepositoryInfo> parseRepositories(const QByteArray &body);

    /** Fetch /v1/status and return the server's stable UUID.  Empty
     *  string on failure (lastError() set).  Result is cached. */
    QString serverId();

    bool isAuthenticated() const { return !_token.isEmpty(); }

    /** Attach a BackendStats object for activity reporting.  Optional;
     *  if unset the backend doesn't report anywhere.  Non-owning. */
    void setStats(class BackendStats *stats) { _stats = stats; }

    /** Inject a token directly (e.g. one loaded from a config file). */
    void setBearerToken(const QString &token) { _token = token; }

    /** Supply the server's id, so it need not be fetched.  serverId()
     *  blocks on /v1/status when it has none, which an asynchronous
     *  caller must avoid: a nested event loop there cancels replies
     *  that are already in flight.  Each repository gets its own
     *  backend, so pass the id the first one discovered to the rest. */
    void setServerId(const QString &id) { _serverId = id; }

    /** The server's id if it is known, without fetching it: empty
     *  until serverId() or fetchStatus() has had it from the server */
    QString knownServerId() const { return _serverId; }

    /** Supply the server's id and its advertised features together,
     *  as one backend learns them from /v1/status and hands them to
     *  the backends made for each repository. */
    void setServerInfo(const QString &id, const QStringList &features)
    {
        _serverId = id;
        _features = features;
    }

    /** The features /v1/status advertised; empty until serverId() has
     *  fetched it or setServerInfo() has been called. */
    QStringList features() const { return _features; }

    /** The API version /v1/status gave, such as "1"; empty until
     *  serverId() has fetched it. */
    QString apiVersion() const { return _apiVersion; }

    /** The newest major API version this client speaks; a server with a
     *  newer one may have changed what it expects, so it is not used. */
    static const int kApiVersion = 1;

    /** True if the server answered, but has no /v1/status: a server
     *  from before the /v1 API, which arrived in paperman-server 1.4.0 */
    bool lacksV1Api() const { return _lacksV1; }

    /** True if the server serves a stack a page at a time (the "pages"
     *  feature): /info for its structure and /pages/{n} for one page.
     *  Without it a stack has to be fetched whole. */
    bool hasPageRoutes() const { return _features.contains(QStringLiteral("pages")); }

    /** True if the server keeps an index of the text on its stacks'
     *  pages and can search it (the "textSearch" feature). */
    bool hasTextSearch() const { return _features.contains(QStringLiteral("textSearch")); }

    //! a stack found by searchText()
    struct TextHit
       {
       QString path;       //!< the stack, relative to the repository
       int page = 0;       //!< the page which best matches, from 0
       QString snippet;    //!< the text around the words, as HTML
       };

    /** Search the text on the pages of a repository's stacks, through
     *  the server's index.
     *
     *  @param repo      the repository
     *  @param text      the words, as the user typed them
     *  @param dir       the folder to look in, relative to the
     *                   repository, with its subfolders; empty for all
     *  @param maxHits   the most stacks to return
     *  @param context   what the reply is tied to; nothing is called
     *                   once it has gone
     *  @param done      called with an empty error and each stack found
     *                   once, best first, or with why the search failed;
     *                   its complete is false while the server is still
     *                   building its index, so may lack stacks */
    using TextDone = std::function<void(const QString &error,
                                        const QList<TextHit> &hits,
                                        bool complete)>;
    void searchText(const QString &repo, const QString &text,
                    const QString &dir, int maxHits, QObject *context,
                    TextDone done);

    /** Search the names of a repository's stacks for @p text, which may
     *  be any part of a name, ignoring case
     *
     *  @param repo      the repository
     *  @param text      what to look for
     *  @param dir       the folder to look in, relative to the
     *                   repository, with its subfolders; empty for all
     *  @param context   what the reply is tied to; nothing is called
     *                   once it has gone
     *  @param done      called with an empty error and the path of each
     *                   stack found, relative to the repository, or with
     *                   why the search failed */
    using NamesDone = std::function<void(const QString &error,
                                         const QStringList &paths)>;
    void searchNames(const QString &repo, const QString &text,
                     const QString &dir, QObject *context, NamesDone done);

    /** True if the server converts and unfolds stacks itself (the
     *  "convert" feature), so convertStack() and unfoldStack() work. */
    bool hasConvert() const { return _features.contains(QStringLiteral("convert")); }

    /** True if the server reads the pages of its stacks in the
     *  background (the "readPages" feature), so readStack() works. */
    bool hasReadPages() const { return _features.contains(QStringLiteral("readPages")); }

    /** Ask the server to read the pages of a stack which have not been
     *  read, before its other stacks.  The server answers at once; the
     *  words arrive with the stack once read, as a change to it.  Once
     *  answered, @p done is called unless @p context has gone */
    void readStack(const QString &repo, const QString &path,
                   QObject *context, Done done);

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
     *  tokens to its own state (e.g. the DirNode being expanded).
     *  A listing of the same folder already on its way is shared,
     *  rather than asked for again, each token having it emitted */
    quint64 browseDirectoryAsync(const QString &repo, const QString &dir);

    /** List a folder without waiting, calling @p done with the listing
     *  once it has come, unless @p context has gone by then */
    void browseDirectory(const QString &repo, const QString &dir,
                         QObject *context,
                         std::function<void (const DirectoryListing &)> done);

    /** Fetch a thumbnail (JPEG bytes) for a single page of a file.
     *  @p size is "small", "medium", or "large" (matches the server's
     *  /thumbnail vocabulary).  Sync: blocks until the server
     *  responds.  Returns empty QByteArray on failure (lastError set). */
    QByteArray fetchThumbnail(const QString &repo, const QString &path,
                              int page = 1,
                              const QString &size = QStringLiteral("medium"));

    /** Async counterpart to fetchThumbnail.  Returns a token; emits
     *  thumbnailReady(token, bytes) when the reply arrives.  Empty
     *  bytes mean the request failed.
     *
     *  @param version  what the server's listing says of the stack, such
     *                  as its time and size; if given, a thumbnail of
     *                  that version comes from the cache, without asking
     *                  the server, and one fetched is kept there */
    quint64 fetchThumbnailAsync(const QString &repo, const QString &path,
                                int page = 1,
                                const QString &size
                                    = QStringLiteral("medium"),
                                const QString &version = QString());

    /** Rotate or flip a page of a stack on the server.  @p page is
     *  1-based; a value below 1 transforms every page.  @p op is the
     *  wire name of the transform: one of "rotate90", "rotate180",
     *  "rotate270", "hflip", "vflip".  Sync: blocks until the server
     *  responds.  Returns true on success; on failure lastError() is
     *  set. */
    bool transformPage(const QString &repo, const QString &path,
                       int page, const QString &op);

    /* Operations on a stack which do not wait, each calling @p done once
       the server has answered, unless @p context has gone by then.  The
       result is a failure if the server says the operation failed, and
       its JSON carries what the server sent back, as for the forms which
       wait: */

    //! rename a stack; the JSON's "name" is the name it got
    void renameStack(const QString &repo, const QString &path,
                     const QString &newName, bool autoRename,
                     QObject *context, Done done);

    //! rename a page, numbered from 1
    void renamePage(const QString &repo, const QString &path, int page,
                    const QString &newName, QObject *context, Done done);

    //! move or copy a stack to a folder; the JSON's "name" is its name there
    void moveStack(const QString &repo, const QString &path,
                   const QString &destDir, bool copy, QObject *context,
                   Done done);

    //! delete a stack
    void deleteStack(const QString &repo, const QString &path,
                     QObject *context, Done done);

    //! change some of a stack's annotations
    void updateAnnotations(const QString &repo, const QString &path,
                           const QJsonObject &updates, QObject *context,
                           Done done);

    //! transform a page, numbered from 1, or all pages with 0
    void transformPage(const QString &repo, const QString &path, int page,
                       const QString &op, QObject *context, Done done);

    /** delete pages, numbered from 1; the JSON's "undoId" is what
     *  undeletePages() needs to bring them back */
    void deletePages(const QString &repo, const QString &path,
                     const QList<int> &pages, QObject *context, Done done);

    //! bring back pages deleted by deletePages()
    void undeletePages(const QString &repo, const QString &path,
                       const QString &undoId, QObject *context, Done done);

    /** copy or move @p pagecount pages from @p pagenum (from 1) to a new
     *  stack; the JSON's "name" is the new stack's */
    void unstackStack(const QString &repo, const QString &path, int pagenum,
                      int pagecount, bool remove,
                      const QString &suggestedName, QObject *context,
                      Done done);

    //! put the pages of @p sources into a stack, at @p insertPage
    void stackStacks(const QString &repo, const QString &destPath,
                     const QStringList &sources, int insertPage,
                     QObject *context, Done done);

    //! copy a stack; the JSON's "name" is the copy's
    void duplicateStack(const QString &repo, const QString &path,
                        QObject *context, Done done);

    //! convert a stack to another type; the JSON's "name" is the new one's
    void convertStack(const QString &repo, const QString &path,
                      const QString &type, int oddEven, QObject *context,
                      Done done);

    //! unfold a stack of booklet pages; the JSON's "name" is the new one's
    void unfoldStack(const QString &repo, const QString &path,
                     QObject *context, Done done);

    /** Fetch a stack's structure without keeping it, as an operation
     *  needing what the stack is now does; the JSON's "pagecount" is how
     *  many pages it has */
    void stackStructure(const QString &repo, const QString &path,
                        QObject *context, Done done);

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

    /** Make a stack of another type, and of some of the pages, from a
     *  stack, beside it on the server, named after it with _copy.
     *  Needs hasConvert().
     *
     *  @param type     the type's extension, such as "pdf"
     *  @param oddEven  1 for the odd pages, 2 for the even, 3 for all
     *  @param newName  returns the new stack's name */
    bool convertStack(const QString &repo, const QString &path,
                      const QString &type, int oddEven, QString *newName);

    /** Unfold a booklet into a new .max stack beside it on the server,
     *  each page split into two in reading order.  Needs hasConvert().
     *
     *  @param newName  returns the new stack's name */
    bool unfoldStack(const QString &repo, const QString &path,
                     QString *newName);

    /** Upload a whole file's bytes to the server at @p path (e.g. a
     *  stack scanned into a remote desk).  The server never
     *  overwrites unless @p overwrite is set (used for the shared
     *  desk file): on a name clash @p finalName differs from the
     *  requested one.  @p etag returns the stored file's validator,
     *  ready for the cache sidecar. */
    bool uploadFile(const QString &repo, const QString &path,
                    const QByteArray &bytes, QString *finalName,
                    QString *etag, bool overwrite = false);

    /** Upload a file without waiting, as uploadFile() does.  Once done,
     *  @p done is called unless @p context has gone; the result's JSON
     *  gives the file's final "name" and its "etag" */
    void uploadFile(const QString &repo, const QString &path,
                    const QByteArray &bytes, bool overwrite,
                    QObject *context, Done done);

    /** Run the server's OCR engine over 1-based page @p page of a
     *  stack.  The server keeps the page's words with the page, or
     *  where the stack cannot, stores the text in the stack's ocr
     *  annotation.  Once done, @p done is called unless @p context has
     *  gone; the result's JSON gives the "text" and, if the server kept
     *  them, the "words" */
    void ocrPage(const QString &repo, const QString &path, int page,
                 QObject *context, Done done);

    /** Open (and keep open) the server's event stream for @p repo.
     *  Each change made by another client is emitted as stackEvent();
     *  our own changes are filtered out by client id.  The stream
     *  reconnects automatically if it drops.  Idempotent. */
    void subscribeEvents(const QString &repo);

    /** Random id identifying this backend instance; sent with every
     *  request so the server can mark events with their origin. */
    QString clientId() const { return _clientId; }

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

    /** Download a whole file of a repository to @p dest, outside the
     *  cache, as a copy to work on which nothing else is reading.  Once
     *  done, @p done is called unless @p context has gone */
    void downloadFile(const QString &repo, const QString &relPath,
                      const QString &dest, QObject *context, Done done);

    /** Async counterpart to ensureCachedFile().  Returns a token at
     *  once and emits cachedFileReady() when the cached copy is
     *  current, so a caller on the GUI thread never blocks and the
     *  network is only ever touched from the thread owning this
     *  object.  An empty path in the signal means the fetch failed
     *  (lastError() set). */
    quint64 ensureCachedFileAsync(const QString &repo,
                                  const QString &relPath);

    /** Bring a file's cached copy up to date without waiting, as
     *  ensureCachedFile() does, calling @p done with its path (empty if
     *  there is none) once done, unless @p context has gone by then */
    void ensureCachedFile(const QString &repo, const QString &relPath,
                          QObject *context,
                          std::function<void (const QString &cachePath,
                                              bool refreshed)> done);

    /** Forget any cached copy of @p relPath, whether fetched whole
     *  (bytes and validator) or a page at a time (the page directory),
     *  so the next fetch downloads afresh. */
    void invalidateCachedFile(const QString &repo, const QString &relPath);

    /** Directory holding a stack fetched a page at a time: the whole-
     *  file cache path with ".d" appended.  It holds info.json, as
     *  /info returns it, and page-N.max for each page fetched (N is
     *  1-based, matching Filemax::pageFileName()).  Empty if the cache
     *  root is unknown.  Not created. */
    QString pageDirFor(const QString &repo, const QString &relPath);

    /** Fetch a stack's structure into <pageDir>/info.json, with each
     *  page's OCR text if @p withText.  Sync; returns the page
     *  directory, or empty on failure (lastError() set). */
    QString fetchStackInfo(const QString &repo, const QString &relPath,
                           bool withText = false);

    /** Async counterpart: emits stackInfoReady(token, pageDir), with
     *  an empty directory on failure. */
    quint64 fetchStackInfoAsync(const QString &repo, const QString &relPath,
                                bool withText = false);

    /** Fetch one page (1-based) into <pageDir>/page-N.max.  Sync;
     *  returns the file's path, or empty on failure. */
    QString fetchPage(const QString &repo, const QString &relPath, int page);

    /** Async counterpart: emits pageReady(token, pagePath), empty on
     *  failure. */
    quint64 fetchPageAsync(const QString &repo, const QString &relPath,
                           int page);

    /** Forget every fetched page of a stack, and its info.json. */
    void invalidatePageDir(const QString &repo, const QString &relPath);

    /** Forget one fetched page, so it is downloaded afresh. */
    void invalidatePage(const QString &repo, const QString &relPath,
                        int page);

signals:
    void browseDirectoryReady(quint64 token,
                              const DirectoryListing &listing);
    void thumbnailReady(quint64 token, const QByteArray &jpegBytes);

    /** A file requested with ensureCachedFileAsync() is now cached.
     *  @p cachePath is empty if the fetch failed; @p refreshed is true
     *  when new bytes were written, so any parsed state is stale. */
    void cachedFileReady(quint64 token, const QString &cachePath,
                         bool refreshed);

    /** A stack's info.json is in place (fetchStackInfoAsync); the
     *  directory is empty if the fetch failed. */
    void stackInfoReady(quint64 token, const QString &pageDir);

    /** One page's file is in place (fetchPageAsync); empty on failure. */
    void pageReady(quint64 token, const QString &pagePath);

    /** A change made by another client arrived on the event stream */
    void stackEvent(const QString &repo, const QString &op,
                    const QString &path, const QString &name);

private:
    /** Run @p work, which must touch nothing but files, on a worker
     *  thread, and call @p done back on this object's thread with what
     *  it returns: an empty string, or why it failed */
    void inBackground(std::function<QString()> work,
                      std::function<void(const QString &error)> done);

    /** Write @p bytes to @p path in the background, since syncing a
     *  file to disk can take a while, and call @p done back on this
     *  object's thread with an empty string, or why it failed.  A write
     *  which fails is tried once more, and logged if that fails too.
     *  Until it is done, the bytes are kept in _writing, for a reader
     *  who wants them meanwhile */
    void writeLater(const QString &path, const QByteArray &bytes,
                    std::function<void(const QString &error)> done);

    /** Write downloaded bytes and their validator into the cache,
     *  returning the cache pathname (empty on failure). */
    QString storeCachedFile(const QString &cachePath, const QByteArray &body,
                            const QString &newEtag, bool *refreshed);

    QByteArray getRequest(const QString &pathAndQuery);
    QByteArray postRequest(const QString &path, const QByteArray &body,
                           const QString &contentType
                               = QStringLiteral("application/json"),
                           int timeoutMs = 0);
    /** Wait for @p reply to finish, spinning an event loop; noted with
     *  BlockWatch, since the desktop should not wait */
    void waitForFinish(QNetworkReply *reply);

    /** Call @p done with what @p reply comes back with once it
     *  finishes, unless @p context has gone by then */
    void whenFinished(QNetworkReply *reply, QObject *context, Done done);

    //! take what the server said in reply to /v1/status
    void takeStatus(int status, const QByteArray &body);

    /** Take the token from a reply to a login, returning why it failed
     *  if it did; @p httpError is the request's own error, if any */
    QString takeLogin(const QByteArray &resp, const QString &httpError);
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

    /** Post an operation on a stack without waiting, calling @p done
     *  with a failure unless the server says it worked */
    void postStackOp(const QString &repo, const QString &path,
                     const QString &verb, const QJsonObject &body,
                     QObject *context, Done done);

    /** Construct an async POST of @p body to @p path, as startGet()
     *  does a GET.  @p timeoutMs overrides the default per-request
     *  transfer timeout, which is too short for an operation the server
     *  takes a while over, such as reading a page. */
    QNetworkReply *startPost(const QString &path, const QByteArray &body,
                             const QString &contentType
                                 = QStringLiteral("application/json"),
                             int timeoutMs = 0);

    /** Construct an async GET against pathAndQuery.  Caller takes
     *  responsibility for the returned reply's signals; reply will
     *  call deleteLater on itself once consumed.  @p ifNoneMatch, when
     *  non-empty, is sent as an If-None-Match header.  @p timeoutMs
     *  overrides the default per-request transfer timeout, which is far
     *  too short for fetching a whole stack. */
    QNetworkReply *startGet(const QString &pathAndQuery,
                            const QString &ifNoneMatch = QString(),
                            int timeoutMs = 0);

    /** Translate a /browse reply (body bytes + reply error state)
     *  into a typed DirectoryListing.  Shared by sync and async
     *  paths. */
    DirectoryListing parseBrowseReply(QNetworkReply *reply);

    /** (Re)connect the event stream for one repo. */
    void startEventStream(const QString &repo);

    /** Follow @p reply in the stats, if any: what @p path is for, which
     *  is a POST if @p post, and @p sent bytes for the request */
    void track(QNetworkReply *reply, const QString &path, bool post,
               qint64 sent);

    QUrl _baseUrl;
    QString _token;
    QString _serverId;
    QStringList _features;   //!< from /v1/status
    QString _apiVersion;     //!< from /v1/status
    bool _lacksV1 = false;   //!< see lacksV1Api()
    QString _clientId;
    QString _lastError;
    QNetworkAccessManager *_nam;
    class BackendStats *_stats = nullptr;
    quint64 _nextAsyncToken = 1;

    /** the tokens waiting for each listing on its way, by repository and
     *  folder, see browseDirectoryAsync() */
    QHash<QString, QList<quint64> > _browsesInFlight;
    QSet<QString> _eventRepos;  //!< repos with an event subscription
    QHash<QString, QByteArray> _writing; //!< see writeLater()
};

#endif // REMOTEBACKEND_H
