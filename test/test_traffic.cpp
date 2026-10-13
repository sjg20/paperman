/*
License: GPL-2
*/

#include <QStandardPaths>
#include <QtTest/QtTest>

#include "test.h"

#include "../desktopmodel.h"
#include "../desktopview.h"
#include "../desktopwidget.h"
#include "../mainwindow.h"
#include "../pagemodel.h"
#include "../pagewidget.h"
#include "../remotebackend.h"

#include "test_traffic.h"

// the port the server listens on, behind the proxy
static const quint16 kPort = 9878;

// the stacks in the folder, each a copy of a five-page test stack
static const QStringList kStacks = {"a.max", "b.max", "c.max", "d.max",
                                    "e.max"};


TestTraffic::TestTraffic (QString name) : Suite (name)
   {
   }


TestTraffic::~TestTraffic () = default;


void TestTraffic::initTestCase ()
   {
   // keep the page cache and server state out of the user's own
   QStandardPaths::setTestModeEnabled (true);
   }


void TestTraffic::cleanupTestCase ()
   {
   QStandardPaths::setTestModeEnabled (false);
   }


void TestTraffic::init ()
   {
   /* other suites leave users for the server in the test-mode settings,
      and then the desktop would stop to ask for a login */
   QFile::remove (QStandardPaths::writableLocation (
                     QStandardPaths::GenericConfigLocation)
                  + "/paperman-server/users.json");

   _config.reset (new QTemporaryDir);
   QVERIFY (_config->isValid ());
   qputenv ("PAPERMAN_CONFIG",
            (_config->path () + "/client.conf").toLocal8Bit ());

   /* a new repository each time, whose name is new too, so that nothing
      is in the desktop's cache from before */
   _repo.reset (new QTemporaryDir);
   QVERIFY (_repo->isValid ());
   QVERIFY (QDir (_repo->path ()).mkdir ("papers"));
   for (const QString &name : kStacks)
      QVERIFY (QFile::copy (testSrc + "/testfile.max",
                            _repo->path () + "/papers/" + name));
   QVERIFY (QFile::copy (testSrc + "/testpdf.pdf",
                         _repo->path () + "/papers/f.pdf"));

   _server.reset (new ServerThread (_repo->path (), kPort));
   QVERIFY (_server->begin ());

   _me.reset (new Mainwindow);
   _desktop = _me->getDesktop ();
   _me->resize (1600, 1000);
   _me->show ();
   QVERIFY (QTest::qWaitForWindowExposed (_me.get ()));
   }


void TestTraffic::cleanup ()
   {
   _me.reset ();
   _desktop = nullptr;
   _server.reset ();
   _repo.reset ();
   qunsetenv ("PAPERMAN_CONFIG");
   _config.reset ();
   }


QString TestTraffic::repoPath () const
   {
   return _server->url ().toString () + "/"
          + QFileInfo (_repo->path ()).fileName ();
   }


TestTraffic::Totals TestTraffic::traffic (const QString &what)
   {
   QMap<QString, Totals> byEndpoint;
   QElapsedTimer quiet, all;

   /* the desktop has finished once nothing more has come for a while;
      the event stream stays open throughout, so is not waited for */
   quiet.start ();
   all.start ();
   while (quiet.elapsed () < 700 && all.elapsed () < 30000)
      {
      QTest::qWait (50);

      const QList<ServerThread::Exchange> got = _server->takeExchanges ();

      for (const ServerThread::Exchange &ex : got)
         {
         Totals &total = byEndpoint [ex.request.section ('?', 0, 0)];

         total.lines << ex.request;
         total.requests++;
         total.sent += ex.sent;
         total.received += ex.received;
         }
      if (!got.isEmpty ())
         quiet.restart ();
      }

   Totals sum;

   if (!what.isEmpty ())
      qInfo ().noquote () << "traffic:" << what;
   for (auto it = byEndpoint.cbegin (); it != byEndpoint.cend (); ++it)
      {
      sum.lines << it->lines;
      sum.requests += it->requests;
      sum.sent += it->sent;
      sum.received += it->received;
      if (!what.isEmpty ())
         qInfo ().noquote () << QString ("  %1 x%2: sent %3, received %4")
                                .arg (it.key (), -50).arg (it->requests)
                                .arg (it->sent).arg (it->received);
      }
   if (!what.isEmpty ())
      qInfo ().noquote () << QString ("  total %1 requests: sent %2, "
                                      "received %3")
                             .arg (sum.requests).arg (sum.sent)
                             .arg (sum.received);
   return sum;
   }


void TestTraffic::withinBudget (const Totals &totals, int requests,
                                qint64 received)
   {
   QVERIFY2 (totals.requests <= requests,
             qPrintable (QString ("%1 requests, over the budget of %2")
                         .arg (totals.requests).arg (requests)));
   QVERIFY2 (totals.received <= received * 5 / 4,
             qPrintable (QString ("received %1 bytes, over the budget of %2")
                         .arg (totals.received).arg (received)));
   }


void TestTraffic::connectServer ()
   {
   bool attached = false;
   QString err;

   _desktop->addRemoteServer (_server->url (), [&] (const QString &error) {
      err = error;
      attached = true;
   });
   QTRY_VERIFY (attached);
   QCOMPARE (err, QString ());
   }


