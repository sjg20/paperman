#include <QApplication>
#include <QDialog>
#include <functional>
#include <QAbstractButton>
#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QMenu>
#include <QPrinter>
#include <QRegularExpression>
#include <QToolButton>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QLineEdit>
#include <QRadioButton>
#include <QTimer>
#include <QtTest/QtTest>

#include "appearance.h"
#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "dirview.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pagedelegate.h"
#include "pageview.h"
#include "pagetools.h"
#include "printopt.h"
#include "pagewidget.h"
#include "pscan.h"
#include "qxmlconfig.h"
#include "fakescan/fakescan.h"
#include "test_fakescan.h"
#include "backendstats.h"
#include "remoteharness.h"
#include "test_screenshots.h"

/* the size of the pictures: big enough to show a folder's stacks beside
   a page, small enough to read on a web page without scaling down */
static const QSize kWindowSize (1440, 900);

/* call \p func with the dialog once it is up, looking every so often,
   since a dialog waits in its own loop for an answer and the test only
   gets in through a timer */
static void whenDialog (QObject *context, std::function<void (QDialog *)> func,
                        int tries = 100)
   {
   QTimer::singleShot (100, context, [context, func, tries] ()
      {
      for (QWidget *widget : QApplication::topLevelWidgets ())
         {
         QDialog *diag = qobject_cast<QDialog *> (widget);

         if (diag && diag->isVisible ())
            {
            func (diag);
            return;
            }
         }
      if (tries > 1)
         whenDialog (context, func, tries - 1);
      });
   }

void TestScreenshots::init ()
   {
   _repo = qEnvironmentVariable ("PAPERMAN_SHOTS_REPO");
   _out = qEnvironmentVariable ("PAPERMAN_SHOTS_DIR");
   if (_repo.isEmpty () || _out.isEmpty ())
      QSKIP ("set PAPERMAN_SHOTS_REPO and PAPERMAN_SHOTS_DIR to take the "
             "screenshots");
   QVERIFY (QDir ().mkpath (_out));
   Appearance::setMode (Appearance::Mode_light);

   /* a text box's cursor is drawn or not as it blinks, so keep it on,
      or the pictures differ from one run to the next */
   QApplication::setCursorFlashTime (0);
   }

void TestScreenshots::cleanup ()
   {
   Appearance::setMode (Appearance::Mode_system);
   }

void TestScreenshots::open (Mainwindow *me, const QString &folder,
                            const QString &stack)
   {
   Desktopwidget *desktop = me->getDesktop ();
   err_info *err = desktop->addDir (_repo);

   QVERIFY2 (!err, err ? err->errstr : "");
   me->resize (kWindowSize);
   me->show ();
   QVERIFY (QTest::qWaitForWindowExposed (me));

   QString path = _repo + "/";

   if (!folder.isEmpty ())
      path += folder + "/";
   QModelIndex dir = desktop->getDirIndex (path);
   QVERIFY2 (dir.isValid (), qPrintable (folder));
   desktop->selectDir (dir);
   settle ();
   if (stack.isEmpty ())
      return;

   Desktopmodel *model = desktop->getModel ();
   Desktopview *view = desktop->getView ();
   QModelIndex parent = view->rootIndex ();

   desktop->getModelconv ()->indexToSource (view->model (), parent);
   QModelIndex ind = model->index (stack, parent);
   QVERIFY2 (ind.isValid (), qPrintable (stack));
   desktop->getModelconv ()->indexToProxy (model, ind);

   /* click it, as a person would, so that the page and its text are
      shown beside the stacks */
   QTest::mouseClick (view->viewport (), Qt::LeftButton, Qt::NoModifier,
                      view->visualRect (ind).center ());
   QCOMPARE (view->currentIndex (), ind);
   settle ();

   // show the whole page beside the stacks
   desktop->_page->slotZoomFit ();
   settle ();
   }

void TestScreenshots::settle ()
   {
   /* the stacks' images and the page are drawn in the background; there
      is nothing to wait on for all of them, so give them time */
   for (int i = 0; i < 20; i++)
      {
      QTest::qWait (100);
      QCoreApplication::processEvents ();
      }
   }

void TestScreenshots::save (QWidget *widget, const QString &name)
   {
   QString path = _out + "/" + name + ".png";

   QVERIFY2 (widget->grab ().save (path), qPrintable (path));
   qInfo () << "saved" << path;
   }

QRect TestScreenshots::within (QWidget *window, QWidget *widget)
   {
   return QRect (widget->mapTo (window, QPoint (0, 0)), widget->size ());
   }

