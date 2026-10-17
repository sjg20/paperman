/*
License: GPL-2
*/

#include "remotebackend.h"

#include "backendstats.h"
#include "blockwatch.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QThread>
#include <QDir>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

#include <memory>


/* Default transfer timeout, for a request which is a plain round-trip,
 * such as asking a server for its status: five seconds is long enough
 * for any honest LAN round-trip and short enough that a dead server
 * surfaces quickly.  Qt's own default is effectively unlimited */
static const int kRequestTimeoutMs = 5000;

/* Fetching a whole stack is not a round-trip: a large one takes tens of
 * seconds over a link slower than a LAN, and the timeout above cancels
 * it long before it arrives, leaving nothing cached and the stack
 * unreadable.  Qt counts the timeout from the last byte received, so
 * this only fires when the transfer really has stalled. */
static const int kFetchTimeoutMs = 120000;

/* Nor is an operation on a stack: the server sends nothing until it has
 * finished, and reading a page, or converting a long stack, takes it
 * longer than a round-trip.  Giving up first reports a failure for an
 * operation the server then completes.  The desktop does not wait for
 * these, so a slow server only holds up the operations behind it */
static const int kOperationTimeoutMs = 120000;

/* Nor is a request the server has to work for, such as listing a folder,
 * making a thumbnail or reading a stack's structure: its first listing of
 * a large folder takes it seconds, and it answers one request at a time,
 * so a request queued behind a few others waits well past a round-trip.
 * Giving up first leaves a folder showing no subfolders */
static const int kWorkTimeoutMs = 120000;


/* Read what a reply brought.  A request which has timed out has been
   aborted, which closes the reply, and reading a closed one only warns */
static QByteArray readReply(QNetworkReply *reply)
{
   return reply->isOpen() ? reply->readAll() : QByteArray();
}


RemoteBackend::RemoteBackend(const QUrl &baseUrl, QObject *parent)
   : QObject(parent)
   , _baseUrl(baseUrl)
   , _clientId(QUuid::createUuid().toString(QUuid::WithoutBraces))
   , _nam(new QNetworkAccessManager(this))
{
}


RemoteBackend::~RemoteBackend() = default;


bool RemoteBackend::login(const QString &user, const QString &password)
{
   QJsonObject body;
   body["user"]     = user;
   body["password"] = password;
   QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

   QByteArray resp = postRequest("/v1/auth/login", data);
   if (resp.isEmpty()) {
      if (_lastError.isEmpty())
         _lastError = "empty response from /v1/auth/login";
      return false;
   }

   _lastError = takeLogin(resp, _lastError);
   return _lastError.isEmpty();
}


void RemoteBackend::login(const QString &user, const QString &password,
                          QObject *context, Done done)
{
   QJsonObject body;
   body["user"]     = user;
   body["password"] = password;
   QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

   whenFinished(startPost("/v1/auth/login", data), context,
                [this, done](const Result &result) {
      Result out = result;
      out.error = result.body.isEmpty()
          ? (result.error.isEmpty() ? QStringLiteral("empty response from "
                                                     "/v1/auth/login")
                                    : result.error)
          : takeLogin(result.body, result.error);
      out.ok = out.error.isEmpty();
      done(out);
   });
}


QString RemoteBackend::takeLogin(const QByteArray &resp,
                                 const QString &httpError)
{
   /* A rejected login still carries a body, and the request has already
      given the HTTP status as its error.  Prefer the server's own message
      ("Invalid credentials") over anything we infer from the missing
      token, so the user is told what actually went wrong. */
   QJsonDocument doc = QJsonDocument::fromJson(resp);
   if (!doc.isObject())
      return httpError.isEmpty() ? QStringLiteral("login response is not a "
                                                  "JSON object")
                                 : httpError;

   QJsonObject obj = doc.object();
   QString token = obj.value("token").toString();
   if (token.isEmpty()) {
      QString serverError = obj.value("error").toString();
      if (!serverError.isEmpty())
         return serverError;
      return httpError.isEmpty() ? QStringLiteral("login response missing "
                                                  "token")
                                 : httpError;
   }

   _token = token;
   return QString();
}


QString RemoteBackend::serverId()
{
   if (!_serverId.isEmpty())
      return _serverId;

   int status = 0;
   QString etag;
   QByteArray body = waitForReplyFull(startGet("/v1/status"), &status, &etag);

   takeStatus(status, body);
   return _serverId;
}


void RemoteBackend::fetchStatus(QObject *context, Done done)
{
   whenFinished(startGet("/v1/status"), context,
                [this, done](const Result &result) {
      takeStatus(result.status, result.body);
      done(result);
   });
}


void RemoteBackend::takeStatus(int status, const QByteArray &body)
{
   // a server from before /v1 answers, but does not know the route
   _lacksV1 = status == 404;
   QJsonDocument doc = QJsonDocument::fromJson(body);
   if (!doc.isObject())
      return;
   _serverId = doc.object().value("serverId").toString();
   _apiVersion = doc.object().value("apiVersion").toString();
   _features.clear();
   const QJsonArray feats = doc.object().value("features").toArray();
   for (const QJsonValue &v : feats)
      _features.append(v.toString());
}


QList<RepositoryInfo> RemoteBackend::listRepositories()
{
   return parseRepositories(getRequest("/repos"));
}


void RemoteBackend::fetchRepositories(QObject *context, Done done)
{
   whenFinished(startGet("/repos", QString(), kWorkTimeoutMs), context, done);
}


QList<RepositoryInfo> RemoteBackend::parseRepositories(const QByteArray &body)
{
   QList<RepositoryInfo> out;

   QJsonDocument doc = QJsonDocument::fromJson(body);
   if (!doc.isObject())
      return out;

   QJsonArray arr = doc.object().value("repositories").toArray();
   for (const QJsonValue &v : arr) {
      QJsonObject o = v.toObject();
      RepositoryInfo r;
      r.name   = o.value("name").toString();
      r.path   = o.value("path").toString();
      r.exists = o.value("exists").toBool();
      out.append(r);
   }
   return out;
}


/* Build the "/browse?repo=...&path=..." path-with-query string. */
static QString browsePathFor(const QString &repo, const QString &dir)
{
   QUrlQuery q;
   q.addQueryItem("repo", repo);
   q.addQueryItem("path", dir);
   return "/browse?" + q.toString();
}


DirectoryListing RemoteBackend::browseDirectory(const QString &repo,
                                                const QString &dir)
{
   QNetworkReply *reply = startGet(browsePathFor(repo, dir), QString(),
                                   kWorkTimeoutMs);

   /* Wait until the reply finishes (sync), then hand the still-live
    * reply to the shared parser before scheduling its deletion. */
   waitForFinish(reply);

   DirectoryListing out = parseBrowseReply(reply);
   reply->deleteLater();
   return out;
}


