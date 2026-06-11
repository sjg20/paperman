#include <QLineEdit>
#include <QToolButton>
#include <QtTest/QtTest>

#include "test.h"

#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pageview.h"
#include "pagewidget.h"
#include "test_pagewidget.h"

//! Read the zoom level shown to the user, e.g. "100%" gives 100
static int zoomValue(QLineEdit *zoom)
{
   return zoom->text().remove('%').toInt();
}

void TestPagewidget::openTestStack(Mainwindow *me, Desktopmodel *&model,
                                   Pagewidget *&page)
{
   auto path = setupRepo();
   Desktopwidget *desktop = me->getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me->resize(1024, 768);
   me->show();
   QVERIFY(QTest::qWaitForWindowExposed(me));
   QTest::qWait(50);

   // Select the 5-page max stack and open it in the page view
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 1);
   me->actionSwap->trigger();

   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);
   page = main->getPage();
   QTRY_COMPARE(main->currentWidget(), (QWidget *)page);

   QModelIndex shown;
   QVERIFY(page->getCurrentIndex(shown, true));
   QCOMPARE(model->data(shown, Qt::DisplayRole).toString(), "testfile");
}

void TestPagewidget::testThumbnailClickShowsPage()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   Pageview *pageview = page->findChild<Pageview *>();
   QVERIFY(pageview);

   // All five pages should appear in the thumbnail list, and the
   // stack's current page (the first) should be the one displayed
   QCOMPARE(pageview->model()->rowCount(), 5);
   QCOMPARE(page->getCurrentPage(), 0);

   // Click the third thumbnail and check that page is displayed
   QModelIndex ind = pageview->model()->index(2, 0);
   QVERIFY(ind.isValid());
   QRect rect = pageview->visualRect(ind);
   QVERIFY(rect.isValid());
   QTest::mouseClick(pageview->viewport(), Qt::LeftButton, Qt::NoModifier,
                     rect.center());
   QCOMPARE(page->getCurrentPage(), 2);

   // And back to the first
   ind = pageview->model()->index(0, 0);
   QTest::mouseClick(pageview->viewport(), Qt::LeftButton, Qt::NoModifier,
                     pageview->visualRect(ind).center());
   QCOMPARE(page->getCurrentPage(), 0);
}

void TestPagewidget::testOpenAtCurrentPage()
{
   Desktopmodel *model;
   Mainwindow me;

   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   // Flip the stack to page 4 on the desktop, as a user would with the
   // page-next arrows
   QModelIndex src_ind = model->index(0, 0, repo_ind);
   model->setData(src_ind, 3, Desktopmodel::Role_pagenum);

   // Open the stack: the page view must show page 4, not some other
   // page. This is a regression test for showPage() passing a fixed
   // page number to showPages()
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 1);
   me.actionSwap->trigger();

   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);
   Pagewidget *page = main->getPage();
   QTRY_COMPARE(main->currentWidget(), (QWidget *)page);
   QCOMPARE(page->getCurrentPage(), 3);
}

void TestPagewidget::testZoomButtons()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QLineEdit *zoom = page->findChild<QLineEdit *>("zoomLevel");
   QToolButton *zoom_in = page->findChild<QToolButton *>("zoomIn");
   QToolButton *zoom_out = page->findChild<QToolButton *>("zoomOut");
   QToolButton *zoom_orig = page->findChild<QToolButton *>("zoomOrig");
   QToolButton *zoom_fit = page->findChild<QToolButton *>("zoomFit");
   QVERIFY(zoom && zoom_in && zoom_out && zoom_orig && zoom_fit);
   QVERIFY(zoom->isEnabled());

   // The original-size button shows the page at 1:4 (25%), which
   // matches a 300dpi scan on a typical screen
   QTest::mouseClick(zoom_orig, Qt::LeftButton);
   QCOMPARE(zoomValue(zoom), 25);

   // Zooming in increases the level; zooming out decreases it
   QTest::mouseClick(zoom_in, Qt::LeftButton);
   int zoomed = zoomValue(zoom);
   QVERIFY(zoomed > 25);

   QTest::mouseClick(zoom_out, Qt::LeftButton);
   QVERIFY(zoomValue(zoom) < zoomed);

   // Fit-to-window picks a sensible scale for the viewport
   QTest::mouseClick(zoom_fit, Qt::LeftButton);
   QVERIFY(zoomValue(zoom) > 0);
}

void TestPagewidget::testZoomLevelEdit()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QLineEdit *zoom = page->findChild<QLineEdit *>("zoomLevel");
   QVERIFY(zoom);

   // Type a specific zoom level and press return
   zoom->selectAll();
   QTest::keyClicks(zoom, "50");
   QTest::keyClick(zoom, Qt::Key_Return);
   QCOMPARE(zoomValue(zoom), 50);

   zoom->selectAll();
   QTest::keyClicks(zoom, "200");
   QTest::keyClick(zoom, Qt::Key_Return);
   QCOMPARE(zoomValue(zoom), 200);
}

