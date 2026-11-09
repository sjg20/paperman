#include <QFrame>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTextEdit>
#include <QToolButton>
#include <QtTest/QtTest>

#include "test.h"

#include "desktopmodel.h"
#include "desktopundo.h"
#include "imagetrim.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "ocrpage.h"
#include "ocrreader.h"
#include "pagemodel.h"
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

void TestPagewidget::testRenderedAtOtherSize()
{
   Pagemodel model(nullptr);
   const QSize small(100, 140), large(400, 560);

   // a page set up at the size of one view
   model.setPagesize(small);
   Pageinfo page;
   page.setup(&model, 0);

   /* drawn by the render thread for another view, with larger pages, such
      as the scan view */
   model.setPagesize(large);
   QPixmap drawn(large);
   drawn.fill(Qt::white);
   page.setPixmap(drawn, large);

   // back in the first view, it is shown at that view's size, not its own
   model.setPagesize(small);
   bool dodgy = false;
   QPixmap shown = page.pixmap(dodgy);
   QVERIFY2(shown.width() <= small.width() && shown.height() <= small.height(),
            qPrintable(QString("%1x%2").arg(shown.width()).arg(shown.height())));
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

//! Mean per-pixel difference between two same-sized images
static int meanDiff(const QImage &a, const QImage &b)
{
   QImage ga = a.convertToFormat(QImage::Format_Grayscale8);
   QImage gb = b.convertToFormat(QImage::Format_Grayscale8);
   qint64 total = 0;

   for (int y = 0; y < ga.height(); y++) {
      const uchar *pa = ga.constScanLine(y);
      const uchar *pb = gb.constScanLine(y);
      for (int x = 0; x < ga.width(); x++)
         total += qAbs(int(pa[x]) - int(pb[x]));
   }
   return total / (qint64(ga.width()) * ga.height());
}

/* QTest::mouseMove() does not reach the widget under Qt 5, so send the
   move of a drag with the left button held */
static void dragTo(QWidget *widget, const QPoint &pos)
{
   QMouseEvent event(QEvent::MouseMove, QPointF(pos),
                     QPointF(widget->mapToGlobal(pos)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
   QApplication::sendEvent(widget, &event);
}

#define WAIT_OPS(model) \
    QTRY_VERIFY_WITH_TIMEOUT(!(model).opsPending(), 10000)

void TestPagewidget::testCropTool()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QToolButton *crop = page->findChild<QToolButton *>("crop");
   QToolButton *apply = page->findChild<QToolButton *>("cropPage");
   QFrame *bar = page->findChild<QFrame *>("cropBar");
   QVERIFY(crop && apply && bar);
   QVERIFY(!page->_area->isCropping());

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   File *f = model->getFile(ind);
   QVERIFY(f);

   QImage first, last, image;
   QSize firstSize, lastSize, size, trueSize;
   int bpp;
   QVERIFY(!f->getImage(0, false, first, firstSize, trueSize, bpp, false));
   QVERIFY(!f->getImage(4, false, last, lastSize, trueSize, bpp, false));

   // the tool starts with the whole page marked
   const QRectF whole(0, 0, 1, 1);
   QTest::mouseClick(crop, Qt::LeftButton);
   QVERIFY(page->_area->isCropping());
   QVERIFY(!bar->isHidden());
   QCOMPARE(page->_area->cropArea(), whole);

   // the buttons mark what is printed, then all of the page again
   QToolButton *fit = page->findChild<QToolButton *>("cropFit");
   QToolButton *full = page->findChild<QToolButton *>("cropFull");
   QVERIFY(fit && full);
   QVERIFY(!fit->icon().isNull() && !full->icon().isNull());
   QTest::mouseClick(fit, Qt::LeftButton);
   QVERIFY(page->_area->cropArea() != whole);
   QVERIFY(whole.contains(page->_area->cropArea()));
   QTest::mouseClick(full, Qt::LeftButton);
   QCOMPARE(page->_area->cropArea(), whole);

   QToolButton *all = page->findChild<QToolButton *>("cropAll");
   QVERIFY(all);
   QVERIFY(!apply->icon().isNull() && !all->icon().isNull());

   // the buttons on the right are icons alone, named for screen readers
   for (const char *name : {"cropPage", "cropAll", "cropEach", "cropCancel"}) {
      QToolButton *button = page->findChild<QToolButton *>(name);
      QVERIFY(button);
      QVERIFY(!button->icon().isNull());
      QVERIFY(button->text().isEmpty());
      QVERIFY(!button->accessibleName().isEmpty());
   }

   // the hint shows only when there is room, so never holds the window wide
   HintLabel *hint = page->findChild<HintLabel *>("cropHint");
   QVERIFY(hint);
   QCOMPARE(hint->minimumSizeHint().width(), 0);
   int narrow = bar->minimumSizeHint().width();
   bar->resize(narrow, bar->height());
   bar->layout()->activate();
   QVERIFY(!hint->fits());
   bar->resize(narrow + hint->sizeHint().width() + 50, bar->height());
   bar->layout()->activate();
   QVERIFY(hint->fits());

   QRectF area(0.25, 0.25, 0.5, 0.5);
   page->_area->setCropArea(area);
   QTest::mouseClick(apply, Qt::LeftButton);
   WAIT_OPS(*model);

   // only the page shown changes, and the tool has finished
   QVERIFY(!page->_area->isCropping());
   QVERIFY(bar->isHidden());
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(size, Imagetrim::areaToRect(firstSize, area).size());
   QCOMPARE(page->_image.size(), image.size());
   QVERIFY(!f->getImage(4, false, image, size, trueSize, bpp, false));
   QCOMPARE(size, lastSize);

   // undo brings the page back, and shows it
   model->getUndoStack()->undo();
   WAIT_OPS(*model);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(size, firstSize);
   QCOMPARE(image, first);
   QCOMPARE(page->_image.size(), first.size());

   // and redo crops it again
   model->getUndoStack()->redo();
   WAIT_OPS(*model);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(size, Imagetrim::areaToRect(firstSize, area).size());
}

void TestPagewidget::testCropDrag()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   // see all of the page, so that its edges are on the screen
   page->findChild<QToolButton *>("zoomFit")->click();
   QTest::qWait(50);
   page->findChild<QToolButton *>("crop")->click();
   page->_area->setCropArea(QRectF(0.2, 0.2, 0.2, 0.2));

   QWidget *view = page->_area->viewport();
   QRectF on = page->_area->cropOnScreen();
   QVERIFY(view->rect().contains(on.toAlignedRect()));

   // dragging the bottom-right corner leaves the top-left where it is
   QPoint corner = on.bottomRight().toPoint();
   QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, corner);
   dragTo(view, corner + QPoint(20, 30));
   QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier,
                       corner + QPoint(20, 30));

   QRectF after = page->_area->cropOnScreen();
   QVERIFY(qAbs(after.left() - on.left()) < 1);
   QVERIFY(qAbs(after.top() - on.top()) < 1);
   QVERIFY(qAbs(after.right() - on.right() - 20) < 2);
   QVERIFY(qAbs(after.bottom() - on.bottom() - 30) < 2);

   // dragging inside draws a new box there, which can then be adjusted
   on = after;
   QPoint middle = on.center().toPoint();
   QPoint end = middle + QPoint(30, 25);
   QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, middle);
   dragTo(view, end);
   QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, end);
   after = page->_area->cropOnScreen();
   QVERIFY(qAbs(after.left() - middle.x()) < 2);
   QVERIFY(qAbs(after.top() - middle.y()) < 2);
   QVERIFY(qAbs(after.right() - end.x()) < 2);
   QVERIFY(qAbs(after.bottom() - end.y()) < 2);

   on = after;
   QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, end);
   dragTo(view, end + QPoint(15, 0));
   QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier,
                       end + QPoint(15, 0));
   after = page->_area->cropOnScreen();
   QVERIFY(qAbs(after.left() - on.left()) < 1);
   QVERIFY(qAbs(after.right() - on.right() - 15) < 2);

   // a click outside does not lose what is marked
   QRectF before = page->_area->cropArea();
   QPoint outside = QPoint(2, 2);
   QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, outside);
   QCOMPARE(page->_area->cropArea(), before);

   // a drag outside marks a new area
   QPoint from(5, 5), to(40, 60);
   QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, from);
   dragTo(view, to);
   QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, to);
   after = page->_area->cropOnScreen();
   QVERIFY(qAbs(after.left() - from.x()) < 2);
   QVERIFY(qAbs(after.bottom() - to.y()) < 2);

   // it never leaves the page
   page->findChild<QToolButton *>("crop")->click();
   QVERIFY(!page->_area->isCropping());
}

