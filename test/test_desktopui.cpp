#include <functional>

#include <QAbstractItemModelTester>
#include <QClipboard>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QSemaphore>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPainter>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QTableWidget>
#include <QSettings>
#include <QToolButton>
#include <QDir>
#include <QDragEnterEvent>
#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest/QtTest>

#include "test.h"
#include "backgrounddialog.h"
#include "backgroundwork.h"
#include "namesdialog.h"

#include "desktopdelegate.h"
#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "ocrpage.h"
#include "ocrreader.h"
#include "dirmodel.h"
#include "filemax.h"
#include "imagetrim.h"
#include "mailer.h"
#include "utils.h"
#include "dirview.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "op.h"
#include "pagecover.h"
#include "pagemodel.h"
#include "pageview.h"
#include "paperstack.h"
#include "pagewidget.h"
#include "pagetools.h"
#include "pscan.h"
#include "searchserver.h"
#include "qxmlconfig.h"
#include <QScrollBar>
#include "clientconf.h"
#include "splitdialog.h"
#include "test_assistant.h"
#include "test_desktopui.h"
#include "test_fakescan.h"
#include "fakescan/fakescan.h"
#include "scansettings.h"

void TestDesktopUi::setupShown(Mainwindow *me, Desktopmodel *&model,
                               QModelIndex &repo_ind)
{
   auto path = setupRepo();
   Desktopwidget *desktop = me->getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   /* Show the window so that items have real positions and can be
      clicked, as a user would. How much of it the stacks get depends on
      the platform's fonts and style: on macOS at 1024 wide only the
      first stack is in view */
   me->resize(1600, 1000);
   me->show();
   QVERIFY(QTest::qWaitForWindowExposed(me));
   QTest::qWait(50);
}

QModelIndex TestDesktopUi::itemIndex(Desktopview *view, int row)
{
   return view->model()->index(row, 0, view->rootIndex());
}

void TestDesktopUi::clickItem(Desktopview *view, int row,
                              Qt::KeyboardModifiers modifiers)
{
   QModelIndex ind = itemIndex(view, row);
   QVERIFY(ind.isValid());

   /* the view finds a stack by the position the model gives it, which a
      folder just opened may not have yet */
   QTRY_VERIFY(view->visualRect(ind).isValid() &&
               view->indexAt(view->visualRect(ind).center()) == ind);
   QTest::mouseClick(view->viewport(), Qt::LeftButton, modifiers,
                     view->visualRect(ind).center());
}

/* The stacks are what the window is for, so on a first run, with no
   saved sizes, they must get a good share of it. The page pane's toolbar
   used to set its minimum width, which with the larger buttons of macOS
   was over half the window, leaving the stacks a strip */
void TestDesktopUi::testStacksGetRoom()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QList<int> sizes = desktop->sizes();

   qDebug() << "panes" << sizes << "window" << me.width();
   QVERIFY2(view->viewport()->width() > me.width() * 2 / 5,
            qPrintable(QString("the stacks have %1 of %2 pixels")
                       .arg(view->viewport()->width()).arg(me.width())));

   // and the page pane enough to show a page beside its list of pages
   QCOMPARE(sizes.size(), 3);
   QVERIFY2(sizes[2] > me.width() / 3,
            qPrintable(QString("the page pane has %1 of %2 pixels")
                       .arg(sizes[2]).arg(me.width())));
}

/* Each of the page pane's views has its own first-run layout: selecting,
   the stacks get most of the room and the pane shows a column of pages
   beside a large preview; moving pages about, the pane gets more, with a
   grid of pages beside a smaller preview; watching them scanned, it gets
   most of the window and shows only pages */
void TestDesktopUi::testFirstRunViews()
{
   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop();

   /* a window the size of a real one, so that no pane is held at its
      least width, which depends on the fonts to hand: in a small one the
      preview's is more than its share, and varies from one machine to
      the next */
   QVERIFY(!desktop->addDir(setupRepo()));
   me.resize(1600, 1000);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Pagewidget *page = desktop->getPagewidget();
   QSplitter *split = page->findChild<QSplitter *>("pages");
   QVERIFY(split);

   /* the preview's share is a range, since the list of pages beside it
      is held at its least width when that is more than its share */
   struct {
      const char *tool;
      int stacks, pane;    // share of the window, per cent, give or take
      int least, most;     // the preview's share of the pane, per cent
   } view[] = {
      { "moveMode", 34, 54, 35, 45 },
      { "scanMode", 14, 74, 0, 0 },
      { "selectMode", 45, 43, 70, 85 },
   };

   for (const auto &v : view) {
      QToolButton *tool = page->findChild<QToolButton *>(v.tool);
      QVERIFY(tool);
      tool->click();
      QCoreApplication::processEvents();

      QList<int> sizes = desktop->sizes();
      QList<int> inner = split->sizes();
      int w = desktop->width();
      int iw = inner[0] + inner[1];

      qDebug() << v.tool << "panes" << sizes << "page pane" << inner;
      QVERIFY2(qAbs(sizes[1] * 100 / w - v.stacks) <= 3
               && qAbs(sizes[2] * 100 / w - v.pane) <= 3,
               qPrintable(QString("%1: panes %2 %3 of %4").arg(v.tool)
                          .arg(sizes[1]).arg(sizes[2]).arg(w)));
      int preview = inner[1] * 100 / qMax(iw, 1);

      QVERIFY2(preview >= v.least && preview <= v.most,
               qPrintable(QString("%1: preview %2 of %3").arg(v.tool)
                          .arg(inner[1]).arg(iw)));
   }
}

/* With no size saved, the window should take most of the screen, however
   big it is, rather than a size fixed in the form which suits none */
void TestDesktopUi::testFirstWindowFitsScreen()
{
   QSettings().remove("mainwindow/geometry");
   Mainwindow me;
   QRect avail = me.screen()->availableGeometry();

   qDebug() << "window" << me.geometry() << "screen" << avail;
   QVERIFY(me.width() > avail.width() * 2 / 3);
   QVERIFY(me.height() > avail.height() * 2 / 3);
   QVERIFY(me.width() <= avail.width() && me.height() <= avail.height());
}

void TestDesktopUi::testClickSelectsStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Nothing should be selected to start with
   QVERIFY(!view->isSelection(Desktopview::SEL_at_least_one));

   // Click the first stack and check it becomes the selection
   clickItem(view, 0);
   QModelIndexList sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 1);
   QCOMPARE(model->data(sel[0], Qt::DisplayRole).toString(), "testfile");

   // Clicking on empty space should deselect everything
   QPoint space(view->viewport()->width() - 5,
                view->viewport()->height() - 5);
   QVERIFY(!view->indexAt(space).isValid());
   QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                     space);
   QVERIFY(!view->isSelection(Desktopview::SEL_at_least_one));
}

//! take focus from the stacks and give it back, in each way it can come
static void refocus(Desktopview *view)
{
   for (Qt::FocusReason reason : {Qt::ActiveWindowFocusReason,
                                  Qt::TabFocusReason,
                                  Qt::OtherFocusReason}) {
      QFocusEvent out(QEvent::FocusOut, reason);
      QFocusEvent in(QEvent::FocusIn, reason);

      QApplication::sendEvent(view, &out);
      QApplication::sendEvent(view, &in);
      QVERIFY(!view->currentIndex().isValid());
   }
}

/* Nothing must be picked by focus coming to the stacks, as when the scan
   panel opens and closes, whether on starting up or after moving to
   another folder: Qt makes the first item current on the first focus, and
   a current stack is shown as if it were clicked. Moving to another folder
   must leave nothing picked, too, rather than the stack picked in the last
   folder, which is no longer shown */
void TestDesktopUi::testFocusSelectsNothing()
{
   Mainwindow me;
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/deepfile.max"));
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/otherfile.max"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   me.resize(1600, 1000);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();
   QSignalSpy shown(view, SIGNAL(itemPreview(const QModelIndex &, int, bool)));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
   QVERIFY(!view->currentIndex().isValid());

   // on starting up
   refocus(view);
   QCOMPARE(shown.count(), 0);

   // pick a stack, then move to another folder
   clickItem(view, 0);
   QVERIFY(view->currentIndex().isValid());

   QModelIndex dir_ind = desktop->findDir(path + "/main/one");
   QVERIFY(dir_ind.isValid());
   Dirview *dir = desktop->_dir;
   dir->scrollTo(dir_ind);
   QTest::mouseClick(dir->viewport(), Qt::LeftButton, Qt::NoModifier,
                     dir->visualRect(dir_ind).center());
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
   QCOMPARE(view->model()->data(itemIndex(view, 0),
                                Qt::DisplayRole).toString(), "deepfile");
   QVERIFY(!view->currentIndex().isValid());
   QVERIFY(!view->isSelection(Desktopview::SEL_at_least_one));
   shown.clear();

   // open the scan panel and close it again, which moves focus about
   if (Fakescan::available()) {
      Scansettings settings(1, FAKESCAN_DEVICE);
      Mainwidget *main = Mainwidget::singleton();

      main->pscan();
      Pscan *pscan = main->findChild<Pscan *>();
      QVERIFY(pscan);
      QTest::qWait(50);
      pscan->hide();
      me.activateWindow();
      QTest::qWait(50);
      QVERIFY(!view->currentIndex().isValid());
   }
   refocus(view);
   QCOMPARE(shown.count(), 0);
   QVERIFY(!view->isSelection(Desktopview::SEL_at_least_one));

   // the keyboard still starts from a stack
   QTest::keyClick(view, Qt::Key_Right);
   QVERIFY(view->currentIndex().isValid());

   // and a click still shows the stack clicked
   shown.clear();
   clickItem(view, 1);
   QCOMPARE(shown.count(), 1);
}

void TestDesktopUi::testPressKeepsStackShown()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   Pagewidget *page = desktop->_page;
   QModelIndex shown;

   // The first stack is clicked and shown
   clickItem(view, 0);
   QTRY_VERIFY(page->getCurrentIndex(shown, true));
   QString first = model->data(shown, Desktopmodel::Role_filename).toString();

   // Pressing another selects it, but leaves the first one shown, so that
   // the other can be dragged into its pages
   QSignalSpy preview(view,
                      SIGNAL(itemPreview(const QModelIndex &, int, bool)));
   QModelIndex other = itemIndex(view, 1);
   QTRY_VERIFY(view->visualRect(other).isValid() &&
               view->indexAt(view->visualRect(other).center()) == other);
   QPoint pos = view->visualRect(other).center();

   QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
   QCOMPARE(view->currentIndex(), other);
   QTest::qWait(400);   // longer than the wait before a stack is shown
   QCOMPARE(preview.count(), 0);
   QVERIFY(page->getCurrentIndex(shown, true));
   QCOMPARE(model->data(shown, Desktopmodel::Role_filename).toString(),
            first);

   // Letting go shows it
   QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
   QVERIFY(preview.count() >= 1);
   QTRY_VERIFY(page->getCurrentIndex(shown, true)
               && model->data(shown, Desktopmodel::Role_filename).toString()
                  != first);
}

void TestDesktopUi::testCtrlClickMultiSelect()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   clickItem(view, 0);
   clickItem(view, 1, Qt::ControlModifier);

   QModelIndexList sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 2);

   // Ctrl-clicking the first again should deselect just that one
   clickItem(view, 0, Qt::ControlModifier);
   sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 1);
   QCOMPARE(model->data(sel[0], Qt::DisplayRole).toString(),
            "testpdf.pdf");

}

/* Stacks sit anywhere on the desk, and one with a long name is wider
   than the gap to the next. A shift-click must pick the stacks from the
   one picked before to the one clicked, in the order of the desk read
   like a page, without a neighbour on the other side whose name reaches
   under the first */
void TestDesktopUi::testShiftClickSelectsRange()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // make four stacks, the first with a long name
   clickItem(view, 0);
   desktop->_act_duplicate->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
   clickItem(view, 1);
   desktop->_act_duplicate->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 4);
   QVERIFY(view->model()->setData(itemIndex(view, 0),
              QString("a stack with a very long name indeed"),
              Qt::EditRole));

   /* three in a row, close enough for the long name to reach the second,
      and one below the second */
   const QPoint pos[] = {{25, 10}, {165, 10}, {305, 10}, {165, 260}};

   for (int i = 0; i < 4; i++)
      QVERIFY(view->model()->setData(itemIndex(view, i), pos[i],
                                     Desktopmodel::Role_position));
   QTest::qWait(200);
   QVERIFY(view->visualRect(itemIndex(view, 0))
              .intersects(view->visualRect(itemIndex(view, 1))));

   auto picked = [&] {
      QList<int> rows;

      for (int i = 0; i < 4; i++)
         if (view->selectionModel()->isSelected(itemIndex(view, i)))
            rows << i;
      return rows;
   };

   // from the second to the third leaves the first alone
   clickItem(view, 1);
   clickItem(view, 2, Qt::ShiftModifier);
   QCOMPARE(picked(), QList<int>({1, 2}));

   /* from the second to the one below it takes the rest of the row too,
      as that comes between them, but still not the first */
   clickItem(view, 1);
   clickItem(view, 3, Qt::ShiftModifier);
   QCOMPARE(picked(), QList<int>({1, 2, 3}));

   // and the same going back up
   clickItem(view, 3);
   clickItem(view, 1, Qt::ShiftModifier);
   QCOMPARE(picked(), QList<int>({1, 2, 3}));

   // a shift-click on the stack already picked leaves just that one
   clickItem(view, 2);
   clickItem(view, 2, Qt::ShiftModifier);
   QCOMPARE(picked(), QList<int>({2}));
}

void TestDesktopUi::testDoubleClickOpensStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);

   // The desktop view should be showing initially
   QCOMPARE(main->currentWidget(), (QWidget *)desktop);

   // Double-click the first stack: the page view should appear,
   // showing that stack
   QModelIndex ind = itemIndex(view, 0);
   QVERIFY(ind.isValid());
   QPoint centre = view->visualRect(ind).center();
   QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                     centre);
   QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      centre);

   Pagewidget *page = main->getPage();
   QTRY_COMPARE(main->currentWidget(), (QWidget *)page);

   QModelIndex shown;
   QVERIFY(page->getCurrentIndex(shown, true));
   QCOMPARE(model->data(shown, Qt::DisplayRole).toString(), "testfile");
}

void TestDesktopUi::testSwapViewAction()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);

   // With a stack selected, the swap action shows it in the page view
   clickItem(view, 0);
   me.actionSwap->trigger();
   QTRY_COMPARE(main->currentWidget(), (QWidget *)main->getPage());

   // Triggering it again returns to the desktop
   me.actionSwap->trigger();
   QTRY_COMPARE(main->currentWidget(), (QWidget *)desktop);
}

void TestDesktopUi::testToolbarOneScanButton()
{
   Mainwindow me;

   /* The toolbar has the button which opens the scan panel, but not one
      which scans at once: that is in the File menu and on F5 */
   QVERIFY(me.toolBar->actions().contains(me.actionPscan));
   QVERIFY(!me.toolBar->actions().contains(me.actionScango));
   QVERIFY(me.menuFile->actions().contains(me.actionScango));
   QCOMPARE(me.actionScango->shortcut(), QKeySequence(Qt::Key_F5));
   QCOMPARE(me.actionScango->text(), QString("Scan now"));
}

void TestDesktopUi::testToolbarStackNavigation()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   QPushButton *prev = desktop->findChild<QPushButton *>("prev");
   QPushButton *next = desktop->findChild<QPushButton *>("next");
   QVERIFY(prev);
   QVERIFY(next);

   clickItem(view, 0);

   // The next button moves the selection to the second stack
   QTest::mouseClick(next, Qt::LeftButton);
   QModelIndexList sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 1);
   QCOMPARE(model->data(sel[0], Qt::DisplayRole).toString(),
            "testpdf.pdf");

   // The prev button moves it back to the first
   QTest::mouseClick(prev, Qt::LeftButton);
   sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 1);
   QCOMPARE(model->data(sel[0], Qt::DisplayRole).toString(), "testfile");

}

void TestDesktopUi::testToolbarPageNavigation()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   QPushButton *p_prev = desktop->findChild<QPushButton *>("pPrev");
   QPushButton *p_next = desktop->findChild<QPushButton *>("pNext");
   QVERIFY(p_prev);
   QVERIFY(p_next);

   // Select the 5-page max stack
   clickItem(view, 0);

   QModelIndex src_ind = model->index(0, 0, repo_ind);
   QCOMPARE(model->data(src_ind, Desktopmodel::Role_pagenum).toInt(), 0);

   // The page-next button moves to the next page
   QTest::mouseClick(p_next, Qt::LeftButton);
   QCOMPARE(model->data(src_ind, Desktopmodel::Role_pagenum).toInt(), 1);

   QTest::mouseClick(p_next, Qt::LeftButton);
   QCOMPARE(model->data(src_ind, Desktopmodel::Role_pagenum).toInt(), 2);

   // The page-prev button moves back
   QTest::mouseClick(p_prev, Qt::LeftButton);
   QCOMPARE(model->data(src_ind, Desktopmodel::Role_pagenum).toInt(), 1);

   // Going back past the first page stays on the first page
   QTest::mouseClick(p_prev, Qt::LeftButton);
   QTest::mouseClick(p_prev, Qt::LeftButton);
   QCOMPARE(model->data(src_ind, Desktopmodel::Role_pagenum).toInt(), 0);

   // Going forward past the last page stays on the last page
   for (int i = 0; i < 10; i++)
      QTest::mouseClick(p_next, Qt::LeftButton);
   QCOMPARE(model->data(src_ind, Desktopmodel::Role_pagenum).toInt(), 4);

}

void TestDesktopUi::testStackNavigationWraps()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   QPushButton *prev = desktop->findChild<QPushButton *>("prev");
   QPushButton *next = desktop->findChild<QPushButton *>("next");
   QVERIFY(prev && next);

   // From the last stack, next wraps around to the first
   clickItem(view, 1);
   QTest::mouseClick(next, Qt::LeftButton);
   QModelIndexList sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 1);
   QCOMPARE(sel[0].row(), 0);

   // From the first stack, prev wraps around to the last
   QTest::mouseClick(prev, Qt::LeftButton);
   sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 1);
   QCOMPARE(sel[0].row(), 1);

}

void TestDesktopUi::testSelectAllAction()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   QVERIFY(!view->isSelection(Desktopview::SEL_at_least_one));

   me.actionSelectall->trigger();

   QModelIndexList sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 2);
}

void TestDesktopUi::testStackAndMenuUndo()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Duplicate the max stack using its context-menu action
   clickItem(view, 0);
   desktop->_act_duplicate->trigger();
   QCOMPARE(model->rowCount(repo_ind), 3);

   /* Select the original and the copy and combine them with the
      stack action. The copy is placed overlapping the original so a
      mouse click cannot select them reliably; select directly */
   view->setSelectionRange(0, 1);
   view->addSelectionRange(2, 1);
   QVERIFY(desktop->_act_stack->isEnabled());
   desktop->_act_stack->trigger();
   QCOMPARE(model->rowCount(repo_ind), 2);

   // The combined stack has the pages of both
   QModelIndex max_ind = model->index(0, 0, repo_ind);
   File *max = model->getFile(max_ind);
   QVERIFY(max);
   QCOMPARE(max->pagecount(), 10);

   // Undo from the Edit menu splits them apart again
   me.actionUndo->trigger();
   QCOMPARE(model->rowCount(repo_ind), 3);

   // Redo from the Edit menu combines them again
   me.actionRedo->trigger();
   QCOMPARE(model->rowCount(repo_ind), 2);

}

/* The stack pane can pick stacks out by part of their names: those found
   are marked and the rest drawn faintly, each where it is */
