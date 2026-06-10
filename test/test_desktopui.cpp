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