void RemoteBackend::browseDirectory(const QString &repo, const QString &dir,
                                    QObject *context,
                                    std::function<void (const DirectoryListing &)>
                                       done)
{
   /* a connection which answers only for this listing, and goes once it
      has, or with the context */
   quint64 token = browseDirectoryAsync(repo, dir);
   auto conn = std::make_shared<QMetaObject::Connection>();
   *conn = connect(this, &RemoteBackend::browseDirectoryReady, context,
                   [token, conn, done](quint64 tok,
                                       const DirectoryListing &listing) {
      if (tok != token)
         return;
      QObject::disconnect(*conn);
      done(listing);
   });
}


quint64 RemoteBackend::browseDirectoryAsync(const QString &repo,
                                            const QString &dir)
{
   quint64 token = _nextAsyncToken++;

   // a listing of this folder on its way already does for this one too
   QString key = repo + '\n' + dir;
   auto pending = _browsesInFlight.find(key);

   if (pending != _browsesInFlight.end()) {
      pending->append(token);
      return token;
   }
   _browsesInFlight.insert(key, {token});

   QNetworkReply *reply = startGet(browsePathFor(repo, dir), QString(),
                                   kWorkTimeoutMs);
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, key]() {
          DirectoryListing listing = parseBrowseReply(reply);
          const QList<quint64> tokens = _browsesInFlight.take(key);

          reply->deleteLater();
          for (quint64 waiting : tokens)
             emit browseDirectoryReady(waiting, listing);
       });
   return token;
}


DirectoryListing RemoteBackend::parseBrowseReply(QNetworkReply *reply)
{
   DirectoryListing out;
   _lastError.clear();

   QByteArray body = readReply(reply);
   if (reply->error() != QNetworkReply::NoError) {
      _lastError = reply->errorString();
   } else {
      int status = reply->attribute(
                       QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (status >= 400)
         _lastError = QString("HTTP %1").arg(status);
   }
   if (!_lastError.isEmpty()) {
      out.error    = _lastError;
      out.notFound = _lastError.contains("404");
      return out;
   }

   QJsonDocument doc = QJsonDocument::fromJson(body);
   if (!doc.isObject()) {
      out.error = "malformed response";
      return out;
   }
   QJsonObject obj = doc.object();

   /* Subdirectories first, matching the on-screen ordering the GUI
    * uses today. */
   for (const QJsonValue &v : obj.value("directories").toArray()) {
      QJsonObject d = v.toObject();
      DirectoryEntry e;
      e.name  = d.value("name").toString();
      e.path  = d.value("path").toString();
      e.isDir = true;
      e.count = d.value("count").toInt();
      out.entries.append(e);
   }
   for (const QJsonValue &v : obj.value("files").toArray()) {
      QJsonObject f = v.toObject();
      DirectoryEntry e;
      e.name     = f.value("name").toString();
      e.path     = f.value("path").toString();
      e.size     = f.value("size").toVariant().toLongLong();
      e.modified = f.value("modified").toString();
      e.pages    = f.value("pages").toInt();
      out.entries.append(e);
   }
   out.ok = true;
   return out;
}


FileFetch RemoteBackend::readFile(const QString &repo, const QString &path)
{
   FileFetch out;

   QUrlQuery q;
   q.addQueryItem("repo", repo);
   q.addQueryItem("path", path);
   QString pq = "/file?" + q.toString();

   QByteArray body = getRequest(pq);
   if (!_lastError.isEmpty()) {
      out.error    = _lastError;
      out.notFound = _lastError.contains("404");
      return out;
   }

   /* The server already sent a Content-Type header; we don't surface
    * headers from getRequest() so derive it the same way the server
    * did, from the path extension. */
   out.bytes       = body;
   out.contentType = Backend::contentTypeForPath(path);
   out.ok          = true;
   return out;
}




/* Build the "/thumbnail?repo=...&path=...&page=N&size=..." path. */
static QString thumbnailPathFor(const QString &repo, const QString &path,
                                int page, const QString &size)
{
   QUrlQuery q;
   q.addQueryItem("repo", repo);
   q.addQueryItem("path", path);
   q.addQueryItem("page", QString::number(page));
   q.addQueryItem("size", size);
   return "/thumbnail?" + q.toString();
}


QByteArray RemoteBackend::fetchThumbnail(const QString &repo,
                                         const QString &path,
                                         int page, const QString &size)
{
   return getRequest(thumbnailPathFor(repo, path, page, size));
}


quint64 RemoteBackend::fetchThumbnailAsync(const QString &repo,
                                           const QString &path,
                                           int page, const QString &size,
                                           const QString &version)
{
   quint64 token = _nextAsyncToken++;

   /* a thumbnail of a version of a stack seen before is in the cache; the
      id must be known already, since finding it out would wait */
   QString cached;

   if (!version.isEmpty() && !_serverId.isEmpty()) {
      QByteArray key = QStringList({repo, path, QString::number(page), size,
                                    version}).join('\n').toUtf8();

      cached = cacheRoot() + "/thumbnails/"
               + QCryptographicHash::hash(key, QCryptographicHash::Sha1)
                    .toHex() + ".jpg";

      QFile in(cached);

      /* one still being written is not on disk yet, or not all of it:
         syncing it can take a while */
      auto writing = _writing.constFind(cached);

      if (writing != _writing.constEnd() || in.open(QIODevice::ReadOnly)) {
         QByteArray bytes = writing != _writing.constEnd() ? *writing
                                                           : in.readAll();

         QMetaObject::invokeMethod(this, [this, token, bytes]() {
            emit thumbnailReady(token, bytes);
         }, Qt::QueuedConnection);
         return token;
      }
   }

   QNetworkReply *reply = startGet(thumbnailPathFor(repo, path, page, size),
                                   QString(), kWorkTimeoutMs);
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, token, cached]() {
          _lastError.clear();
          QByteArray body = readReply(reply);
          if (reply->error() != QNetworkReply::NoError) {
             _lastError = reply->errorString();
             body.clear();
          } else {
             int status = reply->attribute(
                              QNetworkRequest::HttpStatusCodeAttribute).toInt();
             if (status >= 400) {
                _lastError = QString("HTTP %1").arg(status);
                body.clear();
             }
          }
          reply->deleteLater();
          if (!cached.isEmpty() && !body.isEmpty())
             writeLater(cached, body, [](const QString &) {});
          emit thumbnailReady(token, body);
       });
   return token;
}


QNetworkReply *RemoteBackend::startGet(const QString &pathAndQuery,
                                       const QString &ifNoneMatch,
                                       int timeoutMs)
{
   QUrl url(_baseUrl);
   int q = pathAndQuery.indexOf('?');
   if (q < 0) {
      url.setPath(pathAndQuery);
   } else {
      url.setPath(pathAndQuery.left(q));
      url.setQuery(pathAndQuery.mid(q + 1));
   }
   QNetworkRequest req(url);
   req.setTransferTimeout(timeoutMs > 0 ? timeoutMs : kRequestTimeoutMs);
   req.setRawHeader("X-Client-Id", _clientId.toUtf8());
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());
   if (!ifNoneMatch.isEmpty())
      req.setRawHeader("If-None-Match", ifNoneMatch.toUtf8());

   QNetworkReply *reply = _nam->get(req);

   /* Approximate the request size by the URL bytes.  Headers are
      uncounted; tolerable for a user-facing tally. */
   track(reply, pathAndQuery, false, pathAndQuery.toUtf8().size());
   return reply;
}


