/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimer>

#include "config.h"
#include "demoserver.h"
#include "searchserver.h"

#ifdef Q_OS_UNIX
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#endif

//! a request whose headers take longer than this to come in is dropped
static const qint64 kHeaderMs = 10000;

//! a request's headers, at most
static const int kHeaderBytes = 16384;


DemoServer::DemoServer (const QString &templateDir,
                        const QString &sessionDir, const QString &password,
                        quint16 port, QObject *parent)
   : QTcpServer (parent),
     _template (QDir (templateDir).absolutePath ()),
     _sessionDir (QDir (sessionDir).absolutePath ()),
     _password (password),
     _port (port)
   {
   _status = new SearchServer (QStringList {_template}, 0, this);
   _status->setTextIndex (true);

   _poll = new QTimer (this);
   _poll->setInterval (20);
   connect (_poll, &QTimer::timeout, this, &DemoServer::poll);

   _reap = new QTimer (this);
   _reap->setInterval (30000);
   connect (_reap, &QTimer::timeout, this, &DemoServer::reap);
   }


DemoServer::~DemoServer ()
   {
   const QStringList keys = _sessions.keys ();

   for (const QString &key : keys)
      endSession (key);
#ifdef Q_OS_UNIX
   for (int i = 0; i < _pending.size (); i++)
      ::close (_pending[i].fd);
#endif
   }


bool DemoServer::start (QString &error)
   {
#ifndef Q_OS_UNIX
   error = "the demo server needs Unix";
   return false;
#else
   QFileInfo tmpl (_template);

   if (!tmpl.isDir ())
      {
      error = QString ("%1 is not a folder").arg (_template);
      return false;
      }
   if (_sessionDir == _template
       || _sessionDir.startsWith (_template + "/")
       || _template.startsWith (_sessionDir + "/"))
      {
      error = "the session folder and the papers must be apart";
      return false;
      }

   // anything left from before is no one's now
   QDir (_sessionDir).removeRecursively ();
   if (!QDir ().mkpath (_sessionDir))
      {
      error = QString ("cannot make %1").arg (_sessionDir);
      return false;
      }

   _status->prepare ();
   if (!listen (QHostAddress::LocalHost, _port))
      {
      error = errorString ();
      return false;
      }
   _reap->start ();
   return true;
#endif
   }


QString DemoServer::sessionDir (const QString &token) const
   {
   auto it = _sessions.constFind (_tokens.value (token));

   return it == _sessions.constEnd () ? QString () : it->dir;
   }


void DemoServer::incomingConnection (qintptr socketDescriptor)
   {
   Pending pend;

   pend.fd = socketDescriptor;
   pend.age.start ();
   _pending.append (pend);
   if (!_poll->isActive ())
      _poll->start ();
   }


void DemoServer::poll ()
   {
#ifdef Q_OS_UNIX
   static char buf[kHeaderBytes];

   for (int i = _pending.size () - 1; i >= 0; i--)
      {
      Pending pend = _pending[i];

      /* look at what has come, leaving it to be read by whoever serves
         the request */
      ssize_t len = ::recv (pend.fd, buf, sizeof (buf),
                            MSG_PEEK | MSG_DONTWAIT);
      bool waiting = len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
      QByteArray data = len > 0 ? QByteArray (buf, len) : QByteArray ();
      int end = data.indexOf ("\r\n\r\n");

      if (end >= 0)
         {
         _pending.removeAt (i);
         dispatch (pend.fd, data.left (end));
         }
      else if (len == sizeof (buf))
         {
         _pending.removeAt (i);
         reply (pend.fd, 431, "{\"success\":false,"
                "\"error\":\"Request header too large\"}");
         }
      else if ((!waiting && len <= 0) || pend.age.elapsed () > kHeaderMs)
         {
         // closed before the request, or too slow with it
         _pending.removeAt (i);
         ::close (pend.fd);
         }
      }
   if (_pending.isEmpty ())
      _poll->stop ();
#endif
   }


