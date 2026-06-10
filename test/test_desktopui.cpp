#include <QClipboard>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest/QtTest>

#include "test.h"

#include "desktopdelegate.h"
#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "dirview.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pagewidget.h"
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

   QTest::qWait(350);
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
   QTest::qWait(350);
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
   QTest::qWait(350);
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
   QTimer::singleShot(1000, main, [main]() { main->stopScan(false); });
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