void TestPagewidget::testCropStack()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   File *f = model->getFile(ind);
   QVERIFY(f);

   QList<QSize> before;
   QImage image;
   QSize size, trueSize;
   int bpp;

   for (int i = 0; i < f->pagecount(); i++) {
      QVERIFY(!f->getImage(i, false, image, size, trueSize, bpp, false));
      before << size;
   }

   QRectF area(0, 0, 1, 0.5);
   page->findChild<QToolButton *>("crop")->click();
   page->_area->setCropArea(area);
   page->findChild<QToolButton *>("cropAll")->click();
   WAIT_OPS(*model);

   for (int i = 0; i < f->pagecount(); i++) {
      QVERIFY(!f->getImage(i, false, image, size, trueSize, bpp, false));
      QVERIFY2(qAbs(size.height() - before[i].height() / 2) <= 1,
               qPrintable(QString::number(i)));
   }

   // one undo covers them all
   model->getUndoStack()->undo();
   WAIT_OPS(*model);
   for (int i = 0; i < f->pagecount(); i++) {
      QVERIFY(!f->getImage(i, false, image, size, trueSize, bpp, false));
      QCOMPARE(size, before[i]);
   }

   // fitting each page changes none of them to more than they were
   page->findChild<QToolButton *>("crop")->click();
   page->findChild<QToolButton *>("cropEach")->click();
   WAIT_OPS(*model);
   for (int i = 0; i < f->pagecount(); i++) {
      QVERIFY(!f->getImage(i, false, image, size, trueSize, bpp, false));
      QVERIFY(size.width() <= before[i].width());
      QVERIFY(size.height() <= before[i].height());
   }
}

