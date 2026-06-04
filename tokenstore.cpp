/*
License: GPL-2
*/

#include "tokenstore.h"

#include <QRandomGenerator>


TokenStore::TokenStore()
{
}


QString TokenStore::mint(const QString &user, int ttl_days)
{
   /* 32 random bytes, hex-encoded.  Token is opaque to the client. */
   QByteArray raw(32, 0);
   QRandomGenerator::securelySeeded().fillRange(
      reinterpret_cast<quint32 *>(raw.data()), raw.size() / 4);
   QString token = QString::fromLatin1(raw.toHex());

   Info info;
   info.user = user;
   info.expiry = QDateTime::currentDateTime().addDays(ttl_days);
   _tokens.insert(token, info);
   return token;
}


QString TokenStore::lookup(const QString &token)
{
   auto it = _tokens.find(token);
   if (it == _tokens.end())
      return QString();
   if (it->expiry.isValid()
       && it->expiry < QDateTime::currentDateTime()) {
      _tokens.erase(it);
      return QString();
   }
   return it->user;
}


void TokenStore::revoke(const QString &token)
{
   _tokens.remove(token);
}
