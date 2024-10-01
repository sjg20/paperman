#include <QDirModel>
#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "mainwindow.h"
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
            "27_September_2024");

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
            "27_September_2024");

   model->renamePage(max_ind, "new-name");
   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagename).toString(),
            "new-name");

   // Undo the rename
   Desktopundostack *stk = model->getUndoStack();
   Q_ASSERT(stk->canUndo());
   stk->undo();
   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagename).toString(),
            "27_September_2024");
}

void TestOps::testCreateDir()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Dirmodel *dirmodel;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);
   Desktopwidget *desktop = me.getDesktop();

   // create a directory
   QString newPath;
   QModelIndex dir1_ind = desktop->doNewDir("fred", newPath);
   Q_ASSERT(dir1_ind.isValid());
   QCOMPARE(newPath, _tempDir->path() + "/fred");

   // Check that it ended up in the Dirmodel with the right path
   dirmodel = desktop->getDirmodel();
   QString chkPath1 = dirmodel->data(dir1_ind,
                                     QDirModel::FilePathRole).toString();
   QCOMPARE(chkPath1, _tempDir->path() + "/fred");

   QModelIndex chk1_ind = dirmodel->index(newPath);
   QCOMPARE(dir1_ind, chk1_ind);

   QModelIndex dir2_ind = desktop->doNewDir("mary", newPath);
   Q_ASSERT(dir2_ind.isValid());
   QCOMPARE(newPath, _tempDir->path() + "/mary");
   QString chkPath2 = dirmodel->data(dir2_ind,
                                      QDirModel::FilePathRole).toString();
   QCOMPARE(chkPath2, _tempDir->path() + "/mary");

   // Check that the original index is still valid
   Q_ASSERT(dir1_ind.isValid());
   chkPath1 = dirmodel->data(dir1_ind, QDirModel::FilePathRole).toString();
   QCOMPARE(chkPath1, _tempDir->path() + "/fred");

   // Try to create a new directory with the same name
   QModelIndex bad_ind = desktop->doNewDir("mary", newPath);
   Q_ASSERT(!bad_ind.isValid());
   QCOMPARE(newPath, _tempDir->path() + "/mary");

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