Q_LOGGING_CATEGORY(logNet, "paperman.net", QtWarningMsg)

/* Follow @reply, for @method on @path, in the log, when the paperman.net
   category is on: when it starts, when the server answers and how, and
   how it ends */
static void logRequest(QNetworkReply *reply, const char *method,
                       const QString &path)
{
   if (!logNet().isDebugEnabled())
      return;

   static quint64 next = 1;
   quint64 num = next++;
   auto timer = std::make_shared<QElapsedTimer>();

   timer->start();
   qCDebug(logNet).noquote() << QString("#%1 %2 %3%4").arg(num)
      .arg(QString::fromLatin1(method), reply->url().authority(), path);
   QObject::connect(reply, &QNetworkReply::metaDataChanged, reply,
                    [reply, num, timer]() {
      qCDebug(logNet).noquote() << QString("#%1 answered %2 in %3 ms")
         .arg(num).arg(reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt())
         .arg(timer->elapsed());
   });
   QObject::connect(reply, &QNetworkReply::finished, reply,
                    [reply, num, timer]() {
      QString how = reply->error() == QNetworkReply::NoError
         ? QStringLiteral("ok")
         : QString("error %1: %2").arg(int(reply->error()))
                                  .arg(reply->errorString());

      qCDebug(logNet).noquote() << QString("#%1 done in %2 ms, %3")
         .arg(num).arg(timer->elapsed()).arg(how);
   });
}


void RemoteBackend::track(QNetworkReply *reply, const QString &path,
                          bool post, qint64 sent)
{
   logRequest(reply, post ? "POST" : "GET", path);
   if (!_stats)
      return;

   BackendStats *stats = _stats;
   quint64 id = stats->requestStarted(BackendStats::kindOf(path, post));

   stats->recordSent(sent);

   /* Tie the callbacks to the reply, not to _stats: the stats object
      outlives this backend, so a connection owned by it keeps firing
      after the backend (and the reply it captured) have gone.  That
      happens on every startup, where the probe backend is a local whose
      replies are cancelled as it goes out of scope.  Capture the stats
      pointer rather than this, so nothing reaches the dead backend. */
   QObject::connect(reply, &QNetworkReply::metaDataChanged, reply,
       [stats, id]() { stats->requestAnswered(id); });
   QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
       [stats, id](qint64 received, qint64 total) {
          stats->requestProgress(id, received, total);
       });
   QElapsedTimer started;

   started.start();
   QObject::connect(reply, &QNetworkReply::finished, reply,
       [stats, id, reply, started]() {
          QNetworkReply::NetworkError err = reply->error();
          QString error;

          /* A request this side drops is cancelled, which is no failure,
             but so is one which runs out of time.  An error from the
             server about the request itself, such as a file which is not
             there, is an answer: only a failure to get one counts */
          if (err == QNetworkReply::OperationCanceledError) {
             int timeout = reply->request().transferTimeout();

             if (timeout > 0 && started.elapsed() >= timeout)
                error = QObject::tr("No answer in %1 seconds")
                        .arg(timeout / 1000);
          } else if ((err > QNetworkReply::NoError && err < 200)
                     || err >= QNetworkReply::InternalServerError) {
             error = reply->errorString();
          }
          stats->requestFinished(id, error);
       });
}


QByteArray RemoteBackend::getRequest(const QString &pathAndQuery)
{
   return waitForReply(startGet(pathAndQuery));
}


bool RemoteBackend::transformPage(const QString &repo, const QString &path,
                                  int page, const QString &op)
{
   _lastError.clear();

   /* Build the (decoded) endpoint path; postRequest() percent-encodes
      special characters via QUrl::setPath while keeping the slashes as
      path separators, which the server splits on. */
   QString endpoint = "/v1/repos/" + repo + "/stacks/" + path + "/transform";

   QJsonObject body;
   body["page"] = page;
   body["op"]   = op;
   QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

   QByteArray resp = postRequest(endpoint, data);
   if (resp.isEmpty()) {
      if (_lastError.isEmpty())
         _lastError = "empty response from transform";
      return false;
   }

   QJsonDocument doc = QJsonDocument::fromJson(resp);
   if (doc.isObject() && doc.object().value("success").toBool(false))
      return true;

   _lastError = doc.isObject()
                    ? doc.object().value("error").toString("transform failed")
                    : QStringLiteral("invalid transform response");
   return false;
}


/* POST a JSON body to a per-stack endpoint and parse the reply.
   Returns the reply object via *out (may be empty on transport
   error); the return value is the "success" flag. */
bool RemoteBackend::postStackOp(const QString &repo, const QString &path,
                                const QString &verb, const QJsonObject &body,
                                QJsonObject *out)
{
   QString endpoint = "/v1/repos/" + repo + "/stacks/" + path + verb;
   QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

   QByteArray resp = postRequest(endpoint, data,
                                 QStringLiteral("application/json"),
                                 kOperationTimeoutMs);
   QJsonDocument doc = QJsonDocument::fromJson(resp);
   if (!doc.isObject()) {
      if (_lastError.isEmpty())
         _lastError = "invalid response from " + verb;
      if (out)
         *out = QJsonObject();
      return false;
   }
   if (out)
      *out = doc.object();
   if (doc.object().value("success").toBool(false))
      return true;
   _lastError = doc.object().value("error").toString(verb + " failed");
   return false;
}


void RemoteBackend::postStackOp(const QString &repo, const QString &path,
                                const QString &verb, const QJsonObject &body,
                                QObject *context, Done done)
{
   QString endpoint = "/v1/repos/" + repo + "/stacks/" + path + verb;
   QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

   whenFinished(startPost(endpoint, data, QStringLiteral("application/json"),
                          kOperationTimeoutMs),
                context, [verb, done](const Result &result) {
      Result out = result;
      QJsonObject obj = result.json();

      /* the server says why an operation failed, which is more use than
         the HTTP status */
      if (!obj.value("success").toBool(false)) {
         out.ok = false;
         if (obj.contains("error"))
            out.error = obj.value("error").toString();
         else if (out.error.isEmpty())
            out.error = obj.isEmpty() ? "invalid response from " + verb
                                      : verb + " failed";
      }
      done(out);
   });
}


void RemoteBackend::renameStack(const QString &repo, const QString &path,
                                const QString &newName, bool autoRename,
                                QObject *context, Done done)
{
   QJsonObject body;
   body["newName"]    = newName;
   body["autoRename"] = autoRename;
   postStackOp(repo, path, "/rename", body, context, done);
}


