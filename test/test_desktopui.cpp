#include <QPushButton>
#include <QtTest/QtTest>

#include "test.h"

#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pagewidget.h"
#include "test_desktopui.h"

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

   // Show the window so that items have real positions and can be
   // clicked, as a user would
   me->resize(1024, 768);
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

   QRect rect = view->visualRect(ind);
   QVERIFY(rect.isValid());
   QTest::mouseClick(view->viewport(), Qt::LeftButton, modifiers,
                     rect.center());
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

   // Let the preview timer fire so it doesn't outlive the window
   QTest::qWait(350);
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

   QTest::qWait(350);
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

   QTest::qWait(350);
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

   QTest::qWait(350);
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

   QTest::qWait(350);
}

/* URLs which the email operations would open in a browser are captured
   here instead, so tests never launch anything */
static QUrl s_opened_url;

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

   QTest::qWait(350);
}