void TestPagewidget::testCropCancel()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   File *f = model->getFile(ind);
   QImage image;
   QSize before, size, trueSize;
   int bpp;
   QVERIFY(!f->getImage(0, false, image, before, trueSize, bpp, false));
   int undos = model->getUndoStack()->count();

   QToolButton *crop = page->findChild<QToolButton *>("crop");
   crop->click();
   page->_area->setCropArea(QRectF(0, 0, 0.5, 0.5));
   page->findChild<QToolButton *>("cropCancel")->click();
   QVERIFY(!crop->isChecked());
   QVERIFY(!page->_area->isCropping());

   // the second way out is the button it came in by
   crop->click();
   crop->click();
   QVERIFY(!page->_area->isCropping());

   // with the whole page marked there is nothing to do, nor to undo
   crop->click();
   page->findChild<QToolButton *>("cropPage")->click();
   QVERIFY(!page->_area->isCropping());

   WAIT_OPS(*model);
   QCOMPARE(model->getUndoStack()->count(), undos);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(size, before);
}

void TestPagewidget::testCropKeys()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);
   me.activateWindow();
   QVERIFY(QTest::qWaitForWindowActive(&me));

   Mainwidget *main = Mainwidget::singleton();
   QToolButton *crop = page->findChild<QToolButton *>("crop");

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   File *f = model->getFile(ind);
   QImage image;
   QSize before, second, size, trueSize;
   int bpp;
   QVERIFY(!f->getImage(0, false, image, before, trueSize, bpp, false));
   QVERIFY(!f->getImage(1, false, image, second, trueSize, bpp, false));

   // Escape stops cropping, and stays in the page view
   crop->click();
   QTRY_VERIFY(page->_area->hasFocus());
   QTest::keyClick(page->_area, Qt::Key_Escape);
   QVERIFY(!page->_area->isCropping());
   QVERIFY(!crop->isChecked());
   QCOMPARE(main->currentWidget(), (QWidget *)page);

   // Return crops the page to what is marked
   crop->click();
   page->_area->setCropArea(QRectF(0, 0, 1, 0.5));
   QTest::keyClick(page->_area, Qt::Key_Return);
   WAIT_OPS(*model);
   QVERIFY(!page->_area->isCropping());
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QVERIFY(qAbs(size.height() - before.height() / 2) <= 1);

   // so does Enter on the keypad, leaving the other pages alone
   crop->click();
   page->_area->setCropArea(QRectF(0, 0, 1, 0.5));
   QTest::keyClick(page->_area, Qt::Key_Enter, Qt::KeypadModifier);
   WAIT_OPS(*model);
   QVERIFY(!page->_area->isCropping());
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QVERIFY(qAbs(size.height() - before.height() / 4) <= 2);
   QVERIFY(!f->getImage(1, false, image, size, trueSize, bpp, false));
   QCOMPARE(size, second);

   // Shift+Return crops the whole stack
   crop->click();
   page->_area->setCropArea(QRectF(0, 0, 1, 0.5));
   QTest::keyClick(page->_area, Qt::Key_Return, Qt::ShiftModifier);
   WAIT_OPS(*model);
   QVERIFY(!page->_area->isCropping());
   QVERIFY(!f->getImage(1, false, image, size, trueSize, bpp, false));
   QVERIFY(qAbs(size.height() - second.height() / 2) <= 1);

   // with the tool gone, Escape goes back to the desktop as usual
   QTest::keyClick(page->_area, Qt::Key_Escape);
   QTRY_VERIFY(main->currentWidget() != (QWidget *)page);
}