void RemoteBackend::renamePage(const QString &repo, const QString &path,
                               int page, const QString &newName,
                               QObject *context, Done done)
{
   QJsonObject body;
   body["newName"] = newName;
   postStackOp(repo, path + "/pages/" + QString::number(page), "/rename",
               body, context, done);
}


void RemoteBackend::moveStack(const QString &repo, const QString &path,
                              const QString &destDir, bool copy,
                              QObject *context, Done done)
{
   QJsonObject body;
   body["destDir"] = destDir;
   body["copy"]    = copy;
   postStackOp(repo, path, "/move", body, context, done);
}


void RemoteBackend::deleteStack(const QString &repo, const QString &path,
                                QObject *context, Done done)
{
   postStackOp(repo, path, "/delete", QJsonObject(), context, done);
}


void RemoteBackend::updateAnnotations(const QString &repo,
                                      const QString &path,
                                      const QJsonObject &updates,
                                      QObject *context, Done done)
{
   postStackOp(repo, path, "/annotations", updates, context, done);
}


void RemoteBackend::transformPage(const QString &repo, const QString &path,
                                  int page, const QString &op,
                                  QObject *context, Done done)
{
   QJsonObject body;
   body["page"] = page;
   body["op"]   = op;
   postStackOp(repo, path, "/transform", body, context, done);
}


void RemoteBackend::deletePages(const QString &repo, const QString &path,
                                const QList<int> &pages, QObject *context,
                                Done done)
{
   QJsonObject body;
   QJsonArray arr;

   for (int page : pages)
      arr.append(page);
   body["pages"] = arr;
   postStackOp(repo, path + "/pages", "/delete", body, context, done);
}


void RemoteBackend::undeletePages(const QString &repo, const QString &path,
                                  const QString &undoId, QObject *context,
                                  Done done)
{
   QJsonObject body;
   body["undoId"] = undoId;
   postStackOp(repo, path + "/pages", "/undelete", body, context, done);
}


void RemoteBackend::unstackStack(const QString &repo, const QString &path,
                                 int pagenum, int pagecount, bool remove,
                                 const QString &suggestedName,
                                 QObject *context, Done done)
{
   QJsonObject body;
   body["pagenum"]   = pagenum;
   body["pagecount"] = pagecount;
   body["remove"]    = remove;
   if (!suggestedName.isEmpty())
      body["newName"] = suggestedName;
   postStackOp(repo, path, "/unstack", body, context, done);
}


void RemoteBackend::stackStacks(const QString &repo, const QString &destPath,
                                const QStringList &sources, int insertPage,
                                QObject *context, Done done)
{
   QJsonObject body;
   QJsonArray arr;

   for (const QString &src : sources)
      arr.append(src);
   body["sources"] = arr;
   if (insertPage > 0)
      body["insertPage"] = insertPage;
   postStackOp(repo, destPath, "/stack", body, context, done);
}


void RemoteBackend::duplicateStack(const QString &repo, const QString &path,
                                   QObject *context, Done done)
{
   postStackOp(repo, path, "/duplicate", QJsonObject(), context, done);
}


void RemoteBackend::convertStack(const QString &repo, const QString &path,
                                 const QString &type, int oddEven,
                                 QObject *context, Done done)
{
   QJsonObject body;
   body["type"] = type;
   body["oddEven"] = oddEven;
   postStackOp(repo, path, "/duplicate", body, context, done);
}


void RemoteBackend::unfoldStack(const QString &repo, const QString &path,
                                QObject *context, Done done)
{
   postStackOp(repo, path, "/unfold", QJsonObject(), context, done);
}


void RemoteBackend::ocrPage(const QString &repo, const QString &path,
                            int page, QObject *context, Done done)
{
   postStackOp(repo, path + "/pages/" + QString::number(page), "/ocr",
               QJsonObject(), context, done);
}


void RemoteBackend::readStack(const QString &repo, const QString &path,
                              QObject *context, Done done)
{
   postStackOp(repo, path, "/read", QJsonObject(), context, done);
}


bool RemoteBackend::renameStack(const QString &repo, const QString &path,
                                QString &newName, bool autoRename)
{
   QJsonObject body, reply;
   body["newName"]    = newName;
   body["autoRename"] = autoRename;
   if (!postStackOp(repo, path, "/rename", body, &reply))
      return false;
   newName = reply.value("name").toString(newName);
   return true;
}


bool RemoteBackend::renamePage(const QString &repo, const QString &path,
                               int page, const QString &newName)
{
   QJsonObject body;
   body["newName"] = newName;
   return postStackOp(repo,
                      path + "/pages/" + QString::number(page),
                      "/rename", body, nullptr);
}


bool RemoteBackend::moveStack(const QString &repo, const QString &path,
                              const QString &destDir, bool copy,
                              QString *finalName)
{
   QJsonObject body, reply;
   body["destDir"] = destDir;
   body["copy"]    = copy;
   if (!postStackOp(repo, path, "/move", body, &reply))
      return false;
   if (finalName)
      *finalName = reply.value("name").toString();
   return true;
}


bool RemoteBackend::deleteStack(const QString &repo, const QString &path)
{
   return postStackOp(repo, path, "/delete", QJsonObject(), nullptr);
}


bool RemoteBackend::updateAnnotations(const QString &repo,
                                      const QString &path,
                                      const QJsonObject &updates)
{
   return postStackOp(repo, path, "/annotations", updates, nullptr);
}


bool RemoteBackend::deletePages(const QString &repo, const QString &path,
                                const QList<int> &pages, QString *undoId)
{
   QJsonObject body, reply;
   QJsonArray arr;

   for (int page : pages)
      arr.append(page);
   body["pages"] = arr;
   if (!postStackOp(repo, path + "/pages", "/delete", body, &reply))
      return false;
   if (undoId)
      *undoId = reply.value("undoId").toString();
   return true;
}


bool RemoteBackend::undeletePages(const QString &repo, const QString &path,
                                  const QString &undoId)
{
   QJsonObject body;
   body["undoId"] = undoId;
   return postStackOp(repo, path + "/pages", "/undelete", body, nullptr);
}


bool RemoteBackend::unstackStack(const QString &repo, const QString &path,
                                 int pagenum, int pagecount, bool remove,
                                 const QString &suggestedName,
                                 QString *newName)
{
   QJsonObject body, reply;
   body["pagenum"]   = pagenum;
   body["pagecount"] = pagecount;
   body["remove"]    = remove;
   if (!suggestedName.isEmpty())
      body["newName"] = suggestedName;
   if (!postStackOp(repo, path, "/unstack", body, &reply))
      return false;
   if (newName)
      *newName = reply.value("name").toString();
   return true;
}


