#include <QDate>
#include <QPrinter>
#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "file.h"
#include "dirview.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "options.h"
#include "printopt.h"
#include "qxmlconfig.h"
#include "test_ops.h"

void TestOps::testStartup()
{
   Mainwindow *me;

   me = new Mainwindow();
   delete me;
}

void TestOps::testAddRepos()
{
   Mainwindow me;

   Desktopwidget *desktop = me.getDesktop ();

   // Add our test repo
   auto path = setupRepo();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);
}

void TestOps::testDuplicate()
{
   Mainwindow me;

   QModelIndex repo_ind;
   duplicate(&me, repo_ind);
}

void TestOps::testDuplicateUndo()
{
   Mainwindow me;

   QModelIndex repo_ind;
   duplicate(&me, repo_ind);

   Desktopwidget *desktop = me.getDesktop ();
   Desktopmodel *model = desktop->getModel();

   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();

   // We should be back to two files
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
}

void TestOps::testDuplicateStackUndo()
{
   Mainwindow me;

   QModelIndex repo_ind;
   duplicate(&me, repo_ind);

   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView();
   view->addSelectionRange(0, 1);

   // Stack the two max files
   desktop->stackPages();

   // Should be back to two items
   Desktopmodel *model = desktop->getModel();
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);

   // Now undo, to get back to three items
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);
}

void TestOps::testUnstackPage()
{
   Mainwindow me;

   QModelIndex repo_ind;
   duplicate(&me, repo_ind);

   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView();
   view->addSelectionRange(0, 1);

   // Stack the two max files
   desktop->stackPages();

   Desktopmodel *model = desktop->getModel();
   QModelIndex max_ind = model->index(0, 0, repo_ind);
   Q_ASSERT(max_ind.isValid());
   QCOMPARE(model->data(max_ind, Qt::DisplayRole).toString(), "testfile");

   File *max = model->getFile(max_ind);
   Q_ASSERT(max);
   QCOMPARE(max->typeName(), "Max");
   QCOMPARE(max->pagecount(), 10);

   // unstack the first page
   desktop->unstackPage();
   QCOMPARE(max->pagecount(), 9);

   // We now expect three files
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   // Check that the unstacked page looks OK
   QModelIndex page_ind = model->index(2, 0, repo_ind);
   Q_ASSERT(page_ind.isValid());
   QCOMPARE(model->data(page_ind, Qt::DisplayRole).toString(),
            "Page 1");

   File *page = model->getFile(page_ind);
   Q_ASSERT(page);
   QCOMPARE(page->pagecount(), 1);

   // Now undo the unstack-page
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
   QCOMPARE(max->pagecount(), 10);
}

void TestOps::testDuplicateMax()
{
   Mainwindow me;

   QModelIndex repo_ind;
   QModelIndex max_ind;
   prepareDuplicate(&me, repo_ind, max_ind);

   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView();
   Desktopmodel *model = desktop->getModel();
   view->setSelectionRange(1, 1);

   qDebug() << "broken on latest podofo";
   return;
   // Duplicate the PDF file as Max
   desktop->duplicateMax();

   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   QModelIndex dup_ind = model->index(2, 0, repo_ind);
   Desktopmodelconv *modelconv = desktop->getModelconv();

   // Make sure that the new stack is selected
   QModelIndex ind;
   bool has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testpdf_copy");

   File *max2 = model->getFile(ind);
   Q_ASSERT(max2);
   QCOMPARE(max2->typeName(), "Max");

   // Now duplicate this new max file as a PDF
   desktop->duplicatePdf();

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 4);

   // Make sure that the new stack is selected
   dup_ind = model->index(3, 0, repo_ind);
   has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(),
            "testpdf_copy_copy.pdf");

   File *pdf = model->getFile(ind);
   Q_ASSERT(max2);
   QCOMPARE(pdf->typeName(), "PDF");

   // Now undo everything
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   Q_ASSERT(!stk->canUndo());

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
}

void TestOps::testDuplicateEvenOdd()
{
   Mainwindow me;

   QModelIndex max_ind;
   QModelIndex repo_ind;
   prepareDuplicate(&me, repo_ind, max_ind);

   // Duplicate the max file
   Desktopwidget *desktop = me.getDesktop();
   desktop->duplicateEven();

   Desktopmodel *model = desktop->getModel();
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   QModelIndex dup_ind = model->index(2, 0, repo_ind);
   Desktopmodelconv *modelconv = desktop->getModelconv();

   // Make sure that the new stack is selected
   QModelIndex ind = dup_ind;
   bool has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile_copy");

   File *max = model->getFile(dup_ind);
   Q_ASSERT(max);
   QCOMPARE(max->typeName(), "Max");
   QCOMPARE(max->pagecount(), 2);

   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 1);
   desktop->duplicateOdd();

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 4);

   // Make sure that the new stack is selected
   ind = dup_ind;
   has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile_copy1");

   File *max2 = model->getFile(dup_ind);
   Q_ASSERT(max2);
   QCOMPARE(max2->typeName(), "Max");
   QCOMPARE(max2->pagecount(), 2);
}