void TestPagewidget::testCropLeavesStack()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QToolButton *crop = page->findChild<QToolButton *>("crop");
   QFrame *bar = page->findChild<QFrame *>("cropBar");

   // choosing another stack abandons what is marked on this one
   QModelIndex other = page->_index.sibling(page->_index.row() + 1, 0);
   QVERIFY(other.isValid());
   crop->click();
   QVERIFY(page->_area->isCropping());
   page->showPages(page->_model, other, 0, -1, -1);
   QVERIFY(!crop->isChecked());
   QVERIFY(!page->_area->isCropping());
   QVERIFY(bar->isHidden());

   // so does the page view being emptied, leaving no page to crop
   crop->click();
   QVERIFY(page->_area->isCropping());
   page->slotReset();
   QVERIFY(!crop->isChecked());
   QVERIFY(!page->_area->isCropping());
   QVERIFY(bar->isHidden());
   QVERIFY(!crop->isEnabled());
}

void TestPagewidget::testRotateButtons()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QToolButton *rleft = page->findChild<QToolButton *>("rleft");
   QToolButton *rright = page->findChild<QToolButton *>("rright");
   QToolButton *hflip = page->findChild<QToolButton *>("hflip");
   QToolButton *vflip = page->findChild<QToolButton *>("vflip");
   QVERIFY(rleft && rright && hflip && vflip);

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   File *f = model->getFile(ind);
   QVERIFY(f);

   QImage first, last, image;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!f->getImage(0, false, first, size, trueSize, bpp, false));
   QVERIFY(!f->getImage(4, false, last, size, trueSize, bpp, false));

   /* rotate-right transforms only the page being shown; the other
      pages are untouched */
   QTest::mouseClick(rright, Qt::LeftButton);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.width(), first.height());
   QVERIFY(!f->getImage(4, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), last.size());
   QCOMPARE(page->_image.height(), first.width());

   // rotate-left turns it back
   QTest::mouseClick(rleft, Qt::LeftButton);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), first.size());
   QCOMPARE(page->_image.size(), first.size());

   /* the flips match the reference transform (a small difference is
      allowed for JPEG recompression) and are undoable */
   QTest::mouseClick(vflip, Qt::LeftButton);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), first.size());
   QVERIFY(meanDiff(image,
                    File::transformImage(first, File::Transform_vflip)) < 4);
   me.actionUndo->trigger();
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QVERIFY(meanDiff(image, first) < 4);

   QTest::mouseClick(hflip, Qt::LeftButton);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), first.size());
   QVERIFY(meanDiff(image,
                    File::transformImage(first, File::Transform_hflip)) < 4);
   me.actionUndo->trigger();
   QCOMPARE(page->_image.size(), first.size());
}

