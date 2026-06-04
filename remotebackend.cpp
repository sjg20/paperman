/*
License: GPL-2
*/

#include "remotebackend.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>


RemoteBackend::RemoteBackend(const QUrl &baseUrl)
   : _baseUrl(baseUrl)
   , _nam(new QNetworkAccessManager)
{
}


RemoteBackend::~RemoteBackend()
{
   delete _nam;
}


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


DirectoryListing RemoteBackend::browseDirectory(const QString &repo,
                                                const QString &dir)
{
   DirectoryListing out;

   /* URL-encode parameters via QUrlQuery so repo names with funky
    * characters don't break the request. */
   QUrlQuery q;
   q.addQueryItem("repo", repo);
   q.addQueryItem("path", dir);
   QString pq = "/browse?" + q.toString();

   QByteArray body = getRequest(pq);
   QJsonDocument doc = QJsonDocument::fromJson(body);
   if (!doc.isObject())
      return out;
   QJsonObject obj = doc.object();

   /* Subdirectories first, matching the on-screen ordering the GUI
    * uses today. */
   for (const QJsonValue &v : obj.value("directories").toArray()) {
      QJsonObject d = v.toObject();
      DirectoryEntry e;
      e.name  = d.value("name").toString();
      e.path  = d.value("path").toString();
      e.isDir = true;
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
   return out;
}


QByteArray RemoteBackend::getRequest(const QString &pathAndQuery)
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
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());
   return waitForReply(_nam->get(req));
}


QByteArray RemoteBackend::postRequest(const QString &path,
                                      const QByteArray &body)
{
   QUrl url(_baseUrl);
   url.setPath(path);
   QNetworkRequest req(url);
   req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
   if (!_token.isEmpty())
      req.setRawHeader("Authorization", "Bearer " + _token.toUtf8());
   return waitForReply(_nam->post(req, body));
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