void TestDesktopUi::testViewFind()
{
   Mainwindow me;
   auto path = setupRepoWithExtra();
   Desktopwidget *desktop = me.getDesktop();

   QVERIFY(!desktop->addDir(path));
   me.resize(1600, 1000);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 4);
   view->selectionModel()->clear();

   // the stacks are in the order they are read: across, then down
   QModelIndexList order = view->inOrder();
   QCOMPARE(int(order.size()), 4);
   for (int i = 1; i < order.size(); i++)
      {
      QRect before = view->visualRect(order[i - 1]);
      QRect after = view->visualRect(order[i]);

      QVERIFY2(after.top() > before.bottom() || after.left() > before.left(),
               qPrintable(QString("stack %1").arg(i)));
      }

   // with nothing to find, none is found
   QVERIFY(view->find().isEmpty());
   QVERIFY(view->found().isEmpty());
   QVERIFY(!view->isFound(order[0]));

   /* a stack which is selected shows that and nothing else, so see that
      none is before each look; and bring the whole stack into view, as
      its name, which is what is marked, may be off the top */
   auto look = [&](const QModelIndex &ind) {
      view->selectionModel()->clear();

      // from the same place each time, so that a look does not depend on
      // the one before
      view->horizontalScrollBar()->setValue(0);
      view->verticalScrollBar()->setValue(0);
      view->scrollTo(ind);

      // as much of it as shows, which must include its name, at the top
      QRect rect = view->visualRect(ind);
      QRect shown = rect.intersected(view->viewport()->rect());

      return rect.top() >= 0 && shown.height() > rect.height() / 2
            && shown.width() > rect.width() / 2
         ? view->viewport()->grab(shown).toImage() : QImage();
   };
   QList<QImage> plain;
   for (const QModelIndex &ind : order)
      {
      plain << look(ind);
      QVERIFY2(!plain.last().isNull(), qPrintable(ind.data().toString()));
      }

   // stacks are found whatever the case, in the same order, and none goes
   view->setFind("PDF");
   QCOMPARE(view->find(), QString("PDF"));
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 4);

   QModelIndexList found = view->found();
   QCOMPARE(int(found.size()), 2);
   QVERIFY(order.indexOf(found[0]) < order.indexOf(found[1]));
   for (const QModelIndex &ind : order)
      QCOMPARE(view->isFound(ind),
               ind.data().toString().contains("pdf"));

   /* each stays where it is and looks different: one found has its name
      marked, and the rest are faint */
   QCOMPARE(view->inOrder(), order);
   for (int i = 0; i < order.size(); i++)
      QVERIFY2(look(order[i]) != plain[i],
               qPrintable(order[i].data().toString()));

   // one which is not found is paler than it was: nearer the background
   int at = -1;
   for (int i = 0; i < order.size(); i++)
      if (!view->isFound(order[i]))
         at = i;
   QVERIFY(at >= 0);
   auto contrast = [](const QImage &image) {
      int lo = 255, hi = 0;

      for (int y = 0; y < image.height(); y++)
         for (int x = 0; x < image.width(); x++)
            {
            int grey = qGray(image.pixel(x, y));

            lo = qMin(lo, grey);
            hi = qMax(hi, grey);
            }
      return hi - lo;
   };
   QVERIFY(contrast(look(order[at])) < contrast(plain[at]));

   // with nothing to find again, all are as they were
   view->setFind(QString());
   QVERIFY(view->found().isEmpty());
   for (int i = 0; i < order.size(); i++)
      QVERIFY2(look(order[i]) == plain[i],
               qPrintable(order[i].data().toString()));
}

void TestDesktopUi::testFilterStacks()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   QLineEdit *match = desktop->findChild<QLineEdit *>("match");
   QPushButton *cancel = desktop->findChild<QPushButton *>("cancelFilter");
   QVERIFY(match);
   QVERIFY(cancel);

   // Both stacks are visible to start with
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 2);

   // Typing a filter string shows only the matching stack
   QTest::keyClicks(match, "testpdf");
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QModelIndex ind = itemIndex(view, 0);
   QCOMPARE(view->model()->data(ind, Qt::DisplayRole).toString(),
            "testpdf.pdf");

   // A filter which matches nothing shows no stacks
   QTest::keyClicks(match, "zzz");
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 0);

   // The cancel button clears the filter and shows everything again
   QTest::mouseClick(cancel, Qt::LeftButton);
   QCOMPARE(match->text(), QString());
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 2);
}

void TestDesktopUi::testSearchAndLocate()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   // Put a stack in a subdirectory so the search has something to find
   // away from the repo root
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/findme.max"));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();

   // Search the whole repo for the stack, as the folder-search dialog
   // does once the user has entered a name
   desktop->startSearch(path, "findme");
   desktop->specialView("Showing the results of folder search");

   /* Only the matching stack should be shown. In particular, empty
      directories must not appear as phantom nameless stacks */
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QModelIndex ind = itemIndex(view, 0);
   QCOMPARE(view->model()->data(ind, Qt::DisplayRole).toString(),
            "findme");

   // Select the result and locate its folder: the view should change
   // to the stack's directory with the stack selected
   view->setSelectionRange(0, 1);
   desktop->locateFolder();

   QCOMPARE(desktop->getSelectedPath(),
            QString(path + "/main/one"));
   QTRY_COMPARE(view->getSelectedListSource().size(), 1);
   QModelIndex sel = view->getSelectedListSource()[0];
   QCOMPARE(model->data(sel, Desktopmodel::Role_filename).toString(),
            "findme.max");
}

// Keep some words with a page of a stack, as OCR would
static void sayOnPage(const QString &dir, const QString &fname, int pagenum,
                      const QString &text)
{
   Filemax max(dir, fname, nullptr);
   QVERIFY(!max.load());

   OcrPage ocr;
   int i = 0;
   ocr.size = QSize(100, 100);
   for (const QString &word : text.split(' '))
      ocr.words << OcrWord{QRect(10 * i++, 0, 8, 8), word, 90, 0, 0};
   QVERIFY(!max.putPageOcr(pagenum, ocr));
}

void TestDesktopUi::testSearchKey()
{
   Mainwindow me;
   auto path = setupRepo();

   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/findme.max"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));

   /* the dialog is modal, so fill it in once it is up: the name to look
      for, then OK.  Nothing becomes the active window on every platform
      the tests run on, so look for the dialog itself */
   bool shown = false;
   QTimer filler;

   connect(&filler, &QTimer::timeout, &me, [&shown, &filler]() {
      QDialog *diag = nullptr;

      for (QWidget *w : QApplication::topLevelWidgets())
         if (w->isVisible() && w->objectName() == "Search")
            diag = qobject_cast<QDialog *>(w);
      if (!diag)
         return;
      filler.stop();
      shown = true;
      diag->findChild<QLineEdit *>("stackName")->setText("findme");
      diag->accept();
   });
   filler.start(50);
   QTest::keyClick(&me, Qt::Key_S, Qt::ControlModifier);
   QTRY_VERIFY(shown);

   // only the stack found is shown
   Desktopview *view = desktop->getView();

   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QCOMPARE(view->model()->data(itemIndex(view, 0), Qt::DisplayRole)
            .toString(), QString("findme"));
}

void TestDesktopUi::testTextSearch()
{
   Mainwindow me;

   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/findme.max"));
   sayOnPage(path + "/main/one/", "findme.max", 3, "quarterly rates notice");
   sayOnPage(path + "/", "testfile.max", 0, "water rates");

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();

   // the stack with all the words is found, turned to the page with them
   QCOMPARE(desktop->startTextSearch(path, "rates noti"), 1);
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QModelIndex ind = itemIndex(view, 0);
   QCOMPARE(view->model()->data(ind, Qt::DisplayRole).toString(), "findme");
   QCOMPARE(view->model()->data(ind, Desktopmodel::Role_pagenum).toInt(), 3);
   QVERIFY(QFile::exists(path + "/.paperindex"));

   // a word on both is found on both
   QCOMPARE(desktop->startTextSearch(path, "rates"), 2);
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 2);

   // only under the folder searched
   QCOMPARE(desktop->startTextSearch(path + "/main", "rates"), 1);

   /* a PDF made from a stack which has been read has the stack's words,
      and is found with its preview, like any other stack */
   {
      Filemax max(path + "/", "testfile.max", nullptr);
      QVERIFY(!max.load());
      File *pdf = File::createFile(path + "/main/", "rates.pdf", nullptr,
                                   File::Type_pdf);
      QVERIFY(!pdf->create());
      Operation op("Convert", 0, 0);
      QVERIFY(!max.copyTo(pdf, 3, op, false));
      delete pdf;
   }
   QCOMPARE(desktop->startTextSearch(path + "/main", "water"), 1);
   ind = itemIndex(view, 0);
   QCOMPARE(view->model()->data(ind, Qt::DisplayRole).toString(), "rates.pdf");
   QPixmap shown = view->model()->data(ind, Desktopmodel::Role_pixmap)
                      .value<QPixmap>();
   QVERIFY(!shown.isNull());
   // the placeholder for a stack which cannot be shown is square
   QVERIFY2(shown.height() > shown.width(), qPrintable(
               QString("the PDF is shown as %1x%2, not as a page")
               .arg(shown.width()).arg(shown.height())));

   // and nothing is found for words on no page
   QCOMPARE(desktop->startTextSearch(path, "zebra"), 0);
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 0);
}

void TestDesktopUi::testSearchEscapeReturns()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Search for the max stack only
   desktop->startSearch(desktop->getSelectedPath(), "testfile");
   desktop->specialView("Showing the results of folder search");
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QVERIFY(desktop->_toolbar->searchEnabled());

   // Pressing Escape in the view returns to the normal directory view
   QTest::keyClick(view, Qt::Key_Escape);
   QVERIFY(!desktop->_toolbar->searchEnabled());
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
}

void TestDesktopUi::testRepeatedSearch()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   // Add a stack in a subdirectory so a search can find it
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/findme.max"));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();

   /* the model must say how its rows change, as they change, or the
      proxy over it loses track of them */
   QAbstractItemModelTester check(model, QAbstractItemModelTester::
                                  FailureReportingMode::QtTest);
   QAbstractItemModelTester check_proxy(view->model(),
                                        QAbstractItemModelTester::
                                        FailureReportingMode::QtTest);

   /* the first search creates the search desk; later searches re-use
      it. Each must show exactly the matching stacks, with no phantom
      or stale rows */
   desktop->startSearch(path, "testpdf");
   desktop->specialView("Showing the results of folder search");
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);

   desktop->startSearch(path, "test");
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 2);

   desktop->startSearch(path, "findme");
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QCOMPARE(view->model()->data(itemIndex(view, 0),
                                Qt::DisplayRole).toString(), "findme");

   // Escape still returns to the normal directory view
   QTest::keyClick(view, Qt::Key_Escape);
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
}

void TestDesktopUi::testDirTreeNavigation()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   // Put a stack in a subdirectory so there is something to show there
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/deepfile.max"));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();

   // The repo root shows its two stacks
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);

   // Click the subdirectory in the folder tree
   QModelIndex dir_ind = desktop->findDir(path + "/main/one");
   QVERIFY(dir_ind.isValid());
   Dirview *dir = desktop->_dir;
   dir->scrollTo(dir_ind);
   QRect rect = dir->visualRect(dir_ind);
   QVERIFY(rect.isValid());
   QTest::mouseClick(dir->viewport(), Qt::LeftButton, Qt::NoModifier,
                     rect.center());

   // The desktop should now show the stack in that directory
   QCOMPARE(desktop->getSelectedPath(), QString(path + "/main/one"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 1);
   QCOMPARE(view->model()->data(itemIndex(view, 0),
                                Qt::DisplayRole).toString(), "deepfile");

   // Clicking the repo root shows the original stacks again
   QModelIndex root_ind = desktop->findDir(path);
   QVERIFY(root_ind.isValid());
   dir->scrollTo(root_ind);
   QTest::mouseClick(dir->viewport(), Qt::LeftButton, Qt::NoModifier,
                     dir->visualRect(root_ind).center());
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
}

/* URLs which the email operations would open in a browser, and files
   they would give the mail program, are captured here instead, so tests
   never launch anything */
static QUrl s_opened_url;
static QString s_mailed_file;

//! email as @how, capturing what would be opened rather than opening it
static void captureEmail(Mailer::e_how how)
{
   Mailer::setHow(how);
   Mailer::url_capture = &s_opened_url;
   Mailer::file_capture = &s_mailed_file;
   s_opened_url = QUrl();
   s_mailed_file.clear();
}

void TestDesktopUi::testRotateActions()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // select the max stack and capture its first and last pages
   clickItem(view, 0);
   QModelIndex src_ind = model->index(0, 0, repo_ind);
   File *f = model->getFile(src_ind);
   QVERIFY(f);

   QImage orig, lastOrig, image;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!f->getImage(0, false, orig, size, trueSize, bpp, false));
   QVERIFY(!f->getImage(4, false, lastOrig, size, trueSize, bpp, false));

   // the desktop toolbar's rotate-right button turns every page on
   // its side
   QPushButton *rright = desktop->findChild<QPushButton *>("rright");
   QVERIFY(rright);
   QTest::mouseClick(rright, Qt::LeftButton);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.width(), orig.height());
   QCOMPARE(image.height(), orig.width());
   QVERIFY(!f->getImage(4, false, image, size, trueSize, bpp, false));
   QVERIFY(image.width() >= lastOrig.height());
   QVERIFY(image.height() <= lastOrig.width());

   // undo from the Edit menu rotates them all back
   me.actionUndo->trigger();
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), orig.size());
   QVERIFY(!f->getImage(4, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.height(), lastOrig.height());

   // the rotate-left button is wired up too
   QPushButton *rleft = desktop->findChild<QPushButton *>("rleft");
   QVERIFY(rleft);
   QTest::mouseClick(rleft, Qt::LeftButton);
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.width(), orig.height());
   me.actionUndo->trigger();

   // the flip menu action shares the same path
   me.actionVflip->trigger();
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), orig.size());
   me.actionUndo->trigger();
}

void TestDesktopUi::testRotatePageKeepsSelection()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // select the max stack and wait for the preview pane to show it
   clickItem(view, 0);
   Pagewidget *pagew = desktop->_page;
   QTRY_VERIFY(pagew->_pagemodel->rowCount(QModelIndex()) > 1);

   // click the second page's thumbnail in the preview pane
   QModelIndex page_ind = pagew->_pagemodel->index(1, 0, QModelIndex());
   pagew->_pageview->scrollTo(page_ind);
   QRect rect = pagew->_pageview->visualRect(page_ind);
   QVERIFY(rect.isValid());
   QTest::mouseClick(pagew->_pageview->viewport(), Qt::LeftButton,
                     Qt::NoModifier, rect.center());
   QCOMPARE(pagew->getCurrentPage(), 1);

   QModelIndex src_ind = model->index(0, 0, repo_ind);
   File *f = model->getFile(src_ind);
   QVERIFY(f);

   QImage orig, other, image;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!f->getImage(1, false, orig, size, trueSize, bpp, false));
   QVERIFY(!f->getImage(0, false, other, size, trueSize, bpp, false));

   /* rotating the page must not rebuild the whole page model (which
      reloads every thumbnail) and must keep the same page selected */
   QSignalSpy resetSpy(pagew->_pagemodel, SIGNAL(modelReset()));

   QToolButton *rright = pagew->findChild<QToolButton *>("rright");
   QVERIFY(rright);
   QTest::mouseClick(rright, Qt::LeftButton);

   // only the second page is transformed
   QVERIFY(!f->getImage(1, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.width(), orig.height());
   QVERIFY(!f->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), other.size());

   // the selection stays on page two and the view is not rebuilt
   QCOMPARE(pagew->getCurrentPage(), 1);
   QCOMPARE(resetSpy.count(), 0);

   me.actionUndo->trigger();
   QVERIFY(!f->getImage(1, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.size(), orig.size());
   QCOMPARE(pagew->getCurrentPage(), 1);
}

void TestDesktopUi::testRotatePageViewKeepsPreviewPage()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // select the stack: the desktop preview pane shows it on page one
   clickItem(view, 0);
   Pagewidget *preview = desktop->_page;
   QTRY_VERIFY(preview->_pagemodel->rowCount(QModelIndex()) > 1);
   QCOMPARE(preview->getCurrentPage(), 0);

   // open the same stack in the page view and select its second page
   me.actionSwap->trigger();
   Pagewidget *page = Mainwidget::singleton()->getPage();
   QTRY_COMPARE(Mainwidget::singleton()->currentWidget(), (QWidget *)page);
   QTRY_VERIFY(page->_pagemodel->rowCount(QModelIndex()) > 1);

   QModelIndex p2 = page->_pagemodel->index(1, 0, QModelIndex());
   page->_pageview->scrollTo(p2);
   QTest::mouseClick(page->_pageview->viewport(), Qt::LeftButton,
                     Qt::NoModifier, page->_pageview->visualRect(p2).center());
   QCOMPARE(page->getCurrentPage(), 1);

   // rotate the second page in the page view
   QToolButton *rright = page->findChild<QToolButton *>("rright");
   QVERIFY(rright);
   QTest::mouseClick(rright, Qt::LeftButton);

   /* the desktop preview pane is showing a different page, so it must
      stay on its own page rather than jumping to the rotated one */
   QCOMPARE(preview->getCurrentPage(), 0);
   QCOMPARE(page->getCurrentPage(), 1);
}

void TestDesktopUi::testRotatePreviewThumbnailReady()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // select the max stack; the preview pane on the right shows its pages
   clickItem(view, 0);
   Pagewidget *pagew = desktop->_page;
   QTRY_VERIFY(pagew->_pagemodel->rowCount(QModelIndex()) > 1);
   Pagemodel *pm = pagew->_pagemodel;

   /* mimic the displayed state: the shown page's thumbnail has been
      generated and is ready (the background rescale runs on a timer, so
      force it here rather than waiting) */
   pm->ensurePage(0);
   pm->_pages[0]._rescale = true;
   pm->_pages[0].updatePixmap();
   QVERIFY(!pm->_pages[0]._pixmap.isNull());

   // rotate the shown page using the preview pane's rotate button
   QToolButton *rright = pagew->findChild<QToolButton *>("rright");
   QVERIFY(rright);
   QTest::mouseClick(rright, Qt::LeftButton);

   /* the preview pane's thumbnail must show its rotated form straight
      away, rather than blanking until the next background rescale */
   QVERIFY(pm->_pages[0]._valid);
   bool dodgy = true;
   QPixmap thumb = pm->_pages[0].pixmap(dodgy);
   QVERIFY(!thumb.isNull());
   QVERIFY(!dodgy);
}

void TestDesktopUi::testRotateUnloadedStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   QString path;

   /* show the repo once so that the desk caches are written, as in a
      repository which has been used before */
   {
      Mainwindow first;
      Desktopmodel *m;
      QModelIndex ind;

      path = setupRepo();
      Desktopwidget *desktop = first.getDesktop();
      QVERIFY(!desktop->addDir(path));
      first.show();
      QVERIFY(QTest::qWaitForWindowExposed(&first));
      QTest::qWait(50);
      desktop->closing();
      Q_UNUSED(m);
      Q_UNUSED(ind);
   }

   /* a fresh session reads the caches, so the stack's File is not
      loaded and reports no pages: rotating must still work */
   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   Desktopview *view = desktop->getView();

   QModelIndex src_ind = model->index(0, 0, repo_ind);
   qDebug() << "pagecount before:"
            << model->getFile(src_ind)->pagecount();

   view->setSelectionRange(0, 1);
   me.actionRright->trigger();

   Filemax fresh(path + "/", "testfile.max", nullptr);
   QVERIFY(fresh.load() == nullptr);

   QImage image;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!fresh.getImage(0, false, image, size, trueSize, bpp, false));
   QVERIFY(image.width() > image.height());   // it turned on its side
}

void TestDesktopUi::testEmailSingleStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   captureEmail(Mailer::How_program);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Email the max stack as-is
   clickItem(view, 0);
   QVERIFY(desktop->_act_email->isEnabled());
   desktop->_act_email->trigger();

   // The file should be on the clipboard as a URL, ready to paste into
   // the compose window as an attachment
   const QMimeData *mime = QApplication::clipboard()->mimeData();
   QVERIFY(mime);
   QVERIFY(mime->hasUrls());
   QCOMPARE(mime->urls().size(), 1);

   QModelIndex src_ind = model->index(0, 0, repo_ind);
   QCOMPARE(mime->urls()[0].toLocalFile(),
            model->data(src_ind, Desktopmodel::Role_pathname).toString());

   // The original file must still exist for the email program to use
   QVERIFY(QFile::exists(mime->urls()[0].toLocalFile()));

   // the mail program is given the file, and no browser is opened
   QCOMPARE(s_mailed_file, mime->urls()[0].toLocalFile());
   QVERIFY(s_opened_url.isEmpty());
}

void TestDesktopUi::testEmailWithGmail()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   captureEmail(Mailer::How_gmail);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   clickItem(view, 0);
   desktop->_act_email->trigger();

   // the file is on the clipboard, to paste into Gmail's compose page
   const QMimeData *mime = QApplication::clipboard()->mimeData();
   QVERIFY(mime);
   QVERIFY(mime->hasUrls());
   QString file = mime->urls()[0].toLocalFile();

   QCOMPARE(s_opened_url.host(), QString("mail.google.com"));
   QCOMPARE(QUrlQuery(s_opened_url).queryItemValue("su"),
            QFileInfo(file).completeBaseName());
   QVERIFY(s_mailed_file.isEmpty());

   /* the note says how to paste, with the key as the platform shows it,
      such as Ctrl+V, and with any '+' encoded, as Gmail reads one left
      as it is as a space */
   QString paste = QKeySequence(QKeySequence::Paste)
                      .toString(QKeySequence::NativeText);

   QVERIFY(QUrlQuery(s_opened_url).queryItemValue("body", QUrl::FullyDecoded)
              .endsWith(paste));
   QVERIFY(!s_opened_url.query(QUrl::FullyEncoded).contains('+'));
}