void TestOps::testBookletOrder()
{
   // A four-sheet booklet unfolds to eight pages. Counting sheets from the
   // outside in, the reading order is [8|1] [2|7] [6|3] [4|5]
   QVector<QPair<int, bool> > order = File::bookletOrder(4);
   QCOMPARE(order.size(), 8);

   // page 1 is the right half of the outer side (scan 0)
   QCOMPARE(order[0], qMakePair(0, false));
   // page 2 is the left half of the next side (scan 1)
   QCOMPARE(order[1], qMakePair(1, true));
   // page 3 is the right half of scan 2
   QCOMPARE(order[2], qMakePair(2, false));
   // page 4 is the left half of scan 3 (innermost)
   QCOMPARE(order[3], qMakePair(3, true));
   // page 5 is the right half of scan 3
   QCOMPARE(order[4], qMakePair(3, false));
   // page 6 is the left half of scan 2
   QCOMPARE(order[5], qMakePair(2, true));
   // page 7 is the right half of scan 1
   QCOMPARE(order[6], qMakePair(1, false));
   // page 8 is the left half of the outer side (scan 0)
   QCOMPARE(order[7], qMakePair(0, true));

   // Every source page is used for exactly one left and one right half
   QVector<int> lefts(4, 0), rights(4, 0);
   for (auto &slot : order)
      (slot.second ? lefts : rights)[slot.first]++;
   for (int i = 0; i < 4; i++)
      {
      QCOMPARE(lefts[i], 1);
      QCOMPARE(rights[i], 1);
      }
}

void TestOps::testUnfoldBooklet()
{
   Mainwindow me;

   QModelIndex max_ind;
   QModelIndex repo_ind;
   prepareDuplicate(&me, repo_ind, max_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopmodel *model = desktop->getModel();

   // The test booklet has four scanned pages
   File *booklet = model->getFile(max_ind);
   Q_ASSERT(booklet);
   int pages = booklet->pagecount();
   QCOMPARE(pages, 4);

   // Unfold the booklet (only the max file is selected)
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 0);
   desktop->unfoldBooklet();

   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   // The new stack is selected, named after the source and holds twice the
   // number of pages
   QModelIndex ind;
   bool has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   Desktopmodelconv *modelconv = desktop->getModelconv();
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile_unfold");

   File *unfolded = model->getFile(ind);
   Q_ASSERT(unfolded);
   QCOMPARE(unfolded->typeName(), "Max");
   QCOMPARE(unfolded->pagecount(), 2 * pages);

   // Undo removes the new stack
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
}

void TestOps::testDeleteStacks()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop ();

   // Delete the first stack
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 1);

   desktop->doDeleteStacks(false);

   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 1);

   QFile fil(trashFile("testfile.max"));
   Q_ASSERT(fil.exists());

   view->setSelectionRange(0, 1);

   // Now delete the second
   desktop->doDeleteStacks(false);
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 0);

   QFile fil2(trashFile("testpdf.pdf"));
   Q_ASSERT(fil2.exists());

   // Undo both operations
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   Q_ASSERT(stk->canUndo());
   stk->undo();

   Q_ASSERT(!fil.exists());
   Q_ASSERT(!fil2.exists());
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);

   // Delete both stacks
   view->setSelectionRange(0, 2);
   desktop->doDeleteStacks(false);
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 0);
}

void TestOps::testUnstackStacks()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   // Unstack the first stack
   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 1);

   desktop->doUnstackStacks(false);

   // Check that the unstacked pages are selected. The original stack has 5
   // pages so four should be unstacked and selected
   QModelIndexList sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 4);

   // There should now be 6 files
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 6);

   // Undo it
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
   Q_ASSERT(!view->isSelection(Desktopview::SEL_at_least_one));

   // Now duplicate the stack and unstack both
   view->setSelectionRange(0, 1);
   desktop->duplicate();
   view->addSelectionRange(0, 1);

   desktop->doUnstackStacks(false);

   // Check that the unstacked pages are selected. The original stacks has 5
   // pages each so 8 should be unstacked and selected
   sel = view->getSelectedListSource();
   QCOMPARE(sel.size(), 8);

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 11);

   // Undo the unstack
   Q_ASSERT(stk->canUndo());
   stk->undo();
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   // Undo the duplicate
   Q_ASSERT(stk->canUndo());
   stk->undo();
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
}

