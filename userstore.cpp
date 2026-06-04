/*
License: GPL-2
*/

#include "userstore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStandardPaths>


/* Iteration count tuned to ~50 ms on a 2024-era laptop core.  Using
 * PBKDF2-HMAC-SHA256 keeps the dependencies inside Qt; argon2 would be
 * stronger but pulls in libargon2.  Bump iter when CPUs get faster. */
static const int kPbkdf2Iters = 200000;
static const int kSaltBytes   = 16;
static const int kHashBytes   = 32;


static QByteArray randomBytes(int n)
{
   QByteArray out(n, 0);
   QRandomGenerator::securelySeeded().fillRange(
      reinterpret_cast<quint32 *>(out.data()), n / 4);
   /* Tail bytes if n % 4 != 0 — paranoia.  kSaltBytes is divisible by 4. */
   return out;
}


/* PBKDF2-HMAC-SHA256 implementation using Qt's HMAC primitive.  RFC 8018. */
static QByteArray pbkdf2_sha256(const QByteArray &password,
                                const QByteArray &salt, int iters,
                                int outLen)
{
   QByteArray out;
   int blocks = (outLen + 31) / 32;
   for (int i = 1; i <= blocks; i++) {
      QByteArray block_index(4, 0);
      block_index[0] = (char)((i >> 24) & 0xff);
      block_index[1] = (char)((i >> 16) & 0xff);
      block_index[2] = (char)((i >> 8)  & 0xff);
      block_index[3] = (char)(i & 0xff);

      QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
      mac.setKey(password);
      mac.addData(salt);
      mac.addData(block_index);
      QByteArray u = mac.result();
      QByteArray t = u;

      for (int j = 1; j < iters; j++) {
         QMessageAuthenticationCode mac2(QCryptographicHash::Sha256);
         mac2.setKey(password);
         mac2.addData(u);
         u = mac2.result();
         for (int k = 0; k < t.size(); k++)
            t[k] = t[k] ^ u[k];
      }
      out.append(t);
   }
   out.truncate(outLen);
   return out;
}


QString UserStore::hashPassword(const QString &password)
{
   QByteArray salt = randomBytes(kSaltBytes);
   QByteArray hash = pbkdf2_sha256(password.toUtf8(), salt, kPbkdf2Iters,
                                   kHashBytes);
   return QString("pbkdf2-sha256$%1$%2$%3")
            .arg(kPbkdf2Iters)
            .arg(QString::fromLatin1(salt.toBase64()))
            .arg(QString::fromLatin1(hash.toBase64()));
}


bool UserStore::verifyPassword(const QString &password,
                               const QString &storedHash)
{
   QStringList parts = storedHash.split('$');
   if (parts.size() != 4 || parts[0] != "pbkdf2-sha256")
      return false;
   bool ok = false;
   int iters = parts[1].toInt(&ok);
   if (!ok || iters < 1)
      return false;
   QByteArray salt = QByteArray::fromBase64(parts[2].toLatin1());
   QByteArray expected = QByteArray::fromBase64(parts[3].toLatin1());
   if (salt.isEmpty() || expected.isEmpty())
      return false;
   QByteArray actual = pbkdf2_sha256(password.toUtf8(), salt, iters,
                                     expected.size());
   /* Constant-time comparison.  Length must match. */
   if (actual.size() != expected.size())
      return false;
   int diff = 0;
   for (int i = 0; i < actual.size(); i++)
      diff |= (uchar)actual[i] ^ (uchar)expected[i];
   return diff == 0;
}


static QString defaultPath()
{
   return QStandardPaths::writableLocation(
                QStandardPaths::GenericConfigLocation)
          + "/paperman-server/users.json";
}


UserStore::UserStore()
   : _path(defaultPath())
{
   load();
}


UserStore::UserStore(const QString &path)
   : _path(path)
{
   load();
}


bool UserStore::load()
{
   _users.clear();

   QFile f(_path);
   if (!f.exists())
      return true;  /* empty store is fine */
   if (!f.open(QIODevice::ReadOnly))
      return false;
   QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
   if (!doc.isObject())
      return false;
   QJsonObject obj = doc.object();
   for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
      QString name = it.key();
      QJsonObject u = it.value().toObject();
      User user;
      user.name  = name;
      user.hash  = u.value("hash").toString();
      user.admin = u.value("admin").toBool(false);
      QJsonArray repos = u.value("repos").toArray();
      for (auto r : repos)
         user.repos << r.toString();
      _users.insert(name, user);
   }
   return true;
}


bool UserStore::save()
{
   QFileInfo fi(_path);
   QDir().mkpath(fi.dir().path());

   QJsonObject obj;
   for (auto it = _users.constBegin(); it != _users.constEnd(); ++it) {
      const User &u = it.value();
      QJsonObject record;
      record["hash"]  = u.hash;
      record["admin"] = u.admin;
      QJsonArray repos;
      for (const QString &r : u.repos)
         repos.append(r);
      record["repos"] = repos;
      obj[u.name] = record;
   }

   QFile f(_path);
   if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;
   f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
   f.close();
   /* Tighten permissions: hashes are sensitive even though they're
    * derived. */
   QFile::setPermissions(_path,
      QFile::ReadOwner | QFile::WriteOwner);
   return true;
}


QStringList UserStore::userNames() const
{
   QStringList out = _users.keys();
   out.sort();
   return out;
}


bool UserStore::hasUser(const QString &name) const
{
   return _users.contains(name);
}


bool UserStore::addUser(const QString &name, const QString &password)
{
   if (name.isEmpty() || _users.contains(name))
      return false;
   User u;
   u.name = name;
   u.hash = hashPassword(password);
   _users.insert(name, u);
   return save();
}


bool UserStore::setPassword(const QString &name, const QString &password)
{
   auto it = _users.find(name);
   if (it == _users.end())
      return false;
   it->hash = hashPassword(password);
   return save();
}


bool UserStore::delUser(const QString &name)
{
   if (_users.remove(name) == 0)
      return false;
   return save();
}


bool UserStore::setRepos(const QString &name, const QStringList &repos)
{
   auto it = _users.find(name);
   if (it == _users.end())
      return false;
   it->repos = repos;
   return save();
}


bool UserStore::verify(const QString &name, const QString &password) const
{
   auto it = _users.constFind(name);
   if (it == _users.constEnd())
      return false;
   return verifyPassword(password, it->hash);
}


bool UserStore::repoAllowed(const QString &name, const QString &repo) const
{
   auto it = _users.constFind(name);
   if (it == _users.constEnd())
      return false;
   if (it->repos.isEmpty())
      return true;
   return it->repos.contains(repo);
}


const UserStore::User *UserStore::lookup(const QString &name) const
{
   auto it = _users.constFind(name);
   if (it == _users.constEnd())
      return nullptr;
   return &it.value();
}