void TestScreenshots::saveParts (QWidget *window, const QString &name,
                                 const QMap<QString, QRect> &parts)
   {
   QJsonObject obj;

   for (auto it = parts.begin (); it != parts.end (); ++it)
      {
      const QRect &rect = it.value ();

      obj [it.key ()] = QJsonArray ({rect.x (), rect.y (), rect.width (),
                                     rect.height ()});
      }
   obj ["window"] = QJsonArray ({0, 0, window->width (), window->height ()});

   QString path = _out + "/" + name + ".json";
   QFile file (path);

   QVERIFY2 (file.open (QIODevice::WriteOnly), qPrintable (path));
   file.write (QJsonDocument (obj).toJson ());
   }

QMap<QString, QRect> TestScreenshots::desktopParts (Mainwindow *me)
   {
   Desktopwidget *desktop = me->getDesktop ();
   Desktopview *view = desktop->getView ();
   Pagewidget *page = desktop->_page;
   QMap<QString, QRect> parts;

   /* the menus and the main toolbar span the window, so point at the
      menus' titles and the toolbar's buttons */
   QMenuBar *menus = me->menuBar ();
   QRect titles;

   for (QAction *action : menus->actions ())
      titles |= menus->actionGeometry (action);
   parts ["menu"] = titles.translated (menus->mapTo (me, QPoint (0, 0)));
   QList<QToolBar *> bars = me->findChildren<QToolBar *> ();
   if (!bars.isEmpty ())
      {
      QRect buttons;

      for (QAction *action : bars.first ()->actions ())
         {
         QWidget *button = bars.first ()->widgetForAction (action);

         if (button && !action->isSeparator ())
            buttons |= within (me, button);
         }
      parts ["toolbar"] = buttons;
      }
   parts ["folders"] = within (me, desktop->_dir);
   parts ["stack-tools"] = within (me, desktop->_toolbar);
   QLineEdit *filter = desktop->_toolbar->findChild<QLineEdit *> ();
   if (filter)
      parts ["filter"] = within (me, filter);
   parts ["stacks"] = within (me, view);

   // the stack selected, as the view draws it
   QRect stack = view->visualRect (view->currentIndex ());
   stack.translate (view->viewport ()->mapTo (me, QPoint (0, 0)));
   parts ["stack"] = stack;

   parts ["page-tools"] = within (me, page->_tools);
   parts ["thumbnails"] = within (me, page->_pageview);
   parts ["page"] = within (me, page->_area);
   parts ["text"] = within (me, page->_ocr_edit);
   parts ["status"] = within (me, me->statusBar ());
   return parts;
   }

void TestScreenshots::testDesktop ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;
   save (&me, "desktop");
   saveParts (&me, "desktop", desktopParts (&me));
   }

void TestScreenshots::saveIcons (const QString &group,
                                 const QList<QAbstractButton *> &buttons)
   {
   QVERIFY (QDir ().mkpath (_out + "/icons"));
   for (QAbstractButton *button : buttons)
      {
      if (button->icon ().isNull () || !button->isVisible ())
         continue;
      QString name = button->objectName ();

      // a toolbar's buttons are named for their actions
      if (QToolButton *tool = qobject_cast<QToolButton *> (button))
         if (tool->defaultAction ())
            name = tool->defaultAction ()->objectName ();
      name.remove (QRegularExpression ("^action"));
      QString path = QString ("%1/icons/%2-%3.png").arg (_out, group,
                                                         name.toLower ());

      QVERIFY2 (button->icon ().pixmap (QSize (32, 32)).save (path),
                qPrintable (path));
      }
   }

void TestScreenshots::testToolbarIcons ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   /* each toolbar's icons, for the tables of what they do; they are
      picked up by name in doc/desktop.rst */
   Desktopwidget *desktop = me.getDesktop ();
   QList<QToolBar *> bars = me.findChildren<QToolBar *> ();

   QVERIFY (!bars.isEmpty ());
   saveIcons ("main", bars.first ()->findChildren<QAbstractButton *> ());
   saveIcons ("stack",
              desktop->_toolbar->findChildren<QAbstractButton *> ());
   saveIcons ("page",
              desktop->_page->_tools->findChildren<QAbstractButton *> ());
   }

void TestScreenshots::testDesktopDark ()
   {
   Appearance::setMode (Appearance::Mode_dark);

   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (!QTest::currentTestFailed ())
      save (&me, "desktop-dark");
   }

