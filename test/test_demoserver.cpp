/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
*/

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "demoserver.h"
#include "remotebackend.h"
#include "test.h"
#include "test_demoserver.h"

static const char *const kStacks[] = {"one.max", "two.max"};


void TestDemoServer::init ()
   {
#ifndef Q_OS_UNIX
   QSKIP ("the demo server needs Unix");
#endif
   _dir = new QTemporaryDir ();
   QVERIFY (_dir->isValid ());
   _papers = _dir->path () + "/Paperman";
   QVERIFY (QDir ().mkpath (_papers + "/bills"));
   for (const char *name : kStacks)
      QVERIFY (QFile::copy (Test::testSrc + "/testfile.max",
                            _papers + "/bills/" + name));
   }


void TestDemoServer::cleanup ()
   {
   delete _demo;
   _demo = nullptr;
   delete _dir;
   _dir = nullptr;
   }


void TestDemoServer::startDemo ()
   {
   QString error;

   if (!_demo)
      _demo = new DemoServer (_papers, _dir->path () + "/sessions", "demo",
                              0);
   QVERIFY2 (_demo->start (error), qPrintable (error));
   }


QByteArray TestDemoServer::request (const QByteArray &method,
                                    const QString &path,
                                    const QString &token,
                                    const QByteArray &body)
   {
   QTcpSocket sock;
   QByteArray req = method + " " + path.toUtf8() + " HTTP/1.1\r\n"
                    "Host: localhost\r\nConnection: close\r\n";
   QByteArray got;

   sock.connectToHost (QHostAddress::LocalHost, _demo->serverPort ());
   if (!QTest::qWaitFor ([&] {
          return sock.state () == QAbstractSocket::ConnectedState; }, 5000))
      return QByteArray ();
   if (!token.isEmpty ())
      req += "Authorization: Bearer " + token.toLatin1 () + "\r\n";
   if (method == "POST")
      req += "Content-Length: " + QByteArray::number (body.size ()) + "\r\n";
   sock.write (req + "\r\n" + body);

   /* the server and the client share this thread, so let both run, until
      the server closes the connection, having answered */
   if (!QTest::qWaitFor ([&] {
          got += sock.readAll ();
          return sock.state () == QAbstractSocket::UnconnectedState;
       }, 10000))
      qWarning () << "the demo server did not close the connection";
   return got + sock.readAll ();
   }


//! the body of a response
static QByteArray bodyOf (const QByteArray &resp)
   {
   int sep = resp.indexOf ("\r\n\r\n");

   return sep < 0 ? QByteArray () : resp.mid (sep + 4);
   }


QString TestDemoServer::login (const QString &user, const QString &password)
   {
   QJsonObject in {{"user", user}, {"password", password}};
   QByteArray resp = request ("POST", "/v1/auth/login", QString (),
                              QJsonDocument (in).toJson ());

   return QJsonDocument::fromJson (bodyOf (resp)).object ()
          .value ("token").toString ();
   }


void TestDemoServer::testStatusWithoutLogin ()
   {
   startDemo ();

   QByteArray resp = request ("GET", "/v1/status");

   QVERIFY (resp.startsWith ("HTTP/1.1 200"));
   QVERIFY (bodyOf (resp).contains ("\"apiVersion\""));

   // anything else asks for a login, in words a client knows
   resp = request ("GET", "/repos");
   QVERIFY (resp.startsWith ("HTTP/1.1 401"));
   QVERIFY (bodyOf (resp).contains ("credentials"));
   }


void TestDemoServer::testLoginStartsSession ()
   {
   startDemo ();

   QCOMPARE (login ("test", "wrong"), QString ());
   QCOMPARE (login ("", "demo"), QString ());
   QCOMPARE (_demo->sessionCount (), 0);

   QString token = login ();

   QVERIFY (!token.isEmpty ());
   QCOMPARE (_demo->sessionCount (), 1);

   // the session has its own copy of the papers, which it serves
   QString dir = _demo->sessionDir (token);

   QVERIFY (QFile::exists (dir + "/Paperman/bills/one.max"));

   QByteArray resp = request ("GET", "/repos", token);

   QVERIFY (resp.startsWith ("HTTP/1.1 200"));
   QVERIFY (bodyOf (resp).contains ("\"Paperman\""));
   }


