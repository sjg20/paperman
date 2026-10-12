/*
License: GPL-2
*/

#include <QStandardPaths>
#include <QtConcurrent>
#include <QtTest/QtTest>

#include "test.h"

#include "../desktopmodel.h"
#include "../desktopview.h"
#include "../desktopwidget.h"
#include "../file.h"
#include "../mainwindow.h"
#include "../pagemodel.h"
#include "../pagewidget.h"
#include "../remotebackend.h"

#include "remoteharness.h"
#include "test_async.h"

/* Each reply is held back this long, so that waiting for one is
   plain; it is well over AsyncCheck::kMaxStallMs */
static const int kDelayMs = 500;

// the port the server listens on, behind the proxy
static const quint16 kPort = 9877;

/* Check that the stretch of test watched by @check waited for nothing.
   @why says what still waits, when it is expected to, and what it
   waited for is logged so the work left is plain */
#define CHECK_ASYNC(check, why) \
   do { \
      if (why) \
         { \
         if (!check.ok ()) \
            qInfo () << "still" << check.report (); \
         QEXPECT_FAIL ("", why, Continue); \
         } \
      QVERIFY2 (check.ok (), qPrintable (check.report ())); \
   } while (0)


TestAsync::TestAsync (QString name) : Suite (name)
   {
   }


TestAsync::~TestAsync () = default;


void TestAsync::initTestCase ()
   {
   // keep the page cache and server state out of the user's own
   QStandardPaths::setTestModeEnabled (true);
   }


void TestAsync::cleanupTestCase ()
   {
   QStandardPaths::setTestModeEnabled (false);
   }


void TestAsync::init ()
   {
   /* other suites leave users for the server in the test-mode settings,
      and then the desktop would stop to ask for a login */
   QFile::remove (QStandardPaths::writableLocation (
                     QStandardPaths::GenericConfigLocation)
                  + "/paperman-server/users.json");

   _config.reset (new QTemporaryDir);
   QVERIFY (_config->isValid ());
   qputenv ("PAPERMAN_CONFIG", (_config->path () + "/client.conf").toLocal8Bit ());

   _repo.reset (new QTemporaryDir);
   QVERIFY (_repo->isValid ());
   QVERIFY (QDir (_repo->path ()).mkdir ("papers"));
   QVERIFY (QFile::copy (testSrc + "/testfile.max",
                         serverPath ("testfile.max")));

   _server.reset (new ServerThread (_repo->path (), kPort));
   QVERIFY (_server->begin ());

   /* a page cache left by an earlier run would let the desktop show the
      stack without asking the server */
   RemoteBackend probe (_server->url ());
   QVERIFY (!probe.serverId ().isEmpty ());
   QString repo = QFileInfo (_repo->path ()).fileName ();
   probe.invalidatePageDir (repo, "papers/testfile.max");
   probe.invalidateCachedFile (repo, "papers/testfile.max");
   _server->setDelay (kDelayMs);

   _me.reset (new Mainwindow);
   _desktop = _me->getDesktop ();
   _me->resize (1600, 1000);
   _me->show ();
   QVERIFY (QTest::qWaitForWindowExposed (_me.get ()));
   }


void TestAsync::cleanup ()
   {
   _me.reset ();
   _desktop = nullptr;
   _server.reset ();
   _repo.reset ();
   qunsetenv ("PAPERMAN_CONFIG");
   _config.reset ();
   }


QString TestAsync::repoPath () const
   {
   return _server->url ().toString () + "/"
          + QFileInfo (_repo->path ()).fileName ();
   }


QString TestAsync::serverPath (const QString &fname) const
   {
   return _repo->path () + "/papers/" + fname;
   }


void TestAsync::connectServer ()
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


void TestAsync::openFolder ()
   {
   QString path = repoPath () + "/papers";

   QTRY_VERIFY (_desktop->getDirIndex (path).isValid ());
   _desktop->selectDir (_desktop->getDirIndex (path));
   QTRY_VERIFY (stackIndex ().isValid ());
   }


QModelIndex TestAsync::stackIndex ()
   {
   Desktopview *view = _desktop->getView ();
   QModelIndex ind = view->model ()->index (0, 0, view->rootIndex ());

   if (ind.isValid ())
      _desktop->getModelconv ()->indexToSource (view->model (), ind);
   return ind;
   }


void TestAsync::testHarness ()
   {
   // a request the main thread waits for is caught, by its path
      {
      AsyncCheck check;
      RemoteBackend client (_server->url ());

      QVERIFY (!client.serverId ().isEmpty ());
      QVERIFY (!check.ok ());
      QVERIFY2 (check.report ().contains ("GET /v1/status"),
                qPrintable (check.report ()));
      }

   // so is the main thread going without handling events
      {
      AsyncCheck check;

      QThread::msleep (AsyncCheck::kMaxStallMs + 100);
      QVERIFY (!check.ok ());
      QVERIFY2 (check.report ().contains ("stalled"),
                qPrintable (check.report ()));
      }

   // the proxy holds each reply back by the delay set
   RemoteBackend client (_server->url ());
   QElapsedTimer timer;

   timer.start ();
   QVERIFY (!client.serverId ().isEmpty ());
   QVERIFY (timer.elapsed () >= kDelayMs);

   // and a quiet stretch passes
   AsyncCheck check;

   QTest::qWait (50);
   QVERIFY2 (check.ok (), qPrintable (check.report ()));
   }