void TestScreenshots::testPageView ()
   {
   Mainwindow me;

   open (&me, "house", "tenancy agreement 2020.max");
   if (QTest::currentTestFailed ())
      return;

   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView ();
   QModelIndex ind = view->currentIndex ();
   QPoint centre = view->visualRect (ind).center ();

   QTest::mouseDClick (view->viewport (), Qt::LeftButton, Qt::NoModifier,
                       centre);
   Mainwidget *main = Mainwidget::singleton ();
   QTRY_COMPARE (main->currentWidget (), (QWidget *)main->getPage ());
   settle ();

   // turn to the page where it is signed, by clicking its thumbnail
   Pageview *pages = main->getPage ()->_pageview;
   QModelIndex signing = pages->model ()->index (52, 0);

   QVERIFY (signing.isValid ());
   pages->scrollTo (signing);
   QTest::mouseClick (pages->viewport (), Qt::LeftButton, Qt::NoModifier,
                      pages->visualRect (signing).center ());
   settle ();
   main->getPage ()->slotZoomFit ();
   settle ();
   save (&me, "page-view");
   }

void TestScreenshots::testSearch ()
   {
   Mainwindow me;

   open (&me, QString ());
   if (QTest::currentTestFailed ())
      return;

   // fill in the dialog, take its picture and press OK
   bool seen = false;

   whenDialog (this, [this, &seen] (QDialog *diag)
      {
      seen = true;
      diag->findChild<QRadioButton *> ("byText")->setChecked (true);
      diag->findChild<QLineEdit *> ("stackName")->setText ("warranty");
      save (diag, "search-dialog");
      diag->accept ();
      });
   Desktopwidget *desktop = me.getDesktop ();

   desktop->activateSearch ();
   QVERIFY (seen);
   settle ();

   // pick the manual, to show the page it was found on
   Desktopview *view = desktop->getView ();
   QModelIndex found;

   for (int row = 0; row < view->model ()->rowCount (view->rootIndex ());
        row++)
      {
      QModelIndex ind = view->model ()->index (row, 0, view->rootIndex ());

      if (ind.data ().toString ().startsWith ("dishwasher"))
         found = ind;
      }
   QVERIFY (found.isValid ());
   QTest::mouseClick (view->viewport (), Qt::LeftButton, Qt::NoModifier,
                      view->visualRect (found).center ());
   settle ();
   desktop->_page->slotZoomFit ();
   settle ();
   save (&me, "search-results");
   }

void TestScreenshots::testGoToFolder ()
   {
   Mainwindow me;

   open (&me, "bills/2026/10oct");
   if (QTest::currentTestFailed ())
      return;

   bool seen = false;

   whenDialog (this, [this, &seen] (QDialog *diag)
      {
      seen = true;
      QTest::keyClicks (diag->findChild<QLineEdit *> ("folderName"), "jun");
      QTest::qWait (300);
      save (diag, "go-to-folder");
      diag->reject ();
      });
   me.getDesktop ()->goToFolder ();
   QVERIFY (seen);
   }

void TestScreenshots::testScanPanel ()
   {
   if (!Fakescan::available ())
      QSKIP ("the fake scanner is only built on Linux");

   Mainwindow me;

   open (&me, "bills/2026/10oct");
   if (QTest::currentTestFailed ())
      return;

   // scan with the fake scanner, as if it were the one last used
   if (!xmlConfig)
      new QXmlConfig ();
   xmlConfig->setStringValue ("LAST_DEVICE", FAKESCAN_DEVICE);
   Mainwidget::singleton ()->pscan ();

   Pscan *pscan = nullptr;

   for (QWidget *widget : QApplication::topLevelWidgets ())
      if (qobject_cast<Pscan *> (widget))
         pscan = qobject_cast<Pscan *> (widget);
   QVERIFY (pscan);
   QTRY_VERIFY (pscan->isVisible ());
   settle ();

   /* choose the usual preset, so the panel does not show <custom>, and
      an exposure in the middle of its range */
   int item = pscan->preset->findText ("Monochrome 300dpi duplex");

   QVERIFY (item >= 0);
   pscan->preset->setCurrentIndex (item);
   pscan->on_preset_activated (item);
   pscan->bright->setValue (128);
   pscan->brightChanged (128);
   settle ();
   QCOMPARE (pscan->preset->currentText (),
             QString ("Monochrome 300dpi duplex"));
   save (pscan, "scan-panel");

   /* typing in the folder shows the folders to scan into, this month's
      first */
   pscan->folderName->setFocus ();
   QTest::keyClicks (pscan->folderName, "bills");
   settle ();
   save (pscan, "scan-folders");
   pscan->close ();
   }

