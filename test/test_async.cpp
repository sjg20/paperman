/*
License: GPL-2
*/

#include <QClipboard>
#include <QMimeData>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QtTest/QtTest>

#include "test.h"

#include "../blockwatch.h"
#include "../desktopmodel.h"
#include "../desktopundo.h"
#include "../desktopview.h"
#include "../desktopwidget.h"
#include "../dirmodel.h"
#include "../file.h"
#include "../filemax.h"
#include "../mainwindow.h"
#include "../ocrpage.h"
#include "../pagemodel.h"
#include "../pageview.h"
#include "../pagewidget.h"
#include "../remotebackend.h"
#include "../utils.h"

#include "remoteharness.h"
#include "test_async.h"

/* Each reply is held back this long, so that waiting for one is
   plain; it is well over AsyncCheck::kMaxStallMs, except on Windows */
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

   // from here on the main thread must not wait for the server
   _waitsAllowed = false;
   BlockWatch::watch (true);
   _mark = BlockWatch::mark ();

   _me.reset (new Mainwindow);
   _desktop = _me->getDesktop ();
   _me->resize (1600, 1000);
   _me->show ();
   QVERIFY (QTest::qWaitForWindowExposed (_me.get ()));
   }


void TestAsync::cleanup ()
   {
   QStringList waits = BlockWatch::since (_mark);

   // closing the window saves the folder's layout, which waits
   BlockWatch::watch (false);
   _me.reset ();
   _desktop = nullptr;
   _server.reset ();
   _repo.reset ();
   qunsetenv ("PAPERMAN_CONFIG");
   _config.reset ();

   if (!_waitsAllowed)
      QVERIFY2 (waits.isEmpty (),
                qPrintable ("the main thread waited for: "
                            + waits.join (", ")));
   }


QString TestAsync::repoPath () const
   {
   return _server->url ().toString () + "/"
          + QFileInfo (_repo->path ()).fileName ();
   }


int TestAsync::serverPages (const QString &fname) const
   {
   File *f = File::createFile (_repo->path () + "/papers/", fname, nullptr,
                               File::typeFromName (fname));
   int count = -1;

   if (f && !f->load ())
      count = f->pagecount ();
   delete f;
   return count;
   }


QStringList TestAsync::serverStacks () const
   {
   QStringList names = QDir (_repo->path () + "/papers")
                          .entryList (QDir::Files, QDir::Name);

   names.removeAll (".paperdesk");
   return names;
   }