void TestDesktopUi::testEmailMultipleStacksZips()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   captureEmail(Mailer::How_program);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Email both stacks: they should be packed into a single zip
   view->setSelectionRange(0, 2);
   desktop->_act_email->trigger();

   const QMimeData *mime = QApplication::clipboard()->mimeData();
   QVERIFY(mime);
   QVERIFY(mime->hasUrls());
   QCOMPARE(mime->urls().size(), 1);

   QString zip = mime->urls()[0].toLocalFile();
   QVERIFY(zip.endsWith(".zip"));
   QVERIFY(QFile::exists(zip));
   QVERIFY(QFileInfo(zip).size() > 0);

   QCOMPARE(s_mailed_file, zip);

   QFile::remove(zip);
}

void TestDesktopUi::testEmailAsPdfConverts()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   captureEmail(Mailer::How_program);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Email the max stack as PDF: a converted temporary file is sent
   clickItem(view, 0);
   desktop->_act_email_pdf->trigger();

   const QMimeData *mime = QApplication::clipboard()->mimeData();
   QVERIFY(mime);
   QVERIFY(mime->hasUrls());
   QCOMPARE(mime->urls().size(), 1);

   QString pdf = mime->urls()[0].toLocalFile();
   QVERIFY(pdf.endsWith(".pdf"));
   QVERIFY(QFile::exists(pdf));

   // Check it really is a PDF
   QFile fil(pdf);
   QVERIFY(fil.open(QIODevice::ReadOnly));
   QCOMPARE(fil.read(4), QByteArray("%PDF"));
   fil.close();

   QCOMPARE(s_mailed_file, pdf);

   // it goes by the stack's name
   QModelIndex src_ind = model->index(0, 0, repo_ind);
   QString name = QFileInfo(model->data(src_ind, Desktopmodel::Role_filename)
                               .toString()).completeBaseName() + ".pdf";

   QCOMPARE(QFileInfo(pdf).fileName(), name);

   // emailed again, it goes by the same name, though the first is there
   clickItem(view, 0);
   desktop->_act_email_pdf->trigger();
   mime = QApplication::clipboard()->mimeData();
   QVERIFY(mime && mime->hasUrls());
   QString again = mime->urls()[0].toLocalFile();

   QVERIFY(again != pdf);
   QCOMPARE(QFileInfo(again).fileName(), name);
   QCOMPARE(s_mailed_file, again);

   for (const QString &path : {pdf, again}) {
      QFile::remove(path);
      if (QFileInfo(path).dir().dirName().startsWith("paperman-"))
         QDir().rmdir(QFileInfo(path).absolutePath());
   }
}

void TestDesktopUi::testCopyAsPdf()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   captureEmail(Mailer::How_program);

   // start with an empty clipboard so we can be sure copy fills it
   QApplication::clipboard()->clear();

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // copy the max stack: a converted PDF lands on the clipboard
   clickItem(view, 0);
   QVERIFY(desktop->_act_copy->isEnabled());
   QSignalSpy statusSpy(desktop, SIGNAL(newContents(QString)));
   desktop->_act_copy->trigger();

   // the status bar is told what was copied and how to paste it
   QCOMPARE(statusSpy.count(), 1);
   QString status = statusSpy.takeFirst().at(0).toString();
   QVERIFY(status.contains("PDF"));
   QVERIFY(status.contains("Ctrl+V"));

   const QMimeData *mime = QApplication::clipboard()->mimeData();
   QVERIFY(mime);
   QVERIFY(mime->hasUrls());
   QCOMPARE(mime->urls().size(), 1);

   QString pdf = mime->urls()[0].toLocalFile();
   QVERIFY(pdf.endsWith(".pdf"));
   QVERIFY(QFile::exists(pdf));

   // it really is a PDF, ready to paste into a compose window
   QFile fil(pdf);
   QVERIFY(fil.open(QIODevice::ReadOnly));
   QCOMPARE(fil.read(4), QByteArray("%PDF"));
   fil.close();

   // the GNOME copy marker is present so file managers paste the file
   // rather than ignoring a bare uri-list
   QVERIFY(mime->hasFormat("x-special/gnome-copied-files"));
   QByteArray gnome = mime->data("x-special/gnome-copied-files");
   QVERIFY(gnome.startsWith("copy\n"));
   QVERIFY(gnome.contains(QUrl::fromLocalFile(pdf).toEncoded()));

   // copying does not start a message, unlike emailing
   QVERIFY(s_opened_url.isEmpty());
   QVERIFY(s_mailed_file.isEmpty());

   QFile::remove(pdf);
}

/* Press Ctrl-M in @me and fill in the dialog: the folder @folder, and
   whether to @copy rather than move. Return true if the dialog came up */
static bool moveByKey(Mainwindow *me, const QString &folder, bool copy)
{
   bool shown = false;
   QTimer filler;

   /* the dialog is modal, so fill it in once it is up; nothing becomes
      the active window on every platform the tests run on, so look for
      the dialog itself */
   QObject::connect(&filler, &QTimer::timeout, me,
                    [&shown, &filler, folder, copy]() {
      QDialog *diag = nullptr;

      for (QWidget *w : QApplication::topLevelWidgets())
         if (w->isVisible() && w->objectName() == "Move")
            diag = qobject_cast<QDialog *>(w);
      if (!diag)
         return;
      filler.stop();
      shown = true;
      diag->findChild<QCheckBox *>("copy")->setChecked(copy);
      diag->findChild<QLineEdit *>("folderName")->setText(folder);
      QMetaObject::invokeMethod(diag, "accept");
   });
   filler.start(50);
   QTest::keyClick(me, Qt::Key_M, Qt::ControlModifier);
   return QTest::qWaitFor([&shown] { return shown; }, 5000);
}

/* Ctrl-F brings up a box in which a few letters of a folder's name find it,
   and Return goes there */
void TestDesktopUi::testGoToFolder()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   QString path = desktop->getSelectedPath();
   bool typed = false;
   QTimer typist;

   // the dialog is modal, so type into it once it is up
   QObject::connect(&typist, &QTimer::timeout, &me, [&]() {
      QDialog *diag = nullptr;

      for (QWidget *w : QApplication::topLevelWidgets())
         if (w->isVisible() && w->objectName() == "Gotofolder")
            diag = qobject_cast<QDialog *>(w);
      if (!diag)
         return;
      typist.stop();
      typed = true;

      QLineEdit *name = diag->findChild<QLineEdit *>("folderName");

      QTest::keyClicks(name, "mai");

      // the list of folders which match has the keys then
      QWidget *focus = diag->focusWidget();

      QTest::keyClick(focus ? focus : name, Qt::Key_Return);
   });
   typist.start(50);
   QTest::keyClick(&me, Qt::Key_F, Qt::ControlModifier);
   QVERIFY(QTest::qWaitFor([&typed] { return typed; }, 5000));
   QTRY_COMPARE(QDir(desktop->getSelectedPath()).absolutePath(),
                QDir(path + "/main").absolutePath());

   // F4 does it too, as in the pscan dialogue
   QVERIFY(me.actionGotoFolder->shortcuts().contains(QKeySequence(Qt::Key_F4)));
}

void TestDesktopUi::testMoveKey()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   // choose the stack and press Ctrl-M, then name the folder
   clickItem(view, 0);
   QVERIFY(moveByKey(&me, "main", false));

   // it moves there, and leaves the view
   QTRY_VERIFY(QFile::exists(path + "/main/testfile.max"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 1);

   // undo brings it back
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QVERIFY(!QFile::exists(path + "/main/testfile.max"));
}

void TestDesktopUi::testCopyKey()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   // a copy goes to the folder, and the stack stays
   clickItem(view, 0);
   QVERIFY(moveByKey(&me, "main", true));
   QTRY_VERIFY(QFile::exists(path + "/main/testfile.max"));
   QVERIFY(QFile::exists(path + "/testfile.max"));
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 2);

   // undo takes the copy away again
   me.actionUndo->trigger();
   QTRY_VERIFY(!QFile::exists(path + "/main/testfile.max"));
   QVERIFY(QFile::exists(path + "/testfile.max"));
}

void TestDesktopUi::testMoveFoundKey()
{
   Mainwindow me;
   auto path = setupRepo();

   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/main/one/findme.max"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));

   Desktopview *view = desktop->getView();

   desktop->startSearch(path, "findme");
   desktop->specialView("Showing the results of folder search");
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 1);

   // the stack found goes from its folder to the one named
   view->setSelectionRange(0, 1);
   QTRY_VERIFY(desktop->_act_move->isEnabled());
   QVERIFY(moveByKey(&me, "two", false));
   QTRY_VERIFY(QFile::exists(path + "/main/two/findme.max"));
   QVERIFY(!QFile::exists(path + "/main/one/findme.max"));
}

void TestDesktopUi::testDragDropToFolder()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   // Pick up the max stack: build the drag data just as the view does
   clickItem(view, 0);
   QMimeData *mime =
      view->model()->mimeData(view->selectionModel()->selectedIndexes());
   QVERIFY(mime);
   QVERIFY(mime->hasFormat("application/vnd.text.list"));

   /* Drop it on the 'main' folder in the directory tree. Row and
      column are -1 for a drop directly onto an item */
   QModelIndex dir_ind = desktop->findDir(path + "/main");
   QVERIFY(dir_ind.isValid());
   QVERIFY(desktop->_dir_proxy->dropMimeData(mime, Qt::MoveAction, -1, -1,
                                             dir_ind));

   // The file should move on disk and disappear from the view
   QTRY_VERIFY(QFile::exists(path + "/main/testfile.max"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 1);

   // Undo from the Edit menu brings it back
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QVERIFY(!QFile::exists(path + "/main/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);

   delete mime;
}

/* Drop @p mime on @p view at @p pos, as a drag from another program does

   \returns true if the view took the drop */
static bool dropOn(QAbstractItemView *view, const QMimeData *mime,
                   QPoint pos)
{
   QDragEnterEvent enter(pos, Qt::CopyAction | Qt::MoveAction, mime,
                         Qt::LeftButton, Qt::NoModifier);

   QApplication::sendEvent(view->viewport(), &enter);
   if (!enter.isAccepted())
      return false;

   QDropEvent drop(QPointF(pos), Qt::CopyAction | Qt::MoveAction, mime,
                   Qt::LeftButton, Qt::NoModifier);

   QApplication::sendEvent(view->viewport(), &drop);
   return drop.isAccepted();
}

void TestDesktopUi::testDropFiles()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();
   int rows = view->model()->rowCount(view->rootIndex());

   // A picture somewhere else, and a file which is not shown as a stack
   QTemporaryDir other;
   QString picture = other.path() + "/picture.jpg";
   QString notes = other.path() + "/notes.txt";
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", picture));
   QFile file(notes);
   QVERIFY(file.open(QIODevice::WriteOnly));
   file.write("some notes");
   file.close();

   // The notes alone are refused
   QMimeData mime;
   mime.setUrls({QUrl::fromLocalFile(notes)});
   QVERIFY(!dropOn(view, &mime, QPoint(300, 200)));
   QCOMPARE(view->model()->rowCount(view->rootIndex()), rows);

   // With the picture, the picture is copied in and the notes left out
   mime.setUrls({QUrl::fromLocalFile(picture), QUrl::fromLocalFile(notes)});
   QVERIFY(dropOn(view, &mime, QPoint(300, 200)));
   QTRY_VERIFY(QFile::exists(path + "/picture.jpg"));
   QVERIFY(QFile::exists(picture));
   QVERIFY(!QFile::exists(path + "/notes.txt"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), rows + 1);

   // It is a stack, where it was dropped
   QModelIndex ind = model->index("picture.jpg", repo_ind);
   QVERIFY(ind.isValid());
   QPoint scrolled(view->horizontalScrollBar()->value(),
                   view->verticalScrollBar()->value());
   QCOMPARE(model->getFile(ind)->pos(), QPoint(300, 200) + scrolled);

   // Another of the same name gets a name of its own
   QVERIFY(dropOn(view, &mime, QPoint(400, 250)));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), rows + 2);
   QCOMPARE(QDir(path).entryList({"picture*.jpg"}).size(), 2);

   // Undo takes each away again, and leaves the picture where it came from
   me.actionUndo->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), rows + 1);
   me.actionUndo->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), rows);
   QTRY_VERIFY(!QFile::exists(path + "/picture.jpg"));
   QVERIFY(QFile::exists(picture));
}

void TestDesktopUi::testDropFilesOnStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();
   int rows = view->model()->rowCount(view->rootIndex());

   QTemporaryDir other;
   QString picture = other.path() + "/picture.jpg";
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", picture));

   QMimeData mime;
   mime.setUrls({QUrl::fromLocalFile(picture)});

   // Dropped on each stack, the picture becomes a page of it
   for (int row = 0; row < rows; row++) {
      QModelIndex ind = itemIndex(view, row);
      QTRY_VERIFY(view->visualRect(ind).isValid() &&
                  view->indexAt(view->visualRect(ind).center()) == ind);
      QPoint pos = view->visualRect(ind).center();
      QModelIndex src = ind;
      desktop->getModelconv()->indexToSource(view->model(), src);
      QString name = model->getFile(src)->filename();
      int pages = model->getFile(src)->pagecount();

      // Another stack is the current one
      clickItem(view, row ? 0 : rows - 1);
      QVERIFY(view->currentIndex() != ind);

      QVERIFY2(dropOn(view, &mime, pos), qPrintable(name));
      QTRY_COMPARE(model->getFile(model->index(name, repo_ind))->pagecount(),
                   pages + 1);
      QCOMPARE(view->model()->rowCount(view->rootIndex()), rows);

      // The stack it went into is now the one selected and shown
      QModelIndex joined = model->index(name, repo_ind);
      desktop->getModelconv()->indexToProxy(model, joined);
      QTRY_COMPARE(view->currentIndex(), joined);
      QCOMPARE(view->selectionModel()->selectedIndexes(),
               QModelIndexList() << joined);
      QVERIFY(!QFile::exists(path + "/picture.jpg"));
      QVERIFY(QFile::exists(picture));

      // Undo takes the page out and leaves no stack behind
      me.actionUndo->trigger();
      QTRY_COMPARE(model->getFile(model->index(name, repo_ind))->pagecount(),
                   pages);
      QCOMPARE(view->model()->rowCount(view->rootIndex()), rows);
      QVERIFY(!QFile::exists(path + "/picture.jpg"));
   }
}

void TestDesktopUi::testStackSelectsDest()
{
   Mainwindow me;

   // A picture beside the max and PDF stacks
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", path + "/picture.jpg"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));

   Desktopview *view = desktop->getView();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);

   // The picture is the one selected, as it is when it is dragged
   QModelIndex pic = model->index("picture.jpg", repo_ind);
   QModelIndex shown = pic;
   desktop->getModelconv()->indexToProxy(model, shown);
   view->setSelectionRange(shown.row(), 1);
   QCOMPARE(view->currentIndex(), shown);

   // Stacked onto another, it leaves that one selected, not a neighbour
   QModelIndex dest = model->index("testfile.max", repo_ind);
   QModelIndexList list;

   list << pic;
   QVERIFY(!model->stackItems(dest, list, nullptr));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
   dest = model->index("testfile.max", repo_ind);
   desktop->getModelconv()->indexToProxy(model, dest);
   QTRY_COMPARE(view->currentIndex(), dest);
   QCOMPARE(view->selectionModel()->selectedIndexes(),
            QModelIndexList() << dest);
}

void TestDesktopUi::testDragDropGroupToFolder()
{
   Mainwindow me;

   // a third stack so the group spans all three file types
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg",
                       path + "/colour_plasma.jpg"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   Desktopview *view = desktop->getView();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);

   /* select the whole group: click the first, then extend through the
      selection model - item geometry can overlap, which makes
      ctrl-clicks unreliable */
   clickItem(view, 0);
   for (int row = 1; row <= 2; row++)
      view->selectionModel()->select(itemIndex(view, row),
                                     QItemSelectionModel::Select);
   QCOMPARE(view->selectionModel()->selectedIndexes().size(), 3);

   // remember where one member sits, to check undo restores layout
   QModelIndex maxind = model->index("testfile.max", repo_ind);
   QVERIFY(maxind.isValid());
   QPoint oldpos =
      model->data(maxind, Desktopmodel::Role_position).toPoint();

   // build the drag data from the multi-selection, as the view would
   QMimeData *mime =
      view->model()->mimeData(view->selectionModel()->selectedIndexes());
   QVERIFY(mime);

   // drop the group on the 'main' folder in the directory tree
   QModelIndex dir_ind = desktop->findDir(path + "/main");
   QVERIFY(dir_ind.isValid());
   QVERIFY(desktop->_dir_proxy->dropMimeData(mime, Qt::MoveAction, -1, -1,
                                             dir_ind));

   // every member has moved on disk and left the view
   QTRY_VERIFY(QFile::exists(path + "/main/testfile.max"));
   QVERIFY(QFile::exists(path + "/main/testpdf.pdf"));
   QVERIFY(QFile::exists(path + "/main/colour_plasma.jpg"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));
   QVERIFY(!QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(!QFile::exists(path + "/colour_plasma.jpg"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 0);

   // a single undo brings the whole group back...
   me.actionUndo->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
   QVERIFY(QFile::exists(path + "/testfile.max"));
   QVERIFY(QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(QFile::exists(path + "/colour_plasma.jpg"));
   QVERIFY(!QFile::exists(path + "/main/testfile.max"));

   // ...with the remembered stack at its old spot on the desk
   maxind = model->index("testfile.max", repo_ind);
   QVERIFY(maxind.isValid());
   QCOMPARE(model->data(maxind, Desktopmodel::Role_position).toPoint(),
            oldpos);

   // and one redo moves the group out again
   me.actionRedo->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 0);
   QVERIFY(QFile::exists(path + "/main/testfile.max"));
   QVERIFY(QFile::exists(path + "/main/testpdf.pdf"));
   QVERIFY(QFile::exists(path + "/main/colour_plasma.jpg"));

   delete mime;
}


void TestDesktopUi::testRemoteGroupMoveViaUi()
{
   Mainwindow me;

   /* serve the repo over HTTP; the same directory doubles as the
      server's disk so the moves can be checked there */
   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg",
                       path + "/colour_plasma.jpg"));

   SearchServer server(path, 9877);
   QVERIFY(server.start());
   QTest::qWait(100);
   QUrl url("http://localhost:9877");
   QString repo = QFileInfo(path).fileName();
   QString root = url.toString() + "/" + repo;

   Desktopwidget *desktop = me.getDesktop();
   bool attached = false;
   QString err;
   desktop->addRemoteServer(url, [&](const QString &error) {
      err = error;
      attached = true;
   });
   QTRY_VERIFY(attached);
   QVERIFY2(err.isEmpty(), qPrintable(err));

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   // click the remote repository in the folder tree to show its desk
   QModelIndex repo_dir = desktop->findDir(root);
   QVERIFY(repo_dir.isValid());
   Dirview *dir = desktop->_dir;
   dir->scrollTo(repo_dir);
   QRect rect = dir->visualRect(repo_dir);
   QVERIFY(rect.isValid());
   QTest::mouseClick(dir->viewport(), Qt::LeftButton, Qt::NoModifier,
                     rect.center());
   QTRY_COMPARE(desktop->getSelectedPath(), root);

   Desktopview *view = desktop->getView();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
   QTest::qWait(50);   // let the item layout settle before clicking

   // the remote folder tree fills in asynchronously; expand the repo
   // so its 'main' subdirectory arrives
   dir->expand(repo_dir);
   QTRY_VERIFY(desktop->findDir(root + "/main").isValid());

   /* select the whole group through the selection model: the async
      thumbnail fetches resize items as they arrive, so a mouse click
      can land on a neighbour mid-layout */
   view->selectionModel()->clear();
   for (int row = 0; row <= 2; row++)
      view->selectionModel()->select(itemIndex(view, row),
                                     QItemSelectionModel::Select);
   QCOMPARE(view->selectionModel()->selectedIndexes().size(), 3);

   QMimeData *mime =
      view->model()->mimeData(view->selectionModel()->selectedIndexes());
   QVERIFY(mime);

   // drop the group on the remote 'main' folder
   QModelIndex dir_ind = desktop->findDir(root + "/main");
   QVERIFY(desktop->_dir_proxy->dropMimeData(mime, Qt::MoveAction, -1, -1,
                                             dir_ind));

   // the server's files have moved, one after another
   QTRY_VERIFY(QFile::exists(path + "/main/testfile.max"));
   QTRY_VERIFY(QFile::exists(path + "/main/testpdf.pdf"));
   QTRY_VERIFY(QFile::exists(path + "/main/colour_plasma.jpg"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 0);

   // one undo moves the whole group back on the server
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QTRY_VERIFY(QFile::exists(path + "/colour_plasma.jpg"));
   QVERIFY(!QFile::exists(path + "/main/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);

   delete mime;
   server.stop();
}


/* A stack dragged only a little way ends up over where it was. The list
   view takes it for the thing dropped on and moves nothing, so the move
   is done by how far the pointer has come */
void TestDesktopUi::testShortDrag()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopview *view = me.getDesktop()->getView();
   QModelIndex first = itemIndex(view, 0), second = itemIndex(view, 1);
   auto place = [&](const QModelIndex &ind) {
      return view->model()->data(ind, Desktopmodel::Role_position).toPoint();
   };

   // press on a stack, then let go a little way off, still over it
   QPoint centre = view->visualRect(first).center();
   QPoint was = place(first), other = place(second);

   QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                     centre);
   QVERIFY(view->moveDraggedTo(centre + QPoint(12, 7)));
   QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                       centre + QPoint(12, 7));
   QCOMPARE(place(first), was + QPoint(12, 7));
   QTRY_COMPARE(view->visualRect(first).topLeft(),
                view->visualRect(second).topLeft() - other
                   + was + QPoint(12, 7));
   QCOMPARE(place(second), other);

   // it is a step which can be undone
   me.actionUndo->trigger();
   QCOMPARE(place(first), was);

   // a drag which goes nowhere moves nothing
   QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                     view->visualRect(first).center());
   QVERIFY(!view->moveDraggedTo(view->visualRect(first).center()));
   QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                       view->visualRect(first).center());
   QCOMPARE(place(first), was);

   // several stacks selected move together, and come back together
   view->setSelectionRange(0, 2);
   QVERIFY(view->moveSelectedBy(QPoint(-5, 20)));
   QCOMPARE(place(first), QPoint(qMax(0, was.x() - 5), was.y() + 20));
   QCOMPARE(place(second), QPoint(qMax(0, other.x() - 5), other.y() + 20));
   me.actionUndo->trigger();
   QCOMPARE(place(first), was);
   QCOMPARE(place(second), other);

   // no stack goes off the top or the left of the desk
   QVERIFY(view->moveSelectedBy(QPoint(-100000, -100000)));
   QCOMPARE(place(first), QPoint(0, 0));
   QCOMPARE(place(second), QPoint(0, 0));
   me.actionUndo->trigger();
   QCOMPARE(place(first), was);

   // and with nothing selected there is nothing to move
   view->selectionModel()->clear();
   QVERIFY(!view->moveSelectedBy(QPoint(10, 10)));
}