void TestOps::testRenameStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   // Rename the first stack. We cannot use the view since it just allows the
   // user to edit. So call the model function
   QModelIndex ind = model->index(0, 0, repo_ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile");

   model->renameStack(ind, "new-name");
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "new-name");

   // Undo the rename
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile");
}

void TestOps::testRenamePage()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   // Rename the first stack. We cannot use the view since it just allows the
   // user to edit. So call the model function
   QModelIndex max_ind = model->index(0, 0, repo_ind);

   File *max = model->getFile(max_ind);
   Q_ASSERT(max);
   QCOMPARE(max->pagecount(), 0);
   max->load();
   QCOMPARE(max->pagecount(), 5);

   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagename).toString(),
            "Page 1");

   model->renamePage(max_ind, "new-name");
   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagename).toString(),
            "new-name");

   // Undo the rename
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagename).toString(),
            "Page 1");
}

void TestOps::testFindFoldersOtherYear()
{
   Mainwindow me;

   /* a folder which carries a year in its name must still be found
      when the user searches for it by name, even though the year is
      not the current one */
   auto path = setupRepo();
   QDir dir(path);
   Q_ASSERT(dir.mkpath("digs/palace/landscaping 2024"));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Dirmodel *dirmodel = desktop->getDirmodel();
   QModelIndex root = dirmodel->index(path);
   QCOMPARE(root.isValid(), true);

   QStringList missing;
   QStringList folders = dirmodel->findFolders("landscaping", path, root,
                                               missing, nullptr);
   QCOMPARE(folders.size(), 1);
   QVERIFY(folders[0].contains("digs/palace/landscaping 2024"));

   /* with nothing typed, other-year folders are still left out so that
      the suggestion list is not flooded with old directories */
   folders = dirmodel->findFolders("", path, root, missing, nullptr);
   QVERIFY(!folders.join(",").contains("landscaping"));
}

void TestOps::testPrintCountPages()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   // Select both stacks, as a user would before printing
   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 2);

   QModelIndexList list = view->getSelectedListSource();
   QCOMPARE(list.size(), 2);

   // Make sure the files are loaded so the page counts are known
   int expected = 0;
   foreach (QModelIndex ind, list) {
      File *f = model->getFile(ind);
      f->load();
      expected += f->pagecount();
   }
   QCOMPARE(expected, 10);

   // The print options dialog should count the pages of the selection
   QPrinter printer;
   Printopt opt(&printer, list);
   opt.sepSheet->setChecked(false);
   QCOMPARE(opt.countPages(), 10);

   /* with a separator sheet, stacks with an odd number of pages get
      a blank page added: both stacks have 5 pages */
   opt.sepSheet->setChecked(true);
   QCOMPARE(opt.countPages(), 12);
}

void TestOps::testPrintToPdf()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 2);

   QModelIndexList list = view->getSelectedListSource();
   QCOMPARE(list.size(), 2);
   foreach (QModelIndex ind, list)
      model->getFile(ind)->load();

   /* run the part of print() which follows the print dialogue, with
      the printer directed at a PDF file */
   QString out = QString("%1/print_test.pdf").arg(P_tmpdir);
   QFile::remove(out);

   Mainwidget *main = Mainwidget::singleton();
   QVERIFY(main);
   main->_printer = new QPrinter();
   main->_printer->setOutputFileName(out);
   main->_printer->setResolution(150);
   main->_opt = new Printopt(main->_printer, list);
   main->_opt->sepSheet->setChecked(false);
   main->_opt->save();   // copy the widget states into the members
   main->_printer->setFromTo(1, main->_opt->countPages());

   main->printPages(list);

   delete main->_opt;
   main->_opt = nullptr;
   delete main->_printer;
   main->_printer = nullptr;

   // The PDF should exist and contain every page of both stacks
   QVERIFY(QFile::exists(out));

   File *printed = File::createFile(QString(P_tmpdir) + "/",
                                    QString("print_test.pdf"), nullptr,
                                    File::Type_pdf);
   QVERIFY(printed);
   printed->load();
   QCOMPARE(printed->pagecount(), 10);
   delete printed;

   QFile::remove(out);
}