void TestTraffic::openFolder ()
   {
   QString path = repoPath () + "/papers";

   QTRY_VERIFY (_desktop->getDirIndex (path).isValid ());
   _desktop->selectDir (_desktop->getDirIndex (path));

   // every stack is shown, with its thumbnail
   Desktopmodel *model = _desktop->getModel ();

   for (const QString &name : kStacks)
      {
      QTRY_VERIFY (stackIndex (name).isValid ());
      QTRY_VERIFY (!model->data (stackIndex (name),
                                 Desktopmodel::Role_pixmap)
                   .value<QPixmap> ().isNull ());
      }
   }


QModelIndex TestTraffic::stackIndex (const QString &name)
   {
   Desktopview *view = _desktop->getView ();
   QModelIndex root = view->rootIndex ();

   for (int row = 0; row < view->model ()->rowCount (root); row++)
      {
      QModelIndex ind = view->model ()->index (row, 0, root);

      _desktop->getModelconv ()->indexToSource (view->model (), ind);
      if (ind.isValid () && _desktop->getModel ()->data (
             ind, Desktopmodel::Role_filename).toString () == name)
         return ind;
      }
   return QModelIndex ();
   }


void TestTraffic::testConnect ()
   {
   connectServer ();
   withinBudget (traffic ("connect"), 3, 726);
   }


void TestTraffic::testOpenFolder ()
   {
   connectServer ();
   traffic (QString ());
   openFolder ();
   withinBudget (traffic ("open a folder of six stacks"), 9, 19709);
   }


void TestTraffic::testReopenFolder ()
   {
   connectServer ();
   openFolder ();
   traffic (QString ());

   // go to the top of the repository, and back
   QModelIndex top = _desktop->getDirIndex (repoPath ());

   _desktop->selectDir (top);
   QTest::qWait (300);
   openFolder ();
   withinBudget (traffic ("open the folder again"), 5, 1437);
   }


void TestTraffic::testShowStack ()
   {
   connectServer ();
   openFolder ();
   traffic (QString ());
   showStack ("a.max");
   withinBudget (traffic ("show a five-page stack, page by page"), 6,
                 1260039);
   }


void TestTraffic::showStack (const QString &name)
   {
   // let the stacks settle in place first, as a user would
   QTest::qWait (500);

   Desktopview *view = _desktop->getView ();
   QModelIndex ind = stackIndex (name);

   _desktop->getModelconv ()->indexToProxy (_desktop->getModel (), ind);
   QRect rect = view->visualRect (ind);
   QVERIFY (rect.isValid ());
   QTest::mouseClick (view->viewport (), Qt::LeftButton, Qt::NoModifier,
                      rect.center ());

   Pagewidget *page = _desktop->_page;

   /* a click shows the stack again once the time for a double click has
      gone, from its first page, so wait for that before turning */
   QTRY_VERIFY (!page->_image.isNull () && !_desktop->_timer->isActive ());

   // and every other page in turn
   for (int pagenum = 1; pagenum < 5; pagenum++)
      {
      QImage before = page->_image;

      page->slotPreviewPage (page->_pagemodel->index (pagenum, 0,
                                                      QModelIndex ()));
      QTRY_VERIFY2 (page->_pagenum == pagenum && page->_image != before,
                    qPrintable (QString ("want page %1, showing %2, image %3")
                                .arg (pagenum).arg (page->_pagenum)
                                .arg (page->_image == before ? "same"
                                                             : "new")));
      }
   }


void TestTraffic::testNewSession ()
   {
   connectServer ();
   openFolder ();
   showStack ("a.max");
   traffic (QString ());

   // close the window and start again
   _me.reset (new Mainwindow);
   _desktop = _me->getDesktop ();
   _me->resize (1600, 1000);
   _me->show ();
   QVERIFY (QTest::qWaitForWindowExposed (_me.get ()));
   traffic (QString ());
   connectServer ();
   openFolder ();
   showStack ("a.max");
   withinBudget (traffic ("a new session: connect, open the folder and "
                          "show the stack"), 7, 3570);
   }


void TestTraffic::testChangedStack ()
   {
   connectServer ();
   openFolder ();
   traffic (QString ());

   // another client changes a stack, which the listing shows by its time
   QFile changed (_repo->path () + "/papers/b.max");

   QVERIFY (changed.open (QIODevice::ReadWrite));
   QVERIFY (changed.setFileTime (QDateTime::currentDateTime ().addSecs (60),
                                 QFileDevice::FileModificationTime));
   changed.close ();

   // go to the top of the repository, and back
   _desktop->selectDir (_desktop->getDirIndex (repoPath ()));
   QTest::qWait (300);
   openFolder ();

   // only the thumbnail of the stack which changed comes again
   Totals totals = traffic ("open the folder again once a stack has changed");

   QCOMPARE (totals.lines.filter ("/thumbnail").size (), 1);
   QVERIFY (totals.lines.filter ("/thumbnail") [0].contains ("b.max"));
   }


void TestTraffic::testRename ()
   {
   connectServer ();
   openFolder ();
   traffic (QString ());

   _desktop->getModel ()->renameStack (stackIndex ("a.max"), "renamed");
   QTRY_VERIFY (QFile::exists (_repo->path () + "/papers/renamed.max"));
   withinBudget (traffic ("rename a stack"), 1, 159);
   }
