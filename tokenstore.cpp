/*
License: GPL-2
*/

#include "tokenstore.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>


TokenStore::TokenStore()
{
}


TokenStore::TokenStore(const QString &path)
   : _path(path)
{
   load();
}


QString TokenStore::defaultPath()
{
   return QStandardPaths::writableLocation(
                QStandardPaths::GenericConfigLocation)
          + "/paperman-server/tokens.json";
}


QString TokenStore::keyFor(const QString &token)
{
   return QString::fromLatin1(QCryptographicHash::hash(
            token.toLatin1(), QCryptographicHash::Sha256).toHex());
}


void TokenStore::load()
{
   QFile f(_path);

   if (!f.exists())
      return;
   if (!f.open(QIODevice::ReadOnly)) {
      qWarning() << "TokenStore: cannot read" << _path;
      return;
   }
   QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
   if (!doc.isObject()) {
      qWarning() << "TokenStore: cannot parse" << _path;
      return;
   }

   /* tokens which have run out while the server was down go now */
   QDateTime now = QDateTime::currentDateTime();
   bool dropped = false;
   const QJsonObject obj = doc.object();
   for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
      QJsonObject t = it.value().toObject();
      Info info;

      info.user = t.value("user").toString();
      info.expiry = QDateTime::fromString(t.value("expiry").toString(),
                                          Qt::ISODate);
      info.stamp = t.value("stamp").toString();
      if (info.user.isEmpty()
          || (info.expiry.isValid() && info.expiry < now)) {
         dropped = true;
         continue;
      }
      _tokens.insert(it.key(), info);
   }
   if (dropped)
      save();
}


void TokenStore::save()
{
   if (_path.isEmpty())
      return;

   QJsonObject obj;
   for (auto it = _tokens.constBegin(); it != _tokens.constEnd(); ++it) {
      QJsonObject t;

      t["user"] = it->user;
      t["expiry"] = it->expiry.toString(Qt::ISODate);
      t["stamp"] = it->stamp;
      obj[it.key()] = t;
   }

   /* whole or not at all, and private from the start */
   QDir().mkpath(QFileInfo(_path).path());
   QSaveFile f(_path);
   if (!f.open(QIODevice::WriteOnly)) {
      qWarning() << "TokenStore: cannot write" << _path;
      return;
   }
   f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
   f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
   if (!f.commit())
      qWarning() << "TokenStore: cannot write" << _path;
}


QString TokenStore::mint(const QString &user, int ttl_days,
                         const QString &stamp)
{
   /* 32 random bytes, hex-encoded.  Token is opaque to the client. */
   QByteArray raw(32, 0);
   QRandomGenerator::securelySeeded().fillRange(
      reinterpret_cast<quint32 *>(raw.data()), raw.size() / 4);
   QString token = QString::fromLatin1(raw.toHex());

   Info info;
   info.user = user;
   info.expiry = QDateTime::currentDateTime().addDays(ttl_days);
   info.stamp = stamp;
   _tokens.insert(keyFor(token), info);
   save();
   return token;
}


QString TokenStore::lookup(const QString &token, QString *stamp)
{
   auto it = _tokens.find(keyFor(token));
   if (it == _tokens.end())
      return QString();
   if (it->expiry.isValid()
       && it->expiry < QDateTime::currentDateTime()) {
      _tokens.erase(it);
      save();
      return QString();
   }
   if (stamp)
      *stamp = it->stamp;
   return it->user;
}


void TokenStore::revoke(const QString &token)
{
   if (_tokens.remove(keyFor(token)))
      save();
}