/* Alt with an arrow key nudges the stacks selected across the desk, by
   less than can be dragged. A run of nudges is one step to undo. Turning
   the pages of a stack has Shift as well, to leave these keys free */
void TestDesktopUi::testNudge()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopview *view = me.getDesktop()->getView();
   QModelIndex first = itemIndex(view, 0), second = itemIndex(view, 1);
   int step = Desktopview::nudge_step;
   auto place = [&](const QModelIndex &ind) {
      return view->model()->data(ind, Desktopmodel::Role_position).toPoint();
   };

   // put a stack well away from the edges, and select it
   clickItem(view, 0);
   QVERIFY(view->moveSelectedBy(QPoint(200, 200) - place(first)));
   QPoint was = place(first), other = place(second);
   QCOMPARE(was, QPoint(200, 200));

   // each key moves it a step that way, and the other stack stays put
   QTest::keyClick(view, Qt::Key_Right, Qt::AltModifier);
   QCOMPARE(place(first), was + QPoint(step, 0));
   QTest::keyClick(view, Qt::Key_Down, Qt::AltModifier);
   QCOMPARE(place(first), was + QPoint(step, step));
   QTest::keyClick(view, Qt::Key_Left, Qt::AltModifier);
   QTest::keyClick(view, Qt::Key_Left, Qt::AltModifier);
   QCOMPARE(place(first), was + QPoint(-step, step));
   QTest::keyClick(view, Qt::Key_Up, Qt::AltModifier);
   QTest::keyClick(view, Qt::Key_Up, Qt::AltModifier);
   QCOMPARE(place(first), was + QPoint(-step, -step));
   QCOMPARE(place(second), other);
   QTRY_COMPARE(view->visualRect(first).topLeft() - view->visualRect(second).topLeft(),
                place(first) - place(second));

   // the arrow keys count as keypad keys on some systems
   QTest::keyClick(view, Qt::Key_Right, Qt::AltModifier | Qt::KeypadModifier);
   QCOMPARE(place(first), was + QPoint(0, -step));

   // the whole run is undone in one go, back to where it was put
   me.actionUndo->trigger();
   QCOMPARE(place(first), was);

   // a nudge after something else is a step of its own
   QTest::keyClick(view, Qt::Key_Right, Qt::AltModifier);
   QVERIFY(view->moveSelectedBy(QPoint(50, 0)));
   QTest::keyClick(view, Qt::Key_Right, Qt::AltModifier);
   QCOMPARE(place(first), was + QPoint(50 + 2 * step, 0));
   me.actionUndo->trigger();
   QCOMPARE(place(first), was + QPoint(50 + step, 0));
   me.actionUndo->trigger();
   QCOMPARE(place(first), was + QPoint(step, 0));
   me.actionUndo->trigger();
   QCOMPARE(place(first), was);

   // several stacks selected are nudged together
   view->setSelectionRange(0, 2);
   QTest::keyClick(view, Qt::Key_Down, Qt::AltModifier);
   QCOMPARE(place(first), was + QPoint(0, step));
   QCOMPARE(place(second), other + QPoint(0, step));
   me.actionUndo->trigger();
   QCOMPARE(place(first), was);
   QCOMPARE(place(second), other);

   // an arrow key alone still moves from stack to stack
   clickItem(view, 0);
   QCOMPARE(view->currentIndex(), first);
   bool first_is_left = view->visualRect(first).left()
                        < view->visualRect(second).left();
   QTest::keyClick(view, first_is_left ? Qt::Key_Right : Qt::Key_Left);
   QCOMPARE(place(first), was);

   // with nothing selected a nudge does nothing
   view->selectionModel()->clear();
   QTest::keyClick(view, Qt::Key_Right, Qt::AltModifier);
   QCOMPARE(place(first), was);
   QCOMPARE(place(second), other);

   // turning a stack's pages has moved to Alt+Shift with the same keys
   QCOMPARE(me.actionRleft->shortcut(), QKeySequence("Alt+Shift+Left"));
   QCOMPARE(me.actionRright->shortcut(), QKeySequence("Alt+Shift+Right"));
}

void TestDesktopUi::testMoveStackOnDesk()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   // pick the stack up with a click, as a user starting a drag would
   clickItem(view, 0);
   QModelIndexList selected = view->selectionModel()->selectedIndexes();
   QCOMPARE(selected.size(), 1);
   QModelIndex vind = selected[0];

   QModelIndex ind = model->index("testfile.max", repo_ind);
   QVERIFY(ind.isValid());
   QPoint oldpos = model->data(ind, Desktopmodel::Role_position).toPoint();
   QPoint newpos = oldpos + QPoint(150, 80);

   /* complete the gesture the way Desktopview::dropEvent does when a
      drag ends over empty desk space: hand the model the selection's
      old and new positions */
   QModelIndexList list;
   QList<QPoint> oldlist, plist;
   list << ind;
   oldlist << oldpos;
   plist << newpos;
   model->move(list, repo_ind, oldlist, plist);

   // the model and the view both show the stack at its new place
   QCOMPARE(model->data(ind, Desktopmodel::Role_position).toPoint(),
            newpos);
   QTRY_COMPARE(view->visualRect(vind).topLeft(), newpos);

   // the new position reaches the desk file on disk when it flushes
   model->flushAllDesks();
   QFile pd(path + "/.paperdesk");
   QVERIFY(pd.open(QIODevice::ReadOnly));
   QString expect = QString("testfile.max=%1,%2,")
                        .arg(newpos.x()).arg(newpos.y());
   QVERIFY2(pd.readAll().contains(expect.toUtf8()),
            qPrintable(expect));

   // undo from the menu puts it back; redo moves it again
   me.actionUndo->trigger();
   QCOMPARE(model->data(ind, Desktopmodel::Role_position).toPoint(),
            oldpos);
   QTRY_COMPARE(view->visualRect(vind).topLeft(), oldpos);
   me.actionRedo->trigger();
   QCOMPARE(model->data(ind, Desktopmodel::Role_position).toPoint(),
            newpos);
}


void TestDesktopUi::testImportFlow()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   // Make an import directory containing a PDF, as if downloaded
   QString imports = path + "/wibble1";
   QVERIFY(QFile::copy(testSrc + "/testpdf.pdf", imports + "/arrival.pdf"));

   // Show it, as the File->Import menu entries do
   desktop->showImports(imports);

   // The view shows the file to import, with the move action available
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 1);
   view->setSelectionRange(0, 1);
   QVERIFY(desktop->_act_move->isEnabled());

   // Move it into the repo root, as the move-to-folder dialog does
   QModelIndexList to_move = view->getSelectedListSource();
   QString dest = path + "/";
   QStringList sl;
   model->moveToDir(to_move, view->rootIndexSource(), dest, sl);

   QTRY_VERIFY(QFile::exists(path + "/arrival.pdf"));
   QVERIFY(!QFile::exists(imports + "/arrival.pdf"));
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 0);

   // Escape returns to the normal folder view, which now has the
   // imported file as well
   QTest::keyClick(view, Qt::Key_Escape);
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
}

/* Put some sheets of US letter in the fake scanner, unless a real one
   is being used, which has what the person running the test put in it

   \returns the scanner to use, or null if there is none */
static const char *loadSheets(int count)
{
   if (getenv("PM_SCAN_DEVICE"))
      return getenv("PM_SCAN_DEVICE");
   if (!Fakescan::available())
      return nullptr;

   QImage sheet(850, 1100, QImage::Format_RGB32);

   sheet.fill(Qt::white);
   Fakescan::reset();
   for (int i = 0; i < count; i++)
      if (!Fakescan::loadSheet(sheet, QImage(), 100))
         return nullptr;

   return FAKESCAN_DEVICE;
}

void TestDesktopUi::testScanIntoStack()
{
   const char *device = loadSheets(3);

   if (!device)
      QSKIP("no scanner to test with: set PM_SCAN_DEVICE");

   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   /* select the scanner as the last-used device so that ensureScanner()
      opens it without showing the selection dialog; restore the user's
      setting afterwards */
   if (!xmlConfig)
      new QXmlConfig();
   QString old_device = xmlConfig->stringValue("LAST_DEVICE", QString());
   xmlConfig->setStringValue("LAST_DEVICE", device);

   /* a scan reads the settings the user keeps, so say what this test
      wants rather than taking whatever the machine it runs on has: a
      page with nothing on it is thrown away on one and kept on another */
   int old_blank = xmlConfig->intValue("SCAN_BLANK");
   xmlConfig->setIntValue("SCAN_BLANK", 0);

   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);
   if (getenv("PM_SCAN_SET")) {
      QMap<QString, QString> opts;
      foreach (const QString &kv, QString(getenv("PM_SCAN_SET")).split(','))
         opts[kv.section('=', 0, 0)] = kv.section('=', 1);
      main->setScanOptions(opts);
   }
   int before = model->rowCount(repo_ind);

   /* press the stop button (which finishes the batch and ends the scan)
      as soon as the scan is under way, since a real scanner may hold a
      great many sheets. Opening the scanner can take several seconds, so
      wait for it rather than using a fixed delay, which could fire before
      the scan started. PM_SCAN_STOP_MS gives a fixed delay instead, for
      timing a longer run on a real scanner (see PM_SCAN_DEVICE and
      PM_SCAN_SET) */
   if (getenv("PM_SCAN_STOP_MS"))
      QTimer::singleShot(atoi(getenv("PM_SCAN_STOP_MS")), main,
                         [main]() { main->stopScan(false); });
   else {
      QTimer *stopper = new QTimer(main);
      connect(stopper, &QTimer::timeout, main, [main, stopper]() {
         if (main->isScanning()) {
            main->stopScan(false);
            stopper->stop();
         }
      });
      stopper->start(100);
   }
   me.actionScango->trigger();

   xmlConfig->setStringValue("LAST_DEVICE", old_device);
   xmlConfig->setIntValue("SCAN_BLANK", old_blank);

   // A new stack should appear, holding the scanned pages
   QCOMPARE(model->rowCount(repo_ind), before + 1);

   QModelIndex new_ind = model->index(before, 0, repo_ind);
   QVERIFY(new_ind.isValid());
   File *scanned = model->getFile(new_ind);
   QVERIFY(scanned);
   QVERIFY(scanned->pagecount() >= 1);

   // The scanned stack exists on disk in the current folder
   QString pathname =
      model->data(new_ind, Desktopmodel::Role_pathname).toString();
   QVERIFY(QFile::exists(pathname));

   /* the page view followed the scan: it is scrolled to the end and the
      last page is on screen, even once there were more pages than fit */
   Pageview *pageview = me.getDesktop()->getPagewidget()->findChild<Pageview *>();
   QVERIFY(pageview);
   QAbstractItemModel *pages = pageview->model();
   QVERIFY(pages);
   QCOMPARE(pages->rowCount(), scanned->pagecount());
   QScrollBar *vs = pageview->verticalScrollBar();
   QCOMPARE(vs->value(), vs->maximum());
   /* the last row is on screen: its rect may run a few pixels past the
      viewport (cell spacing, or a relayout still pending), so ask only
      that it overlaps the viewport */
   QModelIndex last = pages->index(pages->rowCount() - 1, 0);
   QVERIFY(pageview->viewport()->rect().intersects(pageview->visualRect(last)));
}

void TestDesktopUi::testScanCommandLine()
{
   /* the command-line scan uses the desktop machinery headless: it must
      leave a new stack in the requested directory, and must not disturb
      the saved scanner settings */
   QString path = setupRepo();
   QVERIFY(QDir().mkpath(path + "/inbox"));

   if (!xmlConfig)
      new QXmlConfig();
   QString old_device = xmlConfig->stringValue("LAST_DEVICE", QString());
   int old_single = xmlConfig->intValue("SCAN_SINGLE");

   if (!Fakescan::available())
      QSKIP("the fake scanner is only built on Linux");
   loadSheets(2);
   QCOMPARE(Mainwindow::runScan(path, "inbox", FAKESCAN_DEVICE, 2,
                                QStringList() << "mode=Color"), 0);

   QCOMPARE(xmlConfig->stringValue("LAST_DEVICE", QString()), old_device);
   QCOMPARE(xmlConfig->intValue("SCAN_SINGLE"), old_single);

   QStringList stacks = QDir(path + "/inbox").entryList(
         QStringList() << "*.max", QDir::Files);
   QCOMPARE(stacks.size(), 1);
   Filemax max(path + "/inbox/", stacks[0], nullptr);
   QVERIFY(!max.load());
   QCOMPARE(max.pagecount(), 2);

   // a missing directory is an error rather than a scan
   QCOMPARE(Mainwindow::runScan(path, "nosuch", FAKESCAN_DEVICE, 1), 1);

   utilSetHeadless(false);
}

void TestDesktopUi::testScanCommandLineBlank()
{
   /* with nobody to look at the pages, the command-line scan must leave
      out the blank ones itself, as asked: here the backs of two plain
      sheets scanned on both sides, while the fronts are kept */
   QString path = setupRepo();
   QVERIFY(QDir().mkpath(path + "/inbox"));

   if (!Fakescan::available())
      QSKIP("the fake scanner is only built on Linux");
   if (!xmlConfig)
      new QXmlConfig();
   int old_blank = xmlConfig->intValue("SCAN_BLANK");

   loadSheets(2);
   int ret = Mainwindow::runScan(path, "inbox", FAKESCAN_DEVICE, 0,
                                 QStringList() << "mode=Color"
                                 << "resolution=100"
                                 << "source=ADF Duplex", false, 0,
                                 Paperstack::ignoreIfBack);
   utilSetHeadless(false);
   QCOMPARE(ret, 0);
   QCOMPARE(xmlConfig->intValue("SCAN_BLANK"), old_blank);

   QStringList stacks = QDir(path + "/inbox").entryList(
         QStringList() << "*.max", QDir::Files);
   QCOMPARE(stacks.size(), 1);
   Filemax max(path + "/inbox/", stacks[0], nullptr);
   QVERIFY(!max.load());
   QCOMPARE(max.pagecount(), 2);
}

void TestDesktopUi::testRepositoryAddRemoveUndo()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Dirmodel *dirmodel = desktop->getDirmodel();

   // Add a second repository, as the directory-tree context menu does
   QTemporaryDir extra;
   QVERIFY(extra.isValid());
   int before = dirmodel->rowCount(QModelIndex());
   model->addRepository(extra.path());
   QCOMPARE(dirmodel->rowCount(QModelIndex()), before + 1);

   /* the only undoable command so far should be the repository add;
      programmatic selection changes must not create undo entries */
   Desktopundostack *stk = model->getUndoStack();
   QCOMPARE(stk->count(), 1);

   // Undo removes it again; redo brings it back
   me.actionUndo->trigger();
   QCOMPARE(dirmodel->rowCount(QModelIndex()), before);
   me.actionRedo->trigger();
   QCOMPARE(dirmodel->rowCount(QModelIndex()), before + 1);

   // Removing the repository is also undoable
   model->removeRepository(extra.path());
   QCOMPARE(dirmodel->rowCount(QModelIndex()), before);
   me.actionUndo->trigger();
   QCOMPARE(dirmodel->rowCount(QModelIndex()), before + 1);

   // Take it out again so the settings are left unchanged
   me.actionRedo->trigger();
   QCOMPARE(dirmodel->rowCount(QModelIndex()), before);
}

/* Return in the box where a name is typed finishes the name and goes no
   further. Left to itself a line edit passes the key on, and on a Mac the
   stacks take Return to mean 'edit this name', so the box opened again as
   soon as it had closed */
void TestDesktopUi::testRenameReturnStaysPut()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   clickItem(view, 0);
   view->renameStack(itemIndex(view, 0));

   Desktopeditor *editor = view->findChild<Desktopeditor *>();
   QVERIFY(editor);
   editor->selectAll();
   QTest::keyClicks(editor, "kept");

   // a key the box has no use for is passed on, as it always was
   QKeyEvent other(QEvent::KeyPress, Qt::Key_F13, Qt::NoModifier);

   other.ignore();
   QApplication::sendEvent(editor, &other);
   QVERIFY(!other.isAccepted());

   // Return finishes the name, and is used up by the box
   QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);

   press.ignore();
   QApplication::sendEvent(editor, &press);
   QVERIFY(press.isAccepted());

   // the stack has its new name, and no box for a name is open
   QTRY_VERIFY(QFile::exists(path + "/kept.max"));
   QTest::qWait(100);
   for (Desktopeditor *e : view->findChildren<Desktopeditor *>())
      QVERIFY2(!e->isVisible(), qPrintable(e->text()));
}

void TestDesktopUi::testRenameStackViaEditor()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   // Open the name editor on the first stack, as the rename menu
   // entry does
   view->renameStack(itemIndex(view, 0));

   Desktopeditor *editor = view->findChild<Desktopeditor *>();
   QVERIFY(editor);
   QCOMPARE(editor->text(), QString("testfile"));

   // Type a new name and press return
   editor->selectAll();
   QTest::keyClicks(editor, "renamed");
   QTest::keyClick(editor, Qt::Key_Return);

   QModelIndex src_ind = model->index(0, 0, repo_ind);
   QCOMPARE(model->data(src_ind, Qt::DisplayRole).toString(), "renamed");

   QString path = desktop->getSelectedPath();
   QVERIFY(QFile::exists(path + "/renamed.max"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));

   // The rename can be undone from the Edit menu
   me.actionUndo->trigger();
   QCOMPARE(model->data(src_ind, Qt::DisplayRole).toString(), "testfile");
   QVERIFY(QFile::exists(path + "/testfile.max"));
}

/* Scanned pages are read in the background for as long as it takes: a
   folder which is refreshed gives its stacks new Files, which used to
   make the reader drop them. What is left to read is shown, and quitting
   while there is some asks first */