void TestPagewidget::testRotateThumbnailReady()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QToolButton *rright = page->findChild<QToolButton *>("rright");
   QVERIFY(rright);

   Pagemodel *pm = page->_pagemodel;

   /* mimic the displayed state: the shown page's thumbnail has already
      been generated and is ready (the background rescale runs on a
      timer, so force it here rather than waiting) */
   pm->ensurePage(0);
   pm->_pages[0]._rescale = true;
   pm->_pages[0].updatePixmap();
   QVERIFY(!pm->_pages[0]._pixmap.isNull());

   // rotate the shown page
   QTest::mouseClick(rright, Qt::LeftButton);

   /* the thumbnail must be ready in its rotated form straight away: it
      must not be invalidated (which would blank it until the next
      background rescale) and pixmap() must return a ready image */
   QVERIFY(pm->_pages[0]._valid);
   bool dodgy = true;
   QPixmap thumb = pm->_pages[0].pixmap(dodgy);
   QVERIFY(!thumb.isNull());
   QVERIFY(!dodgy);
}

/* Large page views are decoded on the background render thread; check
   that thumbnails still all become ready (the worker delivers them via a
   queued signal) and that nothing deadlocks. */
void TestPagewidget::testBackgroundRender()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);
   Pagemodel *pm = page->_pagemodel;

   // ask for a large view so the images are decoded off the GUI thread
   page->setPagesize(QSize(1600, 2200));
   // confirm this size really takes the background (full-decode) path
   QModelIndex stack;
   QVERIFY(page->getCurrentIndex(stack, true));
   QVERIFY2(model->imageNeedsDecode(stack, 0, QSize(1600, 2200)),
            "test size did not trigger the background render path");
   for (int i = 0; i < pm->_pages.size(); i++)
      pm->_pages[i]._rescale = true;
   pm->scheduleRescale();

   int n = pm->rowCount(QModelIndex());
   bool ready = QTest::qWaitFor([&] {
      for (int i = 0; i < n; i++)
         if (pm->_pages[i]._pixmap.isNull())
            return false;
      return true;
   }, 10000);
   QVERIFY2(ready, "background render did not produce all thumbnails");
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

/* The scan preview is built from bands as they arrive. With the fujitsu
   lower-edge detection the page starts with a guessed height and the
   real one arrives part way through, so the surface changes shape: what
   was painted must be carried over at the new scale and the rest stay
   hatched, or the page shows gaps and bands in the wrong place */
