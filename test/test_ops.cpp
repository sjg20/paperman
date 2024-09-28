#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopview.h"
#include "desktopwidget.h"
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
