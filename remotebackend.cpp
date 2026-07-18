/*
License: GPL-2
*/

#include "remotebackend.h"

#include "backendstats.h"

#include <QDebug>
#include <QDir>
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

   QJsonDocument doc = QJsonDocument::fromJson(resp);
   if (!doc.isObject()) {
      _lastError = "login response is not a JSON object";
      return false;
   }
   QString token = doc.object().value("token").toString();
   if (token.isEmpty()) {
      _lastError = "login response missing token";
      return false;
   }
   _token = token;
   return true;
}


QString RemoteBackend::serverId()
{
   if (!_serverId.isEmpty())
      return _serverId;

   QByteArray body = getRequest("/v1/status");
   QJsonDocument doc = QJsonDocument::fromJson(body);
   if (!doc.isObject())
      return QString();
   _serverId = doc.object().value("serverId").toString();
   return _serverId;
}


QList<RepositoryInfo> RemoteBackend::listRepositories()
{
   QList<RepositoryInfo> out;

   QByteArray body = getRequest("/repos");
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
   QNetworkReply *reply = startGet(browsePathFor(repo, dir));

   /* Spin the local event loop until the reply finishes (sync),
    * then hand the still-live reply to the shared parser before
    * scheduling its deletion. */
   QEventLoop loop;
   QObject::connect(reply, &QNetworkReply::finished,
                    &loop, &QEventLoop::quit);
   loop.exec();

   DirectoryListing out = parseBrowseReply(reply);
   reply->deleteLater();
   return out;
}


quint64 RemoteBackend::browseDirectoryAsync(const QString &repo,
                                            const QString &dir)
{
   quint64 token = _nextAsyncToken++;
   QNetworkReply *reply = startGet(browsePathFor(repo, dir));
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, token]() {
          DirectoryListing listing = parseBrowseReply(reply);
          reply->deleteLater();
          emit browseDirectoryReady(token, listing);
       });
   return token;
}


DirectoryListing RemoteBackend::parseBrowseReply(QNetworkReply *reply)
{
   DirectoryListing out;
   _lastError.clear();

   QByteArray body = reply->readAll();
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


/* Default transfer timeout for every request.  Qt's own default is
 * effectively unlimited, which means the GUI would freeze for tens of
 * seconds if the server went away.  Five seconds is long enough for
 * any honest LAN round-trip and short enough that a dead server
 * surfaces quickly. */
static const int kRequestTimeoutMs = 5000;


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
                                           int page, const QString &size)
{
   quint64 token = _nextAsyncToken++;
   QNetworkReply *reply = startGet(thumbnailPathFor(repo, path, page, size));
   QObject::connect(reply, &QNetworkReply::finished, this,
       [this, reply, token]() {
          _lastError.clear();
          QByteArray body = reply->readAll();
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
          emit thumbnailReady(token, body);
       });
   return token;
}


QNetworkReply *RemoteBackend::startGet(const QString &pathAndQuery,
                                       const QString &ifNoneMatch)
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
   req.setTransferTimeout(kRequestTimeoutMs);
   req.setRawHeader("X-Client-Id", _clientId.toUtf8());
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());
   if (!ifNoneMatch.isEmpty())
      req.setRawHeader("If-None-Match", ifNoneMatch.toUtf8());

   QNetworkReply *reply = _nam->get(req);

   if (_stats) {
      /* Approximate the request size by the URL bytes.  Headers
       * are uncounted; tolerable for a user-facing tally. */
      _stats->requestStarted();
      _stats->recordSent(pathAndQuery.toUtf8().size());
      QObject::connect(reply, &QNetworkReply::finished, _stats,
          [this, reply]() {
             _stats->recordReceived(reply->bytesAvailable());
             _stats->requestFinished();
          });
   }
   return reply;
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

   QByteArray resp = postRequest(endpoint, data);
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


bool RemoteBackend::ocrPage(const QString &repo, const QString &path,
                            int page, QString *text)
{
   QJsonObject reply;

   if (!postStackOp(repo, path + "/pages/" + QString::number(page),
                    "/ocr", QJsonObject(), &reply))
      return false;
   if (text)
      *text = reply.value("text").toString();
   return true;
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
   QObject::connect(reply, &QNetworkReply::readyRead, this,
       [this, reply, buf, repo]() {
          *buf += reply->readAll();
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
                haveCopy ? etag : QString()),
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

   QDir().mkpath(QFileInfo(cachePath).path());
   QSaveFile out(cachePath);
   if (!out.open(QIODevice::WriteOnly)) {
      _lastError = "cannot write cache file " + cachePath;
      return QString();
   }
   out.write(body);
   if (!out.commit()) {
      _lastError = "cannot commit cache file " + cachePath;
      return QString();
   }

   if (newEtag.isEmpty()) {
      QFile::remove(etagPath);
   } else {
      QFile ef(etagPath);
      if (ef.open(QIODevice::WriteOnly))
         ef.write(newEtag.toUtf8());
   }
   if (refreshed)
      *refreshed = true;
   return cachePath;
}


void RemoteBackend::invalidateCachedFile(const QString &repo,
                                         const QString &relPath)
{
   QString cachePath = cachePathFor(repo, relPath);
   if (cachePath.isEmpty())
      return;
   QFile::remove(cachePath);
   QFile::remove(cachePath + ".etag");
}


QByteArray RemoteBackend::postRequest(const QString &path,
                                      const QByteArray &body,
                                      const QString &contentType)
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
   req.setTransferTimeout(kRequestTimeoutMs);
   req.setRawHeader("X-Client-Id", _clientId.toUtf8());
   req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());

   QNetworkReply *reply = _nam->post(req, body);
   if (_stats) {
      _stats->requestStarted();
      _stats->recordSent(path.toUtf8().size() + body.size());
      QObject::connect(reply, &QNetworkReply::finished, _stats,
          [this, reply]() {
             _stats->recordReceived(reply->bytesAvailable());
             _stats->requestFinished();
          });
   }
   return waitForReply(reply);
}


QByteArray RemoteBackend::waitForReply(QNetworkReply *reply)
{
   _lastError.clear();

   QEventLoop loop;
   QObject::connect(reply, &QNetworkReply::finished,
                    &loop, &QEventLoop::quit);
   loop.exec();

   QByteArray data = reply->readAll();
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

   QEventLoop loop;
   QObject::connect(reply, &QNetworkReply::finished,
                    &loop, &QEventLoop::quit);
   loop.exec();

   QByteArray data = reply->readAll();
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