void TestOps::testOptionsDialog()
{
   if (!xmlConfig)
      new QXmlConfig();
   bool orig = xmlConfig->boolValue("SCAN_USE_JPEG");

   // The dialog loads the current setting
   Options opt(nullptr, nullptr);
   QCOMPARE(opt.jpeg->isChecked(), orig);

   // Toggling it and clicking OK saves the new value
   opt.jpeg->setChecked(!orig);
   opt.ok_clicked();
   QCOMPARE(xmlConfig->boolValue("SCAN_USE_JPEG"), !orig);

   // Changing it back but cancelling leaves the saved value alone
   Options opt2(nullptr, nullptr);
   QCOMPARE(opt2.jpeg->isChecked(), !orig);
   opt2.jpeg->setChecked(orig);
   opt2.cancel_clicked();
   QCOMPARE(xmlConfig->boolValue("SCAN_USE_JPEG"), !orig);

   // Restore the user's setting
   xmlConfig->setBoolValue("SCAN_USE_JPEG", orig);
}

void TestOps::getTestRepo(Mainwindow *me, Desktopmodel*& model,
                          QModelIndex& repo_ind)
{
   // Add our test repo
   auto path = setupRepo();
   Desktopwidget *desktop = me->getDesktop ();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   model = desktop->getModel();
   repo_ind = model->index(0, 0, QModelIndex());
   Q_ASSERT(repo_ind.isValid());
}

void TestOps::duplicate(Mainwindow *me, QModelIndex &repo_ind)
{
   QModelIndex max_ind;
   prepareDuplicate(me, repo_ind, max_ind);

   // Duplicate the max file
   Desktopwidget *desktop = me->getDesktop ();
   desktop->duplicate();

   Desktopmodel *model = desktop->getModel();
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   QModelIndex dup_ind = model->index(2, 0, repo_ind);
   Desktopmodelconv *modelconv = desktop->getModelconv();

   // Make sure that the new stack is selected
   QModelIndex ind = dup_ind;
   bool has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile_copy");

   File *max2 = model->getFile(max_ind);
   Q_ASSERT(max2);
   QCOMPARE(max2->typeName(), "Max");
}

void TestOps::prepareDuplicate(Mainwindow *me, QModelIndex &repo_ind,
                               QModelIndex &max_ind)
{
   Desktopwidget *desktop = me->getDesktop ();

   // Add our test repo
   auto path = setupRepo();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Desktopview *view = desktop->getView();

   QModelIndex ind;
   bool has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, false);

   // There should be one repository
   Desktopmodel *model = desktop->getModel();
   int rows = model->rowCount(QModelIndex());
   QCOMPARE(1, rows);

   repo_ind = model->index(0, 0, QModelIndex());
   Q_ASSERT(repo_ind.isValid());

   // We expect two files
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);

   max_ind = model->index(0, 0, repo_ind);
   Q_ASSERT(max_ind.isValid());

   QCOMPARE(model->data(max_ind, Qt::DisplayRole).toString(), "testfile");

   // Check the two files in the dir
   File *max = model->getFile(max_ind);
   Q_ASSERT(max);
   QCOMPARE(max->typeName(), "Max");

   QModelIndex pdf_ind = model->index(1, 0, repo_ind);
   Q_ASSERT(pdf_ind.isValid());

   File *pdf = model->getFile(pdf_ind);
   Q_ASSERT(pdf);
   QCOMPARE(pdf->typeName(), "PDF");

   view->setSelectionRange(0, 1);
   has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
}

void TestOps::testCreateDir()
{
   Mainwindow me;

   // Add our test repo
   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   // Create a new subdirectory
   QString newDirPath = path + "/subdir";
   QModelIndex dirIndex;
   bool ok = desktop->newDir(newDirPath, dirIndex);
   QCOMPARE(ok, true);

   // Check that the directory was created on disk
   QDir dir(newDirPath);
   QCOMPARE(dir.exists(), true);

   // Check that we can find the directory through the widget
   QModelIndex foundIndex = desktop->findDir(newDirPath);
   QCOMPARE(foundIndex.isValid(), true);
}

void TestOps::testCreateDirInNewParent()
{
   Mainwindow me;

   // Add our test repo
   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   // Create a top-level subdirectory to trigger initial cache building
   QString topDir = path + "/subdir";
   QModelIndex dirIndex;
   bool ok = desktop->newDir(topDir, dirIndex);
   QCOMPARE(ok, true);

   // Create a new directory on the filesystem, bypassing paperman. This
   // simulates a directory that was created outside the app, or one that
   // appeared since the cache was last built (e.g. a new year directory)
   QDir(path + "/main/one").mkdir("c");
   QDir dir(path + "/main/one/c");
   QCOMPARE(dir.exists(), true);

   // Now try to create a subdirectory inside the new directory. The cache
   // does not know about "main/one/c", so refreshCache() must handle a
   // missing parent gracefully rather than crashing.
   QString nestedDir = path + "/main/one/c/leaf";
   QModelIndex nestedIndex;
   ok = desktop->newDir(nestedDir, nestedIndex);
   QCOMPARE(ok, true);

   // Check that the directory was created on disk
   QDir leafDir(nestedDir);
   QCOMPARE(leafDir.exists(), true);
}