void TestDemoServer::testSessionsApart ()
   {
   startDemo ();

   // two people may give the same name
   QString first = login ("test");
   QString second = login ("test");

   QVERIFY (!first.isEmpty () && !second.isEmpty ());
   QVERIFY (first != second);
   QCOMPARE (_demo->sessionCount (), 2);

   QByteArray resp = request ("POST",
      "/v1/repos/Paperman/stacks/bills/one.max/delete", first);

   QVERIFY2 (resp.startsWith ("HTTP/1.1 200"), resp.constData ());
   QVERIFY (!QFile::exists (_demo->sessionDir (first)
                            + "/Paperman/bills/one.max"));

   // the other session and the papers themselves still have it
   QVERIFY (QFile::exists (_demo->sessionDir (second)
                           + "/Paperman/bills/one.max"));
   QVERIFY (QFile::exists (_papers + "/bills/one.max"));
   }


void TestDemoServer::testQuietSessionEnds ()
   {
   DemoServer::Limits limits;

   limits.idleMs = 1;
   _demo = new DemoServer (_papers, _dir->path () + "/sessions", "demo", 0);
   _demo->setLimits (limits);
   startDemo ();

   QString token = login ();
   QString dir = _demo->sessionDir (token);

   QVERIFY (QFileInfo::exists (dir));

   // once nothing is connected and it is quiet for long enough
   QTest::qWait (20);
   _demo->reap ();
   QCOMPARE (_demo->sessionCount (), 0);
   QVERIFY (!QFileInfo::exists (dir));

   // its token no longer works
   QVERIFY (request ("GET", "/repos", token).startsWith ("HTTP/1.1 401"));
   }


void TestDemoServer::testSessionLimit ()
   {
   DemoServer::Limits limits;

   limits.sessions = 1;
   _demo = new DemoServer (_papers, _dir->path () + "/sessions", "demo", 0);
   _demo->setLimits (limits);
   startDemo ();

   QVERIFY (!login ().isEmpty ());

   QJsonObject in {{"user", "another"}, {"password", "demo"}};
   QByteArray resp = request ("POST", "/v1/auth/login", QString (),
                              QJsonDocument (in).toJson ());

   QVERIFY (resp.startsWith ("HTTP/1.1 503"));
   QVERIFY (bodyOf (resp).contains ("busy"));
   QCOMPARE (_demo->sessionCount (), 1);
   }


void TestDemoServer::testUploadLimit ()
   {
   DemoServer::Limits limits;
   qint64 papers = QFileInfo (_papers + "/bills/one.max").size ()
                   * int (sizeof (kStacks) / sizeof (kStacks[0]));

   // room for the papers and a little more
   limits.maxBytes = papers + 1000;
   _demo = new DemoServer (_papers, _dir->path () + "/sessions", "demo", 0);
   _demo->setLimits (limits);
   startDemo ();

   QString token = login ();

   QByteArray resp = request ("POST",
      "/v1/repos/Paperman/stacks/bills/small.pdf/upload", token,
      QByteArray (500, 'x'));

   QVERIFY2 (resp.startsWith ("HTTP/1.1 200"), resp.constData ());

   resp = request ("POST", "/v1/repos/Paperman/stacks/bills/big.pdf/upload",
                   token, QByteArray (5000, 'x'));
   QVERIFY2 (resp.startsWith ("HTTP/1.1 413"), resp.constData ());
   QVERIFY (!QFile::exists (_demo->sessionDir (token)
                            + "/Paperman/bills/big.pdf"));
   }


//! the names of the stacks in the bills folder, as @p client sees them
static QStringList billNames (RemoteBackend &client)
   {
   QStringList names;

   for (const DirectoryEntry &entry
        : client.browseDirectory ("Paperman", "bills").entries)
      names << entry.name;
   names.sort ();
   return names;
   }


void TestDemoServer::testClient ()
   {
   startDemo ();

   QUrl url (QString ("http://127.0.0.1:%1").arg (_demo->serverPort ()));
   RemoteBackend first (url);
   RemoteBackend second (url);
   const QStringList both = {"one.max", "two.max"};

   // nothing is shown without the demo's password
   QVERIFY (!first.login ("daniel", "wrong"));
   QVERIFY (billNames (first).isEmpty ());

   // the same name twice gives two sessions
   QVERIFY (first.login ("daniel", "demo"));
   QVERIFY (second.login ("daniel", "demo"));
   QCOMPARE (_demo->sessionCount (), 2);
   QCOMPARE (billNames (first), both);
   QCOMPARE (billNames (second), both);

   // what one does, the other does not see
   QVERIFY (first.deleteStack ("Paperman", "bills/one.max"));
   QCOMPARE (billNames (first), QStringList {"two.max"});
   QCOMPARE (billNames (second), both);
   }