void DemoServer::dispatch (qintptr fd, const QByteArray &header)
   {
   const QList<QByteArray> lines = header.split ('\n');
   const QList<QByteArray> request = lines.value (0).trimmed ().split (' ');
   QByteArray method = request.value (0);
   QByteArray path = request.value (1);
   QByteArray token;

   path = path.left (path.indexOf ('?') < 0 ? path.size ()
                                            : path.indexOf ('?'));
   for (const QByteArray &line : lines)
      {
      int colon = line.indexOf (':');

      if (colon > 0 && line.left (colon).trimmed ().toLower ()
                          == "authorization")
         {
         QByteArray value = line.mid (colon + 1).trimmed ();

         if (value.toLower ().startsWith ("bearer "))
            token = value.mid (7).trimmed ();
         }
      }

   auto it = _sessions.find (_tokens.value (QString::fromLatin1 (token)));

   if (method == "POST" && path == "/v1/auth/login")
      login (fd);
   else if (it != _sessions.end ())
      it->server->takeConnection (fd);
   else if (path == "/v1/status")
      reply (fd, 200, QJsonDocument (_status->statusObject ())
                         .toJson (QJsonDocument::Compact));
   else if (path == "/" || path == "/status")
      reply (fd, 200, QString ("{\"status\":\"running\",\"version\":\"%1\"}")
                         .arg (CONFIG_version_str).toUtf8 ());
   else
      /* the same words as SearchServer's, which a client takes as a
         sign to log in */
      reply (fd, 401, "{\"success\":false,\"error\":\"Invalid or missing "
             "credentials. Log in to start a session, or to join one with "
             "its name and password\"}");
   }


void DemoServer::login (qintptr fd)
   {
   QTcpSocket *sock = new QTcpSocket (this);
   QByteArray *got = new QByteArray;
   QTimer *limit = new QTimer (sock);

   if (!sock->setSocketDescriptor (fd))
      {
      delete got;
      delete sock;
      return;
      }
   limit->setSingleShot (true);
   connect (limit, &QTimer::timeout, sock, &QTcpSocket::abort);
   connect (sock, &QTcpSocket::disconnected, sock, [sock, got] () {
      delete got;
      sock->deleteLater ();
   });

   auto handle = [this, sock, got] () {
      if (sock->property ("answered").toBool ())
         return;
      got->append (sock->readAll ());

      int end = got->indexOf ("\r\n\r\n");

      if (end < 0)
         return;

      // wait for the body, which says who is logging in
      qint64 want = 0;

      for (const QByteArray &line : got->left (end).split ('\n'))
         if (line.toLower ().startsWith ("content-length:"))
            want = line.mid (15).trimmed ().toLongLong ();
      if (got->size () < end + 4 + want)
         return;

      QJsonObject in = QJsonDocument::fromJson (
                          got->mid (end + 4, want)).object ();
      QString user = in.value ("user").toString ();
      QString password = in.value ("password").toString ();
      QString token;

      sock->setProperty ("answered", true);
      if (user.isEmpty () || password.isEmpty ()
          || (!_password.isEmpty () && password != _password))
         {
         reply (sock, 401, "{\"success\":false,"
                "\"error\":\"Invalid credentials\"}");
         return;
         }

      QString err = joinSession (user, password, token);

      if (!err.isEmpty ())
         {
         reply (sock, 503, QJsonDocument (QJsonObject {
                              {"success", false}, {"error", err}})
                              .toJson (QJsonDocument::Compact));
         return;
         }

      QJsonObject out;

      out["token"] = token;
      out["user"] = user;
      out["expiry"] = QDateTime::currentDateTime ().addDays (1)
                         .toString (Qt::ISODate);
      reply (sock, 200, QJsonDocument (out).toJson (QJsonDocument::Compact));
   };

   connect (sock, &QTcpSocket::readyRead, this, handle);
   limit->start (kHeaderMs);
   handle ();
   }


//! copy the folder @p from to @p to, which must not yet be there
static bool copyTree (const QString &from, const QString &to)
   {
   if (!QDir ().mkpath (to))
      return false;

   QDirIterator it (from, QDir::AllEntries | QDir::Hidden | QDir::System
                    | QDir::NoDotAndDotDot);

   while (it.hasNext ())
      {
      it.next ();

      QFileInfo info = it.fileInfo ();
      QString dest = to + "/" + info.fileName ();

      if (info.isSymLink ())
         continue;
      if (info.isDir () ? !copyTree (info.filePath (), dest)
                        : !QFile::copy (info.filePath (), dest))
         return false;
      }
   return true;
   }


