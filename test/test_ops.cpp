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
