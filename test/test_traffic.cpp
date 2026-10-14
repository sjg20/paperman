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
#include "../utils.h"

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
         total.connections += ex.connected;
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
      sum.connections += it->connections;
      sum.sent += it->sent;
      sum.received += it->received;
      if (!what.isEmpty ())
         qInfo ().noquote () << QString ("  %1 x%2: sent %3, received %4")
                                .arg (it.key (), -50).arg (it->requests)
                                .arg (it->sent).arg (it->received);
      }
   if (!what.isEmpty ())
      qInfo ().noquote () << QString ("  total %1 requests on %2 "
                                      "connections: sent %3, received %4")
                             .arg (sum.requests).arg (sum.connections)
                             .arg (sum.sent).arg (sum.received);
   return sum;
   }


void TestTraffic::withinBudget (const Totals &totals, int requests,
                                int connections, qint64 received)
   {
   QVERIFY2 (totals.requests <= requests,
             qPrintable (QString ("%1 requests, over the budget of %2")
                         .arg (totals.requests).arg (requests)));
   QVERIFY2 (totals.connections <= connections,
             qPrintable (QString ("%1 connections, over the budget of %2")
                         .arg (totals.connections).arg (connections)));
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


/** true once the stack at @a ind is built and shows its thumbnail, rather
   than the hatched stand-in it has until that comes, or the icon of one
   which cannot be had, and the view has recorded the thumbnail's size */
static bool stackDrawn (const QModelIndex &ind)
   {
   static const QImage placeholder = utilIconPixmap ("unknown", 64).toImage ();
   const QAbstractItemModel *model = ind.model ();
   QPixmap pixmap = model->data (ind, Desktopmodel::Role_pixmap)
                    .value<QPixmap> ();
   QSize size = model->data (ind, Desktopmodel::Role_preview_maxsize)
                .toSize ();

   return ((File *)ind.internalPointer ())->valid () && !pixmap.isNull ()
          && pixmap.toImage () != placeholder
          && size.expandedTo (pixmap.size ()) == size;
   }


void TestTraffic::openFolder ()
   {
   QString path = repoPath () + "/papers";

   QTRY_VERIFY (_desktop->getDirIndex (path).isValid ());
   _desktop->selectDir (_desktop->getDirIndex (path));

   /* every stack is shown with its thumbnail, and the view has measured
      that, so the stack's size is in the layout the session saves. The
      view measures only what it draws, so bring each stack into view in
      turn, the PDF too, which is out of view to the right */
   Desktopview *view = _desktop->getView ();

   for (const QString &name : kStacks + QStringList ("f.pdf"))
      {
      QTRY_VERIFY (stackIndex (name).isValid ());

      QModelIndex shown = stackIndex (name);

      _desktop->getModelconv ()->indexToProxy (_desktop->getModel (), shown);
      view->scrollTo (shown);
      QTRY_VERIFY (stackDrawn (stackIndex (name)));
      }
   view->scrollToTop ();
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
   withinBudget (traffic ("connect"), 3, 2, 669);
   }


void TestTraffic::testOpenFolder ()
   {
   connectServer ();
   traffic (QString ());
   openFolder ();
   withinBudget (traffic ("open a folder of six stacks"), 8, 6, 18367);
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
   withinBudget (traffic ("open the folder again"), 5, 1, 926);
   }


void TestTraffic::testClickStack ()
   {
   connectServer ();
   openFolder ();
   traffic (QString ());
   showStack ("a.max", false);
   withinBudget (traffic ("click a five-page stack"), 6, 1, 72331);
   }


void TestTraffic::testZoomIn ()
   {
   connectServer ();
   openFolder ();
   showStack ("a.max", false);

   /* the colour first page comes as an image made for display, which is
      drawn as big as the page itself would be */
   Pagewidget *page = _desktop->_page;
   QSize trueSize = _desktop->getModel ()->pageTrueSize (stackIndex ("a.max"),
                                                       0);

   QVERIFY (page->_imageScale > 1.0);
   QVERIFY (qAbs (page->_pixmap.height ()
                  - qRound (trueSize.height () * page->_scale)) <= 2);
   traffic (QString ());

   // zooming in to full size needs the page itself
   page->slotNewScale (1.0, false);
   QTRY_COMPARE (page->_imageScale, 1.0);
   QCOMPARE (page->_image.size (), trueSize);

   Totals totals = traffic ("zoom a page in to full size");

   QCOMPARE (totals.lines.filter ("/pages/1").size (), 1);
   withinBudget (totals, 1, 0, 518202);
   }


void TestTraffic::testRotateView ()
   {
   connectServer ();
   openFolder ();
   showStack ("a.max", false);

   Pagewidget *page = _desktop->_page;

   QVERIFY (page->_imageScale > 1.0);
   QVERIFY (page->_image.height () > page->_image.width ());
   traffic (QString ());

   /* rotating the page on the server makes the image in the cache out of
      date, so a new one comes, turned */
   _desktop->getModel ()->transformPage (stackIndex ("a.max"), 0,
                                         File::Transform_rotate90);
   QTRY_VERIFY (page->_image.width () > page->_image.height ());

   Totals totals = traffic ("rotate a page shown as an image made for "
                            "display");

   QVERIFY (totals.lines.filter ("size=1000").size () >= 1);
   }


void TestTraffic::testShowStack ()
   {
   connectServer ();
   openFolder ();
   traffic (QString ());
   showStack ("a.max");
   /* the list asks for a thumbnail of each page not here yet, so how
      many it asks for before each page comes depends on timing */
   withinBudget (traffic ("show a five-page stack, page by page"), 10,
                 1, 335000);
   }


void TestTraffic::showStack (const QString &name, bool turn)
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
   if (!turn)
      return;

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
                          "show the stack"), 6, 4, 1492);
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
   withinBudget (traffic ("rename a stack"), 1, 0, 140);
   }


void TestTraffic::testListLargeFolder ()
   {
   // a folder of 200 stacks, which need only be there to be listed
   QDir repo (_repo->path ());

   QVERIFY (repo.mkdir ("many"));
   for (int i = 0; i < 200; i++)
      {
      QFile stack (repo.filePath (QString ("many/stack-%1.max")
                                  .arg (i, 3, 10, QChar ('0'))));

      QVERIFY (stack.open (QIODevice::WriteOnly));
      }

   RemoteBackend client (_server->url ());
   bool done = false;
   DirectoryListing listing;

   client.browseDirectory (QFileInfo (_repo->path ()).fileName (), "many",
                           this, [&] (const DirectoryListing &got) {
      listing = got;
      done = true;
   });
   QTRY_VERIFY (done);
   QVERIFY (listing.ok);
   QCOMPARE (listing.entries.size (), 200);
   withinBudget (traffic ("list a folder of 200 stacks"), 1, 1, 1324);
   }


void TestTraffic::testCloseUnchanged ()
   {
   // a session which opens the folder saves its layout as it closes
   connectServer ();
   openFolder ();
   _me.reset (new Mainwindow);
   _desktop = _me->getDesktop ();
   _me->resize (1600, 1000);
   _me->show ();
   QVERIFY (QTest::qWaitForWindowExposed (_me.get ()));

   // the next finds it there, so closing has nothing to send back
   connectServer ();
   openFolder ();
   traffic (QString ());
   _me.reset ();
   _desktop = nullptr;

   Totals totals = traffic ("close a session, with the layout unchanged");

   // the event stream ends as the window closes, which is all there is
   QCOMPARE (totals.lines.filter ("/events").size (), totals.lines.size ());
   }