void TestAsync::testConnect ()
   {
   AsyncCheck check;

   connectServer ();
   QVERIFY (_desktop->getDirIndex (repoPath ()).isValid ());
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testConnectHung ()
   {
   AsyncCheck check;
   bool attached = false;

   _server->setDelay (-1);
   _desktop->addRemoteServer (_server->url (), [&] (const QString &) {
      attached = true;
   });

   // the window carries on while the server says nothing
   QTest::qWait (kDelayMs * 2);
   QVERIFY (!attached);
   QVERIFY (!_desktop->getDirIndex (repoPath ()).isValid ());
   CHECK_ASYNC (check, nullptr);

   // and closing it with the request still out is safe
   _me.reset ();
   QTest::qWait (100);
   }


void TestAsync::testConnectAtStartup ()
   {
   /* a local repository too, so that the window shows that rather than
      opening the server's folder, which is not checked here */
   QTemporaryDir local;
   QVERIFY (local.isValid ());

   AsyncCheck check;
   QString host = QString ("127.0.0.1:%1").arg (_server->url ().port ());

   _me->startup (QStringList () << local.path (), host);
   QTRY_VERIFY (_desktop->getDirIndex (repoPath ()).isValid ());
   QVERIFY (!_desktop->getSelectedPath ().startsWith (repoPath ()));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testOpenFolder ()
   {
   connectServer ();

   AsyncCheck check;

   openFolder ();
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testOpenFolderHung ()
   {
   connectServer ();
   _server->setDelay (-1);

   AsyncCheck check;
   QString path = repoPath () + "/papers";

   _desktop->selectDir (_desktop->getDirIndex (path));

   // the window carries on, with the folder empty for now
   QTest::qWait (kDelayMs * 2);
   QVERIFY (!stackIndex ().isValid ());
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testLeaveFolder ()
   {
   connectServer ();
   openFolder ();

   // move the stack, so that the folder's layout has changed
   QVERIFY (_desktop->getModel ()->setData (stackIndex (), QPoint (432, 210),
                                            Desktopmodel::Role_position));

   AsyncCheck check;

   // leave the folder, which saves the layout on the server
   _desktop->selectDir (_desktop->getDirIndex (repoPath ()));
   QTRY_VERIFY_WITH_TIMEOUT (QFile::exists (serverPath (".paperdesk")),
                             kDelayMs * 10);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testOtherClient ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;

   /* another client renames the stack, from a thread of its own so that
      its waiting is not counted against the desktop's */
   QUrl url = _server->url ();
   QString repo = QFileInfo (_repo->path ()).fileName ();
   QFuture<bool> renamed = QtConcurrent::run ([url, repo] () {
      RemoteBackend other (url);
      QString name = "other.max";

      return other.renameStack (repo, "papers/testfile.max", name);
   });

   // the desktop follows, listing the folder again
   QTRY_VERIFY_WITH_TIMEOUT (renamed.isFinished (), kDelayMs * 10);
   QVERIFY (renamed.result ());
   Desktopmodel *model = _desktop->getModel ();
   QTRY_VERIFY_WITH_TIMEOUT (stackIndex ().isValid ()
                             && model->data (stackIndex (),
                                             Desktopmodel::Role_filename)
                                   .toString () == "other.max",
                             kDelayMs * 10);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testStaleThumbnail ()
   {
   connectServer ();

   QString path = repoPath () + "/papers";

   QTRY_VERIFY (_desktop->getDirIndex (path).isValid ());

   /* the listing comes after the usual delay, and the thumbnail asked
      for once it has come is held back for longer */
   _desktop->selectDir (_desktop->getDirIndex (path));
   QTest::qWait (kDelayMs / 5);
   _server->setDelay (kDelayMs * 4);
   QTRY_VERIFY (stackIndex ().isValid ());

   // the stack goes before its thumbnail comes, which must not reach it
   QModelIndex stack = stackIndex ();

   QAbstractItemModel *model = _desktop->getModel ();

   QVERIFY (model->removeRows (stack.row (), 1, stack.parent ()));
   QTest::qWait (kDelayMs * 5);
   }


void TestAsync::testLocate ()
   {
   connectServer ();

   AsyncCheck check;

   // as locateFolder() does, from a search's results
   _desktop->_scroll_to = "testfile.max";
   QTRY_VERIFY (_desktop->getDirIndex (repoPath () + "/papers").isValid ());
   _desktop->selectDir (_desktop->getDirIndex (repoPath () + "/papers"), true);

   Desktopview *view = _desktop->getView ();
   QTRY_COMPARE (view->getSelectedItem ().data (Desktopmodel::Role_filename)
                    .toString (), QString ("testfile.max"));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testPreviewStack ()
   {
   connectServer ();
   openFolder ();
   QTRY_VERIFY (!_desktop->getModel ()->data (stackIndex (),
                                             Desktopmodel::Role_pixmap)
                .value<QPixmap> ().isNull ());

   AsyncCheck check;

   clickStack ();

   // the stack's pages are listed, and the first is shown
   Desktopmodel *model = _desktop->getModel ();
   QModelIndex stack = stackIndex ();
   QTRY_VERIFY2 (model->data (stack, Desktopmodel::Role_message).toString ()
                 .contains ("Page 1 of 5"),
                 qPrintable (model->data (stack, Desktopmodel::Role_message)
                             .toString ()));
   QTRY_COMPARE (_desktop->_page->_pagemodel->rowCount (QModelIndex ()), 5);
   QTRY_VERIFY (!_desktop->_page->_image.isNull ());
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::clickStack ()
   {
   // wait for the stack to settle in place, with its thumbnail
   Desktopmodel *model = _desktop->getModel ();
   QTRY_VERIFY (!model->data (stackIndex (), Desktopmodel::Role_pixmap)
                .value<QPixmap> ().isNull ());
   QTest::qWait (kDelayMs);

   Desktopview *view = _desktop->getView ();
   QModelIndex ind = view->model ()->index (0, 0, view->rootIndex ());
   QRect rect = view->visualRect (ind);

   QVERIFY (rect.isValid ());
   QTest::mouseClick (view->viewport (), Qt::LeftButton, Qt::NoModifier,
                      rect.center ());
   }


void TestAsync::testPreviewPage ()
   {
   connectServer ();
   openFolder ();
   clickStack ();
   Pagewidget *page = _desktop->_page;
   QTRY_VERIFY (!page->_image.isNull ());
   QImage first = page->_image;

   AsyncCheck check;

   // show the third page, which has not come yet
   page->slotPreviewPage (page->_pagemodel->index (2, 0, QModelIndex ()));
   QTRY_VERIFY (page->_pagenum == 2 && page->_image != first);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testPreviewHung ()
   {
   connectServer ();
   openFolder ();
   QTRY_VERIFY (!_desktop->getModel ()->data (stackIndex (),
                                             Desktopmodel::Role_pixmap)
                .value<QPixmap> ().isNull ());
   _server->setDelay (-1);

   AsyncCheck check;

   clickStack ();

   // the window carries on while the server says nothing
   QTest::qWait (kDelayMs * 2);
   QVERIFY (_desktop->_page->_image.isNull ());
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testTurnStackPage ()
   {
   connectServer ();
   openFolder ();

   Desktopmodel *model = _desktop->getModel ();
   QModelIndex stack = stackIndex ();
   QTRY_VERIFY (!model->data (stack, Desktopmodel::Role_pixmap)
                .value<QPixmap> ().isNull ());
   QImage before = model->data (stack, Desktopmodel::Role_pixmap)
                      .value<QPixmap> ().toImage ();

   AsyncCheck check;

   QVERIFY (model->setData (stack, 2, Desktopmodel::Role_pagenum));
   QTRY_VERIFY (model->data (stack, Desktopmodel::Role_pixmap)
                .value<QPixmap> ().toImage () != before);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testRename ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;

   _desktop->getModel ()->renameStack (stackIndex (), "renamed");
   QTRY_VERIFY (QFile::exists (serverPath ("renamed.max")));
   CHECK_ASYNC (check, "renaming waits for the server");
   }


void TestAsync::testRotate ()
   {
   connectServer ();
   openFolder ();

   auto pageSize = [this] () {
      File *f = File::createFile (_repo->path () + "/papers/",
                                  "testfile.max", nullptr, File::Type_max);
      QSize size, trueSize;
      int bpp, bytes, compressed;
      QDateTime stamp;

      if (!f || f->load () || f->getImageInfo (0, size, trueSize, bpp, bytes,
                                               compressed, stamp))
         size = QSize ();
      delete f;
      return size;
   };
   QSize before = pageSize ();
   QVERIFY (before.isValid ());

   AsyncCheck check;

   _desktop->getModel ()->transformPage (stackIndex (), 0,
                                         File::Transform_rotate90);
   QTRY_COMPARE (pageSize (), before.transposed ());
   CHECK_ASYNC (check, "rotating waits for the server");
   }


void TestAsync::testTrash ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   QModelIndex stack = stackIndex ();

   _desktop->getModel ()->trashStack (stack);
   QTRY_VERIFY (!QFile::exists (serverPath ("testfile.max")));
   CHECK_ASYNC (check, "moving to the trash waits for the server");
   }