//! a random string of @p bytes bytes, as hex
static QString randomHex (int bytes)
   {
   QByteArray raw (bytes, 0);

   for (char &ch : raw)
      ch = char (QRandomGenerator::system ()->bounded (256));
   return QString::fromLatin1 (raw.toHex ());
   }


//! the tokens a session keeps, as logging in again gives another
static const int kMaxTokens = 20;


QString DemoServer::joinSession (const QString &user,
                                 const QString &password, QString &token)
   {
   /* the name and password choose the session; keep neither, nor what a
      key is made from */
   QString key = QString::fromLatin1 (QCryptographicHash::hash (
                    (user + QChar (0) + password).toUtf8 (),
                    QCryptographicHash::Sha256).toHex ());
   auto it = _sessions.find (key);

   token = randomHex (32);
   if (it != _sessions.end ())
      {
      it->tokens.append (token);
      _tokens.insert (token, key);
      while (it->tokens.size () > kMaxTokens)
         _tokens.remove (it->tokens.takeFirst ());
      return QString ();
      }

   if (_sessions.size () >= _limits.sessions)
      reap ();
   if (_sessions.size () >= _limits.sessions)
      return "The demo is busy: please try again later";

   Session sess;

   sess.dir = _sessionDir + "/" + randomHex (8);
   sess.repo = sess.dir + "/" + QFileInfo (_template).fileName ();
   if (!copyTree (_template, sess.repo))
      {
      QDir (sess.dir).removeRecursively ();
      return "The demo cannot start a session";
      }

   sess.server = new SearchServer (QStringList {sess.repo}, 0, this);
   sess.server->setTextIndex (true);
   sess.server->setMaxRepoBytes (_limits.maxBytes);
   sess.server->prepare ();
   sess.age.start ();
   sess.tokens.append (token);

   _sessions.insert (key, sess);
   _tokens.insert (token, key);
   qInfo () << "DemoServer: started a session in" << sess.dir << "with"
            << _sessions.size () << "under way";
   return QString ();
   }


void DemoServer::endSession (const QString &key)
   {
   Session sess = _sessions.take (key);

   if (!sess.server)
      return;
   for (const QString &token : sess.tokens)
      _tokens.remove (token);

   // the server's connections are its children, so close with it
   delete sess.server;
   SearchServer::removeDataFiles (sess.repo);
   QDir (sess.dir).removeRecursively ();
   qInfo () << "DemoServer: ended the session in" << sess.dir << "with"
            << _sessions.size () << "under way";
   }


void DemoServer::reap ()
   {
   const QStringList keys = _sessions.keys ();

   for (const QString &key : keys)
      {
      Session &sess = _sessions[key];

      /* a client left open keeps asking to hear of changes, so a session
         is in use only while it is asked for other things */
      bool idle = sess.server->msSinceRequest () > _limits.idleMs;
      bool old = sess.age.elapsed () > _limits.lifetimeMs;

      /* uploads are refused beyond the limit, but copying stacks is not:
         a session which grows well past it is ended */
      bool big = SearchServer::folderBytes (sess.dir)
                 > _limits.maxBytes + _limits.maxBytes / 2;

      if (idle || old || big)
         endSession (key);
      }
   }


void DemoServer::reply (qintptr fd, int code, const QByteArray &body)
   {
   QTcpSocket *sock = new QTcpSocket (this);

   if (!sock->setSocketDescriptor (fd))
      {
      delete sock;
      return;
      }

   /* read what has come of the request, so that closing the connection
      does not reset it before the reply is read */
   sock->readAll ();
   connect (sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
   reply (sock, code, body);
   }


void DemoServer::reply (QTcpSocket *sock, int code, const QByteArray &body)
   {
   const char *reason = code == 200 ? "OK"
                      : code == 401 ? "Unauthorized"
                      : code == 431 ? "Request Header Fields Too Large"
                      : code == 503 ? "Service Unavailable" : "Error";
   QByteArray out = QString ("HTTP/1.1 %1 %2\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: %3\r\n"
                             "Connection: close\r\n\r\n")
                       .arg (code).arg (reason).arg (body.size ()).toLatin1 ();

   sock->write (out + body);
   sock->disconnectFromHost ();
   }