void TestAsync::doAndUndo (std::function<void ()> op)
   {
   Desktopmodel *model = _desktop->getModel ();

   op ();
   model->getUndoStack ()->undo ();
   QVERIFY (model->opsPending ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   }


QString TestAsync::serverPath (const QString &fname) const
   {
   return _repo->path () + "/papers/" + fname;
   }


void TestAsync::indexServer ()
   {
   _server.reset (new ServerThread (_repo->path (), kPort));
   _server->setTextIndex (true);
   _server->setReadPages (true);
   QVERIFY (_server->begin ());
   _server->setDelay (kDelayMs);
   }


void TestAsync::closeWindow (bool destroy)
   {
   QStringList waits = BlockWatch::since (_mark);

   QVERIFY2 (waits.isEmpty (), qPrintable ("the main thread waited for: "
                                           + waits.join (", ")));
   _waitsAllowed = true;
   if (destroy)
      _me.reset ();
   else
      _me->close ();
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
   // this waits on purpose, to show that the checks see it
   _waitsAllowed = true;

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

   /* the watch over the whole test saw both waits, inside the checks
      and out, so cleanup() fails a test which waits anywhere */
   QCOMPARE (BlockWatch::since (_mark).filter ("GET /v1/status").size (), 2);
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
   closeWindow (true);
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


void TestAsync::testWaitingThumbnail ()
   {
   connectServer ();

   QString path = repoPath () + "/papers";

   QTRY_VERIFY (_desktop->getDirIndex (path).isValid ());

   // the thumbnail asked for once the listing has come is held back
   _desktop->selectDir (_desktop->getDirIndex (path));
   QTest::qWait (kDelayMs / 5);
   _server->setDelay (kDelayMs * 4);
   QTRY_VERIFY (stackIndex ().isValid ());

   /* meanwhile the stack shows a hatched page in its place, rather than
      the icon of a stack whose image cannot be had */
   QModelIndex stack = stackIndex ();
   QAbstractItemModel *model = _desktop->getModel ();
   QPixmap waiting = model->data (stack, Desktopmodel::Role_pixmap)
                     .value<QPixmap> ();
   QImage unknown = utilIconPixmap ("unknown", 64).toImage ();

   QCOMPARE (waiting.size (), QSize (106, 150));
   QVERIFY (waiting.toImage () != unknown);

   // the view draws it, but its size is not the stack's to keep
   _desktop->getView ()->viewport ()->repaint ();
   QVERIFY (!model->data (stack, Desktopmodel::Role_preview_maxsize)
            .toSize ().isValid ());

   // then the thumbnail comes, and the view takes its size
   QTRY_VERIFY_WITH_TIMEOUT (model->data (stack, Desktopmodel::Role_pixmap)
                             .value<QPixmap> ().toImage ()
                             != waiting.toImage (), kDelayMs * 10);
   QPixmap thumbnail = model->data (stack, Desktopmodel::Role_pixmap)
                       .value<QPixmap> ();

   QVERIFY (thumbnail.toImage () != unknown);
   _desktop->getView ()->viewport ()->repaint ();
   QTRY_COMPARE (model->data (stack, Desktopmodel::Role_preview_maxsize)
                 .toSize (), thumbnail.size ());
   }


void TestAsync::testSlowFolders ()
   {
   // folders a few deep, as a year's bills are
   QStringList years = {"2021", "2022", "2023", "2024", "2025"};

   for (const QString &year : years)
      QVERIFY (QDir (_repo->path ()).mkpath ("bills/" + year + "/gas"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
   // nothing reads a reply which has gone
   QTest::failOnWarning (QRegularExpression ("device not open"));
#endif
   connectServer ();

   /* a server which has not listed these folders before can take a few
      seconds over each, and answers one request at a time, so a listing
      may well come after more than the usual few seconds */
   _server->setDelay (6000);

   // each year's folder is listed, and shows its own
   for (const QString &year : years)
      QTRY_VERIFY_WITH_TIMEOUT (_desktop->getDirIndex (
         repoPath () + "/bills/" + year + "/gas").isValid (), 40000);
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


void TestAsync::scanStack ()
   {
   Desktopmodel *model = _desktop->getModel ();
   QModelIndex parent = stackIndex ().parent ();
   Filemaxpage page;
   QByteArray data (64 * 48, '\x80');
   QString title = "page 1";

   page.addData (64, 48, 8, 64, title, false, false, 1, data, data.size ());
   QVERIFY (!page.compress ());
   QVERIFY (!model->beginScan (parent, "scanned"));

   // as pageStarting() does, so that the page view has a place for it
   emit model->beginningPage ();
   QVERIFY (!model->addPageToScan (&page, ""));
   }


void TestAsync::dropConvert ()
   {
   auto *remote = dynamic_cast<RemoteBackend *> (
                     _desktop->_model->backendForRoot (repoPath ()));
   QVERIFY (remote && remote->hasConvert ());

   QStringList features = remote->features ();

   features.removeAll ("convert");
   remote->setServerInfo (remote->serverId (), features);
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


void TestAsync::testPreviewWhole ()
   {
   // a PDF is fetched whole, and stored in the cache as such
   QVERIFY (QFile::copy (testSrc + "/testpdf.pdf", serverPath ("testpdf.pdf")));
   connectServer ();
   openFolder ();

   Desktopmodel *model = _desktop->getModel ();
   QModelIndex pdf = model->index ("testpdf.pdf", stackIndex ().parent ());
   QVERIFY (pdf.isValid ());
   QTRY_VERIFY (!model->data (pdf, Desktopmodel::Role_pixmap)
                .value<QPixmap> ().isNull ());

   Desktopview *view = _desktop->getView ();
   QModelIndex ind = pdf;

   _desktop->getModelconv ()->indexToProxy (model, ind);
   QRect rect = view->visualRect (ind);
   QVERIFY (rect.isValid ());

   /* drawing the PDF's page is the desktop's own work, so only a wait
      for the server fails this */
   AsyncCheck check;

   check.allowStall ();
   QTest::mouseClick (view->viewport (), Qt::LeftButton, Qt::NoModifier,
                      rect.center ());
   /* the page is drawn at full size, which takes a slow runner several
      seconds on top of fetching the PDF */
   QTRY_VERIFY_WITH_TIMEOUT (!_desktop->_page->_image.isNull (),
                             kDelayMs * 40);
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


void TestAsync::testRepaintChanged ()
   {
   connectServer ();
   openFolder ();
   clickStack ();
   Pagewidget *page = _desktop->_page;
   QTRY_VERIFY (!page->_image.isNull ());

   // the server rotates a page, so the stack's structure is fetched again
   Desktopmodel *model = _desktop->getModel ();

   model->transformPage (stackIndex (), 0, File::Transform_rotate90);
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);

   // its first page is drawn afresh meanwhile, without waiting for it
   AsyncCheck check;

   page->_pagemodel->updatePage (0);
   page->_pageview->viewport ()->repaint ();
   CHECK_ASYNC (check, nullptr);

   /* and the page is asked for, so it comes, or for a colour page an
      image of it made for display */
   QTRY_VERIFY_WITH_TIMEOUT (model->pageHere (stackIndex (), 0)
                             || page->_imageScale > 1.0, kDelayMs * 10);
   }


void TestAsync::testRename ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;

   _desktop->getModel ()->renameStack (stackIndex (), "renamed");
   QTRY_VERIFY (QFile::exists (serverPath ("renamed.max")));
   CHECK_ASYNC (check, nullptr);
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
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testTrash ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   QModelIndex stack = stackIndex ();

   _desktop->getModel ()->trashStack (stack);
   QTRY_VERIFY (!QFile::exists (serverPath ("testfile.max")));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testUndoRename ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();

   /* undo before the server has answered the rename: the undo waits its
      turn, so it renames back what the rename made */
   model->renameStack (stackIndex (), "renamed");
   model->getUndoStack ()->undo ();
   QVERIFY (model->opsPending ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QVERIFY (QFile::exists (serverPath ("testfile.max")));
   QVERIFY (!QFile::exists (serverPath ("renamed.max")));
   QCOMPARE (model->data (stackIndex (), Desktopmodel::Role_filename)
                .toString (), QString ("testfile.max"));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testUndoTrash ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndex stack = stackIndex ();

   /* the undo needs the name the stack got in the trash, which only the
      server's answer gives */
   model->trashStack (stack);
   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QVERIFY (QFile::exists (serverPath ("testfile.max")));
   QVERIFY (!QFile::exists (_repo->path () + "/.maxview-trash/testfile.max"));
   QVERIFY (stackIndex ().isValid ());
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testRenamePage ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   QModelIndex stack = stackIndex ();

   _desktop->getModel ()->renamePage (stack, "remote page");
   QTRY_VERIFY_WITH_TIMEOUT (!_desktop->getModel ()->opsPending (),
                             kDelayMs * 10);
   File *f = File::createFile (_repo->path () + "/papers/", "testfile.max",
                               nullptr, File::Type_max);
   QVERIFY (f && !f->load ());
   QCOMPARE (f->pageTitle (0), QString ("remote page"));
   delete f;
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testAnnotations ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   QModelIndex stack = stackIndex ();
   QHash<int, QString> updates;

   updates [File::Annot_author] = "A. Writer";
   _desktop->getModel ()->updateAnnot (stack, updates);
   QTRY_VERIFY_WITH_TIMEOUT (!_desktop->getModel ()->opsPending (),
                             kDelayMs * 10);
   File *f = File::createFile (_repo->path () + "/papers/", "testfile.max",
                               nullptr, File::Type_max);
   QString author;
   QVERIFY (f && !f->load () && !f->getAnnot (File::Annot_author, author));
   QCOMPARE (author, QString ("A. Writer"));
   delete f;
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testRenameHung ()
   {
   connectServer ();
   openFolder ();
   _server->setDelay (-1);

   AsyncCheck check;

   // the window carries on while the server says nothing
   _desktop->getModel ()->renameStack (stackIndex (), "renamed");
   QTest::qWait (kDelayMs * 2);
   QVERIFY (_desktop->getModel ()->opsPending ());
   CHECK_ASYNC (check, nullptr);

   // and closing it with the rename still out is safe
   closeWindow (true);
   QTest::qWait (100);
   }


void TestAsync::testDeletePages ()
   {
   connectServer ();
   openFolder ();
   QCOMPARE (serverPages ("testfile.max"), 5);

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndex stack = stackIndex ();
   QBitArray pages (5);

   pages.setBit (1);
   model->deletePages (stack, pages);
   QVERIFY (model->opsPending ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverPages ("testfile.max"), 4);

   /* the undo needs the server's token for the deleted page, so it waits
      for the delete even when pressed before the server has answered */
   model->deletePages (stack, pages);
   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverPages ("testfile.max"), 4);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testUnstackPage ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndex stack = stackIndex ();

   model->unstackPage (stack, 0, true);
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverPages ("testfile.max"), 4);
   QCOMPARE (serverStacks ().size (), 2);

   // the undo needs the name the server gave the new stack
   doAndUndo ([&] () { model->unstackPage (stack, 0, true); });
   QCOMPARE (serverPages ("testfile.max"), 4);
   QCOMPARE (serverStacks ().size (), 2);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testUnstackStacks ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndexList list;

   list << stackIndex ();
   model->unstackStacks (list, stackIndex ().parent ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   QCOMPARE (serverPages ("testfile.max"), 1);
   QCOMPARE (serverStacks ().size (), 5);
   QTRY_COMPARE (model->rowCount (stackIndex ().parent ()), 5);

   // the undo stacks back what the server made, by the names it gave
   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   QCOMPARE (serverPages ("testfile.max"), 5);
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));

   // and pressed before the server has answered, waits for it
   list.clear ();
   list << stackIndex ();
   doAndUndo ([&] () {
      model->unstackStacks (list, stackIndex ().parent ());
   });
   QCOMPARE (serverPages ("testfile.max"), 5);
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testDuplicate ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndexList list;

   list << stackIndex ();
   model->duplicate (list, stackIndex ().parent ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverStacks ().size (), 2);
   QTRY_COMPARE (model->rowCount (stackIndex ().parent ()), 2);

   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));

   doAndUndo ([&] () { model->duplicate (list, stackIndex ().parent ()); });
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testConvert ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndexList list;

   list << stackIndex ();
   model->duplicatePdf (list, stackIndex ().parent ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   QStringList stacks = serverStacks ();
   QCOMPARE (stacks.size (), 2);
   QVERIFY2 (stacks.filter (".pdf").size () == 1, qPrintable (stacks.join (",")));

   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testUnfold ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndexList list;

   list << stackIndex ();
   model->unfoldBooklet (list, stackIndex ().parent ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   QCOMPARE (serverStacks ().size (), 2);

   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testOcr ()
   {
   // the server reads the page with its own engine
   if (QStandardPaths::findExecutable ("tesseract").isEmpty ())
      QSKIP ("tesseract is not installed");
   connectServer ();
   openFolder ();
   clickStack ();
   Pagewidget *page = _desktop->_page;
   QTRY_VERIFY (!page->_image.isNull ());

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   bool done = false;
   err_info *err = nullptr;

   model->ocrPageLater (stackIndex (), 0, [&] (err_info *e, const QString &) {
      err = e;
      done = true;
   });
   QVERIFY (!done);
   QTRY_VERIFY_WITH_TIMEOUT (done, 90000);   // reading a page is slow
   QVERIFY2 (!err, err ? err->errstr : "");
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testReadStack ()
   {
   indexServer ();
   connectServer ();
   openFolder ();

   AsyncCheck check;
   bool done = false;
   err_info *err = nullptr;

   _desktop->getModel ()->askServerToReadLater (stackIndex (),
                                                [&] (err_info *e) {
      err = e;
      done = true;
   });
   QVERIFY (!done);
   QTRY_VERIFY_WITH_TIMEOUT (done, kDelayMs * 10);
   QVERIFY2 (!err, err ? err->errstr : "");
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testSearch ()
   {
   // give the stack some words for the server's index to find
   {
      Filemax max (_repo->path () + "/papers/", "testfile.max", nullptr);
      OcrPage page;

      page.size = QSize (100, 100);
      page.words << OcrWord {QRect (0, 0, 10, 10), "invoice", 90, 0, 0};
      QVERIFY (!max.load () && !max.putPageOcr (2, page));
   }
   indexServer ();
   connectServer ();
   openFolder ();

   AsyncCheck check;
   int count = -2;
   bool complete = false;

   // the server reads its stacks first, so ask until it has
   QElapsedTimer timer;

   timer.start ();
   do
      {
      bool done = false;

      _desktop->textSearchLater (repoPath (), "invoice",
                                 [&] (int num, bool all) {
         count = num;
         complete = all;
         done = true;
      });
      QVERIFY (!done);
      QTRY_VERIFY_WITH_TIMEOUT (done, kDelayMs * 10);
      if (!complete)
         QTest::qWait (200);
      } while (!complete && timer.elapsed () < 15000);
   QVERIFY (complete);
   QCOMPARE (count, 1);

   Desktopmodel *model = _desktop->getModel ();
   QModelIndex found = stackIndex ();
   QVERIFY (found.isValid ());
   QCOMPARE (model->data (found, Desktopmodel::Role_pagenum).toInt (), 2);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testScan ()
   {
   connectServer ();
   openFolder ();
   scanStack ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   bool uploaded = false;
   err_info *err = nullptr;
   QString fname;

   QVERIFY (!model->confirmScan (&fname, nullptr, [&] (err_info *e) {
      err = e;
      uploaded = true;
   }));
   QVERIFY (!uploaded);
   QCOMPARE (fname, QString ("scanned.max"));

   // the stack is in the folder meanwhile, and stays when it is listed
   QModelIndex parent = stackIndex ().parent ();
   QVERIFY (model->index (fname, parent).isValid ());
   model->refresh (parent);
   QTRY_VERIFY_WITH_TIMEOUT (uploaded, kDelayMs * 10);
   QVERIFY2 (!err, err ? err->errstr : "");
   QVERIFY (QFile::exists (serverPath (fname)));
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QVERIFY (model->index (fname, parent).isValid ());
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testScanClose ()
   {
   connectServer ();
   openFolder ();
   scanStack ();

   // closing the window waits for the stack to reach the server
   QVERIFY (!_desktop->getModel ()->confirmScan ());
   closeWindow (false);
   QVERIFY (QFile::exists (serverPath ("scanned.max")));
   }


void TestAsync::testFetchStacks ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   Desktopmodel *model = _desktop->getModel ();
   QModelIndex stack = stackIndex ();
   bool done = false;
   err_info *err = nullptr;

   QVERIFY (!model->pageHere (stack, 4));
   model->fetchStacksLater (QModelIndexList () << stack, [&] (err_info *e) {
      err = e;
      done = true;
   });
   QVERIFY (!done);
   QTRY_VERIFY_WITH_TIMEOUT (done, kDelayMs * 20);
   QVERIFY2 (!err, err ? err->errstr : "");
   for (int pagenum = 0; pagenum < 5; pagenum++)
      QVERIFY (model->pageHere (stack, pagenum));
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testConvertHere ()
   {
   connectServer ();
   openFolder ();
   dropConvert ();

   /* the copy is converted here on the main thread, as a local stack is,
      which can hold it for a while; only the server must not */
   AsyncCheck check;

   check.allowStall ();
   Desktopmodel *model = _desktop->getModel ();
   QModelIndexList list;

   list << stackIndex ();
   model->duplicatePdf (list, stackIndex ().parent ());
   QVERIFY (model->opsPending ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   QCOMPARE (serverStacks (),
             QStringList ({"testfile.max", "testfile_copy.pdf"}));

   model->getUndoStack ()->undo ();
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 10);
   QCOMPARE (serverStacks (), QStringList ({"testfile.max"}));

   list.clear ();
   list << stackIndex ();
   model->unfoldBooklet (list, stackIndex ().parent ());
   QTRY_VERIFY_WITH_TIMEOUT (!model->opsPending (), kDelayMs * 20);
   QCOMPARE (serverStacks (),
             QStringList ({"testfile.max", "testfile_unfold.max"}));
   QCOMPARE (serverPages ("testfile_unfold.max"), 10);
   CHECK_ASYNC (check, nullptr);
   }


void TestAsync::testCopyFiles ()
   {
   connectServer ();
   openFolder ();

   AsyncCheck check;
   bool done = false;
   err_info *err = nullptr;

   QApplication::clipboard ()->clear ();
   _desktop->getModel ()->sendFilesLater (QModelIndexList () << stackIndex (),
                                          File::Type_other, true,
                                          [&] (err_info *e) {
      err = e;
      done = true;
   });
   QVERIFY (!done);
   QTRY_VERIFY_WITH_TIMEOUT (done, kDelayMs * 10);
   QVERIFY2 (!err, err ? err->errstr : "");

   // the clipboard has a copy of the whole stack
   const QMimeData *mime = QApplication::clipboard ()->mimeData ();
   QVERIFY (mime && mime->urls ().size () == 1);
   QFileInfo fi (mime->urls () [0].toLocalFile ());
   File *f = File::createFile (fi.absolutePath () + "/", fi.fileName (),
                               nullptr, File::Type_max);

   QVERIFY (f && !f->load ());
   QCOMPARE (f->pagecount (), 5);
   delete f;
   QFile::remove (fi.filePath ());
   CHECK_ASYNC (check, nullptr);
   }