void TestOps::testMoveToDir()
{
   Mainwindow me;

   // Add our test repo with extra files
   auto path = setupRepoWithExtra();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   Q_ASSERT(repo_ind.isValid());

   // We expect four files (testfile.max, testpdf.pdf, movefile.max, movepdf.pdf)
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 4);

   // Create two subdirectories to move files into
   QString dir1Path = path + "/moved1";
   QString dir2Path = path + "/moved2";
   QModelIndex dirIndex;
   bool ok = desktop->newDir(dir1Path, dirIndex);
   QCOMPARE(ok, true);
   ok = desktop->newDir(dir2Path, dirIndex);
   QCOMPARE(ok, true);

   // Find all four files by name
   QModelIndex moveFileInd, movePdfInd, testFileInd, testPdfInd;
   for (int i = 0; i < files; i++) {
      QModelIndex ind = model->index(i, 0, repo_ind);
      QString name = model->data(ind, Qt::DisplayRole).toString();
      if (name == "movefile") {
         moveFileInd = ind;
      } else if (name == "movepdf" || name == "movepdf.pdf") {
         movePdfInd = ind;
      } else if (name == "testfile") {
         testFileInd = ind;
      } else if (name == "testpdf" || name == "testpdf.pdf") {
         testPdfInd = ind;
      }
   }
   Q_ASSERT(moveFileInd.isValid());
   Q_ASSERT(movePdfInd.isValid());
   Q_ASSERT(testFileInd.isValid());
   Q_ASSERT(testPdfInd.isValid());

   // Verify the model index fields for each file
   QCOMPARE(model->data(moveFileInd, Qt::DisplayRole).toString(), "movefile");
   File *moveFile = model->getFile(moveFileInd);
   Q_ASSERT(moveFile);
   QCOMPARE(moveFile->typeName(), "Max");

   File *movePdf = model->getFile(movePdfInd);
   Q_ASSERT(movePdf);
   QCOMPARE(movePdf->typeName(), "PDF");

   QCOMPARE(model->data(testFileInd, Qt::DisplayRole).toString(), "testfile");
   File *testFile = model->getFile(testFileInd);
   Q_ASSERT(testFile);
   QCOMPARE(testFile->typeName(), "Max");

   File *testPdf = model->getFile(testPdfInd);
   Q_ASSERT(testPdf);
   QCOMPARE(testPdf->typeName(), "PDF");

   // Move movefile.max and movepdf.pdf to moved1/
   QModelIndexList list1;
   list1 << moveFileInd << movePdfInd;
   QString destDir1 = dir1Path + "/";
   QStringList trashList;
   model->moveToDir(list1, repo_ind, destDir1, trashList);

   // Check that we now have 2 files in the original directory
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);

   // Check that the files exist in moved1/
   QFile moved1Max(dir1Path + "/movefile.max");
   QFile moved1Pdf(dir1Path + "/movepdf.pdf");
   QCOMPARE(moved1Max.exists(), true);
   QCOMPARE(moved1Pdf.exists(), true);

   // Find the remaining two files again (indices may have changed)
   files = model->rowCount(repo_ind);
   testFileInd = QModelIndex();
   testPdfInd = QModelIndex();
   for (int i = 0; i < files; i++) {
      QModelIndex ind = model->index(i, 0, repo_ind);
      QString name = model->data(ind, Qt::DisplayRole).toString();
      if (name == "testfile") {
         testFileInd = ind;
      } else if (name == "testpdf" || name == "testpdf.pdf") {
         testPdfInd = ind;
      }
   }
   Q_ASSERT(testFileInd.isValid());
   Q_ASSERT(testPdfInd.isValid());

   // Verify the model index fields for the remaining files
   QCOMPARE(model->data(testFileInd, Qt::DisplayRole).toString(), "testfile");
   testFile = model->getFile(testFileInd);
   Q_ASSERT(testFile);
   QCOMPARE(testFile->typeName(), "Max");

   testPdf = model->getFile(testPdfInd);
   Q_ASSERT(testPdf);
   QCOMPARE(testPdf->typeName(), "PDF");

   // Move testfile.max and testpdf.pdf to moved2/
   QModelIndexList list2;
   list2 << testFileInd << testPdfInd;
   QString destDir2 = dir2Path + "/";
   model->moveToDir(list2, repo_ind, destDir2, trashList);

   // Check that original directory is now empty
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 0);

   // Check that the files exist in moved2/
   QFile moved2Max(dir2Path + "/testfile.max");
   QFile moved2Pdf(dir2Path + "/testpdf.pdf");
   QCOMPARE(moved2Max.exists(), true);
   QCOMPARE(moved2Pdf.exists(), true);

   // Check that no files exist in the original directory
   QFile origMoveMax(path + "/movefile.max");
   QFile origMovePdf(path + "/movepdf.pdf");
   QFile origTestMax(path + "/testfile.max");
   QFile origTestPdf(path + "/testpdf.pdf");
   QCOMPARE(origMoveMax.exists(), false);
   QCOMPARE(origMovePdf.exists(), false);
   QCOMPARE(origTestMax.exists(), false);
   QCOMPARE(origTestPdf.exists(), false);

   // Undo the second move (testfile/testpdf to moved2/)
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();

   // Check that we have 2 files again (testfile and testpdf)
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);
   QCOMPARE(origTestMax.exists(), true);
   QCOMPARE(origTestPdf.exists(), true);
   QCOMPARE(moved2Max.exists(), false);
   QCOMPARE(moved2Pdf.exists(), false);

   // Verify the restored files have correct model index fields
   testFileInd = QModelIndex();
   testPdfInd = QModelIndex();
   for (int i = 0; i < files; i++) {
      QModelIndex ind = model->index(i, 0, repo_ind);
      QString name = model->data(ind, Qt::DisplayRole).toString();
      if (name == "testfile") {
         testFileInd = ind;
      } else if (name == "testpdf" || name == "testpdf.pdf") {
         testPdfInd = ind;
      }
   }
   Q_ASSERT(testFileInd.isValid());
   Q_ASSERT(testPdfInd.isValid());

   QCOMPARE(model->data(testFileInd, Qt::DisplayRole).toString(), "testfile");
   testFile = model->getFile(testFileInd);
   Q_ASSERT(testFile);
   QCOMPARE(testFile->typeName(), "Max");

   testPdf = model->getFile(testPdfInd);
   Q_ASSERT(testPdf);
   QCOMPARE(testPdf->typeName(), "PDF");

   // Undo the first move (movefile/movepdf to moved1/)
   Q_ASSERT(stk->canUndo());
   stk->undo();

   // Check that we have all 4 files back
   files = model->rowCount(repo_ind);
   QCOMPARE(files, 4);
   QCOMPARE(origMoveMax.exists(), true);
   QCOMPARE(origMovePdf.exists(), true);
   QCOMPARE(moved1Max.exists(), false);
   QCOMPARE(moved1Pdf.exists(), false);

   // Verify all four restored files have correct model index fields
   moveFileInd = QModelIndex();
   movePdfInd = QModelIndex();
   testFileInd = QModelIndex();
   testPdfInd = QModelIndex();
   for (int i = 0; i < files; i++) {
      QModelIndex ind = model->index(i, 0, repo_ind);
      QString name = model->data(ind, Qt::DisplayRole).toString();
      if (name == "movefile") {
         moveFileInd = ind;
      } else if (name == "movepdf" || name == "movepdf.pdf") {
         movePdfInd = ind;
      } else if (name == "testfile") {
         testFileInd = ind;
      } else if (name == "testpdf" || name == "testpdf.pdf") {
         testPdfInd = ind;
      }
   }
   Q_ASSERT(moveFileInd.isValid());
   Q_ASSERT(movePdfInd.isValid());
   Q_ASSERT(testFileInd.isValid());
   Q_ASSERT(testPdfInd.isValid());

   QCOMPARE(model->data(moveFileInd, Qt::DisplayRole).toString(), "movefile");
   moveFile = model->getFile(moveFileInd);
   Q_ASSERT(moveFile);
   QCOMPARE(moveFile->typeName(), "Max");

   movePdf = model->getFile(movePdfInd);
   Q_ASSERT(movePdf);
   QCOMPARE(movePdf->typeName(), "PDF");

   QCOMPARE(model->data(testFileInd, Qt::DisplayRole).toString(), "testfile");
   testFile = model->getFile(testFileInd);
   Q_ASSERT(testFile);
   QCOMPARE(testFile->typeName(), "Max");

   testPdf = model->getFile(testPdfInd);
   Q_ASSERT(testPdf);
   QCOMPARE(testPdf->typeName(), "PDF");
}