void TestPagewidget::testScanPreviewReshape()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);
   Pagemodel *pm = page->_pagemodel;

   pm->beginningScan();
   pm->beginningPage();
   int pagenum = pm->rowCount(QModelIndex()) - 1;
   Pageinfo &pi = pm->_pages[pagenum];
   QVERIFY(pi.scanning());

   // the first band arrives while the page is thought to be portrait
   QImage band(100, 20, QImage::Format_RGB32);
   band.fill(Qt::red);
   pm->newScaledImage(band, 0, pagenum, QSize(100, 130));
   QCOMPARE(pi._scan_image.size(), QSize(100, 130));
   QCOMPARE(pi._scan_painted, 20);
   QCOMPARE(pi._scan_image.pixelColor(50, 10), QColor(Qt::red));

   // the height is learnt: the page is landscape, so the surface shrinks
   band.fill(Qt::blue);
   pm->newScaledImage(band, 20, pagenum, QSize(100, 80));
   QCOMPARE(pi._scan_image.size(), QSize(100, 80));
   QCOMPARE(pi._scan_painted, 40);
   QCOMPARE(pi._scan_image.pixelColor(50, 10), QColor(Qt::red));
   QCOMPARE(pi._scan_image.pixelColor(50, 30), QColor(Qt::blue));

   // below the bands the surface is still the hatched placeholder
   const QImage &img = pi._scan_image;
   int white = 0, other = 0;
   for (int y = 45; y < 80; y++)
      for (int x = 0; x < 100; x++)
         {
         QColor col = img.pixelColor(x, y);
         if (col == QColor(Qt::white))
            white++;
         else
            {
            QVERIFY(col != QColor(Qt::red) && col != QColor(Qt::blue));
            other++;
            }
         }
   QVERIFY(white > 0 && other > 0);

   // the preview stands for the page at the current page size
   QCOMPARE(pi._size, pm->_pagesize);
   pm->endingScan();
}

void TestPagewidget::testPageOcrShown()
{
   Desktopmodel *model;
   Pagewidget *page;
   Mainwindow me;

   openTestStack(&me, model, page);

   QModelIndex ind;
   QVERIFY(page->getCurrentIndex(ind, true));
   File *file = model->getFile(ind);
   QVERIFY(file);
   QVERIFY(model->keepsOcr(file));

   // nothing has been read, so the stack's annotation is shown
   QTextEdit *edit = page->_ocr_edit;
   QVERIFY(!edit->isReadOnly());
   QCOMPARE(edit->toPlainText(), QString());

   /* read the stack's first two pages with an engine which names each
      page, as Read text does */
   Mainwidget *main = Mainwidget::singleton();
   OcrReader *reader = main->ocrReader();
   reader->setEngine([](QImage &image, OcrPage &ocr) {
      ocr = OcrPage();
      ocr.size = image.size();
      ocr.words << OcrWord{QRect(0, 0, 10, 10), "page", 90, 0, 0}
                << OcrWord{QRect(0, 20, 10, 10),
                           QString::number(image.width()), 90, 1, 0};
      return QString();
   });
   QSignalSpy idle(reader, &OcrReader::idle);
   QVERIFY(QMetaObject::invokeMethod(main->getDesktop(), "readText"));
   QVERIFY(idle.wait(20000));

   // the page being shown has its text shown as soon as it is read
   QImage image;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!file->getImage(0, false, image, size, trueSize, bpp, false));
   QString expect = QString("page\n%1").arg(image.width());
   QCOMPARE(edit->toPlainText(), expect);
   QVERIFY(edit->isReadOnly());

   // saving the stack's attributes leaves its OCR annotation alone
   QLineEdit *author = page->findChild<QLineEdit *>("author");
   QToolButton *save = page->findChild<QToolButton *>("save");
   QVERIFY(author && save);
   QTest::keyClicks(author, "Fred");
   QTest::mouseClick(save, Qt::LeftButton);
   QCOMPARE(model->data(ind, Desktopmodel::Role_ocr).toString(), QString());

   // a page which has not been read shows the annotation again
   QVERIFY(!file->putPageOcr(1, OcrPage()));
   Pageview *pageview = page->findChild<Pageview *>();
   QRect rect = pageview->visualRect(pageview->model()->index(1, 0));
   QTest::mouseClick(pageview->viewport(), Qt::LeftButton, Qt::NoModifier,
                     rect.center());
   QCOMPARE(page->getCurrentPage(), 1);
   QVERIFY(!edit->isReadOnly());
   QCOMPARE(edit->toPlainText(), QString());
}