bool RemoteBackend::stackStacks(const QString &repo, const QString &destPath,
                                const QStringList &sources, int insertPage)
{
   QJsonObject body;
   QJsonArray arr;

   for (const QString &src : sources)
      arr.append(src);
   body["sources"] = arr;
   if (insertPage > 0)
      body["insertPage"] = insertPage;
   return postStackOp(repo, destPath, "/stack", body, nullptr);
}


bool RemoteBackend::duplicateStack(const QString &repo, const QString &path,
                                   QString *newName)
{
   QJsonObject reply;

   if (!postStackOp(repo, path, "/duplicate", QJsonObject(), &reply))
      return false;
   if (newName)
      *newName = reply.value("name").toString();
   return true;
}


bool RemoteBackend::convertStack(const QString &repo, const QString &path,
                                 const QString &type, int oddEven,
                                 QString *newName)
{
   QJsonObject body, reply;

   body["type"] = type;
   body["oddEven"] = oddEven;
   if (!postStackOp(repo, path, "/duplicate", body, &reply))
      return false;
   if (newName)
      *newName = reply.value("name").toString();
   return true;
}


bool RemoteBackend::unfoldStack(const QString &repo, const QString &path,
                                QString *newName)
{
   QJsonObject reply;

   if (!postStackOp(repo, path, "/unfold", QJsonObject(), &reply))
      return false;
   if (newName)
      *newName = reply.value("name").toString();
   return true;
}


void RemoteBackend::uploadFile(const QString &repo, const QString &path,
                               const QByteArray &bytes, bool overwrite,
                               QObject *context, Done done)
{
   QString endpoint = "/v1/repos/" + repo + "/stacks/" + path + "/upload";
   if (overwrite)
      endpoint += "?overwrite=true";

   whenFinished(startPost(endpoint, bytes, "application/octet-stream"),
                context, [done](const Result &result) {
      Result out = result;
      QJsonObject obj = result.json();
      if (!obj.value("success").toBool(false)) {
         out.ok = false;
         if (out.error.isEmpty())
            out.error = obj.isEmpty() ? QStringLiteral("invalid upload "
                                                       "response")
                                      : obj.value("error").toString("upload "
                                                                    "failed");
      }
      done(out);
   });
}


bool RemoteBackend::uploadFile(const QString &repo, const QString &path,
                               const QByteArray &bytes, QString *finalName,
                               QString *etag, bool overwrite)
{
   QString endpoint = "/v1/repos/" + repo + "/stacks/" + path + "/upload";
   if (overwrite)
      endpoint += "?overwrite=true";

   QByteArray resp = postRequest(endpoint, bytes,
                                 "application/octet-stream");
   QJsonDocument doc = QJsonDocument::fromJson(resp);
   if (!doc.isObject() || !doc.object().value("success").toBool(false)) {
      if (_lastError.isEmpty())
         _lastError = doc.isObject()
                          ? doc.object().value("error").toString("upload failed")
                          : QStringLiteral("invalid upload response");
      return false;
   }
   if (finalName)
      *finalName = doc.object().value("name").toString();
   if (etag)
      *etag = doc.object().value("etag").toString();
   return true;
}


void RemoteBackend::searchText(const QString &repo, const QString &text,
                               const QString &dir, int maxHits,
                               QObject *context, TextDone done)
{
   QUrlQuery q;

   q.addQueryItem("text", text);
   if (!dir.isEmpty())
      q.addQueryItem("path", dir);
   q.addQueryItem("max", QString::number(maxHits));

   whenFinished(startGet("/v1/repos/"
                         + QString::fromUtf8(QUrl::toPercentEncoding(repo))
                         + "/search?" + q.query(QUrl::FullyEncoded),
                         QString(), kWorkTimeoutMs),
                context, [done](const Result &result) {
      // the server says why it failed, which is more use than the status
      QJsonObject obj = result.json();
      QList<TextHit> hits;

      if (!result.ok || !obj.value("success").toBool()) {
         QString why = obj.value("error").toString();

         if (why.isEmpty())
            why = result.error.isEmpty()
                  ? QStringLiteral("the server cannot search") : result.error;
         done(why, hits, true);
         return;
      }
      for (const QJsonValue &v : obj.value("results").toArray()) {
         QJsonObject r = v.toObject();
         TextHit hit;

         hit.path = r.value("path").toString();
         hit.page = r.value("page").toInt(1) - 1;   // the server's is 1-based
         hit.snippet = r.value("snippet").toString();
         hits << hit;
      }
      done(QString(), hits, obj.value("complete").toBool(true));
   });
}


void RemoteBackend::searchNames(const QString &repo, const QString &text,
                                const QString &dir, QObject *context,
                                NamesDone done)
{
   QUrlQuery q;

   q.addQueryItem("repo", repo);
   q.addQueryItem("q", text);
   if (!dir.isEmpty())
      q.addQueryItem("path", dir);
   q.addQueryItem("recursive", "true");

   whenFinished(startGet("/search?" + q.query(QUrl::FullyEncoded), QString(),
                         kWorkTimeoutMs),
                context, [done](const Result &result) {
      QJsonObject obj = result.json();
      QStringList paths;

      if (!result.ok || !obj.value("success").toBool()) {
         QString why = obj.value("error").toString();

         if (why.isEmpty())
            why = result.error.isEmpty()
                  ? QStringLiteral("the server cannot search") : result.error;
         done(why, paths);
         return;
      }
      for (const QJsonValue &v : obj.value("results").toArray())
         paths << v.toObject().value("path").toString();
      done(QString(), paths);
   });
}


void RemoteBackend::subscribeEvents(const QString &repo)
{
   if (_eventRepos.contains(repo))
      return;
   _eventRepos.insert(repo);
   startEventStream(repo);
}


void RemoteBackend::startEventStream(const QString &repo)
{
   QUrl url(_baseUrl);
   url.setPath("/v1/repos/" + repo + "/events");
   QNetworkRequest req(url);
   /* no transfer timeout: the stream stays open indefinitely */
   req.setRawHeader("X-Client-Id", _clientId.toUtf8());
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());

   QNetworkReply *reply = _nam->get(req);
   auto buf = std::make_shared<QByteArray>();

   logRequest(reply, "GET", url.path());

   /* the stream is wanted from now on, and connected once the server
      answers it */
   if (_stats) {
      BackendStats *stats = _stats;

      stats->setEventStream(repo, false);
      QObject::connect(reply, &QNetworkReply::metaDataChanged, reply,
          [stats, reply, repo]() {
             int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();

             stats->setEventStream(repo, status == 200);
          });
      QObject::connect(reply, &QNetworkReply::finished, reply,
          [stats, repo]() { stats->setEventStream(repo, false); });
   }
   QObject::connect(reply, &QNetworkReply::readyRead, this,
       [this, reply, buf, repo]() {
          *buf += readReply(reply);
          int pos;
          while ((pos = buf->indexOf("\n\n")) >= 0) {
             QByteArray frame = buf->left(pos);
             buf->remove(0, pos + 2);
             int d = frame.indexOf("data: ");
             if (d < 0)
                continue;      // comment / keep-alive frame
             QJsonDocument doc = QJsonDocument::fromJson(frame.mid(d + 6));
             if (!doc.isObject())
                continue;
             QJsonObject o = doc.object();
             if (o.value("origin").toString() == _clientId)
                continue;      // our own change, already applied
             emit stackEvent(o.value("repo").toString(repo),
                             o.value("op").toString(),
                             o.value("path").toString(),
                             o.value("name").toString());
          }
       });
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, repo]() {
          reply->deleteLater();
          /* reconnect unless the subscription was dropped */
          if (_eventRepos.contains(repo))
             QTimer::singleShot(3000, this, [this, repo]() {
                if (_eventRepos.contains(repo))
                   startEventStream(repo);
             });
       });
}