void TestDesktopUi::testReadingStatus()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   OcrReader *reader = Mainwidget::singleton()->ocrReader();
   QPushButton *status = me.findChild<QPushButton *>("readingStatus");
   QVERIFY(status);
   QVERIFY(status->isHidden());

   // a stack of a few pages which can keep what is read
   File *file = nullptr;
   for (int row = 0; !file && row < model->rowCount(repo_ind); row++) {
      File *f = model->getFile(model->index(row, 0, repo_ind));

      if (model->keepsOcr(f) && !f->load() && f->pagecount() >= 3)
         file = f;
   }
   QVERIFY(file);
   const QString path = file->pathname();
   const QString dir = QFileInfo(path).path();
   int count = file->pagecount();
   QCOMPARE(model->fileForPath(path), file);
   QVERIFY(!model->fileForPath(dir + "/missing.max"));

   // read a page at a time, each when told to
   QSemaphore started, go;
   reader->setWorkers(1);
   reader->setEngine([&](QImage &image, OcrPage &ocr) {
      started.release();
      go.acquire();
      ocr = OcrPage();
      ocr.size = image.size();
      ocr.words << OcrWord{QRect(0, 0, 10, 10), "read", 90, 0, 0};
      return QString();
   });
   auto letGo = qScopeGuard([&go] { go.release(1000); });
   auto waitFor = [](QSemaphore &sem) {
      QElapsedTimer timer;

      timer.start();
      while (!sem.tryAcquire()) {
         if (timer.elapsed() > 10000)
            return false;
         QTest::qWait(5);
      }
      return true;
   };
   QSignalSpy idle(reader, &OcrReader::idle);

   reader->addFile(file);
   QVERIFY(waitFor(started));
   QVERIFY(!status->isHidden());
   QCOMPARE(status->text(),
            QString("Reading text: %1 pages in 1 stack").arg(count));

   // the folder is refreshed, so the stack has another File
   QPointer<File> old = file;
   model->refresh(dir);
   QVERIFY(!old);
   File *fresh = model->fileForPath(path);
   QVERIFY(fresh);
   go.release();
   QVERIFY(waitFor(started));
   QCOMPARE(status->text(),
            QString("Reading text: %1 pages in 1 stack").arg(count - 1));

   /* pressing the status shows what is waiting, where reading can be
      paused: the page being read is finished and no more is started */
   QVERIFY(!me.findChild<Backgrounddialog *>());
   QTest::mouseClick(status, Qt::LeftButton);
   Backgrounddialog *work = me.findChild<Backgrounddialog *>();
   QVERIFY(work);
   QVERIFY(work->isVisible());
   QCOMPARE(work->listed(), QStringList(QDir::cleanPath(path)));
   QCOMPARE(work->summary(),
            QString("Reading the text of scanned pages: %1 pages in 1 stack "
                    "left.").arg(count - 1));

   // how many pages are left is shown whole, whatever else is cut short
   QTableWidget *stacks = work->findChild<QTableWidget *>("stacks");
   QVERIFY(stacks);
   QString left = stacks->item(0, 2)->text();
   QCOMPARE(left, QString("%1, reading now").arg(count - 1));
   QCOMPARE(stacks->horizontalHeader()->sectionResizeMode(2),
            QHeaderView::ResizeToContents);
   QStyleOptionViewItem opt;
   opt.initFrom(stacks);
   int needs = stacks->itemDelegate()->sizeHint(opt,
                           stacks->model()->index(0, 2)).width();
   QVERIFY2(stacks->columnWidth(2) >= needs,
            qPrintable(QString("%1 wide, needs %2 for '%3'")
                          .arg(stacks->columnWidth(2)).arg(needs).arg(left)));

   QPushButton *pause = work->findChild<QPushButton *>("pause");
   QVERIFY(pause);
   QCOMPARE(pause->text(), QString("&Pause"));
   pause->click();
   QCOMPARE(pause->text(), QString("&Carry on"));
   QCOMPARE(status->text(),
            QString("Reading text paused: %1 pages in 1 stack").arg(count - 1));
   go.release();
   QTest::qWait(150);
   QCOMPARE(started.available(), 0);
   QCOMPARE(status->text(),
            QString("Reading text paused: %1 pages in 1 stack").arg(count - 2));
   QCOMPARE(work->summary(),
            QString("Reading the text of scanned pages is paused: %1 pages "
                    "in 1 stack left.").arg(count - 2).replace("1 pages",
                                                                "1 page"));
   pause->click();
   QVERIFY(waitFor(started));
   count--;

   // the Tools menu shows the same window
   work->close();
   QVERIFY(!work->isVisible());
   me.actionBackground->trigger();
   QVERIFY(work->isVisible());
   QCOMPARE(me.findChildren<Backgrounddialog *>().size(), 1);

   /* quitting asks first: the answer is given by whoever is at the
      screen, here a timer which presses the button named */
   Mainwindow::allowQuitQuestion(true);
   auto forbid = qScopeGuard([] { Mainwindow::allowQuitQuestion(false); });
   QString asked;
   auto answer = [&me, &asked](const QString &button) {
      QTimer *timer = new QTimer(&me);

      asked.clear();
      connect(timer, &QTimer::timeout, &me, [&me, &asked, timer, button] {
         auto *box = me.findChild<QMessageBox *>("quitReading");

         if (!box || !box->isVisible())
            return;
         timer->deleteLater();
         asked = box->text();
         for (QAbstractButton *b : box->buttons())
            if (b->text() == button)
               b->click();
      });
      timer->start(20);
   };

   answer("Keep reading");
   QVERIFY(!me.close());
   QVERIFY(me.isVisible());
   QCOMPARE(asked, QString("Paperman is still reading the text of scanned "
                           "pages: %1 pages in 1 stack.").arg(count - 1));

   // the reading goes on, through the stack's new File
   go.release(1000);
   QVERIFY(idle.wait(10000));
   QVERIFY(status->isHidden());
   fresh = model->fileForPath(path);
   QVERIFY(fresh);
   for (int i = 0; i < count; i++) {
      OcrPage ocr;

      QVERIFY(!fresh->getPageOcr(i, ocr));
      QCOMPARE(ocr.text(), QString("read"));
   }

   // the window says that nothing is left, and offers nothing to stop
   QVERIFY(work->listed().isEmpty());
   QVERIFY(work->summary().contains("Nothing is waiting"));
   QVERIFY(!work->findChild<QPushButton *>("stop")->isEnabled());
   QVERIFY(!pause->isEnabled());

   /* a stack put back on the list can be dropped from it again, which
      leaves nothing to read */
   OcrPage none;
   QVERIFY(!fresh->putPageOcr(0, none));
   reader->setPaused(true);
   reader->addFile(fresh);
   QCOMPARE(work->listed(), QStringList(QDir::cleanPath(path)));
   QVERIFY(!status->isHidden());
   QPushButton *remove = work->findChild<QPushButton *>("remove");
   QVERIFY(!remove->isEnabled());
   work->selectRow(0);
   QVERIFY(remove->isEnabled());
   remove->click();
   QVERIFY(work->listed().isEmpty());
   QVERIFY(status->isHidden());
   QVERIFY(!reader->isBusy());
   reader->setPaused(false);

   // and so can everything at once
   reader->setPaused(true);
   reader->addFile(fresh);
   QVERIFY(work->findChild<QPushButton *>("stop")->isEnabled());
   work->findChild<QPushButton *>("stop")->click();
   QVERIFY(work->listed().isEmpty());
   QVERIFY(!reader->isBusy());
   reader->setPaused(false);

   // with nothing left to read, quitting does not ask
   asked.clear();
   QVERIFY(me.close());
   QVERIFY(asked.isEmpty());
}

/* Work which takes a while is listed in the Background work window,
   with how far it has got, and can be stopped there if it stops when
   told to */
void TestDesktopUi::testBackgroundJobs()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   me.actionBackground->trigger();
   Backgrounddialog *work = me.findChild<Backgrounddialog *>();
   QVERIFY(work);
   QPushButton *stop = work->findChild<QPushButton *>("stopJob");
   QVERIFY(stop);
   QVERIFY(work->jobsListed().isEmpty());
   QVERIFY(!stop->isEnabled());

   {
      // an operation is listed while it runs, with how far it has got
      Operation op("Converting things", 10, nullptr);
      QTRY_COMPARE(work->jobsListed(), QStringList("Converting things: 0%"));
      op.setProgress(5);
      QTRY_COMPARE(work->jobsListed(), QStringList("Converting things: 50%"));

      // it cannot be stopped unless whatever runs it stops when told to
      QVERIFY(!stop->isEnabled());
      op.setCancellable(true);
      QTRY_VERIFY(stop->isEnabled());
      QVERIFY(!op.cancelled());
      stop->click();
      QVERIFY(op.cancelled());
      QVERIFY(!stop->isEnabled());
      QCOMPARE(work->jobsListed(),
               QStringList("Converting things: 50%, stopping"));

      // with more than one job, the one selected is the one stopped
      Operation first("Printing", 4, nullptr), second("Packaging", 0, nullptr);
      first.setCancellable(true);
      second.setCancellable(true);
      QTRY_COMPARE(int(work->jobsListed().size()), 3);
      QCOMPARE(work->jobsListed()[1], QString("Printing: 0%"));
      QVERIFY(!stop->isEnabled());
      work->selectJob(2);
      QVERIFY(stop->isEnabled());
      stop->click();
      QVERIFY(second.cancelled());
      QVERIFY(!first.cancelled());
   }

   // and is gone from the list when it finishes
   QTRY_VERIFY(work->jobsListed().isEmpty());
   QVERIFY(!stop->isEnabled());
}

/* The stack menu is kept short: what works on the contents of a stack
   is in a submenu. Every item has a shortcut, and none is used twice.
   The Background work window is opened from the View menu */
void TestDesktopUi::testStackMenu()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   QSettings().remove("assistant");
   clickItem(view, 0);

   /* the menu waits for a choice in a loop of its own, so look at it and
      close it from a timer. This gives what it shows, with a submenu's
      items after its title, in brackets */
   auto shown = [&]() -> QStringList {
      QStringList items;
      bool seen = false;
      QTimer timer;

      connect(&timer, &QTimer::timeout, &me, [&] {
         for (QWidget *top : QApplication::topLevelWidgets()) {
            QMenu *menu = qobject_cast<QMenu *>(top);

            if (!menu || !menu->isVisible())
               continue;
            seen = true;
            for (QAction *action : menu->actions()) {
               if (action->isSeparator() || !action->isVisible())
                  continue;

               QString text = action->text().remove('&');

               if (!action->menu()) {
                  items << text;
                  continue;
               }

               QStringList sub;

               for (QAction *child : action->menu()->actions())
                  if (!child->isSeparator() && child->isVisible())
                     sub << child->text().remove('&');
               items << text + " [" + sub.join(", ") + "]";
            }
            timer.stop();
            menu->close();
            return;
         }
      });
      timer.start(50);

      QPoint pos = view->visualRect(itemIndex(view, 0)).center();
      QContextMenuEvent event(QContextMenuEvent::Mouse, pos,
                              view->viewport()->mapToGlobal(pos));

      QApplication::sendEvent(view->viewport(), &event);
      return seen ? items : QStringList("no menu");
   };

   QCOMPARE(shown(), QStringList({
      "Locate folder", "Stack", "Unstack page", "Unstack all", "Duplicate",
      "Move to folder", "Delete stack", "Rename stack", "Rename page",
      "Process... [Read text (OCR), Reverse pages]",
      "Duplicate... [Duplicate page, as Max, as PDF, even pages only, "
         "odd pages only, as JPEG, Unfold booklet]",
      "Email... [Files, as Max, as PDF, Copy as PDF]"}));

   // with the assistant on, what it does is under Process too
   Assistant::setEnabled(true);
   QVERIFY(shown().contains("Process... [Read text (OCR), Suggest name, "
                            "Split into documents..., Reverse pages]"));
   QSettings().remove("assistant");

   // every item has a shortcut, which nothing else in the window has
   QHash<QString, QStringList> used;

   for (QAction *action : me.findChildren<QAction *>())
      if (!action->shortcut().isEmpty() && !action->text().isEmpty())
         used[action->shortcut().toString()] << action->text().remove('&');

   int count = 0;

   for (QAction *action : view->actions()) {
      if (action->text().isEmpty())
         continue;

      QString key = action->shortcut().toString();
      QString name = action->text().remove('&');

      QVERIFY2(!key.isEmpty(), qPrintable(name + " has no shortcut"));
      QVERIFY2(used.value(key) == QStringList(name),
               qPrintable(key + " is used by " + used.value(key).join(", ")));
      count++;
   }
   QVERIFY(count >= 20);
   QCOMPARE(desktop->_act_read_text->shortcut(), QKeySequence("Ctrl+T"));
   QCOMPARE(desktop->_act_suggest_name->shortcut(), QKeySequence("Ctrl+F2"));
   QCOMPARE(desktop->_act_suggest_split->shortcut(),
            QKeySequence("Ctrl+Shift+U"));
   QCOMPARE(desktop->_act_reverse->shortcut(), QKeySequence("Ctrl+R"));
}

/* The window showing work in the background is opened from the View
   menu, or with a key, by those who want to keep it open */
void TestDesktopUi::testBackgroundInViewMenu()
{
   Mainwindow me;

   QVERIFY(me.menuView->actions().contains(me.actionBackground));
   QVERIFY(!me.menuTools->actions().contains(me.actionBackground));
   QCOMPARE(me.actionBackground->text(), QString("&Background work..."));
   QCOMPARE(me.actionBackground->shortcut(), QKeySequence("Ctrl+B"));

   // nothing else in the window has that key
   for (QAction *action : me.findChildren<QAction *>())
      if (action != me.actionBackground)
         QVERIFY2(action->shortcut() != me.actionBackground->shortcut(),
                  qPrintable(action->text()));

   QVERIFY(!me.findChild<Backgrounddialog *>());
   me.actionBackground->trigger();
   Backgrounddialog *work = me.findChild<Backgrounddialog *>();
   QVERIFY(work);
   QVERIFY(work->isVisible());
   work->close();
}

/* Ctrl with an arrow key moves the keyboard focus without changing what
   is selected, as in any list, so that Ctrl+Space can add the stack it
   reaches. The focus is marked, and the preview stays with what is
   selected, since that is what anything done next works on */
void TestDesktopUi::testStackFocus()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopview *view = me.getDesktop()->getView();
   QItemSelectionModel *sel = view->selectionModel();
   QStringList shown;

   connect(view, &Desktopview::itemPreview, &me,
           [&shown](const QModelIndex &ind, int, bool) {
      shown << ind.data().toString();
   });

   auto selected = [&]() {
      QStringList names;

      for (const QModelIndex &ind : sel->selectedIndexes())
         names << ind.data().toString();
      names.sort();
      return names;
   };

   // the two stacks, in the order they are across the desk
   QModelIndex left = itemIndex(view, 0), right = itemIndex(view, 1);
   if (view->visualRect(left).left() > view->visualRect(right).left())
      std::swap(left, right);
   const QString lname = left.data().toString();
   const QString rname = right.data().toString();

   // a click selects a stack, puts the focus on it and shows it
   QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                     view->visualRect(right).center());
   QCOMPARE(selected(), QStringList(rname));
   QCOMPARE(view->currentIndex(), right);
   QCOMPARE(shown.value(shown.size() - 1), rname);

   // Ctrl with an arrow moves the focus only: the same stack is shown
   int before = shown.size();
   QTest::keyClick(view, Qt::Key_Left, Qt::ControlModifier);
   QCOMPARE(view->currentIndex(), left);
   QCOMPARE(selected(), QStringList(rname));
   QCOMPARE(int(shown.size()), before);

   // Ctrl+Space adds the stack with the focus to those selected
   QTest::keyClick(view, Qt::Key_Space, Qt::ControlModifier);
   QStringList both({lname, rname});
   both.sort();
   QCOMPARE(selected(), both);

   // and takes it away again, leaving the first, which is still shown
   QTest::keyClick(view, Qt::Key_Space, Qt::ControlModifier);
   QCOMPARE(selected(), QStringList(rname));
   QCOMPARE(view->currentIndex(), left);
   QCOMPARE(shown.value(shown.size() - 1), rname);

   /* Space alone selects the stack with the focus as well, and since it
      is now selected it is the one shown */
   QTest::keyClick(view, Qt::Key_Space);
   QCOMPARE(selected(), both);
   QCOMPARE(shown.value(shown.size() - 1), lname);

   // a plain arrow moves the selection, the focus and the preview, once
   before = shown.size();
   QTest::keyClick(view, Qt::Key_Right);
   QCOMPARE(selected(), QStringList(rname));
   QCOMPARE(view->currentIndex(), right);
   QCOMPARE(int(shown.size()), before + 1);
   QCOMPARE(shown.last(), rname);

   // and so does a click, which shows the stack when the button comes up
   before = shown.size();
   QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                     view->visualRect(left).center());
   QCOMPARE(selected(), QStringList(lname));
   QCOMPARE(int(shown.size()), before + 1);
   QCOMPARE(shown.last(), lname);
   QTest::keyClick(view, Qt::Key_Right);
   QCOMPARE(selected(), QStringList(rname));

   /* the focus is marked where it says something: on a stack which is not
      selected, or on one of several which are */
   QStyle::State focus = QStyle::State_HasFocus | QStyle::State_Enabled;
   QVERIFY(!Desktopdelegate::showsFocus(focus | QStyle::State_Selected, 1));
   QVERIFY(Desktopdelegate::showsFocus(focus, 1));
   QVERIFY(Desktopdelegate::showsFocus(focus, 0));
   QVERIFY(Desktopdelegate::showsFocus(focus | QStyle::State_Selected, 2));
   QVERIFY(!Desktopdelegate::showsFocus(QStyle::State_Selected, 2));
   QVERIFY(!Desktopdelegate::showsFocus(QStyle::State_Enabled, 0));

   /* and it is drawn: with the focus moved to the stack which is not
      selected, that stack looks different, and the other does not */
   view->setFocus();
   QTRY_VERIFY(view->hasFocus());
   auto look = [&](const QModelIndex &ind) {
      return view->viewport()->grab(view->visualRect(ind)).toImage();
   };
   QCOMPARE(selected(), QStringList(rname));
   QCOMPARE(view->currentIndex(), right);
   QImage left_plain = look(left), right_alone = look(right);

   QTest::keyClick(view, Qt::Key_Left, Qt::ControlModifier);
   QCOMPARE(view->currentIndex(), left);
   QVERIFY(look(left) != left_plain);
   QCOMPARE(look(right), right_alone);

   // back on the lone selected stack, both look as they did
   QTest::keyClick(view, Qt::Key_Right, Qt::ControlModifier);
   QCOMPARE(view->currentIndex(), right);
   QCOMPARE(look(left), left_plain);
   QCOMPARE(look(right), right_alone);

   // with both selected, the one with the focus is marked
   QTest::keyClick(view, Qt::Key_Left, Qt::ControlModifier);
   QTest::keyClick(view, Qt::Key_Space, Qt::ControlModifier);
   QCOMPARE(selected(), both);
   QImage left_focus = look(left);
   QTest::keyClick(view, Qt::Key_Right, Qt::ControlModifier);
   QCOMPARE(view->currentIndex(), right);
   QVERIFY(look(left) != left_focus);
   QVERIFY(look(right) != right_alone);
}