void TestOps::testChangeDir()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   Desktopwidget *desktop = me.getDesktop();
   Desktopview *view = desktop->getView();

   me.show();
   QVERIFY(QTest::qWaitForWindowExposed(&me));

   // The repo root shows its two stacks
   auto path = desktop->getSelectedPath();
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);

   // Change to an empty subdirectory: no stacks should be shown
   QModelIndex dir_ind = desktop->findDir(path + "/main");
   QVERIFY(dir_ind.isValid());
   desktop->selectDir(dir_ind);
   QCOMPARE(desktop->getSelectedPath(), QString(path + "/main"));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 0);

   // Change back to the repo root: the stacks reappear
   desktop->selectDir(desktop->findDir(path));
   QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
}

void TestOps::testDeleteDir()
{
   Mainwindow me;

   // Add our test repo
   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Dirmodel *dirmodel = desktop->getDirmodel();
   Q_ASSERT(dirmodel);

   // Create a subdirectory
   QString subDirPath = path + "/to_delete";
   QModelIndex dirIndex;
   bool ok = desktop->newDir(subDirPath, dirIndex);
   QCOMPARE(ok, true);

   // Verify the directory exists
   QDir dir(subDirPath);
   QCOMPARE(dir.exists(), true);

   // Find the directory in the model
   QModelIndex subDirIndex = dirmodel->index(subDirPath);
   QCOMPARE(subDirIndex.isValid(), true);

   // Delete the directory
   bool removed = dirmodel->rmdir(subDirIndex);
   QCOMPARE(removed, true);

   // Verify the directory no longer exists on disk
   QCOMPARE(dir.exists(), false);
}

