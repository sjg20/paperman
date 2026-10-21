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

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTcpServer>

class QTcpSocket;
class QTimer;
class SearchServer;

/** A server for people to try Paperman with, which gives each of them a
    copy of some papers of their own.

    Logging in starts a session: a copy of the papers in the template
    folder, served by a SearchServer of its own to the token the login
    returns. The name and password given choose the session, so logging
    in with them again, as from a phone beside the desktop, joins it, and
    both see the same papers, changing as either changes them. Anyone
    else, with another name or password, has a session of their own, so
    each visitor can file, rename and scan as they like, and sees nothing
    of anyone else's. A session ends once it has been quiet for a while,
    and its copy is deleted. Being quiet means being asked for nothing, as
    a client left open asks only to hear of changes; however busy, a
    session ends a day after it started, so that one kept in use by a
    script does not go on for ever.

    Each connection is looked at before it is handed on: the request's
    headers are peeked at, without being read, to find its token, then
    the connection goes to that session's server, which reads the request
    as if it had accepted the connection itself. Logging in, the status
    and a request without a valid token are answered here, and their
    connections closed.

    It listens only on the loopback address, as it is meant to sit behind
    a web server which gives it HTTPS and limits how much each client
    sends. It needs Unix, for peeking at a connection */
class DemoServer : public QTcpServer
   {
   Q_OBJECT

public:
   struct Limits
      {
      int sessions = 100;      //!< sessions at once, at most
      /** how long a session may go without being asked for anything but
          its event stream, which a client keeps open whether it is used
          or not */
      qint64 idleMs = 3600000;
      qint64 lifetimeMs = 24 * 3600000LL;  //!< how long a session may last
      qint64 maxBytes = 200LL * 1024 * 1024;  //!< space for each session
      };

   /** Prepare a demo server

      \param templateDir  the papers each session starts with; the
                          folder's name is the repository's
      \param sessionDir   where the sessions' copies go: everything in it
                          is deleted when the server starts
      \param password     the password which logs in, or an empty string
                          to let any in
      \param port         the port to listen on, on the loopback address
      \param parent       the parent object */
   DemoServer (const QString &templateDir, const QString &sessionDir,
               const QString &password, quint16 port,
               QObject *parent = nullptr);
   ~DemoServer () override;

   //! set the limits, before start()
   void setLimits (const Limits &limits) { _limits = limits; }

   /** Clear out the session folder and start listening

      \param error  returns what went wrong
      \returns true if listening */
   bool start (QString &error);

   //! the number of sessions under way
   int sessionCount () const { return _sessions.size (); }

   //! the folder of the session with @p token, or an empty string
   QString sessionDir (const QString &token) const;

   /** end the sessions which have been quiet too long, are too old or
       use too much */
   void reap ();

protected:
   void incomingConnection (qintptr socketDescriptor) override;

private:
   //! a connection whose request has not yet come in full
   struct Pending
      {
      qintptr fd;
      QElapsedTimer age;
      };

   struct Session
      {
      QStringList tokens;       //!< those it was given, the newest last
      QString dir;              //!< the session's folder
      QString repo;             //!< its copy of the papers
      SearchServer *server = nullptr;  //!< serves the copy
      QElapsedTimer age;        //!< since it started
      };

   //! look at each pending connection, handing on those with a request
   void poll ();

   /** Act on a connection whose request headers have come in

      \param fd      the connection
      \param header  its request line and headers, still unread */
   void dispatch (qintptr fd, const QByteArray &header);

   //! answer a login on @p fd, starting a session
   void login (qintptr fd);

   /** Join the session which a name and password choose, starting it if
       it is not under way

      \param user      the name
      \param password  the password
      \param token     returns a new token for the session
      \returns an empty string if it worked, else what went wrong */
   QString joinSession (const QString &user, const QString &password,
                        QString &token);

   //! end the session with @p key, deleting its copy
   void endSession (const QString &key);

   //! answer on @p fd and close it, the connection not having been read
   void reply (qintptr fd, int code, const QByteArray &body);

   //! answer on a socket and close it
   static void reply (QTcpSocket *sock, int code, const QByteArray &body);

   QString _template;           //!< see the constructor
   QString _sessionDir;
   QString _password;
   quint16 _port;
   Limits _limits;
   SearchServer *_status;       //!< serves the template, for the status
   QList<Pending> _pending;
   //! by key, which comes from the name and password
   QHash<QString, Session> _sessions;
   QHash<QString, QString> _tokens;    //!< the key of each token's session
   QTimer *_poll;
   QTimer *_reap;
   };
