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
#include <QUrlQuery>


RemoteBackend::RemoteBackend(const QUrl &baseUrl, QObject *parent)
   : QObject(parent)
   , _baseUrl(baseUrl)
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
                                      const QByteArray &body)
{
   QUrl url(_baseUrl);
   url.setPath(path);
   QNetworkRequest req(url);
   req.setTransferTimeout(kRequestTimeoutMs);
   req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
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