void TestDesktopUi::testSuggestName()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();

   // the stack with text to name it from
   QModelIndex ind = model->index("testpdf.pdf", repo_ind);
   QVERIFY(ind.isValid());
   desktop->getModelconv()->indexToProxy(model, ind);
   view->setSelectionRange(ind.row(), 1);

   // it is not offered, nor does anything, until it is turned on
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);

   desktop->_assistant = &assistant;
   QSettings().remove("assistant");
   desktop->updateActions();
   QVERIFY(!desktop->_act_suggest_name->isVisible());
   desktop->suggestName();
   QVERIFY(backend->_prompt.isEmpty());
   QVERIFY(!view->findChild<Desktopeditor *>());

   Assistant::setEnabled(true);
   desktop->updateActions();
   QVERIFY(desktop->_act_suggest_name->isEnabled());

   // the name suggested is in the editor, for the user to accept
   QJsonObject given;

   given["type"] = "report";
   given["date"] = "2026";
   backend->_answer["name"] = "Test document 2026";
   backend->_answer["facts"] = given;
   desktop->suggestName();
   QVERIFY(!backend->_prompt.isEmpty());
   QVERIFY(backend->_prompt.contains("- testfile\n"));

   Desktopeditor *editor = view->findChild<Desktopeditor *>();
   QVERIFY(editor);
   QCOMPARE(editor->text(), QString("Test document 2026"));
   QVERIFY(QFile::exists(path + "/testpdf.pdf"));

   // the facts which came with the name are not kept until it is accepted
   QVERIFY(model->facts(model->index("testpdf.pdf", repo_ind)).isEmpty());

   QTest::keyClick(editor, Qt::Key_Return);
   QTRY_VERIFY(QFile::exists(path + "/Test document 2026.pdf"));
   QVERIFY(!QFile::exists(path + "/testpdf.pdf"));

   Docfacts kept = model->facts(model->index("Test document 2026.pdf",
                                             repo_ind));
   QCOMPARE(kept.type, QString("report"));
   QCOMPARE(kept.date, QString("2026"));

   // and one undo gives the old name back, and takes the facts away
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(model->facts(model->index("testpdf.pdf", repo_ind)).isEmpty());

   /* facts offered with a name lapse when the name box closes, so a name
      given later by hand does not bring them */
   model->offerFacts(model->index("testpdf.pdf", repo_ind), kept);
   QVERIFY(QMetaObject::invokeMethod(desktop, "nameBoxClosed"));
   model->renameStack(model->index("testpdf.pdf", repo_ind), "by hand");
   QTRY_VERIFY(QFile::exists(path + "/by hand.pdf"));
   QVERIFY(model->facts(model->index("by hand.pdf", repo_ind)).isEmpty());
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));

   // offered and not withdrawn, they go with the next name, as one step
   model->offerFacts(model->index("testpdf.pdf", repo_ind), kept);
   model->renameStack(model->index("testpdf.pdf", repo_ind), "with facts");
   QTRY_VERIFY(QFile::exists(path + "/with facts.pdf"));
   QCOMPARE(model->facts(model->index("with facts.pdf", repo_ind)), kept);
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(model->facts(model->index("testpdf.pdf", repo_ind)).isEmpty());

   // even when the name is the one the stack has
   model->offerFacts(model->index("testpdf.pdf", repo_ind), kept);
   model->renameStack(model->index("testpdf.pdf", repo_ind), "testpdf");
   QCOMPARE(model->facts(model->index("testpdf.pdf", repo_ind)), kept);
   me.actionUndo->trigger();
   QVERIFY(model->facts(model->index("testpdf.pdf", repo_ind)).isEmpty());

   // a failure is reported, and leaves the name alone
   QSignalSpy said(desktop, SIGNAL(newContents(QString)));

   backend->_answer = QJsonObject();
   backend->_error = "The assistant could not answer";
   ind = model->index("testpdf.pdf", repo_ind);
   desktop->getModelconv()->indexToProxy(model, ind);
   view->setSelectionRange(ind.row(), 1);
   desktop->suggestName();
   QCOMPARE(said.last().at(0).toString(),
            QString("The assistant could not answer"));

   desktop->_assistant = nullptr;
   QSettings().remove("assistant");
}

void TestDesktopUi::testSuggestSplit()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();
   auto select = [&](const QString &name) {
      QModelIndex ind = model->index(name, repo_ind);

      desktop->getModelconv()->indexToProxy(model, ind);
      view->setSelectionRange(ind.row(), 1);
   };
   auto answer = [](const QList<QPair<int, QString>> &docs) {
      QJsonArray arr;

      for (const auto &doc : docs) {
         QJsonObject obj;

         obj["first_page"] = doc.first;
         obj["name"] = doc.second;
         arr << obj;
      }

      QJsonObject top;

      top["documents"] = arr;
      return top;
   };

   // the stack with text on its pages to divide it by
   QModelIndex ind = model->index("testpdf.pdf", repo_ind);
   QVERIFY(ind.isValid());

   File *f = model->getFile(ind);
   QVERIFY(!f->load());
   int pagecount = f->pagecount();
   int rows = model->rowCount(repo_ind);

   QVERIFY(pagecount >= 4);
   select("testpdf.pdf");

   // it is not offered, nor does anything, until it is turned on
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);

   desktop->_assistant = &assistant;
   QSettings().remove("assistant");
   desktop->updateActions();
   QVERIFY(!desktop->_act_suggest_split->isVisible());
   desktop->suggestSplit();
   QVERIFY(backend->_prompt.isEmpty());

   Assistant::setEnabled(true);
   desktop->updateActions();
   QVERIFY(desktop->_act_suggest_split->isEnabled());

   // one document is nothing to divide, so nothing is shown
   QSignalSpy said(desktop, SIGNAL(newContents(QString)));

   backend->_answer = answer({{1, "All of it"}});
   desktop->suggestSplit();
   QVERIFY(backend->_prompt.contains(QString("The stack has %1 pages")
                                        .arg(pagecount)));
   QVERIFY(backend->_prompt.contains("- testfile\n"));
   QVERIFY(!desktop->findChild<Splitdialog *>());
   QVERIFY(said.last().at(0).toString().contains("only one document"));

   // three documents are shown, each with its pages and its name
   backend->_answer = answer({{1, "Power bill"}, {2, "Bank letter"},
                              {4, "Rates notice"}});
   desktop->suggestSplit();

   Splitdialog *dialog = desktop->findChild<Splitdialog *>();
   QVERIFY(dialog);
   QCOMPARE(dialog->docs().size(), 3);
   QCOMPARE(dialog->pagesText(0), QString("1 (1 page)"));
   QCOMPARE(dialog->pagesText(1), QString("2-3 (2 pages)"));

   // nothing has changed yet, and cancelling leaves it so
   QCOMPARE(model->rowCount(repo_ind), rows);
   dialog->reject();
   QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
   QVERIFY(!desktop->findChild<Splitdialog *>());
   QCOMPARE(model->rowCount(repo_ind), rows);

   /* asked again, the last document is joined to the one before it, a
      name is changed, and it is done */
   select("testpdf.pdf");
   desktop->suggestSplit();
   dialog = desktop->findChild<Splitdialog *>();
   QVERIFY(dialog);

   QPushButton *join = nullptr;

   for (QPushButton *button : dialog->findChildren<QPushButton *>())
      if (button->text().contains("Join"))
         join = button;
   QVERIFY(join);
   QVERIFY(!join->isEnabled());
   dialog->selectDoc(2);
   QVERIFY(join->isEnabled());
   join->click();
   QCOMPARE(dialog->docs().size(), 2);
   QCOMPARE(dialog->pagesText(1),
            QString("2-%1 (%2 pages)").arg(pagecount).arg(pagecount - 1));

   QTableWidget *table = dialog->findChild<QTableWidget *>();
   QVERIFY(table);
   table->item(0, 0)->setText("Electricity: March");
   QCOMPARE(dialog->docs()[0].name, QString("Electricity March"));

   dialog->accept();
   QTRY_COMPARE(model->rowCount(repo_ind), rows + 1);
   QVERIFY(QFile::exists(path + "/Electricity March.pdf"));
   QVERIFY(QFile::exists(path + "/Bank letter.pdf"));
   QVERIFY(!QFile::exists(path + "/testpdf.pdf"));

   File *first = model->getFile(model->index("Electricity March.pdf",
                                             repo_ind));
   File *second = model->getFile(model->index("Bank letter.pdf", repo_ind));
   QVERIFY(first && second);
   QVERIFY(!second->load());
   QCOMPARE(first->pagecount(), 1);
   QCOMPARE(second->pagecount(), pagecount - 1);

   // and one undo puts the stack back together, with its name
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QCOMPARE(model->rowCount(repo_ind), rows);
   QCOMPARE(model->getFile(model->index("testpdf.pdf", repo_ind))
               ->pagecount(), pagecount);

   // a failure is reported, and shows nothing
   QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
   backend->_answer = QJsonObject();
   backend->_error = "The assistant could not answer";
   select("testpdf.pdf");
   desktop->suggestSplit();
   QCOMPARE(said.last().at(0).toString(),
            QString("The assistant could not answer"));
   QVERIFY(!desktop->findChild<Splitdialog *>());

   desktop->_assistant = nullptr;
   QSettings().remove("assistant");
}

/* A name comes from the first pages which say anything. Pages which say
   nothing are read first when there is too little to go on, and what is
   read is sent but not kept. This has a window of its own, so that the
   only name editor is the one it opens */
void TestDesktopUi::testSuggestNameReadsPages()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);

   desktop->_assistant = &assistant;
   QSettings().remove("assistant");
   Assistant::setEnabled(true);
   desktop->updateActions();

   QAtomicInt reads;
   desktop->_name_engine = [&reads](QImage &image, OcrPage &ocr) {
      ocr = OcrPage();
      ocr.size = image.size();
      ocr.words << OcrWord{QRect(0, 0, 10, 10),
                           QString("width%1read%2").arg(image.width())
                              .arg(int(++reads)), 90, 0, 0};
      return QString();
   };

   /* every page of the PDF has text, so none is read for its name. The
      answer is an error, so that no name is offered */
   QModelIndex ind = model->index("testpdf.pdf", repo_ind);
   QVERIFY(ind.isValid());
   desktop->getModelconv()->indexToProxy(model, ind);
   view->setSelectionRange(ind.row(), 1);
   backend->_error = "No name this time";
   desktop->suggestName();
   QVERIFY(!backend->_prompt.isEmpty());
   QVERIFY(backend->_prompt.contains("=== Page 1 ===\n"));
   QCOMPARE(int(reads), 0);
   QVERIFY(!view->findChild<Desktopeditor *>());
   backend->_error.clear();

   // the pages of the max stack say nothing, so its first pages are read
   QModelIndex max_ind = model->index("testfile.max", repo_ind);
   QVERIFY(max_ind.isValid());
   File *max = model->getFile(max_ind);
   QVERIFY(!max->load());
   int max_pages = max->pagecount();
   QVERIFY(max_pages >= 2);
   OcrPage none;
   QVERIFY(max->getPageOcr(0, none) || none.isEmpty());

   backend->_prompt.clear();
   backend->_answer["name"] = "Read from pages";
   ind = max_ind;
   desktop->getModelconv()->indexToProxy(model, ind);
   view->setSelectionRange(ind.row(), 1);
   desktop->suggestName();
   QVERIFY(backend->_prompt.isEmpty());
   QTRY_VERIFY(!backend->_prompt.isEmpty());
   QCOMPARE(int(reads), qMin(max_pages, int(Assistant::name_look_pages)));
   QVERIFY2(backend->_prompt.contains(QRegularExpression(
               "=== Page 1 ===\\nwidth\\d+read\\d+\\n")),
            qPrintable(backend->_prompt));
   QVERIFY(backend->_prompt.contains("=== Page 2 ===\n"));

   // what was read is not kept in the stack
   QVERIFY(max->getPageOcr(0, none) || none.isEmpty());

   /* the name is offered, and accepting it renames the stack: looking at
      its pages has not left the file open, which would stop that on
      Windows */
   Desktopeditor *editor = nullptr;
   QTRY_VERIFY((editor = view->findChild<Desktopeditor *>()));
   QCOMPARE(editor->text(), QString("Read from pages"));
   QTest::keyClick(editor, Qt::Key_Return);
   QTRY_VERIFY(QFile::exists(path + "/Read from pages.max"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));

   desktop->_name_engine = nullptr;
   desktop->_assistant = nullptr;
   QSettings().remove("assistant");
}

/* The window for checking suggested names gives back only what is to
   be done: the stacks ticked, with a name which is not the one they have */
void TestDesktopUi::testNamesDialog()
{
   Namesdialog dialog;
   QTableWidget *table = dialog.findChild<QTableWidget *>("names");
   QPushButton *rename = dialog.findChild<QPushButton *>("rename");

   QVERIFY(table && rename);
   QVERIFY(!rename->isEnabled());
   QCOMPARE(dialog.renameText(), QString("&Rename 0 stacks"));

   dialog.setNames(QStringList({"26-Nov-26", "26-Nov-26_1", "26-Nov-26_2"}),
                   QStringList({"power bill", QString(), "26-Nov-26_2"}),
                   QStringList({QString(), "no text", QString()}));
   QCOMPARE(table->rowCount(), 3);

   // a stack with a name is ticked; one with none says why and is not
   QVERIFY(dialog.ticked(0));
   QVERIFY(!dialog.ticked(1));
   QCOMPARE(table->item(0, 0)->text(), QString("26-Nov-26"));
   QCOMPARE(table->item(0, 1)->text(), QString("power bill"));
   QCOMPARE(table->item(1, 0)->text(), QString("26-Nov-26_1 (no text)"));
   QCOMPARE(table->item(1, 1)->text(), QString());

   // the name a stack has cannot be changed here, only the new one
   QVERIFY(!(table->item(0, 0)->flags() & Qt::ItemIsEditable));
   QVERIFY(table->item(0, 1)->flags() & Qt::ItemIsEditable);

   // a name the same as the stack has is nothing to do
   QCOMPARE(dialog.names(), QStringList({"power bill", QString(), QString()}));
   QCOMPARE(dialog.count(), 1);
   QCOMPARE(dialog.renameText(), QString("&Rename 1 stack"));
   QVERIFY(rename->isEnabled());

   // a name typed for a stack counts once the stack is ticked
   dialog.setName(1, "  water bill ");
   QCOMPARE(dialog.count(), 1);
   dialog.setTicked(1, true);
   QCOMPARE(dialog.names(),
            QStringList({"power bill", "water bill", QString()}));
   QCOMPARE(dialog.renameText(), QString("&Rename 2 stacks"));

   // and with nothing ticked there is nothing to rename
   dialog.setTicked(0, false);
   dialog.setTicked(1, false);
   QCOMPARE(dialog.count(), 0);
   QVERIFY(!rename->isEnabled());

   /* the facts which came with each name are shown beside it, with the
      amount and its currency together */
   Docfacts bill, none;

   bill.type = "bill";
   bill.from = "Northern Power";
   bill.date = "2026-03-03";
   bill.period = "Feb 2026";
   bill.amount = "184.20";
   bill.currency = "NZD";
   bill.summary = "Electricity bill";
   dialog.setNames(QStringList({"26-Nov-26", "26-Nov-26_1", "26-Nov-26_2"}),
                   QStringList({"power bill", QString(), "26-Nov-26_2"}),
                   QStringList({QString(), "no text", QString()}));
   dialog.setFacts(QList<Docfacts>({bill, none, bill}));
   QCOMPARE(table->columnCount(), int(Namesdialog::col_count));
   QCOMPARE(table->item(0, Namesdialog::col_kind)->text(), QString("bill"));
   QCOMPARE(table->item(0, Namesdialog::col_from)->text(),
            QString("Northern Power"));
   QCOMPARE(table->item(0, Namesdialog::col_date)->text(),
            QString("2026-03-03"));
   QCOMPARE(table->item(0, Namesdialog::col_amount)->text(),
            QString("184.20 NZD"));
   QVERIFY(table->item(0, Namesdialog::col_kind)->toolTip()
              .contains("Feb 2026"));
   QCOMPARE(table->item(1, Namesdialog::col_kind)->text(), QString());

   /* a stack which is ticked keeps its facts, as given; one which is not
      keeps none. A stack with facts but the name it has still counts */
   QCOMPARE(dialog.facts(), QList<Docfacts>({bill, none, bill}));
   QCOMPARE(dialog.names(), QStringList({"power bill", QString(), QString()}));
   QCOMPARE(dialog.count(), 2);
   dialog.setTicked(2, false);
   QCOMPARE(dialog.facts(), QList<Docfacts>({bill, none, none}));
   QCOMPARE(dialog.count(), 1);

   // a fact can be put right, and one which is not in its form is dropped
   dialog.setFact(0, Namesdialog::col_from, " Southern Power ");
   dialog.setFact(0, Namesdialog::col_amount, "190 aud");
   dialog.setFact(0, Namesdialog::col_date, "last March");

   Docfacts changed = bill;

   changed.from = "Southern Power";
   changed.amount = "190";
   changed.currency = "AUD";
   changed.date.clear();
   QCOMPARE(dialog.facts()[0], changed);

   // what has no column is kept as it was given
   QCOMPARE(dialog.facts()[0].period, QString("Feb 2026"));
   QCOMPARE(dialog.facts()[0].summary, QString("Electricity bill"));

   // an amount with no currency loses the one it had
   dialog.setFact(0, Namesdialog::col_amount, "190");
   QCOMPARE(dialog.facts()[0].currency, QString());
}

/* With several stacks selected, a name is asked for each, and all are
   shown to be checked: a name can be changed, a stack left out, and what
   is agreed to is renamed as one step */
void TestDesktopUi::testSuggestNames()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);
   int stacks = model->rowCount(repo_ind);

   QVERIFY(stacks >= 2);
   desktop->_assistant = &assistant;
   QSettings().remove("assistant");
   Assistant::setEnabled(true);

   // the pages of the max stack say nothing unless they are read
   bool readable = false;
   desktop->_name_engine = [&readable](QImage &image, OcrPage &ocr) {
      ocr = OcrPage();
      ocr.size = image.size();
      if (readable)
         ocr.words << OcrWord{QRect(0, 0, 10, 10), "words", 90, 0, 0};
      return QString();
   };

   // it is offered for several stacks as well as for one
   view->setSelectionRange(0, stacks);
   desktop->updateActions();
   QVERIFY(desktop->_act_suggest_name->isEnabled());

   /* a stack with nothing to go on gets no name and says why; the other
      stacks being named are not given as examples of how to name things */
   for (int i = 0; i < stacks; i++)
      backend->_names << QString("Name %1").arg(i + 1);
   desktop->suggestName();

   Namesdialog *dialog = nullptr;
   QTRY_VERIFY((dialog = desktop->findChild<Namesdialog *>()));
   QVERIFY(dialog->isVisible());
   QCOMPARE(backend->_asked, stacks - 1);
   QVERIFY(!backend->_prompt.contains("- testfile\n"));
   QVERIFY(!backend->_prompt.contains("- testpdf\n"));
   QVERIFY(!view->findChild<Desktopeditor *>());

   QTableWidget *table = dialog->findChild<QTableWidget *>("names");
   QVERIFY(table);
   QCOMPARE(table->rowCount(), stacks);

   int max_row = -1, pdf_row = -1;

   for (int row = 0; row < stacks; row++) {
      QString stack = table->item(row, 0)->text();

      if (stack.startsWith("testfile"))
         max_row = row;
      else if (stack == "testpdf")
         pdf_row = row;
   }
   QVERIFY(max_row >= 0 && pdf_row >= 0);
   QCOMPARE(table->item(max_row, 0)->text(), QString("testfile (no text)"));
   QVERIFY(!dialog->ticked(max_row));
   QVERIFY(dialog->ticked(pdf_row));
   QCOMPARE(table->item(pdf_row, 1)->text(), QString("Name 1"));
   QCOMPARE(dialog->count(), stacks - 1);

   // leaving the window renames nothing
   dialog->reject();
   QTRY_VERIFY(!desktop->findChild<Namesdialog *>());
   QVERIFY(QFile::exists(path + "/testpdf.pdf"));

   // with its pages read, every stack has a name suggested, with facts
   QJsonObject given;

   given["type"] = "letter";
   given["from"] = "The Council";
   given["date"] = "2026-05";
   backend->_answer["facts"] = given;
   readable = true;
   backend->_names.clear();
   backend->_asked = 0;
   for (int i = 0; i < stacks; i++)
      backend->_names << QString("Paper %1").arg(i + 1);
   view->setSelectionRange(0, stacks);
   desktop->suggestName();
   QTRY_VERIFY((dialog = desktop->findChild<Namesdialog *>()));
   QCOMPARE(backend->_asked, stacks);
   QCOMPARE(dialog->count(), stacks);
   QCOMPARE(dialog->renameText(),
            QString("&Rename %1 stacks").arg(stacks));
   table = dialog->findChild<QTableWidget *>("names");

   max_row = pdf_row = -1;
   for (int row = 0; row < stacks; row++) {
      if (table->item(row, 0)->text() == "testfile")
         max_row = row;
      else if (table->item(row, 0)->text() == "testpdf")
         pdf_row = row;
   }
   QVERIFY(max_row >= 0 && pdf_row >= 0);

   // a name can be changed, and one which is the same as before is left
   QString max_name = table->item(max_row, 1)->text();
   QVERIFY(max_name.startsWith("Paper "));
   dialog->setName(pdf_row, "Chosen by hand");
   QCOMPARE(dialog->names()[pdf_row], QString("Chosen by hand"));
   dialog->setName(pdf_row, "testpdf");
   QCOMPARE(dialog->names()[pdf_row], QString());

   // though the stack still has its facts to keep, so it still counts
   QCOMPARE(dialog->count(), stacks);
   dialog->setName(pdf_row, "Chosen by hand");

   // a stack which is not ticked keeps its name
   for (int row = 0; row < stacks; row++)
      if (row != max_row && row != pdf_row)
         dialog->setTicked(row, false);
   QCOMPARE(dialog->count(), 2);
   QCOMPARE(dialog->renameText(), QString("&Rename 2 stacks"));

   // the facts which came with the names are shown, and can be put right
   QCOMPARE(table->item(pdf_row, Namesdialog::col_kind)->text(),
            QString("letter"));
   QCOMPARE(table->item(max_row, Namesdialog::col_from)->text(),
            QString("The Council"));
   dialog->setFact(pdf_row, Namesdialog::col_kind, "notice");

   QPushButton *rename = dialog->findChild<QPushButton *>("rename");
   QVERIFY(rename && rename->isEnabled());
   rename->click();
   QTRY_VERIFY(QFile::exists(path + "/Chosen by hand.pdf"));
   QTRY_VERIFY(QFile::exists(path + "/" + max_name + ".max"));
   QVERIFY(!QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));

   // each stack renamed has its facts, as they were left in the window
   Docfacts kept = model->facts(model->index("Chosen by hand.pdf", repo_ind));
   QCOMPARE(kept.type, QString("notice"));
   QCOMPARE(kept.from, QString("The Council"));
   QCOMPARE(kept.date, QString("2026-05"));
   kept = model->facts(model->index(max_name + ".max", repo_ind));
   QCOMPARE(kept.type, QString("letter"));

   // and one undo puts the names back, and takes the facts away
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QVERIFY(model->facts(model->index("testpdf.pdf", repo_ind)).isEmpty());
   QVERIFY(model->facts(model->index("testfile.max", repo_ind)).isEmpty());

   // asking can be given up from the list of work going on
   Backgroundwork *work = Backgroundwork::instance();
   int before = work->jobs().size();
   QTRY_VERIFY(!desktop->findChild<Namesdialog *>());
   backend->_names.clear();
   backend->_asked = 0;
   backend->_hold = true;
   view->setSelectionRange(0, stacks);
   desktop->suggestName();
   QTRY_VERIFY(backend->_asked > 0);
   QCOMPARE(int(work->jobs().size()), before + 1);
   QCOMPARE(work->jobs().last()->title(), QString("Suggesting names"));
   work->jobs().last()->requestStop();
   QTRY_COMPARE(int(work->jobs().size()), before);
   backend->release();
   QTest::qWait(100);
   QVERIFY(!desktop->findChild<Namesdialog *>());

   desktop->_name_engine = nullptr;
   desktop->_assistant = nullptr;
   QSettings().remove("assistant");
}

