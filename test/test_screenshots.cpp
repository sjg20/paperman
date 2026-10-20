#include <QApplication>
#include <QDialog>
#include <functional>
#include <QAbstractButton>
#include <QAction>
#include <QDir>
#include <QRegularExpression>
#include <QToolButton>
#include <QFile>
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
#include "dirview.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pageview.h"
#include "pagetools.h"
#include "pagewidget.h"
#include "pscan.h"
#include "qxmlconfig.h"
#include "fakescan/fakescan.h"
#include "test_fakescan.h"
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