void TestPagewidget::testRotateButtons()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QToolButton *rleft = page->findChild<QToolButton *>("rleft");
   QToolButton *rright = page->findChild<QToolButton *>("rright");
   QToolButton *r180 = page->findChild<QToolButton *>("vflip");
   QVERIFY(rleft && rright && r180);

   QCOMPARE(page->_rotate, 0);

   // Rotate right goes clockwise in 90-degree steps
   QTest::mouseClick(rright, Qt::LeftButton);
   QCOMPARE(page->_rotate, 90);

   QTest::mouseClick(rright, Qt::LeftButton);
   QCOMPARE(page->_rotate, 180);

   // Rotate left goes back
   QTest::mouseClick(rleft, Qt::LeftButton);
   QCOMPARE(page->_rotate, 90);

   // The 180 button turns the page upside down from where it is
   QTest::mouseClick(r180, Qt::LeftButton);
   QCOMPARE(page->_rotate, 270);

   // A full circle returns to normal
   QTest::mouseClick(rright, Qt::LeftButton);
   QCOMPARE(page->_rotate, 0);
}

//! Count dark pixels in an image, sampling every few pixels
static int darkPixels(const QImage &img)
{
   QImage grey = img.convertToFormat(QImage::Format_Grayscale8);
   int dark = 0;

   for (int y = 0; y < grey.height(); y += 4) {
      const uchar *p = grey.constScanLine(y);
      for (int x = 0; x < grey.width(); x += 4)
         if (p[x] < 128)
            dark++;
   }
   return dark;
}

void TestPagewidget::testRotateActionDisplay()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   // the displayed page has content
   QSize before = page->_image.size();
   QVERIFY(darkPixels(page->_image) > 50);

   /* rotating in the page view must show the rotated page, not a
      blank or stale one */
   me.actionRright->trigger();
   QCOMPARE(page->_image.size(), QSize(before.height(), before.width()));
   QVERIFY(darkPixels(page->_image) > 50);

   // rotating a second time works too (back to the original size)
   me.actionRright->trigger();
   QCOMPARE(page->_image.size(), before);
   QVERIFY(darkPixels(page->_image) > 50);

   // a third press reaches 270 degrees
   me.actionRright->trigger();
   QCOMPARE(page->_image.size(), QSize(before.height(), before.width()));
   QVERIFY(darkPixels(page->_image) > 50);

   // undo returns to 180 degrees, shown immediately
   me.actionUndo->trigger();
   QCOMPARE(page->_image.size(), before);
   QVERIFY(darkPixels(page->_image) > 50);
}

void TestPagewidget::testEditAttributesSave()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QLineEdit *author = page->findChild<QLineEdit *>("author");
   QLineEdit *title = page->findChild<QLineEdit *>("title");
   QLineEdit *keywords = page->findChild<QLineEdit *>("keywords");
   QToolButton *save = page->findChild<QToolButton *>("save");
   QVERIFY(author && title && keywords && save);

   // The fields start out empty and nothing needs saving
   QCOMPARE(author->text(), QString());
   QVERIFY(!save->isEnabled());

   // Type some attribute text: the save button becomes available
   QTest::keyClicks(author, "Fred Bloggs");
   QTest::keyClicks(title, "Annual tax return");
   QTest::keyClicks(keywords, "tax");
   QVERIFY(save->isEnabled());

   QTest::mouseClick(save, Qt::LeftButton);

   // The annotations should now be stored in the stack
   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   QCOMPARE(model->data(ind, Desktopmodel::Role_author).toString(),
            "Fred Bloggs");
   QCOMPARE(model->data(ind, Desktopmodel::Role_title).toString(),
            "Annual tax return");
   QCOMPARE(model->data(ind, Desktopmodel::Role_keywords).toString(),
            "tax");

   // Everything is saved, so the save button is disabled again
   QVERIFY(!save->isEnabled());
}

void TestPagewidget::testEditAttributesRevert()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QLineEdit *author = page->findChild<QLineEdit *>("author");
   QToolButton *save = page->findChild<QToolButton *>("save");
   QToolButton *revert = page->findChild<QToolButton *>("revert");
   QVERIFY(author && save && revert);

   // Save an author name
   QTest::keyClicks(author, "Fred");
   QTest::mouseClick(save, Qt::LeftButton);

   // Make a further edit, then revert: the saved name comes back
   QTest::keyClicks(author, " Bloggs");
   QCOMPARE(author->text(), QString("Fred Bloggs"));
   QVERIFY(revert->isEnabled());
   QTest::mouseClick(revert, Qt::LeftButton);
   QCOMPARE(author->text(), QString("Fred"));

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   QCOMPARE(model->data(ind, Desktopmodel::Role_author).toString(),
            "Fred");
}