/* Names take a while to come, and the user does not sit still: only one
   lot is asked for at a time, the window of names does not get in the way,
   and a name for one stack goes in the name box only if nothing has moved
   on meanwhile */
void TestDesktopUi::testSuggestNameMovedOn()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QString path = desktop->getSelectedPath();
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);
   int stacks = model->rowCount(repo_ind);

   desktop->_assistant = &assistant;
   QSettings().remove("assistant");
   Assistant::setEnabled(true);
   desktop->_name_engine = [](QImage &image, OcrPage &ocr) {
      ocr = OcrPage();
      ocr.size = image.size();
      ocr.words << OcrWord{QRect(0, 0, 10, 10), "words", 90, 0, 0};
      return QString();
   };
   QSignalSpy said(desktop, SIGNAL(newContents(QString)));

   auto select = [&](const QString &fname) {
      QModelIndex ind = model->index(fname, repo_ind);

      desktop->getModelconv()->indexToProxy(model, ind);
      view->setSelectionRange(ind.row(), 1);
   };

   // while names are being asked for, asking again is put off
   backend->_hold = true;
   backend->_answer["name"] = "A name";
   view->setSelectionRange(0, stacks);
   desktop->suggestName();
   QTRY_COMPARE(backend->_asked, stacks);
   desktop->suggestName();
   QCOMPARE(said.last().at(0).toString(),
            QString("Names are still being asked for"));
   QTest::qWait(50);
   QCOMPARE(backend->_asked, stacks);
   QVERIFY(!desktop->findChild<Namesdialog *>());

   /* the window of names does not stop anything else being done, and
      once it has come, names can be asked for again */
   backend->release();
   Namesdialog *dialog = nullptr;
   QTRY_VERIFY((dialog = desktop->findChild<Namesdialog *>()));
   QVERIFY(dialog->isVisible());
   QVERIFY(!dialog->isModal());
   QCOMPARE(dialog->windowModality(), Qt::NonModal);
   dialog->reject();
   QTRY_VERIFY(!desktop->findChild<Namesdialog *>());

   /* a name for one stack is not put in the name box if another stack
      has been selected meanwhile: it is shown in the window, where it
      can be accepted */
   backend->_asked = 0;
   backend->_hold = true;
   backend->_answer["name"] = "Bank statement";
   select("testpdf.pdf");
   desktop->suggestName();
   QTRY_COMPARE(backend->_asked, 1);
   select("testfile.max");
   backend->release();
   QTRY_VERIFY((dialog = desktop->findChild<Namesdialog *>()));
   QVERIFY(!view->findChild<Desktopeditor *>());
   QCOMPARE(dialog->names(), QStringList("Bank statement"));
   QCOMPARE(dialog->renameText(), QString("&Rename 1 stack"));
   QCOMPARE(said.last().at(0).toString(),
            QString("Check the name suggested"));
   dialog->accept();
   QTRY_VERIFY(QFile::exists(path + "/Bank statement.pdf"));
   QVERIFY(QFile::exists(path + "/testfile.max"));
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QTRY_VERIFY(!desktop->findChild<Namesdialog *>());

   /* nor if a name is being typed when the answer comes: what is being
      typed is left alone */
   backend->_asked = 0;
   backend->_hold = true;
   backend->_answer["name"] = "Second thoughts";
   select("testpdf.pdf");
   desktop->suggestName();
   QTRY_COMPARE(backend->_asked, 1);

   QModelIndex ind = model->index("testpdf.pdf", repo_ind);
   desktop->getModelconv()->indexToProxy(model, ind);
   view->renameStack(ind);
   Desktopeditor *editor = view->findChild<Desktopeditor *>();
   QVERIFY(editor);
   QVERIFY(view->editing());
   QString typed = editor->text();
   backend->release();
   QTRY_VERIFY((dialog = desktop->findChild<Namesdialog *>()));
   QCOMPARE(editor->text(), typed);
   QCOMPARE(dialog->names(), QStringList("Second thoughts"));
   dialog->reject();
   QTest::keyClick(editor, Qt::Key_Escape);
   QTRY_VERIFY(!desktop->findChild<Namesdialog *>());

   desktop->_name_engine = nullptr;
   desktop->_assistant = nullptr;
   QSettings().remove("assistant");
}

void TestDesktopUi::testDirFilterHidesOtherYears()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   // Year-named directories, one current and one old
   QString this_year = QString::number(QDate::currentDate().year());
   QString last_year = QString::number(QDate::currentDate().year() - 1);

   auto path = setupRepo();
   QVERIFY(QDir(path).mkdir(this_year));
   QVERIFY(QDir(path).mkdir(last_year));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   QVERIFY(!err);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   me.resize(1024, 768);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTest::qWait(50);

   /* the View->dir-filter action toggles the year/month filter: with
      it on, only directories for the current year are offered */
   me.actionDirFilter->setChecked(false);
   me.actionDirFilter->trigger();   // now on
   QVERIFY(desktop->findDir(path + "/" + this_year).isValid());
   QVERIFY(!desktop->findDir(path + "/" + last_year).isValid());

   me.actionDirFilter->trigger();   // off again
   QVERIFY(desktop->findDir(path + "/" + this_year).isValid());
   QVERIFY(desktop->findDir(path + "/" + last_year).isValid());
}



/* Progress is reported to the window, which puts a bar in the status bar
   for as long as an operation is running. An operation reports its
   progress with the event loop running, though, so the user can start
   another one while the first is still going: when that one finished it
   used to take the bar away, and the operation still running then
   reported its progress to a bar which was no longer there */
void TestDesktopUi::testOverlappingOperations()
{
   Mainwindow me;

   me.show ();
   QVERIFY (QTest::qWaitForWindowExposed (&me));

   Operation outer ("outer", 10, 0);

   outer.setProgress (1);
      {
      // an operation which starts and finishes while the first runs
      Operation inner ("inner", 10, 0);

      inner.setProgress (5);
      }

   // the first one is still going, and still has somewhere to report to
   outer.setProgress (2);

   /* an operation which asks for no count at all divides by it to work
      out a percentage */
   Operation unknown ("unknown", 0, 0);

   unknown.setProgress (3);
}


void TestDesktopUi::testPageNumberBackground()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopview *view = me.getDesktop()->getView();

   // a stack of several pages shows its page number
   QModelIndex ind;
   for (int row = 0; row < view->model()->rowCount(view->rootIndex()); row++) {
      QModelIndex i = itemIndex(view, row);
      if (view->model()->data(i, Desktopmodel::Role_pagecount).toInt() > 1)
         ind = i;
   }
   QVERIFY(ind.isValid());

   QColor button = QApplication::palette().color(QPalette::Button);
   QImage image = view->viewport()->grab(view->visualRect(ind)).toImage();
   QVERIFY(!image.isNull());

   // the bottom of the item, where the page number is
   int filled = 0, total = 0;
   for (int y = image.height() * 3 / 4; y < image.height(); y++)
      for (int x = 0; x < image.width(); x++, total++)
         if (image.pixelColor(x, y).rgb() == button.rgb())
            filled++;
   QVERIFY2(filled < total / 20,
            qPrintable(QString("%1 of %2 pixels are the button colour")
                       .arg(filled).arg(total)));
}


void TestDesktopUi::testStackIndexId()
{
   // a desk's ID is a small number
   QVERIFY(Desktopmodel::isDeskId(0));
   QVERIFY(Desktopmodel::isDeskId(12));

   /* a stack's is the address of its File, here one which a desktop
      once had, whose lower 32 bits are small enough to be a desk's */
   if (sizeof(quintptr) == 8)
      QVERIFY(!Desktopmodel::isDeskId(quintptr(0x7f5a00002120ULL)));
   QVERIFY(!Desktopmodel::isDeskId(quintptr(0x10000)));
}


void TestDesktopUi::testFolderIo()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   const IoMeasure &io = model->folderIo();
   QVERIFY(io.isActive());
   QVERIFY2(io.what().startsWith("folder "), qPrintable(io.what()));

   /* the stacks are read as they are drawn, and the reading of each is
      measured with it */
   QString path = model->getFile(model->index(0, 0, repo_ind))->pathname();
   QTRY_VERIFY(io.byPath().value(path).reads > 0);
   QVERIFY(io.byPath().value(path).opens > 0);
   QVERIFY(io.counts().readBytes >= io.byPath().value(path).readBytes);
   QVERIFY2(io.report().contains(QFileInfo(path).fileName() + ": read "),
            qPrintable(io.report()));
}


void TestDesktopUi::testStackIo()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   Pagewidget *page = desktop->_page;

   // the folder's stacks are read first, so they do not count below
   QModelIndex stack = model->index(0, 0, repo_ind);
   QTRY_VERIFY(model->folderIo().byPath().value(
                  model->getFile(stack)->pathname()).reads > 0);

   clickItem(view, 0);
   QTRY_VERIFY(!page->_image.isNull());

   // showing it is measured from the click, and drawing its page reads it
   QString name = model->data(stack).toString();
   QVERIFY(page->_ioShow.isActive());
   QCOMPARE(page->_ioShow.what(), "stack " + name);
   QVERIFY(page->_ioShow.counts().reads > 0);

   // as is filling the strip with its pages
   const IoMeasure &strip = page->_pagemodel->_ioStrip;
   QVERIFY(strip.isActive());
   QCOMPARE(strip.what(), "page strip of " + name);
   QTRY_VERIFY(strip.counts().reads > 0);
}


#define WAIT_OPS(model) \
    QTRY_VERIFY_WITH_TIMEOUT(!(model)->opsPending(), 10000)

QPixmap TestDesktopUi::shownPixmap(Desktopmodel *model,
                                   const QModelIndex &stack)
{
   return model->data(stack, Desktopmodel::Role_pixmap).value<QPixmap>();
}

QPixmap TestDesktopUi::pagePreview(File *f, int pagenum)
{
   QPixmap pixmap;

   if (f->getPreviewPixmap(pagenum, pixmap, false))
      return QPixmap();
   return pixmap;
}

void TestDesktopUi::testDeskCover()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   QUndoStack *undo = model->getUndoStack();

   QModelIndex stack = model->index(0, 0, repo_ind);
   File *f = model->getFile(stack);
   QCOMPARE(f->filename(), QString("testfile.max"));
   QModelIndex vind = itemIndex(view, 0);
   QCOMPARE(view->model()->data(vind, Qt::DisplayRole).toString(),
            QString("testfile"));

   // once read and looked through, the stack has no cover
   QTRY_COMPARE(f->coverState(), File::Cover_known);
   QCOMPARE(f->coverPage(), -1);
   const QPixmap first = pagePreview(f, 0);
   const QPixmap second = pagePreview(f, 1);
   QVERIFY(!first.isNull() && !second.isNull());
   QTRY_COMPARE(shownPixmap(model, stack).size(), first.size());
   const QSize wide = model->data(stack, Desktopmodel::Role_preview_maxsize)
                         .toSize();
   QVERIFY(wide.width() >= first.width());
   QTRY_VERIFY(view->visualRect(vind).isValid());
   const QRect before = view->visualRect(vind);

   // the right half of the first page is the front cover
   const QRectF half(0.5, 0, 0.5, 1);
   model->setCover(stack, 0, Pagecover::fromBox(half));
   WAIT_OPS(model);
   const QRect box = Imagetrim::areaToRect(first.size(), half);
   QCOMPARE(f->coverPage(), 0);
   QCOMPARE(f->coverSize(), box.size());

   // which is all the stack shows of the page, and is its size
   QPixmap shown = shownPixmap(model, stack);
   QCOMPARE(shown.size(), box.size());
   QCOMPARE(shown.toImage(), first.copy(box).toImage());
   QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize).toSize(),
            box.size());
   QTRY_VERIFY(view->visualRect(vind).width() < before.width());
   const QRect covered = view->visualRect(vind);

   /* stepping to the next page with the arrow on the stack shows it
      shrunk to fit the cover, so the stack stays the same size */
   desktop->slotItemClicked(vind, Desktopdelegate::Point_right);
   QCOMPARE(model->data(stack, Desktopmodel::Role_pagenum).toInt(), 1);
   shown = shownPixmap(model, stack);
   QCOMPARE(shown.size(), second.size().scaled(box.size(),
                                               Qt::KeepAspectRatio));
   QVERIFY(shown.width() <= box.width() && shown.height() <= box.height());
   QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize).toSize(),
            box.size());
   QTest::qWait(50);
   QCOMPARE(view->visualRect(vind).size(), covered.size());

   // and back again, the cover
   desktop->slotItemClicked(vind, Desktopdelegate::Point_left);
   QCOMPARE(shownPixmap(model, stack).toImage(), first.copy(box).toImage());
   desktop->slotItemClicked(vind, Desktopdelegate::Point_right);

   // with the cover cleared the page is shown whole, at the size it was
   model->setCover(stack, 0, Pagecover());
   WAIT_OPS(model);
   QCOMPARE(f->coverPage(), -1);
   QCOMPARE(shownPixmap(model, stack).size(), second.size());
   QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize).toSize(),
            wide);
   QTRY_COMPARE(view->visualRect(vind).size(), before.size());

   // undo gives the stack its cover again
   QCOMPARE(undo->undoText(), QString("Clear cover"));
   undo->undo();
   WAIT_OPS(model);
   QCOMPARE(f->coverPage(), 0);
   QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize).toSize(),
            box.size());
   QCOMPARE(shownPixmap(model, stack).size(),
            second.size().scaled(box.size(), Qt::KeepAspectRatio));
   QTRY_COMPARE(view->visualRect(vind).size(), covered.size());

   // and then takes it away
   undo->undo();
   WAIT_OPS(model);
   QCOMPARE(f->coverPage(), -1);
   QCOMPARE(shownPixmap(model, stack).size(), second.size());
   QTRY_COMPARE(view->visualRect(vind).size(), before.size());

   // a change to the cover page moves the cover with it
   undo->redo();
   WAIT_OPS(model);
   QCOMPARE(f->coverPage(), 0);
   model->setData(stack, 0, Desktopmodel::Role_pagenum);
   model->cropPage(stack, 0, QRectF(0.25, 0, 0.75, 1));
   WAIT_OPS(model);
   QPixmap cropped = pagePreview(f, 0);
   QRect moved = Imagetrim::areaToRect(cropped.size(),
                                       QRectF(1.0 / 3, 0, 2.0 / 3, 1));
   QVERIFY(qAbs(f->coverSize().width() - moved.width()) <= 1);
   QCOMPARE(shownPixmap(model, stack).size(), f->coverSize());

   // a whole page as the cover is shown whole, and sets the size
   model->setCover(stack, 1, Pagecover::wholePage());
   WAIT_OPS(model);
   QCOMPARE(f->coverPage(), 1);
   QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize).toSize(),
            second.size());
   QCOMPARE(shownPixmap(model, stack).size(), cropped.size());
   QVERIFY(model->setData(stack, 1, Desktopmodel::Role_pagenum));
   QCOMPARE(shownPixmap(model, stack).toImage(), second.toImage());
}

void TestDesktopUi::testDeskCoverFound()
{
   QString path = setupRepo();
   const QRectF middle(0.25, 0.25, 0.5, 0.5);
   QSize third;

   // the middle of the third page is the cover, before the folder is opened
   {
      Filemax max(path + "/", "testfile.max", nullptr);
      QMap<QString, QString> props;

      QVERIFY(!max.load());
      props["cover"] = Pagecover::fromBox(middle).toString();
      QVERIFY(!max.putPageProps(2, props));
      third = Imagetrim::areaToRect(pagePreview(&max, 2).size(), middle)
                 .size();
      QVERIFY(!third.isEmpty());
   }

   {
      Mainwindow me;
      Desktopwidget *desktop = me.getDesktop();
      QVERIFY(!desktop->addDir(path));
      me.resize(1600, 1000);
      me.show();
      QVERIFY(QTest::qWaitForWindowExposed(&me));

      Desktopmodel *model = desktop->getModel();
      QModelIndex repo_ind = model->index(0, 0, QModelIndex());
      QModelIndex stack = model->index(0, 0, repo_ind);
      File *f = model->getFile(stack);
      QCOMPARE(f->filename(), QString("testfile.max"));

      // the stack is looked through once read, and still opens at page 1
      QTRY_COMPARE(f->coverState(), File::Cover_known);
      QCOMPARE(f->coverPage(), 2);
      QCOMPARE(f->coverSize(), third);
      QCOMPARE(model->data(stack, Desktopmodel::Role_pagenum).toInt(), 0);
      QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize)
                  .toSize(), third);
      QSize page = pagePreview(f, 0).size();
      if (page.width() > third.width() || page.height() > third.height())
         page.scale(third, Qt::KeepAspectRatio);
      QCOMPARE(shownPixmap(model, stack).size(), page);

      // the cover page shows the cover
      QVERIFY(model->setData(stack, 2, Desktopmodel::Role_pagenum));
      QPixmap cover = pagePreview(f, 2);
      QCOMPARE(shownPixmap(model, stack).toImage(),
               cover.copy(Imagetrim::areaToRect(cover.size(), middle))
                  .toImage());
      QVERIFY(model->setData(stack, 0, Desktopmodel::Role_pagenum));
      desktop->closing();
   }

   // the folder's layout notes the cover, with the stack's time and size
   QFile layout(path + "/.paperdesk");
   QVERIFY(layout.open(QIODevice::ReadOnly));
   QFileInfo info(path + "/testfile.max");
   QString expect = QString(",2,%1,%2,%3,%4,%5\n")
                       .arg(info.lastModified().toSecsSinceEpoch())
                       .arg(info.size()).arg(third.width())
                       .arg(third.height())
                       .arg(Pagecover::fromBox(middle).toString());
   QString saved = QString::fromUtf8(layout.readAll());
   QVERIFY2(saved.contains(expect), qPrintable(saved));
   layout.close();

   /* so next time the stack has the cover's size before it is read, and
      is not looked through again */
   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));
   Desktopmodel *model = desktop->getModel();
   QModelIndex stack = model->index(0, 0, model->index(0, 0, QModelIndex()));
   File *f = model->getFile(stack);
   QVERIFY(!f->valid());
   QCOMPARE(f->coverState(), File::Cover_noted);
   QCOMPARE(model->data(stack, Desktopmodel::Role_preview_maxsize).toSize(),
            third);

   me.resize(1600, 1000);
   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));
   QTRY_VERIFY(f->valid() && f->hasImage());
   QCOMPARE(f->coverState(), File::Cover_known);
   QVERIFY(model->_pending_cover_list.isEmpty());
   QCOMPARE(f->coverPage(), 2);
}