QString RemoteBackend::cacheRoot()
{
   QString id = serverId();
   if (id.isEmpty())
      return QString();
   return QStandardPaths::writableLocation(
              QStandardPaths::GenericCacheLocation)
          + "/paperman/" + id;
}


QString RemoteBackend::cacheDirFor(const QString &repo,
                                   const QString &dirInRepo)
{
   QString root = cacheRoot();
   if (root.isEmpty())
      return QString();
   QString dir = root + "/" + repo;
   if (!dirInRepo.isEmpty())
      dir += "/" + dirInRepo;
   return dir;
}


QString RemoteBackend::cachePathFor(const QString &repo,
                                    const QString &relPath)
{
   QString root = cacheRoot();
   if (root.isEmpty())
      return QString();
   return root + "/" + repo + "/" + relPath;
}


/* Build the "/file?repo=...&path=..." path-with-query string. */
static QString wholeFilePathFor(const QString &repo, const QString &path)
{
   QUrlQuery q;
   q.addQueryItem("repo", repo);
   q.addQueryItem("path", path);
   return "/file?" + q.toString();
}


void RemoteBackend::downloadFile(const QString &repo, const QString &relPath,
                                 const QString &dest, QObject *context,
                                 Done done)
{
   whenFinished(startGet(wholeFilePathFor(repo, relPath), QString(),
                         kFetchTimeoutMs),
                context, [dest, done](const Result &result) {
      Result out = result;

      if (out.ok) {
         QSaveFile file(dest);

         if (!file.open(QIODevice::WriteOnly)
             || file.write(out.body) != out.body.size() || !file.commit()) {
            out.ok = false;
            out.error = QString("cannot write %1: %2")
                           .arg(dest, file.errorString());
         }
      }
      out.body.clear();
      done(out);
   });
}


QString RemoteBackend::ensureCachedFile(const QString &repo,
                                        const QString &relPath,
                                        bool *refreshed)
{
   if (refreshed)
      *refreshed = false;

   QString cachePath = cachePathFor(repo, relPath);
   if (cachePath.isEmpty()) {
      _lastError = "cannot determine the server's cache directory";
      return QString();
   }

   QString etagPath = cachePath + ".etag";
   QString etag;
   bool haveCopy = QFile::exists(cachePath);
   if (haveCopy) {
      QFile ef(etagPath);
      if (ef.open(QIODevice::ReadOnly))
         etag = QString::fromUtf8(ef.readAll()).trimmed();
   }

   int status = 0;
   QString newEtag;
   QByteArray body = waitForReplyFull(
       startGet(wholeFilePathFor(repo, relPath),
                haveCopy ? etag : QString(), kFetchTimeoutMs),
       &status, &newEtag);

   if (status == 304)
      return cachePath;          // cached copy is current

   if (status != 200) {
      /* Server unreachable or unhappy: fall back to the cached copy
         if there is one, so already-fetched stacks stay viewable. */
      if (haveCopy) {
         qInfo() << "RemoteBackend: using cached copy of" << relPath
                 << "after fetch error:" << _lastError;
         return cachePath;
      }
      if (_lastError.isEmpty())
         _lastError = QString("HTTP %1").arg(status);
      return QString();
   }

   return storeCachedFile(cachePath, body, newEtag, refreshed);
}


/* Write freshly-downloaded bytes (and their validator) into the cache.
   Shared by the sync and async fetch paths. */
/* Write a stack fetched whole to the cache, with its validator; returns
   an empty string, or why it failed.  It touches nothing but the files,
   so may run on any thread */
static QString storeFile(const QString &cachePath, const QByteArray &body,
                         const QString &newEtag)
{
   QString etagPath = cachePath + ".etag";

   QDir().mkpath(QFileInfo(cachePath).path());
   QSaveFile out(cachePath);
   if (!out.open(QIODevice::WriteOnly))
      return "cannot write cache file " + cachePath;
   out.write(body);
   if (!out.commit())
      return "cannot commit cache file " + cachePath;

   if (newEtag.isEmpty()) {
      QFile::remove(etagPath);
   } else {
      QFile ef(etagPath);
      if (ef.open(QIODevice::WriteOnly))
         ef.write(newEtag.toUtf8());
   }
   return QString();
}


QString RemoteBackend::storeCachedFile(const QString &cachePath,
                                       const QByteArray &body,
                                       const QString &newEtag,
                                       bool *refreshed)
{
   QString error = storeFile(cachePath, body, newEtag);

   if (!error.isEmpty()) {
      _lastError = error;
      return QString();
   }
   if (refreshed)
      *refreshed = true;
   return cachePath;
}


/* Async counterpart to ensureCachedFile(): returns at once and emits
   cachedFileReady() when the copy is current.  The reply is handled on
   the thread that owns this object, so the network stack is only ever
   touched from there. */
void RemoteBackend::ensureCachedFile(const QString &repo,
                                     const QString &relPath,
                                     QObject *context,
                                     std::function<void (const QString &,
                                                         bool)> done)
{
   // as in browseDirectory()
   quint64 token = ensureCachedFileAsync(repo, relPath);
   auto conn = std::make_shared<QMetaObject::Connection>();
   *conn = connect(this, &RemoteBackend::cachedFileReady, context,
                   [token, conn, done](quint64 tok, const QString &path,
                                       bool refreshed) {
      if (tok != token)
         return;
      QObject::disconnect(*conn);
      done(path, refreshed);
   });
}


