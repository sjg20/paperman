#include <QApplication>
#include <QDialog>
#include <functional>
#include <QDir>
#include <QLineEdit>
#include <QRadioButton>
#include <QTimer>
#include <QtTest/QtTest>

#include "appearance.h"
#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pageview.h"
#include "pagewidget.h"
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

void TestScreenshots::testDesktop ()
   {
   Mainwindow me;

   open (&me, "house", "kitchen quote.max");
   if (!QTest::currentTestFailed ())
      save (&me, "desktop");
   }

void TestScreenshots::testDesktopDark ()
   {
   Appearance::setMode (Appearance::Mode_dark);

   Mainwindow me;

   open (&me, "house", "kitchen quote.max");
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

   open (&me, "bills/power");
   if (QTest::currentTestFailed ())
      return;

   bool seen = false;

   whenDialog (this, [this, &seen] (QDialog *diag)
      {
      seen = true;
      QTest::keyClicks (diag->findChild<QLineEdit *> ("folderName"), "rec");
      QTest::qWait (300);
      save (diag, "go-to-folder");
      diag->reject ();
      });
   me.getDesktop ()->goToFolder ();
   QVERIFY (seen);
   }