/* the image of each page of a stack, as the page view gets it */
static QList<QImage> stripImages(File *f)
{
   QList<QImage> images;

   for (int i = 0; i < f->pagecount(); i++)
      {
      QImage image;
      QSize size, trueSize;
      int bpp;

      if (f->getImage(i, false, image, size, trueSize, bpp, false))
         image = QImage();
      images << image;
      }
   return images;
}

void TestDesktopUi::testDropIntoPages()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   Pagewidget *page = desktop->_page;
   QString path = desktop->getSelectedPath();

   // A picture beside the two stacks, the only page of its size
   QImage picture(testSrc + "/colour_plasma.jpg");
   QVERIFY(!picture.isNull());
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", path + "/picture.jpg"));
   QStringList added("picture.jpg");
   model->addFilesToDesk(path + "/", added);
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);

   // which page of the max stack the picture is, or -1
   auto find = [&]() {
      File *f = model->getFile(model->index("testfile.max", repo_ind));
      QList<QImage> images = stripImages(f);
      int found = -1;

      for (int i = 0; i < images.size(); i++)
         if (images[i].size() == picture.size())
            found = i;
      return found;
   };
   auto shownName = [&]() {
      QModelIndex shown;

      return page->getCurrentIndex(shown, true)
         ? model->data(shown, Desktopmodel::Role_filename).toString()
         : QString();
   };

   // The max stack is the one shown
   QModelIndex stack = model->index("testfile.max", repo_ind);
   QModelIndex ind = stack;
   desktop->getModelconv()->indexToProxy(model, ind);
   clickItem(view, ind.row());

   Pageview *pageview = page->_pageview;
   QAbstractItemModel *pages = pageview->model();
   QTRY_COMPARE(pages->rowCount(), 5);
   QTRY_COMPARE(shownName(), QString("testfile.max"));
   QVERIFY(pageview->moveAllowed());
   QCOMPARE(find(), -1);

   // The stack shown does not go into its own pages
   emit pageview->stacksDropped(QStringList("testfile.max"), 1);
   QCOMPARE(model->getFile(stack)->pagecount(), 5);
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 3);

   // The picture's stack, dropped before the third page, becomes that page
   emit pageview->stacksDropped(QStringList("picture.jpg"), 2);
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
   stack = model->index("testfile.max", repo_ind);
   QCOMPARE(model->getFile(stack)->pagecount(), 6);
   QCOMPARE(find(), 2);
   QTRY_COMPARE(pages->rowCount(), 6);
   QCOMPARE(shownName(), QString("testfile.max"));
   ind = stack;
   desktop->getModelconv()->indexToProxy(model, ind);
   QTRY_COMPARE(view->currentIndex(), ind);

   // Undo makes it a stack again
   me.actionUndo->trigger();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
   stack = model->index("testfile.max", repo_ind);
   QCOMPARE(model->getFile(stack)->pagecount(), 5);
   QCOMPARE(find(), -1);
   QVERIFY(QFile::exists(path + "/picture.jpg"));

   // A file from another program is taken too, where it is dropped
   ind = stack;
   desktop->getModelconv()->indexToProxy(model, ind);
   clickItem(view, ind.row());
   QTRY_COMPARE(pages->rowCount(), 5);
   QTRY_COMPARE(shownName(), QString("testfile.max"));

   QTemporaryDir other;
   QString file = other.path() + "/photo.jpg";
   QString notes = other.path() + "/notes.txt";
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", file));
   QFile text(notes);
   QVERIFY(text.open(QIODevice::WriteOnly));
   text.write("some notes");
   text.close();

   QTRY_VERIFY(pageview->visualRect(pages->index(4, 0)).isValid());
   QRect rect = pageview->visualRect(pages->index(3, 0));
   bool across = pageview->visualRect(pages->index(4, 0)).top() == rect.top();
   QPoint farHalf = across ? QPoint(rect.right() - 2, rect.center().y())
                           : QPoint(rect.center().x(), rect.bottom() - 2);
   QCOMPARE(pageview->dropRow(farHalf), 4);

   // a file which cannot be a stack is refused
   QMimeData mime;
   mime.setUrls({QUrl::fromLocalFile(notes)});
   QVERIFY(!dropOn(pageview, &mime, farHalf));

   mime.setUrls({QUrl::fromLocalFile(file)});
   QVERIFY(dropOn(pageview, &mime, farHalf));
   QTRY_COMPARE(model->getFile(model->index("testfile.max", repo_ind))
                   ->pagecount(), 6);
   QCOMPARE(find(), 4);
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 3);
   QVERIFY(QFile::exists(file));
   QTRY_COMPARE(pages->rowCount(), 6);

   me.actionUndo->trigger();
   QTRY_COMPARE(model->getFile(model->index("testfile.max", repo_ind))
                   ->pagecount(), 5);
   QCOMPARE(find(), -1);
   QCOMPARE(view->model()->rowCount(view->rootIndex()), 3);
}

void TestDesktopUi::testReorderInStrip()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   /* the page list beside the desk, which follows the model's changes and
      its commits of a scan, as the user sees it */
   setupShown(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();
   Pagewidget *page = desktop->_page;
   clickItem(desktop->getView(), 0);

   Pageview *pageview = page->_pageview;
   QAbstractItemModel *pages = pageview->model();
   QTRY_COMPARE(pages->rowCount(), 5);
   QModelIndex stack = model->index(0, 0, repo_ind);
   QList<QImage> before = stripImages(model->getFile(stack));

   // pages can be moved in the usual mode
   QVERIFY(pageview->moveAllowed());

   /* a press on a page's image can start a drag, and a press on its
      caption or between pages sweeps out a selection */
   auto mouse = [&](QEvent::Type type, const QPoint &pos,
                    Qt::MouseButton button) {
      QPointF at(pos);
      QPointF global(pageview->viewport()->mapToGlobal(pos));
      QMouseEvent event(type, at, global,
                        button, type == QEvent::MouseMove ? Qt::LeftButton
                                                          : button,
                        Qt::NoModifier);
      QApplication::sendEvent(pageview->viewport(), &event);
   };
   auto caption = [&](int row) {
      QRect rect = pageview->visualRect(pages->index(row, 0));
      return QPoint(rect.center().x(), rect.bottom() - 6);
   };
   QRect first = pageview->visualRect(pages->index(0, 0));
   QPoint image(first.center().x(), first.top() + first.height() / 3);

   // the image has to have come for there to be one to press on
   QTRY_VERIFY((mouse(QEvent::MouseButtonPress, image, Qt::LeftButton),
                mouse(QEvent::MouseButtonRelease, image, Qt::LeftButton),
                pageview->dragEnabled()));

   mouse(QEvent::MouseButtonPress, caption(0), Qt::LeftButton);
   QVERIFY(!pageview->dragEnabled());
   mouse(QEvent::MouseMove, caption(0) + QPoint(0, 20), Qt::NoButton);
   mouse(QEvent::MouseMove, caption(1), Qt::NoButton);
   mouse(QEvent::MouseButtonRelease, caption(1), Qt::LeftButton);
   QVERIFY(pageview->selectionModel()->isRowSelected(0, QModelIndex()));
   QVERIFY(pageview->selectionModel()->isRowSelected(1, QModelIndex()));
   QVERIFY(!pageview->selectionModel()->isRowSelected(2, QModelIndex()));

   /* a drop on the near half of a page goes before it, else after it: the
      left half where the pages are side by side, the top half in a column,
      as here; between pages, before the next */
   QRect rect = pageview->visualRect(pages->index(2, 0));
   bool across = pageview->visualRect(pages->index(3, 0)).top() == rect.top();
   QPoint nearHalf = across ? QPoint(rect.left() + 2, rect.center().y())
                            : QPoint(rect.center().x(), rect.top() + 2);
   QPoint farHalf = across ? QPoint(rect.right() - 2, rect.center().y())
                           : QPoint(rect.center().x(), rect.bottom() - 2);
   QPoint gap = across ? QPoint(rect.right() + 2, rect.center().y())
                       : QPoint(rect.center().x(), rect.bottom() + 2);
   QCOMPARE(pageview->dropRow(nearHalf), 2);
   QCOMPARE(pageview->dropRow(farHalf), 3);
   QVERIFY(!pageview->indexAt(gap).isValid());
   QCOMPARE(pageview->dropRow(gap), 3);
   QRect last = pageview->visualRect(pages->index(4, 0));
   QCOMPARE(pageview->dropRow(QPoint(last.right() + 2, last.bottom() + 40)),
            5);

   // the pages move as they are, with nothing read again for the others
   QSignalSpy resets(pages, &QAbstractItemModel::modelReset);
   QSignalSpy moves(pages, &QAbstractItemModel::layoutChanged);

   // Ctrl+Left moves the fourth page, shown, one earlier, and keeps it shown
   QTest::mouseClick(pageview->viewport(), Qt::LeftButton, Qt::NoModifier,
                     pageview->visualRect(pages->index(3, 0)).center());
   QCOMPARE(page->getCurrentPage(), 3);
   QString stackPath = model->getFile(stack)->pathname();
   IoStats::reset();
   QTest::keyClick(pageview, Qt::Key_Left, Qt::ControlModifier);

   // selecting a stack has not read all of it, so only its table is written
   QCOMPARE(IoStats::forPath(stackPath).writes, qint64(1));
   QList<QImage> after = stripImages(model->getFile(stack));
   QCOMPARE(after, QList<QImage>({before[0], before[1], before[3], before[2],
                                  before[4]}));
   QTRY_COMPARE(page->getCurrentPage(), 2);

   // each page is numbered and named for where it is now
   for (int row = 0; row < 5; row++)
      {
      QModelIndex ind = pages->index(row, 0);
      Pageinfo *info = pages->data(ind, Pagemodel::Role_pageinfo)
                          .value<Pageinfo *>();

      QCOMPARE(info->pagenum(), row + 1);
      QCOMPARE(info->pagename(), pages->data(ind).toString());
      }
   QVERIFY(pageview->selectionModel()->isRowSelected(2, QModelIndex()));

   // Ctrl+Left at the start, or Ctrl+Right at the end, does nothing
   pageview->selectionModel()->select(pages->index(0, 0),
                                      QItemSelectionModel::ClearAndSelect);
   QTest::keyClick(pageview, Qt::Key_Up, Qt::ControlModifier);
   QCOMPARE(stripImages(model->getFile(stack)), after);

   /* a page dropped at the end goes last, its mark for removal with it;
      the others have none */
   QVERIFY(pages->setData(pages->index(0, 0), true, Pagemodel::Role_remove));
   page->slotPagesMoved({0}, 5);
   QCOMPARE(stripImages(model->getFile(stack)),
            QList<QImage>({after[1], after[2], after[3], after[4], after[0]}));
   for (int row = 0; row < 5; row++)
      QCOMPARE(pages->data(pages->index(row, 0),
                           Pagemodel::Role_remove).toBool(), row == 4);

   // and undo puts them back, and the marks with them
   Desktopundostack *stk = model->getUndoStack();
   stk->undo();
   QVERIFY(pages->data(pages->index(0, 0), Pagemodel::Role_remove).toBool());
   stk->undo();
   QCOMPARE(stripImages(model->getFile(stack)), before);
   QCOMPARE(resets.count(), 0);
   QCOMPARE(moves.count(), 4);
}


void TestDesktopUi::testViewModeRestored()
{
   auto tidy = qScopeGuard([] {
      QSettings qs;

      qs.remove("desktopwidget");
      qs.remove("mainwidget");
   });

   // open a window and give it to @p fn, then close it, as on quitting
   auto session = [&](const std::function<void(Pagewidget *)> &fn) {
      Mainwindow me;

      me.resize(1600, 1000);
      me.show();
      QVERIFY(QTest::qWaitForWindowExposed(&me));
      fn(me.getDesktop()->_page);
      me.close();
   };

   // the first time, the pointer
   session([&](Pagewidget *page) {
      QCOMPARE(page->_mode, Pagewidget::Mode_select);
      QVERIFY(page->_tools->selectMode->isChecked());
      page->_tools->moveMode->click();
      QCOMPARE(page->_mode, Pagewidget::Mode_move);
   });

   // the hand comes back, with its tool shown as chosen
   session([&](Pagewidget *page) {
      QCOMPARE(page->_mode, Pagewidget::Mode_move);
      QVERIFY(page->_tools->moveMode->isChecked());
      QVERIFY(!page->_tools->selectMode->isChecked());

      // quitting during a scan, the tool to come back to is the one chosen
      page->_tools->scanMode->setChecked(true);
      page->_prescan_mode = Pagewidget::Mode_move;
      QCOMPARE(page->_mode, Pagewidget::Mode_scan);
   });
   session([&](Pagewidget *page) {
      QCOMPARE(page->_mode, Pagewidget::Mode_move);
   });

   // and a setting which makes no sense is the pointer
   QSettings().setValue("desktopwidget/mode", 99);
   session([&](Pagewidget *page) {
      QCOMPARE(page->_mode, Pagewidget::Mode_select);
      QVERIFY(page->_tools->selectMode->isChecked());
   });
}


void TestDesktopUi::testViewModeLayout()
{
   auto tidy = qScopeGuard([] {
      QSettings qs;

      qs.remove("desktopwidget");
      qs.remove("mainwidget");
   });
   auto similar = [](const QList<int> &got, const QList<int> &want) {
      if (got.size() != want.size())
         return false;
      for (int i = 0; i < got.size(); i++)
         if (qAbs(got[i] - want[i]) > 40)
            return false;
      return true;
   };
   QList<int> select, move;

   // a layout for each of two modes, quitting in the second
   {
      Mainwindow me;

      me.resize(1600, 1000);
      me.show();
      QVERIFY(QTest::qWaitForWindowExposed(&me));
      Desktopwidget *desktop = me.getDesktop();

      desktop->setSizes({200, 800, 600});
      select = desktop->sizes();
      desktop->_page->_tools->moveMode->click();
      desktop->setSizes({200, 300, 1100});
      move = desktop->sizes();
      QVERIFY(!similar(select, move));
      me.close();
   }

   /* starting in the other mode, as settings saved before the mode was do,
      gives that mode's layout, not the one left at the last exit */
   QSettings().remove("desktopwidget/mode");
   {
      Mainwindow me;

      me.resize(1600, 1000);
      me.show();
      QVERIFY(QTest::qWaitForWindowExposed(&me));
      Desktopwidget *desktop = me.getDesktop();

      QCOMPARE(desktop->_page->_mode, Pagewidget::Mode_select);
      QVERIFY2(similar(desktop->sizes(), select),
               qPrintable(QString("%1,%2,%3").arg(desktop->sizes()[0])
                          .arg(desktop->sizes()[1]).arg(desktop->sizes()[2])));

      // and the mode left is as it was
      desktop->_page->_tools->moveMode->click();
      QVERIFY(similar(desktop->sizes(), move));
   }
}


void TestDesktopUi::testDragImage()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();
   clickItem(desktop->getView(), 0);

   Pageview *pageview = desktop->_page->_pageview;
   QAbstractItemModel *pages = pageview->model();
   QTRY_COMPARE(pages->rowCount(), 5);

   // the image of the page has to have come for it to be shown
   QModelIndex first = pages->index(0, 0);
   Pageinfo *info = first.data(Pagemodel::Role_pageinfo).value<Pageinfo *>();
   bool dodgy = true;
   QTRY_VERIFY((info->pixmap(dodgy), !dodgy));

   /* a small page, the same size however many are dragged, which is more
      than the one page's image would be */
   QPixmap one = pageview->dragPixmap(first, 1);
   QPixmap five = pageview->dragPixmap(first, 5);
   QVERIFY(!one.isNull());
   QCOMPARE(five.size(), one.size());
   QVERIFY2(one.width() < 100 && one.height() < 100,
            qPrintable(QString("%1x%2").arg(one.width()).arg(one.height())));

   // and only several say how many
   QVERIFY(one.toImage() != five.toImage());
}


void TestDesktopUi::testDropMarker()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();
   clickItem(desktop->getView(), 0);

   Pageview *pageview = desktop->_page->_pageview;
   QAbstractItemModel *pages = pageview->model();
   QTRY_COMPARE(pages->rowCount(), 5);
   auto rect = [&](int row) {
      return pageview->visualRect(pages->index(row, 0));
   };

   // in a column: a line across the gap above the page dropped on
   QVERIFY(rect(0).left() == rect(1).left());
   QLine line = pageview->dropLine(QPoint(rect(2).center().x(),
                                          rect(2).top() + 2));
   QCOMPARE(line.y1(), line.y2());
   QVERIFY(line.y1() >= rect(1).bottom() && line.y1() <= rect(2).top());
   QCOMPARE(line.x1(), rect(2).left());
   QCOMPARE(line.x2(), rect(2).right());

   // or below the last, past the end
   line = pageview->dropLine(QPoint(rect(4).center().x(),
                                    rect(4).bottom() + 40));
   QCOMPARE(line.y1(), line.y2());
   QVERIFY(line.y1() >= rect(4).bottom());

   // widen the list to rows of a few pages, which run on to the next row
   pageview->setFixedWidth(rect(0).width() * 3 + 40);
   QTRY_VERIFY(rect(0).top() == rect(1).top()
               && rect(0).top() != rect(4).top());
   int wrap = 1;
   while (rect(wrap).top() == rect(0).top())
      wrap++;

   // a line down the gap between two pages side by side
   line = pageview->dropLine(QPoint(rect(0).right() - 2, rect(0).center().y()));
   QCOMPARE(line.x1(), line.x2());
   QVERIFY(line.x1() >= rect(0).right() && line.x1() <= rect(1).left());
   QCOMPARE(line.y1(), rect(0).top());

   // past the last of a row, beside it, and not by the first of the next
   QPoint end(rect(wrap - 1).right() - 2, rect(wrap - 1).center().y());
   QCOMPARE(pageview->dropRow(end), wrap);
   line = pageview->dropLine(end);
   QCOMPARE(line.x1(), line.x2());
   QVERIFY(line.x1() >= rect(wrap - 1).right());
   QCOMPARE(line.y1(), rect(wrap - 1).top());

   // and before the first of the next, beside that
   QPoint start(rect(wrap).left() + 2, rect(wrap).center().y());
   QCOMPARE(pageview->dropRow(start), wrap);
   line = pageview->dropLine(start);
   QVERIFY(line.x1() <= rect(wrap).left());
   QCOMPARE(line.y1(), rect(wrap).top());
}


void TestDesktopUi::testAddServer()
{
   QTemporaryDir serverDir, configDir;
   QVERIFY(serverDir.isValid());
   QVERIFY(configDir.isValid());
   QString repoRoot = serverDir.path() + "/papers";
   QVERIFY(QDir().mkpath(repoRoot + "/banks"));

   // keep the servers which the person running the test has out of it
   qputenv("PAPERMAN_CONFIG",
           (configDir.path() + "/client.conf").toLocal8Bit());
   auto restoreConfig = qScopeGuard([] { qunsetenv("PAPERMAN_CONFIG"); });

   const quint16 port = 9884;
   SearchServer server(repoRoot, port);
   QVERIFY(server.start());
   QTest::qWait(100);

   Mainwindow me;
   Dirmodel *dirs = me.getDesktop()->getDirmodel();
   QUrl url(QString("http://localhost:%1").arg(port));
   int before = dirs->rowCount(QModelIndex());
   QVERIFY(me.menuFile->actions().contains(me.actionAddServer));

   // A machine and port are enough: the server is looked for there
   QString result = "pending";
   me.addServer(QString("localhost:%1").arg(port),
                [&](const QString &error) { result = error; });
   QTRY_COMPARE_WITH_TIMEOUT(result, QString(), 10000);
   QVERIFY(dirs->hasServer(url));
   QCOMPARE(dirs->rowCount(QModelIndex()), before + 1);

   // It is remembered for next time
   QList<ServerEntry> listed = ClientConfig::load();
   QCOMPARE(listed.size(), 1);
   QVERIFY(ClientConfig::sameServer(listed[0].url, url));

   // Adding it again changes nothing
   result = "pending";
   me.addServer(url.toString() + "/",
                [&](const QString &error) { result = error; });
   QTRY_VERIFY_WITH_TIMEOUT(result != "pending", 10000);
   QVERIFY(!result.isEmpty());
   QCOMPARE(dirs->rowCount(QModelIndex()), before + 1);
   QCOMPARE(ClientConfig::load().size(), 1);

   // A place with no server is reported and not remembered
   result = "pending";
   me.addServer(QString("http://localhost:%1").arg(port + 1),
                [&](const QString &error) { result = error; });
   QTRY_VERIFY_WITH_TIMEOUT(result != "pending", 10000);
   QVERIFY(!result.isEmpty());
   QCOMPARE(dirs->rowCount(QModelIndex()), before + 1);
   QCOMPARE(ClientConfig::load().size(), 1);

   server.stop();
}
