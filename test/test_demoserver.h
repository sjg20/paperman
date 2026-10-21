/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
*/

#pragma once

#include "suite.h"

class DemoServer;
class QTemporaryDir;

/** Tests of the demo server, which gives each login a copy of the papers
    of its own */
class TestDemoServer : public Suite
   {
   Q_OBJECT

public:
   using Suite::Suite;

private slots:
   void init ();
   void cleanup ();

   //! the status is there without a login, but nothing else is
   void testStatusWithoutLogin ();

   //! the demo's password starts a session, with a copy of the papers
   void testLoginStartsSession ();

   //! what is done in one session is not seen in another
   void testSessionsApart ();

   //! a session quiet for long enough ends, and its copy goes
   void testQuietSessionEnds ();

   //! no more sessions start than the limit allows
   void testSessionLimit ();

   //! an upload which takes a session over its space is refused
   void testUploadLimit ();

   /** the client the desktop program uses logs in and works with its own
       session, apart from another client's */
   void testClient ();

private:
   /** Start a demo server on a copy of a few test stacks, with @p limits
       if given */
   void startDemo ();

   /** Send a request to the demo server

      \param method  GET or POST
      \param path    the path and query
      \param token   the session's token, if any
      \param body    the body, for a POST
      \returns the whole response, headers and body */
   QByteArray request (const QByteArray &method, const QString &path,
                       const QString &token = QString (),
                       const QByteArray &body = QByteArray ());

   //! log in, returning the token, or an empty string
   QString login (const QString &user = "test",
                  const QString &password = "demo");

   QTemporaryDir *_dir = nullptr;   //!< holds the papers and the sessions
   QString _papers;                 //!< the template
   DemoServer *_demo = nullptr;
   };