void TestScreenshots::saveMenu (QWidget *widget, QPoint pos,
                                const QString &name, const QString &sub)
   {
   /* the menu waits for a choice in a loop of its own, like a dialog,
      so take its picture and close it from a timer */
   bool seen = false;

   std::function<void (int)> look = [&] (int tries)
      {
      for (QWidget *top : QApplication::topLevelWidgets ())
         {
         QMenu *menu = qobject_cast<QMenu *> (top);

         if (menu && menu->isVisible () && !sub.isEmpty ())
            {
            // open the submenu beside its item, as hovering on it does
            for (QAction *action : menu->actions ())
               if (action->menu ()
                   && action->text ().remove ('&') == sub)
                  {
                  QRect item = menu->actionGeometry (action);

                  menu->setActiveAction (action);
                  action->menu ()->popup (
                     menu->mapToGlobal (item.topRight ()));
                  QTest::qWait (300);
                  seen = true;
                  save (action->menu (), name);
                  action->menu ()->close ();
                  }
            menu->close ();
            return;
            }
         if (menu && menu->isVisible ())
            {
            seen = true;
            save (menu, name);
            menu->close ();
            return;
            }
         }
      if (tries > 1)
         QTimer::singleShot (100, this, [&look, tries] { look (tries - 1); });
      };
   QTimer::singleShot (100, this, [&look] { look (100); });

   QContextMenuEvent event (QContextMenuEvent::Mouse, pos,
                            widget->mapToGlobal (pos));

   QApplication::sendEvent (widget, &event);
   QVERIFY (seen);
   }

void TestScreenshots::testStackMenu ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   Desktopview *view = me.getDesktop ()->getView ();

   saveMenu (view->viewport (),
             view->visualRect (view->currentIndex ()).center (),
             "stack-menu");
   }

void TestScreenshots::testDuplicateMenu ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   Desktopview *view = me.getDesktop ()->getView ();

   saveMenu (view->viewport (),
             view->visualRect (view->currentIndex ()).center (),
             "duplicate-menu", "Duplicate...");
   }

void TestScreenshots::testFolderMenu ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar");
   if (QTest::currentTestFailed ())
      return;

   Dirview *folders = me.getDesktop ()->_dir;

   saveMenu (folders->viewport (),
             folders->visualRect (folders->currentIndex ()).center (),
             "folder-menu");
   }

void TestScreenshots::testMoveToFolder ()
   {
   Mainwindow me;

   open (&me, "bills/2026/10oct", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   bool seen = false;

   whenDialog (this, [this, &seen] (QDialog *diag)
      {
      seen = true;
      QTest::keyClicks (diag->findChild<QLineEdit *> ("folderName"), "tax");
      QTest::qWait (300);
      save (diag, "move-to-folder");
      diag->reject ();
      });
   me.getDesktop ()->moveToFolder ();
   QVERIFY (seen);
   }

void TestScreenshots::testFilter ()
   {
   Mainwindow me;

   open (&me, "bills/2026/01jan");
   if (QTest::currentTestFailed ())
      return;

   QLineEdit *filter =
      me.getDesktop ()->_toolbar->findChild<QLineEdit *> ();

   QVERIFY (filter);
   QTest::keyClicks (filter, "bill");
   settle ();
   save (&me, "filter");
   }

void TestScreenshots::testRemovePage ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   /* mark the second page to be removed, as a click on its tick does,
      and show the first */
   Pagewidget *page = me.getDesktop ()->_page;
   Pageview *pages = page->_pageview;
   QModelIndex kept = pages->model ()->index (0, 0);
   QModelIndex removed = pages->model ()->index (1, 0);

   QVERIFY (removed.isValid ());
   page->slotItemClicked (removed, Pagedelegate::Point_remove);
   settle ();

   // only the preview pane, where this happens
   save (page, "page-remove");

   /* point at each page as a whole: an arrow to its tick would run
      across its name */
   QMap<QString, QRect> parts;
   QPoint origin = pages->viewport ()->mapTo (page, QPoint ());

   parts ["kept"] = pages->visualRect (kept).translated (origin);
   parts ["removed"] = pages->visualRect (removed).translated (origin);
   for (const char *name : {"save", "revert"})
      {
      QWidget *button = page->_tools->findChild<QWidget *> (name);

      QVERIFY2 (button, name);
      parts [name] = within (page, button);
      }
   saveParts (page, "page-remove", parts);
   }