quint64 RemoteBackend::ensureCachedFileAsync(const QString &repo,
                                             const QString &relPath)
{
   quint64 token = _nextAsyncToken++;

   /* cachePathFor() needs the server id, and serverId() fetches one
      with a blocking request when it has none.  That nested event loop
      would deliver replies that are still in flight elsewhere, so an
      asynchronous caller must not reach it: the id is seeded when the
      backend is made, and an empty one here means we cannot work out
      where to cache without blocking. */
   QString cachePath = _serverId.isEmpty() ? QString()
                                           : cachePathFor(repo, relPath);
   if (cachePath.isEmpty()) {
      _lastError = "cannot determine the server's cache directory";
      QMetaObject::invokeMethod(this, [this, token]() {
         emit cachedFileReady(token, QString(), false);
      }, Qt::QueuedConnection);
      return token;
   }

   QString etag;
   bool haveCopy = QFile::exists(cachePath);
   if (haveCopy) {
      QFile ef(cachePath + ".etag");
      if (ef.open(QIODevice::ReadOnly))
         etag = QString::fromUtf8(ef.readAll()).trimmed();
   }

   QNetworkReply *reply = startGet(wholeFilePathFor(repo, relPath),
                                   haveCopy ? etag : QString(),
                                   kFetchTimeoutMs);
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, token, cachePath, haveCopy]() {
          _lastError.clear();
          QByteArray body = readReply(reply);
          int status = reply->attribute(
                           QNetworkRequest::HttpStatusCodeAttribute).toInt();
          QString newEtag = QString::fromUtf8(
                                reply->rawHeader("ETag")).trimmed();
          if (reply->error() != QNetworkReply::NoError
              && status != 304)
             _lastError = reply->errorString();
          reply->deleteLater();

          if (status == 304) {
             emit cachedFileReady(token, cachePath, false);
             return;
          }
          if (status != 200) {
             /* Unreachable or unhappy: an existing copy still shows,
                matching the sync path's behaviour. */
             if (haveCopy) {
                qInfo() << "RemoteBackend: using cached copy after fetch"
                        << "error:" << _lastError;
                emit cachedFileReady(token, cachePath, false);
             } else {
                if (_lastError.isEmpty())
                   _lastError = QString("HTTP %1").arg(status);
                emit cachedFileReady(token, QString(), false);
             }
             return;
          }

          inBackground([cachePath, body, newEtag]() {
             return storeFile(cachePath, body, newEtag);
          }, [this, token, cachePath](const QString &error) {
             if (!error.isEmpty()) {
                _lastError = error;
                emit cachedFileReady(token, QString(), false);
                return;
             }
             emit cachedFileReady(token, cachePath, true);
          });
       });
   return token;
}


void RemoteBackend::invalidateCachedFile(const QString &repo,
                                         const QString &relPath)
{
   QString cachePath = cachePathFor(repo, relPath);
   if (cachePath.isEmpty())
      return;
   QFile::remove(cachePath);
   QFile::remove(cachePath + ".etag");
   /* and any copy held a page at a time: the pages are as suspect as
      the whole file would be */
   invalidatePageDir(repo, relPath);
}



static QString stackInfoPathFor(const QString &repo, const QString &path,
                                bool withText)
{
   QString p = "/v1/repos/" + repo + "/stacks/" + path + "/info";
   if (withText)
      p += "?text=1";
   return p;
}


static QString stackPagePathFor(const QString &repo, const QString &path,
                                int page)
{
   return "/v1/repos/" + repo + "/stacks/" + path + "/pages/"
          + QString::number(page);
}


/* The file holding one page in a page directory; the name matches
   Filemax::pageFileName(), which this file cannot include since the
   command-line client is built without the stack code. */
static QString pageFileIn(const QString &pageDir, int page)
{
   return pageDir + QString("/page-%1.max").arg(page);
}


//! how long writeLater() waits before trying a failed write again
static const int kRewriteMs = 200;

/* Write bytes so a reader never sees a half-written file: a page file
   is opened as a stack the moment it exists. */
static bool writeWhole(const QString &path, const QByteArray &bytes,
                       QString *error)
{
   QDir().mkpath(QFileInfo(path).path());
   QSaveFile out(path);
   if (!out.open(QIODevice::WriteOnly)) {
      *error = "cannot write " + path + ": " + out.errorString();
      return false;
   }
   out.write(bytes);
   if (!out.commit()) {
      *error = "cannot commit " + path + ": " + out.errorString();
      return false;
   }
   return true;
}


void RemoteBackend::inBackground(std::function<QString()> work,
                                 std::function<void(const QString &)> done)
{
   auto *watcher = new QFutureWatcher<QString>(this);

   QObject::connect(watcher, &QFutureWatcherBase::finished, this,
                    [watcher, done]() {
      QString error = watcher->result();

      watcher->deleteLater();
      done(error);
   });
   watcher->setFuture(QtConcurrent::run(work));
}


void RemoteBackend::writeLater(const QString &path, const QByteArray &bytes,
                               std::function<void(const QString &)> done)
{
   _writing[path] = bytes;
   inBackground([path, bytes]() {
      QString error;

      /* a file can be held for a moment, as by a virus checker on
         Windows, so that putting the new one in its place fails: wait a
         little and try once more */
      if (!writeWhole(path, bytes, &error)) {
         QThread::msleep(kRewriteMs);
         error.clear();
         writeWhole(path, bytes, &error);
      }
      return error;
   }, [this, path, bytes, done](const QString &error) {
      // unless a later write of the file is on its way
      auto it = _writing.find(path);

      if (it != _writing.end() && *it == bytes)
         _writing.erase(it);

      /* the callers carry on without the file, so a cache misses where
         it should not; say why */
      if (!error.isEmpty())
         qWarning().noquote() << "RemoteBackend:" << error;
      done(error);
   });
}


QString RemoteBackend::pageDirFor(const QString &repo, const QString &relPath)
{
   QString p = cachePathFor(repo, relPath);
   return p.isEmpty() ? QString() : p + ".d";
}


QString RemoteBackend::fetchStackInfo(const QString &repo,
                                      const QString &relPath, bool withText)
{
   QString dir = pageDirFor(repo, relPath);
   if (dir.isEmpty()) {
      _lastError = "cannot determine the server's cache directory";
      return QString();
   }

   int status = 0;
   QString etag;
   QByteArray body = waitForReplyFull(
       startGet(stackInfoPathFor(repo, relPath, withText), QString(),
                kWorkTimeoutMs), &status, &etag);
   if (status != 200) {
      if (_lastError.isEmpty())
         _lastError = QString("HTTP %1").arg(status);
      return QString();
   }
   if (!writeWhole(dir + "/info.json", body, &_lastError))
      return QString();
   return dir;
}


void RemoteBackend::stackStructure(const QString &repo, const QString &path,
                                   QObject *context, Done done)
{
   whenFinished(startGet(stackInfoPathFor(repo, path, false), QString(),
                         kWorkTimeoutMs), context, done);
}