void TestOps::testRenameDir()
{
   Mainwindow me;

   // Add our test repo
   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Dirmodel *dirmodel = desktop->getDirmodel();
   Q_ASSERT(dirmodel);

   // Create a subdirectory
   QString oldPath = path + "/old_name";
   QModelIndex dirIndex;
   bool ok = desktop->newDir(oldPath, dirIndex);
   QCOMPARE(ok, true);

   // Verify the directory exists
   QDir oldDir(oldPath);
   QCOMPARE(oldDir.exists(), true);

   // Rename the directory using the filesystem
   QString newPath = path + "/new_name";
   QDir dir;
   ok = dir.rename(oldPath, newPath);
   QCOMPARE(ok, true);

   // Verify the old directory no longer exists
   QCOMPARE(oldDir.exists(), false);

   // Verify the new directory exists
   QDir newDir(newPath);
   QCOMPARE(newDir.exists(), true);

   // Refresh the directory model to pick up the change
   QModelIndex repoIndex = dirmodel->index(path);
   dirmodel->refresh(repoIndex);

   // Verify the new directory can be found in the model
   QModelIndex newDirIndex = dirmodel->index(newPath);
   QCOMPARE(newDirIndex.isValid(), true);

   // Verify the old directory cannot be found
   QModelIndex oldDirIndex = dirmodel->index(oldPath);
   QCOMPARE(oldDirIndex.isValid(), false);
}

// Build the "MMmmm" subdirectory name for a given date, matching the
// convention utilDetectMatches expects (e.g. "06jun").
static QString monthDirName(const QDate& date)
{
   return date.toString("MM") + date.toString("MMM").toLower();
}

// Returns ("bills/YEAR/MMmmm" for the previous month, suggestion for the
// current month) using today's date, so the test isn't tied to any specific
// month of the year.
static void monthSuggestionPaths(QString& prev_subpath,
                                 QString& current_suggestion)
{
   QDate today = QDate::currentDate();
   QDate prev = today.addMonths(-1);
   prev_subpath = QString("bills/%1/%2").arg(prev.year(), 4, 10, QChar('0'))
                                        .arg(monthDirName(prev));
   current_suggestion = QString("bills/%1/%2").arg(today.year(), 4, 10,
                                                   QChar('0'))
                                              .arg(monthDirName(today));
}

void TestOps::testFindFoldersSuggestsMonth()
{
   Mainwindow me;

   QString prev_subpath, expected_suggestion;
   monthSuggestionPaths(prev_subpath, expected_suggestion);

   // Set up a repo with the previous month's year folder but no month
   // directories yet.
   auto path = setupRepo();
   QDir dir(path);
   QString year_subpath = prev_subpath.section('/', 0, 1);  // bills/YYYY
   Q_ASSERT(dir.mkpath(year_subpath));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Dirmodel *dirmodel = desktop->getDirmodel();
   Q_ASSERT(dirmodel);

   QModelIndex root = dirmodel->index(path);
   QCOMPARE(root.isValid(), true);

   // Trigger initial cache building by calling findFolders
   QStringList missing;
   QStringList folders = dirmodel->findFolders("bills", path, root, missing,
                                               nullptr);
   QCOMPARE(missing.size(), 0);

   // Create the previous-month directory through the app, as the user did
   QString prev_full = path + "/" + prev_subpath;
   QModelIndex prevIndex;
   bool ok = desktop->newDir(prev_full, prevIndex);
   QCOMPARE(ok, true);

   // Re-obtain the root index since newDir modifies the model
   root = dirmodel->index(path);
   QCOMPARE(root.isValid(), true);

   // Search again - the in-memory cache should now include the new
   // directory and suggest creating the current month
   folders = dirmodel->findFolders("bills", path, root, missing, nullptr);

   QVERIFY2(missing.contains(expected_suggestion),
            qPrintable(QString("Expected '%1' in missing list, "
                               "got: [%2]")
                       .arg(expected_suggestion).arg(missing.join(", "))));
}

