#include <QClipboard>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QtTest/QtTest>

#include "test.h"

#include "desktopdelegate.h"
#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "filemax.h"
#include "utils.h"
#include "dirview.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pagemodel.h"
#include "pageview.h"
#include "pagewidget.h"
#include "searchserver.h"
#include "qxmlconfig.h"
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

/* URLs which the email operations would open in a browser are captured
   here instead, so tests never launch anything */
static QUrl s_opened_url;

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
   Desktopmodel::url_capture = &s_opened_url;
   s_opened_url = QUrl();

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

   // A Gmail compose window should have been requested
   QCOMPARE(s_opened_url.host(), QString("mail.google.com"));

}

void TestDesktopUi::testEmailMultipleStacksZips()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopmodel::url_capture = &s_opened_url;
   s_opened_url = QUrl();

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

   QCOMPARE(s_opened_url.host(), QString("mail.google.com"));

   QFile::remove(zip);
}

void TestDesktopUi::testEmailAsPdfConverts()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopmodel::url_capture = &s_opened_url;
   s_opened_url = QUrl();

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

   QCOMPARE(s_opened_url.host(), QString("mail.google.com"));

   QFile::remove(pdf);
}

void TestDesktopUi::testCopyAsPdf()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);
   Desktopmodel::url_capture = &s_opened_url;
   s_opened_url = QUrl();

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

   // copying does not open a browser window, unlike emailing
   QVERIFY(s_opened_url.isEmpty());

   QFile::remove(pdf);
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
   QString err = desktop->addRemoteServer(url);
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

   // the server's files have moved
   QTRY_VERIFY(QFile::exists(path + "/main/testfile.max"));
   QVERIFY(QFile::exists(path + "/main/testpdf.pdf"));
   QVERIFY(QFile::exists(path + "/main/colour_plasma.jpg"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 0);

   // one undo moves the whole group back on the server
   me.actionUndo->trigger();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QVERIFY(QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(QFile::exists(path + "/colour_plasma.jpg"));
   QVERIFY(!QFile::exists(path + "/main/testfile.max"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);

   delete mime;
   server.stop();
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

void TestDesktopUi::testScanIntoStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   setupShown(&me, model, repo_ind);

   /* select the simulated scanner as the last-used device so that
      ensureScanner() opens it without showing the selection dialog;
      restore the user's setting afterwards */
   if (!xmlConfig)
      new QXmlConfig();
   QString old_device = xmlConfig->stringValue("LAST_DEVICE", QString());
   xmlConfig->setStringValue("LAST_DEVICE", "simulscan");

   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);
   int before = model->rowCount(repo_ind);

   /* the simulated ADF holds 120 pages, so press the stop button (which
      finishes the current page and ends the scan) shortly after the
      scan begins */
   QTimer::singleShot(400, main, [main]() { main->stopScan(false); });
   me.actionScango->trigger();

   xmlConfig->setStringValue("LAST_DEVICE", old_device);

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

   QCOMPARE(Mainwindow::runScan(path, "inbox", "simulscan", 2,
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
   QCOMPARE(Mainwindow::runScan(path, "nosuch", "simulscan", 1), 1);

   utilSetHeadless(false);
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