quint64 RemoteBackend::fetchStackInfoAsync(const QString &repo,
                                           const QString &relPath,
                                           bool withText)
{
   quint64 token = _nextAsyncToken++;

   /* as in ensureCachedFileAsync(): working out the directory must not
      block on /v1/status, so the id has to be known already */
   QString dir = _serverId.isEmpty() ? QString() : pageDirFor(repo, relPath);
   if (dir.isEmpty()) {
      _lastError = "cannot determine the server's cache directory";
      QMetaObject::invokeMethod(this, [this, token]() {
         emit stackInfoReady(token, QString());
      }, Qt::QueuedConnection);
      return token;
   }

   QNetworkReply *reply = startGet(stackInfoPathFor(repo, relPath, withText),
                                   QString(), kWorkTimeoutMs);
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, token, dir]() {
          _lastError.clear();
          QByteArray body = readReply(reply);
          int status = reply->attribute(
                           QNetworkRequest::HttpStatusCodeAttribute).toInt();
          if (reply->error() != QNetworkReply::NoError)
             _lastError = reply->errorString();
          reply->deleteLater();

          if (status != 200) {
             if (_lastError.isEmpty())
                _lastError = QString("HTTP %1").arg(status);
             emit stackInfoReady(token, QString());
             return;
          }
          writeLater(dir + "/info.json", body,
                     [this, token, dir](const QString &error) {
             if (!error.isEmpty()) {
                _lastError = error;
                emit stackInfoReady(token, QString());
                return;
             }
             emit stackInfoReady(token, dir);
          });
       });
   return token;
}


QString RemoteBackend::fetchPage(const QString &repo, const QString &relPath,
                                 int page)
{
   QString dir = pageDirFor(repo, relPath);
   if (dir.isEmpty()) {
      _lastError = "cannot determine the server's cache directory";
      return QString();
   }

   int status = 0;
   QString etag;
   QByteArray body = waitForReplyFull(
       startGet(stackPagePathFor(repo, relPath, page), QString(),
                kFetchTimeoutMs),
       &status, &etag);
   if (status != 200) {
      if (_lastError.isEmpty())
         _lastError = QString("HTTP %1").arg(status);
      return QString();
   }
   QString path = pageFileIn(dir, page);
   if (!writeWhole(path, body, &_lastError))
      return QString();
   return path;
}


quint64 RemoteBackend::fetchPageAsync(const QString &repo,
                                      const QString &relPath, int page)
{
   quint64 token = _nextAsyncToken++;

   QString dir = _serverId.isEmpty() ? QString() : pageDirFor(repo, relPath);
   if (dir.isEmpty()) {
      _lastError = "cannot determine the server's cache directory";
      QMetaObject::invokeMethod(this, [this, token]() {
         emit pageReady(token, QString());
      }, Qt::QueuedConnection);
      return token;
   }

   QNetworkReply *reply = startGet(stackPagePathFor(repo, relPath, page),
                                   QString(), kFetchTimeoutMs);
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, token, dir, page]() {
          _lastError.clear();
          QByteArray body = readReply(reply);
          int status = reply->attribute(
                           QNetworkRequest::HttpStatusCodeAttribute).toInt();
          if (reply->error() != QNetworkReply::NoError)
             _lastError = reply->errorString();
          reply->deleteLater();

          if (status != 200) {
             if (_lastError.isEmpty())
                _lastError = QString("HTTP %1").arg(status);
             emit pageReady(token, QString());
             return;
          }
          QString path = pageFileIn(dir, page);

          writeLater(path, body, [this, token, path](const QString &error) {
             if (!error.isEmpty()) {
                _lastError = error;
                emit pageReady(token, QString());
                return;
             }
             emit pageReady(token, path);
          });
       });
   return token;
}


void RemoteBackend::invalidatePageDir(const QString &repo,
                                      const QString &relPath)
{
   QString dir = pageDirFor(repo, relPath);
   if (!dir.isEmpty())
      QDir(dir).removeRecursively();
}


void RemoteBackend::invalidatePage(const QString &repo, const QString &relPath,
                                   int page)
{
   QString dir = pageDirFor(repo, relPath);
   if (!dir.isEmpty())
      QFile::remove(pageFileIn(dir, page));
}


QByteArray RemoteBackend::postRequest(const QString &path,
                                      const QByteArray &body,
                                      const QString &contentType,
                                      int timeoutMs)
{
   return waitForReply(startPost(path, body, contentType, timeoutMs));
}


QNetworkReply *RemoteBackend::startPost(const QString &path,
                                        const QByteArray &body,
                                        const QString &contentType,
                                        int timeoutMs)
{
   QUrl url(_baseUrl);
   int q = path.indexOf('?');
   if (q < 0) {
      url.setPath(path);
   } else {
      url.setPath(path.left(q));
      url.setQuery(path.mid(q + 1));
   }
   QNetworkRequest req(url);
   req.setTransferTimeout(timeoutMs > 0 ? timeoutMs : kRequestTimeoutMs);
   req.setRawHeader("X-Client-Id", _clientId.toUtf8());
   req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());

   QNetworkReply *reply = _nam->post(req, body);

   track(reply, path, true, path.toUtf8().size() + body.size());
   return reply;
}


QJsonObject RemoteBackend::Result::json() const
{
   return QJsonDocument::fromJson(body).object();
}


void RemoteBackend::whenFinished(QNetworkReply *reply, QObject *context,
                                 Done done)
{
   /* Tie the callback to the context, so that it is dropped if the
      context goes first; the reply itself goes with this backend */
   QObject::connect(reply, &QNetworkReply::finished,
                    context ? context : this, [reply, done]() {
      Result result;

      result.body = readReply(reply);
      result.status = reply->attribute(
                          QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (reply->error() != QNetworkReply::NoError)
         result.error = reply->errorString();
      else if (result.status >= 400)
         result.error = QString("HTTP %1").arg(result.status);
      result.ok = result.error.isEmpty();
      reply->deleteLater();
      done(result);
   });
}


void RemoteBackend::waitForFinish(QNetworkReply *reply)
{
   if (reply->isFinished())
      return;

   const char *method = "GET";
   if (reply->operation() == QNetworkAccessManager::PostOperation)
      method = "POST";
   else if (reply->operation() == QNetworkAccessManager::PutOperation)
      method = "PUT";
   BlockWatch::note(QString("%1 %2").arg(method, reply->url().path()));

   QEventLoop loop;
   QObject::connect(reply, &QNetworkReply::finished,
                    &loop, &QEventLoop::quit);
   loop.exec();
}


QByteArray RemoteBackend::waitForReply(QNetworkReply *reply)
{
   _lastError.clear();

   waitForFinish(reply);

   QByteArray data = readReply(reply);
   if (reply->error() != QNetworkReply::NoError) {
      _lastError = reply->errorString();
   } else {
      int status = reply->attribute(
                       QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (status >= 400)
         _lastError = QString("HTTP %1").arg(status);
   }
   reply->deleteLater();
   return data;
}


QByteArray RemoteBackend::waitForReplyFull(QNetworkReply *reply, int *status,
                                           QString *etag)
{
   _lastError.clear();

   waitForFinish(reply);

   QByteArray data = readReply(reply);
   *status = reply->attribute(
                 QNetworkRequest::HttpStatusCodeAttribute).toInt();
   *etag = QString::fromUtf8(reply->rawHeader("ETag"));
   if (reply->error() != QNetworkReply::NoError && *status != 304)
      _lastError = reply->errorString();
   else if (*status >= 400)
      _lastError = QString("HTTP %1").arg(*status);
   reply->deleteLater();
   return data;
}