void TestScreenshots::testDragToFolder ()
   {
   Mainwindow me;

   open (&me, "bills/2026/06jun", "pine and nail.max");
   if (QTest::currentTestFailed ())
      return;

   /* the picture is of the window as the drag starts; the annotation
      draws where the stack goes, from the parts saved here: a receipt
      from the hardware shop, which belongs with the house */
   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView ();
   Dirview *folders = desktop->_dir;
   QAbstractItemModel *model = folders->model ();
   QModelIndexList found = model->match (model->index (0, 0),
                                         Qt::DisplayRole, "house", 1,
                                         Qt::MatchExactly
                                         | Qt::MatchRecursive);

   QVERIFY (!found.isEmpty ());
   save (&me, "drag-to-folder");

   QMap<QString, QRect> parts;

   parts ["stack"] = view->visualRect (view->currentIndex ())
                        .translated (view->viewport ()->mapTo (&me,
                                                               QPoint ()));
   parts ["folder"] = folders->visualRect (found.first ())
                         .translated (folders->viewport ()->mapTo (
                                         &me, QPoint ()));
   saveParts (&me, "drag-to-folder", parts);
   }

void TestScreenshots::testEmailMenu ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   Desktopview *view = me.getDesktop ()->getView ();

   saveMenu (view->viewport (),
             view->visualRect (view->currentIndex ()).center (),
             "email-menu", "Email...");
   }

void TestScreenshots::testPrintOptions ()
   {
   Mainwindow me;

   open (&me, "bills/2026/03mar", "power bill.max");
   if (QTest::currentTestFailed ())
      return;

   /* the options are a tab of the print dialog, which is the
      platform's own, so show them on their own, as Paperman builds them
      for printing the stack selected */
   Desktopview *view = me.getDesktop ()->getView ();
   QModelIndexList list = view->getSelectedList (true, true);
   QPrinter printer;
   Printopt opt (&printer, list);

   opt.resize (opt.sizeHint ());
   opt.show ();
   QVERIFY (QTest::qWaitForWindowExposed (&opt));

   // the layout leaves room at the foot, which is no use in a picture
   opt.resize (opt.width (), opt.childrenRect ().bottom () + 12);
   settle ();
   save (&opt, "print-options");
   }

void TestScreenshots::testServer ()
   {
   /* serve the corpus from a server in this process, on the port a
      server usually has, and show the desktop attached to it, as on
      another computer. It is reached straight, not through the test
      proxy, so that the folder pane shows a usual address */
   ServerThread server (_repo, 8080);

   if (!server.begin ())
      QSKIP ("cannot serve on port 8080, which something else has");
   QUrl url ("http://localhost:8080");

   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop ();

   me.resize (kWindowSize);
   me.show ();
   QVERIFY (QTest::qWaitForWindowExposed (&me));

   bool attached = false;
   QString err;

   desktop->addRemoteServer (url, [&] (const QString &error)
      {
      err = error;
      attached = true;
      });
   QTRY_VERIFY_WITH_TIMEOUT (attached, 20000);
   QCOMPARE (err, QString ());

   QString path = url.toString () + "/" + QFileInfo (_repo).fileName ()
                  + "/bills/2026/03mar";

   QTRY_VERIFY_WITH_TIMEOUT (desktop->getDirIndex (path).isValid (), 20000);
   desktop->selectDir (desktop->getDirIndex (path));

   // give the folder pane room for the server's address
   QList<int> sizes = desktop->sizes ();

   sizes [2] -= 90;
   sizes [0] += 90;
   desktop->setSizes (sizes);

   /* the stacks' page counts and pictures come from the server a few at
      a time, and the stack pane grows as they arrive, so wait until it
      has answered everything for a second */
   BackendStats *stats = desktop->_model->stats ();
   QElapsedTimer timer;

   timer.start ();
   for (int quiet = 0; quiet < 5 && timer.elapsed () < 30000; )
      {
      QTest::qWait (200);
      quiet = stats->activeRequests () ? 0 : quiet + 1;
      }
   QCOMPARE (stats->activeRequests (), 0);
   desktop->getView ()->doItemsLayout ();
   settle ();
   save (&me, "server");

   // the server's repository, and the connection to it
   Dirview *folders = desktop->_dir;
   QAbstractItemModel *model = folders->model ();
   QModelIndex top = model->index (0, 0);
   QMap<QString, QRect> parts;

   parts ["repository"] = folders->visualRect (top).translated (
      folders->viewport ()->mapTo (&me, QPoint ()));
   parts ["connection"] = within (&me, desktop->netStatus ());
   saveParts (&me, "server", parts);
   }