void TestOps::testFindFoldersSuggestsMonthViaDesktop()
{
   Mainwindow me;

   QString prev_subpath, expected_suggestion;
   monthSuggestionPaths(prev_subpath, expected_suggestion);
   QString year_subpath = prev_subpath.section('/', 0, 1);  // bills/YYYY
   QString prev_leaf = prev_subpath.section('/', -1);       // MMmmm

   // Set up a repo with the previous month's year folder but no month
   // directories yet.
   auto path = setupRepo();
   QDir dir(path);
   Q_ASSERT(dir.mkpath(year_subpath));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Dirmodel *dirmodel = desktop->getDirmodel();
   Q_ASSERT(dirmodel);

   // Trigger initial cache building through the desktop path, which
   // uses getRootDirectory() / getRootIndex() internally
   QStringList missing;
   QString dirPath;
   QStringList folders = desktop->findFolders("bills", dirPath, missing);
   QCOMPARE(dirPath, path);
   QCOMPARE(missing.size(), 0);

   // Simulate the UI workflow: user navigates to bills/YYYY in the
   // Dirview tree, then right-clicks to create directories. The UI
   // doNewDir() creates via _model->mkdir() then re-selects the parent.
   QModelIndex year_src = dirmodel->index(path + "/" + year_subpath);
   QVERIFY2(year_src.isValid(),
            qPrintable(year_subpath + " should exist in the model"));
   QModelIndex year_proxy = desktop->_dir_proxy->mapFromSource(year_src);
   desktop->_dir->selectContextItem(year_proxy);

   // Create the previous-month directory via doNewDir(), as the UI does
   // from the right-click menu
   QString newPath;
   QModelIndex prev_ind = desktop->doNewDir(prev_leaf, newPath);
   QVERIFY2(prev_ind.isValid(),
            qPrintable("doNewDir " + prev_leaf + " should succeed"));

   // Check that getRootIndex() still works after doNewDir
   QModelIndex root = desktop->getRootIndex();
   QVERIFY2(root.isValid(),
            "getRootIndex() should still be valid after doNewDir");
   QVERIFY2(dirmodel->isRoot(root),
            "getRootIndex() should return a root index");

   // Search through the desktop path - this is how the pscan dialog
   // finds folders.
   folders = desktop->findFolders("bills", dirPath, missing);

   QVERIFY2(missing.contains(expected_suggestion),
            qPrintable(QString("Expected '%1' in missing list, "
                               "got: [%2]")
                       .arg(expected_suggestion).arg(missing.join(", "))));
}

void TestOps::testFindFoldersSuggestsMonthAfterRefresh()
{
   Mainwindow me;

   QString prev_subpath, expected_suggestion;
   monthSuggestionPaths(prev_subpath, expected_suggestion);
   QString year_subpath = prev_subpath.section('/', 0, 1);  // bills/YYYY
   QString prev_leaf = prev_subpath.section('/', -1);       // MMmmm

   // Set up a repo with the previous month's year folder but no month
   // directories yet.
   auto path = setupRepo();
   QDir dir(path);
   Q_ASSERT(dir.mkpath(year_subpath));

   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Dirmodel *dirmodel = desktop->getDirmodel();
   Q_ASSERT(dirmodel);

   // Trigger initial cache building
   QStringList missing;
   QString dirPath;
   QStringList folders = desktop->findFolders("bills", dirPath, missing);
   QCOMPARE(dirPath, path);
   QCOMPARE(missing.size(), 0);

   // Set the Dirview context to bills/YYYY, as if the user right-clicked
   QModelIndex year_src = dirmodel->index(path + "/" + year_subpath);
   QVERIFY(year_src.isValid());
   QModelIndex year_proxy = desktop->_dir_proxy->mapFromSource(year_src);
   desktop->_dir->selectContextItem(year_proxy);

   // Create the previous month directory via doNewDir, as the UI does
   QString newPath;
   desktop->doNewDir(prev_leaf, newPath);

   // User presses "Refresh cache" from the context menu, which also
   // uses _context to determine the refresh point
   desktop->refreshDirmodelCache(path);

   // Now search via desktop path
   folders = desktop->findFolders("bills", dirPath, missing);

   QVERIFY2(missing.contains(expected_suggestion),
            qPrintable(QString("Expected '%1' in missing list "
                               "after cache refresh, got: [%2]")
                       .arg(expected_suggestion).arg(missing.join(", "))));
}
