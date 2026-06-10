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
