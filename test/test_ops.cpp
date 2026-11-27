#include <QAbstractItemModelTester>
#include <QDate>
#include <QMessageBox>
#include <QPrinter>
#include <QtTest/QtTest>
#include <QDir>

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
#include "backgroundwork.h"
#include "activitylog.h"
#include "docfacts.h"
#include "ocrpage.h"
#include "filemax.h"
#include "ocrreader.h"
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

   /* Nothing has displayed the desk, so the file has not been loaded
      yet and would report no pages.  The checks below scale with the
      page count, so any multi-page fixture works. */
   File *booklet = model->getFile(max_ind);
   Q_ASSERT(booklet);
   QVERIFY(!booklet->load());
   int pages = booklet->pagecount();
   QVERIFY(pages >= 2);

   // Unfold the booklet (only the max file is selected)
   Desktopview *view = desktop->getView();
   view->setSelectionRange(0, 1);
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

/* the image of each page of a stack, as the page view gets it */
static QList<QImage> stackImages(File *f)
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

void TestOps::testReorderPages()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   QModelIndex max_ind = model->index(0, 0, repo_ind);
   File *max = model->getFile(max_ind);
   QVERIFY(max);
   max->load();
   QCOMPARE(max->pagecount(), 5);
   QList<QImage> before = stackImages(max);
   QVERIFY(before[0] != before[3]);

   // a stack of several pages, here and in a file which can, can be
   QVERIFY(model->canReorderPages(max_ind));

   // the fourth page first, with the stack showing it
   QVERIFY(model->setData(max_ind, 3, Desktopmodel::Role_pagenum));
   const QVector<int> order = {3, 0, 1, 2, 4};
   model->reorderPages(max_ind, order);

   max = model->getFile(max_ind);
   QList<QImage> after = stackImages(max);
   for (int i = 0; i < order.size(); i++)
      QVERIFY2(after[i] == before[order[i]], qPrintable(QString::number(i)));
   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagenum).toInt(), 0);

   // undo puts them back, and the page shown with them
   Desktopundostack *stk = model->getUndoStack();
   QVERIFY(stk->canUndo());
   QCOMPARE(stk->undoText(), QString("Reorder pages"));
   stk->undo();
   QCOMPARE(stackImages(model->getFile(max_ind)), before);
   QCOMPARE(model->data(max_ind, Desktopmodel::Role_pagenum).toInt(), 3);

   stk->redo();
   QCOMPARE(stackImages(model->getFile(max_ind)), after);
}

void TestOps::testFindHidesTrash()
{
   Mainwindow me;
   auto path = setupRepo();
   QDir dir(path);

   /* the trash, and what is in it; a name which starts with a dot is
      hidden only on Unix, so this matters on Windows */
   QVERIFY(dir.mkpath(".maxview-trash/old"));
   QVERIFY(QFile::copy(testSrc + "/testfile.max",
                       path + "/.maxview-trash/gone.max"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Dirmodel *dirmodel = desktop->getDirmodel();
   QModelIndex root = dirmodel->index(path);
   QVERIFY(root.isValid());

   QStringList missing;
   QStringList folders = dirmodel->findFolders("", path, root, missing,
                                               nullptr);

   for (const QString &folder : folders)
      QVERIFY2(!folder.contains(".maxview-trash"), qPrintable(folder));
   QVERIFY(dirmodel->findFiles("gone", path, root, nullptr).isEmpty());
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
   QString out = QDir::tempPath() + "/print_test.pdf";
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

   File *printed = File::createFile(QDir::tempPath() + "/",
                                    QString("print_test.pdf"), nullptr,
                                    File::Type_pdf);
   QVERIFY(printed);
   printed->load();
   QCOMPARE(printed->pagecount(), 10);
   delete printed;

   QFile::remove(out);
}

/* A conversion can be stopped from the list of work going on: what has
   been made so far is thrown away, and nothing is added to the folder */
void TestOps::testConvertStopped()
{
   Mainwindow me;

   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());
   int rows = model->rowCount(repo_ind);

   QModelIndexList list;
   list << model->index("testfile.max", repo_ind);
   QVERIFY(list[0].isValid());
   QVERIFY(!model->getFile(list[0])->load());
   QVERIFY(model->getFile(list[0])->pagecount() > 1);

   /* ask for it to stop as soon as it lets other things run, which is
      after its first page; it is the only work going on */
   Backgroundwork *work = Backgroundwork::instance();
   int before = work->jobs().size();
   QStringList seen;
   bool asked = false;
   QSignalSpy said(desktop, SIGNAL(newContents(QString)));

   connect(work, &Backgroundwork::changed, &me, [&] {
      if (asked || work->jobs().size() <= before)
         return;
      Backgroundjob *job = work->jobs().last();

      seen << job->title();
      if (job->stoppable() && job->percent() > 0) {
         asked = true;
         job->requestStop();
      }
   });
   model->duplicatePdf(list, repo_ind);
   QVERIFY(asked);
   QVERIFY(seen.contains("Convert to PDF"));
   QCOMPARE(int(work->jobs().size()), before);

   // nothing was made, and the user is told without a window to dismiss
   QCOMPARE(model->rowCount(repo_ind), rows);
   QVERIFY(!QFile::exists(path + "/testfile_copy.pdf"));

   // left alone, the same conversion makes the copy
   asked = true;
   model->duplicatePdf(list, repo_ind);
   QTRY_VERIFY(QFile::exists(path + "/testfile_copy.pdf"));
   QCOMPARE(model->rowCount(repo_ind), rows + 1);
}

/* A stack's name may hold dots: only what follows the last one is its
   extension. A name used to be cut at its first dot, so '4.4BSD manual'
   became '4' */
void TestOps::testNameWithDots()
{
   Mainwindow me;

   auto path = setupRepo();

   // a PDF which comes with dots in its name
   QVERIFY(QFile::copy(testSrc + "/testpdf.pdf",
                       path + "/Invoice 12.03.2026.pdf"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   // it is a PDF, called all of that
   QModelIndex ind = model->index("Invoice 12.03.2026.pdf", repo_ind);
   QVERIFY(ind.isValid());
   File *f = model->getFile(ind);
   QCOMPARE(f->type(), File::Type_pdf);
   QCOMPARE(f->stackLeaf(), QString("Invoice 12.03.2026"));
   QCOMPARE(f->ext(), QString(".pdf"));

   // a max stack given a name with dots keeps all of it, and its type
   ind = model->index("testfile.max", repo_ind);
   QVERIFY(ind.isValid());
   model->renameStack(ind, "4.4BSD Programmer's Manual");
   QTRY_VERIFY(QFile::exists(path + "/4.4BSD Programmer's Manual.max"));
   QVERIFY(!QFile::exists(path + "/4.max"));
   ind = model->index("4.4BSD Programmer's Manual.max", repo_ind);
   QVERIFY(ind.isValid());
   f = model->getFile(ind);
   QCOMPARE(f->type(), File::Type_max);
   QCOMPARE(f->stackLeaf(), QString("4.4BSD Programmer's Manual"));
   QCOMPARE(f->ext(), QString(".max"));
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(),
            QString("4.4BSD Programmer's Manual"));

   // it can be renamed again, and each rename undone
   model->renameStack(ind, "v1.2 notes");
   QTRY_VERIFY(QFile::exists(path + "/v1.2 notes.max"));
   model->getUndoStack()->undo();
   QTRY_VERIFY(QFile::exists(path + "/4.4BSD Programmer's Manual.max"));
   model->getUndoStack()->undo();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));

   // a copy is named after all of the name too
   QModelIndexList list;
   list << model->index("Invoice 12.03.2026.pdf", repo_ind);
   QVERIFY(list[0].isValid());
   model->duplicate(list, repo_ind);
   QTRY_VERIFY(QFile::exists(path + "/Invoice 12.03.2026_copy.pdf"));

   // a name which another stack starts with is still made different
   ind = model->index("testfile.max", repo_ind);
   model->renameStack(ind, "Invoice 12.03.2026");
   QTRY_VERIFY(!QFile::exists(path + "/testfile.max"));
   QVERIFY(!QFile::exists(path + "/Invoice 12.max"));
   QVERIFY(!QFile::exists(path + "/Invoice 12_1.max"));

   QStringList made = QDir(path).entryList(QStringList("Invoice 12.03.2026*.max"));
   QCOMPARE(int(made.size()), 1);
}

/* Facts about a stack's document are kept in its file, as a step which
   can be undone, and go with a new name as one step */
void TestOps::testStackFacts()
{
   Mainwindow me;

   auto path = setupRepoWithExtra();
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", path + "/photo.jpg"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   QModelIndex max = model->index("testfile.max", repo_ind);
   QModelIndex pdf = model->index("testpdf.pdf", repo_ind);
   QModelIndex jpg = model->index("photo.jpg", repo_ind);
   QVERIFY(max.isValid() && pdf.isValid() && jpg.isValid());

   // a max file and a PDF keep facts; a JPEG has nowhere to
   QVERIFY(model->keepsFacts(model->getFile(max)));
   QVERIFY(model->keepsFacts(model->getFile(pdf)));
   QVERIFY(!model->keepsFacts(model->getFile(jpg)));
   QVERIFY(!model->keepsFacts(nullptr));

   Docfacts bill, statement;

   bill.type = "bill";
   bill.from = "Northern Power";
   bill.date = "2026-03-03";
   bill.amount = "184.20";
   bill.currency = "NZD";
   statement.type = "bank statement";
   statement.from = "Kiwi Bank";
   statement.date = "2025-07";

   // facts are kept, made fit to keep, and can be undone and redone
   QVERIFY(model->facts(max).isEmpty());

   Docfacts rough = bill;

   rough.date = "3rd March";
   rough.from = "  Northern Power \n";
   QVERIFY(model->setFacts(max, rough));

   Docfacts kept = bill;

   kept.date.clear();
   QCOMPARE(model->facts(max), kept);
   QVERIFY(model->setFacts(max, bill));
   QCOMPARE(model->facts(max), bill);

   // the same again changes nothing, and nor do facts for a JPEG
   QVERIFY(!model->setFacts(max, bill));
   QVERIFY(!model->setFacts(jpg, bill));
   QVERIFY(model->facts(jpg).isEmpty());

   model->getUndoStack()->undo();
   QCOMPARE(model->facts(max), kept);
   model->getUndoStack()->undo();
   QVERIFY(model->facts(max).isEmpty());
   model->getUndoStack()->redo();
   model->getUndoStack()->redo();
   QCOMPARE(model->facts(max), bill);

   // they are in the stack's file, so a fresh look at it finds them
   {
      Filemax file(path + "/", "testfile.max", nullptr);
      Docfacts read;

      QVERIFY(!file.load());
      QVERIFY(!Docfacts::read(&file, read));
      QCOMPARE(read, bill);
   }
   model->getUndoStack()->undo();
   model->getUndoStack()->undo();
   QVERIFY(model->facts(max).isEmpty());

   /* a name and facts together are one step: the max gets both, the PDF
      facts alone, and the JPEG its name without the facts it cannot keep */
   QModelIndexList list;
   list << max << pdf << jpg;
   QList<Docfacts> facts({bill, statement, bill});

   QCOMPARE(model->renameStacks(list, QStringList({"power bill", QString(),
                                                   "holiday"}), facts), 3);
   QTRY_VERIFY(QFile::exists(path + "/power bill.max"));
   QVERIFY(QFile::exists(path + "/testpdf.pdf"));
   QTRY_VERIFY(!QFile::exists(path + "/photo.jpg"));
   QCOMPARE(model->facts(model->index("power bill.max", repo_ind)), bill);
   QCOMPARE(model->facts(model->index("testpdf.pdf", repo_ind)), statement);

   model->getUndoStack()->undo();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QTRY_VERIFY(QFile::exists(path + "/photo.jpg"));
   QVERIFY(model->facts(model->index("testfile.max", repo_ind)).isEmpty());
   QVERIFY(model->facts(model->index("testpdf.pdf", repo_ind)).isEmpty());

   model->getUndoStack()->redo();
   QTRY_VERIFY(QFile::exists(path + "/power bill.max"));
   QCOMPARE(model->facts(model->index("power bill.max", repo_ind)), bill);
   QCOMPARE(model->facts(model->index("testpdf.pdf", repo_ind)), statement);

   // with the names and facts they have already, there is nothing to do
   list.clear();
   list << model->index("power bill.max", repo_ind)
        << model->index("testpdf.pdf", repo_ind);
   QCOMPARE(model->renameStacks(list, QStringList({"power bill", QString()}),
                                QList<Docfacts>({bill, statement})), 0);
}

/* Several stacks renamed together are one step to undo */
void TestOps::testRenameStacks()
{
   Mainwindow me;

   auto path = setupRepoWithExtra();
   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   QModelIndexList list;
   list << model->index("testfile.max", repo_ind)
        << model->index("testpdf.pdf", repo_ind)
        << model->index("movefile.max", repo_ind)
        << model->index("movepdf.pdf", repo_ind);
   for (const QModelIndex &ind : list)
      QVERIFY(ind.isValid());

   /* one keeps the name it has and one is given none, so two are renamed,
      each keeping its type */
   QStringList names({"power bill", "bank statement", "movefile", QString()});
   QCOMPARE(model->renameStacks(list, names), 2);
   QTRY_VERIFY(QFile::exists(path + "/power bill.max"));
   QTRY_VERIFY(QFile::exists(path + "/bank statement.pdf"));
   QVERIFY(!QFile::exists(path + "/testfile.max"));
   QVERIFY(!QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(QFile::exists(path + "/movefile.max"));
   QVERIFY(QFile::exists(path + "/movepdf.pdf"));

   // one undo puts both names back, and one redo gives them again
   model->getUndoStack()->undo();
   QTRY_VERIFY(QFile::exists(path + "/testfile.max"));
   QTRY_VERIFY(QFile::exists(path + "/testpdf.pdf"));
   QVERIFY(!QFile::exists(path + "/power bill.max"));
   QVERIFY(!QFile::exists(path + "/bank statement.pdf"));
   model->getUndoStack()->redo();
   QTRY_VERIFY(QFile::exists(path + "/power bill.max"));
   QTRY_VERIFY(QFile::exists(path + "/bank statement.pdf"));

   // with nothing to do, nothing is done
   QCOMPARE(model->renameStacks(QModelIndexList(), QStringList()), 0);
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

   /* a record of activity is kept unless the box is unticked, which is
      remembered */
   QSettings().remove("activity");
   {
      Options first(nullptr, nullptr);

      QVERIFY(first.keepActivity->isChecked());
      first.keepActivity->setChecked(false);
      first.cancel_clicked();
      QVERIFY(ActivityLog::kept());

      Options second(nullptr, nullptr);

      second.keepActivity->setChecked(false);
      second.ok_clicked();
      QVERIFY(!ActivityLog::kept());

      Options third(nullptr, nullptr);

      QVERIFY(!third.keepActivity->isChecked());
      third.keepActivity->setChecked(true);
      third.ok_clicked();
      QVERIFY(ActivityLog::kept());
      QSettings().remove("activity");
   }

   /* how many pages are read at once is left to Paperman until a number
      is chosen, and can only be chosen while pages are read at all */
   QSettings().remove("ocr");
   Options opt3(nullptr, nullptr);
   QVERIFY(opt3.readText->isChecked());
   QVERIFY(opt3.readWorkers->isEnabled());
   QCOMPARE(opt3.readWorkers->value(), 0);
   QCOMPARE(opt3.readWorkers->text(), QString("Automatic"));
   QCOMPARE(OcrReader::workers(), OcrReader::autoWorkers());
   QVERIFY(OcrReader::autoWorkers() >= 1
           && OcrReader::autoWorkers() <= OcrReader::max_auto_workers);
   QVERIFY(opt3.readWorkers->maximum() >= 1);

   opt3.readWorkers->setValue(1);
   opt3.ok_clicked();
   QCOMPARE(OcrReader::workersSetting(), 1);
   QCOMPARE(OcrReader::workers(), 1);

   // a reader made now reads that many, and the dialog shows the choice
   OcrReader reader;
   QCOMPARE(reader._workers, 1);
   Options opt4(nullptr, nullptr);
   QCOMPARE(opt4.readWorkers->value(), 1);
   opt4.readText->setChecked(false);
   QVERIFY(!opt4.readWorkers->isEnabled());
   opt4.cancel_clicked();
   QVERIFY(OcrReader::enabled());
   QSettings().remove("ocr");
}

void TestOps::testDeletePdfPages()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);
   QModelIndex ind = model->index("testpdf.pdf", repo_ind);
   QVERIFY(ind.isValid());
   QVERIFY(!model->getFile(ind)->load());
   int count = model->getFile(ind)->pagecount();
   QVERIFY(count > 2);

   // the pages marked in the page view go, as one step to undo
   QBitArray pages(count);

   pages.setBit(0);
   pages.setBit(2);
   model->deletePages(ind, pages);
   ind = model->index("testpdf.pdf", repo_ind);
   QCOMPARE(model->getFile(ind)->pagecount(), count - 2);

   model->getUndoStack()->undo();
   ind = model->index("testpdf.pdf", repo_ind);
   QCOMPARE(model->getFile(ind)->pagecount(), count);
}

void TestOps::testDeleteJpegStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);
   QString path = model->deskToDirname(repo_ind);
   QModelIndexList list;

   list << model->index("testfile.max", repo_ind);
   QVERIFY(!model->getFile(list[0])->load());
   int pages = model->getFile(list[0])->pagecount();
   QVERIFY(pages > 1);

   // a copy as JPEG is a file for each page
   model->duplicateJpeg(list, repo_ind);
   QString name = "testfile_copy_p1.jpg";
   QTRY_VERIFY(model->index(name, repo_ind).isValid());
   QCOMPARE(model->getFile(model->index(name, repo_ind))->pagecount(), pages);

   // deleting it moves every page to the trash
   QModelIndexList del;

   del << model->index(name, repo_ind);
   model->trashStacks(del, repo_ind);
   QTRY_VERIFY(!model->index(name, repo_ind).isValid());
   for (int i = 1; i <= pages; i++) {
      QString page = QString("testfile_copy_p%1.jpg").arg(i);

      QVERIFY2(!QFile::exists(path + page), qPrintable(page));
      QVERIFY2(QFile::exists(trashFile(page)), qPrintable(page));
   }

   // undoing it brings the whole stack back
   model->getUndoStack()->undo();
   QTRY_VERIFY(model->index(name, repo_ind).isValid());
   QCOMPARE(model->getFile(model->index(name, repo_ind))->pagecount(), pages);
   for (int i = 1; i <= pages; i++) {
      QString page = QString("testfile_copy_p%1.jpg").arg(i);

      QVERIFY2(QFile::exists(path + page), qPrintable(page));
      QVERIFY2(!QFile::exists(trashFile(page)), qPrintable(page));
   }
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

void TestOps::testOfferRepository()
{
   QSettings qs;

   qs.remove("offeredRepository");
   QTemporaryDir tmp;
   QString dir = QDir(tmp.path()).canonicalPath() + "/Paperman";

   QVERIFY(Mainwindow::defaultRepository().endsWith("/Paperman"));
   {
      Mainwindow me;

      // one can be added at any time from the File menu
      QVERIFY(me.findChild<QAction *>("actionAddRepository"));

      // with nowhere to keep papers, take the folder offered
      QVERIFY(me.needsRepository());
      QTimer::singleShot(0, [] {
         auto *box = qobject_cast<QMessageBox *>(
            QApplication::activeModalWidget());
         QVERIFY(box);
         for (QAbstractButton *button : box->buttons())
            if (button->text() == "Use this folder")
               button->click();
      });
      me.offerRepository(dir);
      QVERIFY(QDir(dir).exists());
      QVERIFY(!me.needsRepository());
      QVERIFY(me.getDesktop()->findDir(dir).isValid());
   }

   // it is only asked once, whatever the answer
   Mainwindow again;
   bool asked = false;

   QVERIFY(again.needsRepository());
   QTimer::singleShot(0, [&asked] {
      if (QWidget *box = QApplication::activeModalWidget()) {
         asked = true;
         box->close();
      }
   });
   again.offerRepository(dir);
   QVERIFY(!asked);
   qs.remove("offeredRepository");
}

void TestOps::testCreateDirThroughSymlink()
{
#ifdef Q_OS_WIN
   QSKIP("QFile::link() makes a shortcut on Windows, not a symlink");
#endif
   Mainwindow me;

   /* reach the repository through a symlink, as a temporary directory on
      macOS is, under /var, which is a symlink to /private/var */
   auto path = setupRepo();
   QTemporaryDir links;
   QString link = links.path() + "/repo";

   QVERIFY(QFile::link(path, link));
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(link);
   Q_ASSERT(!err);

   QString newDirPath = link + "/subdir";
   QModelIndex dirIndex;
   bool ok = desktop->newDir(newDirPath, dirIndex);
   QCOMPARE(ok, true);
   QVERIFY(QDir(path + "/subdir").exists());
   QVERIFY(desktop->findDir(newDirPath).isValid());
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

void TestOps::testStackConverts()
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
   int rows = model->rowCount(repo_ind);
   /* the stacks' files, leaving out Paperman's own, which start with a
      dot and are listed with the rest on Windows */
   auto stackFiles = [&path]() {
      QStringList names;

      for (const QString &name : QDir(path).entryList(QDir::Files,
                                                      QDir::Name))
         if (!name.startsWith('.'))
            names << name;
      return names;
   };
   QStringList before = stackFiles();

   // Stacked onto each, it is converted and becomes a page, and undo
   // brings the picture back as it was
   for (const QString &name : QStringList({"testfile.max", "testpdf.pdf"})) {
      QModelIndex dest = model->index(name, repo_ind);
      QModelIndex pic = model->index("picture.jpg", repo_ind);
      QVERIFY2(dest.isValid() && pic.isValid(), qPrintable(name));
      QVERIFY(!model->getFile(dest)->load());
      int pages = model->getFile(dest)->pagecount();
      QPoint pos = model->getFile(pic)->pos();
      QModelIndexList list;

      list << pic;
      QVERIFY(!model->stackItems(dest, list, nullptr));
      dest = model->index(name, repo_ind);
      QCOMPARE(model->getFile(dest)->pagecount(), pages + 1);
      QCOMPARE(model->rowCount(repo_ind), rows - 1);
      QVERIFY(!QFile::exists(path + "/picture.jpg"));

      model->getUndoStack()->undo();
      dest = model->index(name, repo_ind);
      pic = model->index("picture.jpg", repo_ind);
      QVERIFY(pic.isValid());
      QCOMPARE(model->getFile(dest)->pagecount(), pages);
      QCOMPARE(model->rowCount(repo_ind), rows);
      QCOMPARE(model->getFile(pic)->pos(), pos);
      QCOMPARE(stackFiles(), before);
   }
}

void TestOps::testPagesLeaveSize()
{
   Mainwindow me;
   QModelIndex repo_ind;

   duplicate(&me, repo_ind);

   Desktopmodel *model = me.getDesktop()->getModel();
   const QSize wide(5000, 400);

   // The stack is as wide as a wide page which has been seen in it
   auto widen = [&]() {
      QModelIndex ind = model->index(0, 0, repo_ind);

      model->setData(ind, wide, Desktopmodel::Role_preview_maxsize);
      QCOMPARE(model->data(ind, Desktopmodel::Role_preview_maxsize)
               .toSize(), wide);
   };
   auto width = [&]() {
      QModelIndex ind = model->index(0, 0, repo_ind);

      return model->data(ind, Desktopmodel::Role_preview_maxsize)
            .toSize().width();
   };

   // Pages which join a stack and leave again by Undo
   QModelIndex dest = model->index(0, 0, repo_ind);
   QModelIndexList list;

   list << model->index(2, 0, repo_ind);
   QVERIFY(!model->getFile(list[0])->load());
   QVERIFY(!model->stackItems(dest, list, nullptr));
   QCOMPARE(model->getFile(model->index(0, 0, repo_ind))->pagecount(), 10);
   widen();
   model->getUndoStack()->undo();
   QCOMPARE(model->getFile(model->index(0, 0, repo_ind))->pagecount(), 5);
   QVERIFY(width() < wide.width());

   // A page which is unstacked
   QModelIndex ind = model->index(0, 0, repo_ind);

   widen();
   model->unstackPage(ind, 0, true);
   QCOMPARE(model->getFile(model->index(0, 0, repo_ind))->pagecount(), 4);
   QVERIFY(width() < wide.width());

   // A page which is deleted
   QBitArray pages(4);

   ind = model->index(0, 0, repo_ind);
   pages.setBit(1);
   widen();
   model->deletePages(ind, pages);
   QCOMPARE(model->getFile(model->index(0, 0, repo_ind))->pagecount(), 3);
   QVERIFY(width() < wide.width());

   // Every page but one, unstacked
   list.clear();
   list << model->index(0, 0, repo_ind);
   widen();
   QVERIFY(!model->unstackStacks(list, repo_ind));
   QCOMPARE(model->getFile(model->index(0, 0, repo_ind))->pagecount(), 1);
   QVERIFY(width() < wide.width());
}

void TestOps::testStackOnPdf()
{
   Mainwindow me;

   auto path = setupRepo();
   QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", path + "/picture.jpg"));

   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   // The picture is the only page of its size
   QImage picture(testSrc + "/colour_plasma.jpg");
   QVERIFY(!picture.isNull());

   // which page of the PDF the picture is, or -1
   auto find = [&]() {
      File *f = model->getFile(model->index("testpdf.pdf", repo_ind));
      int found = -1;

      for (int pagenum = 0; pagenum < f->pagecount(); pagenum++) {
         QSize size, trueSize;
         int bpp, image_size, compressed_size;
         QDateTime timestamp;

         if (!f->getImageInfo(pagenum, size, trueSize, bpp, image_size,
                              compressed_size, timestamp)
             && size == picture.size())
            found = pagenum;
      }
      return found;
   };

   // It goes in before the page shown, whichever that is
   for (int shown : {0, 2}) {
      QModelIndex dest = model->index("testpdf.pdf", repo_ind);
      QVERIFY(!model->getFile(dest)->load());
      int pages = model->getFile(dest)->pagecount();
      QModelIndexList list;

      model->getFile(dest)->setPagenum(shown);
      QCOMPARE(find(), -1);
      list << model->index("picture.jpg", repo_ind);
      QVERIFY(list[0].isValid());
      QVERIFY(!model->stackItems(dest, list, nullptr));
      dest = model->index("testpdf.pdf", repo_ind);
      QCOMPARE(model->getFile(dest)->pagecount(), pages + 1);
      QCOMPARE(find(), shown);

      // Undo takes that page out, leaving the PDF's own pages
      model->getUndoStack()->undo();
      dest = model->index("testpdf.pdf", repo_ind);
      QCOMPARE(model->getFile(dest)->pagecount(), pages);
      QCOMPARE(find(), -1);
      QVERIFY(model->index("picture.jpg", repo_ind).isValid());
   }
}

void TestOps::testStackAtPage()
{
   Mainwindow me;

   auto path = setupRepo();
   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   /* the model must not say that a row has changed before it has the
      row, or the proxy over it loses track of its rows */
   QAbstractItemModelTester check(model, QAbstractItemModelTester::
                                  FailureReportingMode::QtTest);

   // The picture is the only page of its size
   QImage picture(testSrc + "/colour_plasma.jpg");
   QVERIFY(!picture.isNull());

   // which page of a stack the picture is, or -1
   auto find = [&](const QString &name) {
      File *f = model->getFile(model->index(name, repo_ind));
      int found = -1;

      for (int pagenum = 0; pagenum < f->pagecount(); pagenum++) {
         QSize size, trueSize;
         int bpp, image_size, compressed_size;
         QDateTime timestamp;

         if (!f->getImageInfo(pagenum, size, trueSize, bpp, image_size,
                              compressed_size, timestamp)
             && size == picture.size()) {
            if (found != -1)
               return -2;
            found = pagenum;
         }
      }
      return found;
   };

   for (const QString &name : QStringList({"testfile.max", "testpdf.pdf"})) {
      QModelIndex dest = model->index(name, repo_ind);
      QVERIFY(!model->getFile(dest)->load());
      int pages = model->getFile(dest)->pagecount();
      int shown = model->getFile(dest)->pagenum();
      QCOMPARE(find(name), -1);

      // As a stack, it goes in before the page given, or after the last
      for (int pagenum : {2, 0, pages}) {
         QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg",
                             path + "/picture.jpg"));
         QStringList added("picture.jpg");
         model->addFilesToDesk(path + "/", added);
         dest = model->index(name, repo_ind);
         QModelIndexList list;

         list << model->index("picture.jpg", repo_ind);
         QVERIFY(list[0].isValid());
         QVERIFY(!model->stackItems(dest, list, nullptr, pagenum));
         dest = model->index(name, repo_ind);
         QCOMPARE(model->getFile(dest)->pagecount(), pages + 1);
         QCOMPARE(find(name), pagenum);
         QCOMPARE(model->getFile(dest)->pagenum(), shown);

         model->getUndoStack()->undo();
         dest = model->index(name, repo_ind);
         QCOMPARE(model->getFile(dest)->pagecount(), pages);
         QCOMPARE(find(name), -1);
         QVERIFY(QFile::remove(path + "/picture.jpg"));
         QStringList gone("picture.jpg");
         model->removeFilesFromDesk(path + "/", gone);
      }

      // As a file from elsewhere, it does the same
      QTemporaryDir other;
      QString file = other.path() + "/picture.jpg";
      QVERIFY(QFile::copy(testSrc + "/colour_plasma.jpg", file));
      for (int pagenum : {1, pages}) {
         dest = model->index(name, repo_ind);
         model->addFiles(QStringList(file), repo_ind, QPoint(300, 200), dest,
                         pagenum);
         dest = model->index(name, repo_ind);
         QCOMPARE(model->getFile(dest)->pagecount(), pages + 1);
         QCOMPARE(find(name), pagenum);

         model->getUndoStack()->undo();
         dest = model->index(name, repo_ind);
         QCOMPARE(model->getFile(dest)->pagecount(), pages);
         QCOMPARE(find(name), -1);
      }
   }
}

void TestOps::testSplitStack()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   // the image of each page of a stack
   auto images = [&](const QString &name) {
      File *f = model->getFile(model->index(name, repo_ind));
      QList<QImage> list;

      if (!f || f->load())
         return list;
      for (int i = 0; i < f->pagecount(); i++) {
         QImage image;
         QSize size, trueSize;
         int bpp;

         if (!f->getImage(i, false, image, size, trueSize, bpp, false))
            list << image;
      }
      return list;
   };

   for (const QString &ext : {QString(".max"), QString(".pdf")}) {
      QString orig = ext == ".max" ? "testfile.max" : "testpdf.pdf";
      QString first = "First" + ext, second = "Second 3.5" + ext;
      QModelIndex ind = model->index(orig, repo_ind);
      QList<QImage> before = images(orig);
      int count = before.size();
      int rows = model->rowCount(repo_ind);

      QVERIFY2(count >= 4, qPrintable(orig));

      // pages which do not leave each stack one of its own are refused
      QVERIFY(!model->splitStack(ind, {0, 2}, {"a", "b"}, ""));
      QVERIFY(!model->splitStack(ind, {2, 2}, {"a", "b"}, ""));
      QVERIFY(!model->splitStack(ind, {2, count}, {"a", "b"}, ""));
      QVERIFY(!model->splitStack(ind, {2}, {"a", "b"}, ""));
      QVERIFY(!model->splitStack(ind, {}, {}, ""));
      QCOMPARE(model->rowCount(repo_ind), rows);

      /* the first page stays, with a new name; the next two are one
         stack, with the name given, dot and all, and the rest another,
         named after the stack */
      QVERIFY(model->splitStack(ind, {1, 3}, {"Second 3.5", ""}, "First"));
      QCOMPARE(model->rowCount(repo_ind), rows + 2);
      QVERIFY(!model->index(orig, repo_ind).isValid());
      QVERIFY(model->index(first, repo_ind).isValid());

      QModelIndex second_ind = model->index(second, repo_ind);
      QVERIFY2(second_ind.isValid(), qPrintable(second));

      // the last is the only one not there before
      QModelIndex third_ind = model->index(rows + 1, 0, repo_ind);
      QString third = model->data(third_ind,
                                  Desktopmodel::Role_filename).toString();
      QVERIFY2(third.endsWith(ext), qPrintable(third));
      QCOMPARE(second_ind.row(), rows);

      QCOMPARE(images(first), before.mid(0, 1));
      QCOMPARE(images(second), before.mid(1, 2));
      QCOMPARE(images(third), before.mid(3));

      // one undo puts it all back, name and all
      model->getUndoStack()->undo();
      QCOMPARE(model->rowCount(repo_ind), rows);
      QVERIFY(!model->index(first, repo_ind).isValid());
      QVERIFY(images(orig) == before);

      // and one redo divides it again
      model->getUndoStack()->redo();
      QCOMPARE(model->rowCount(repo_ind), rows + 2);
      QCOMPARE(images(first), before.mid(0, 1));
      QCOMPARE(images(second), before.mid(1, 2));
      model->getUndoStack()->undo();
      QVERIFY(images(orig) == before);

      /* each stack can be given facts as it is made: the stack itself,
         with or without a new name, and each new one, except where there
         are none to give */
      Docfacts bill, letter;

      bill.type = "bill";
      bill.from = "Northern Power";
      letter.type = "letter";
      letter.date = "2026-03";
      QVERIFY(model->splitStack(ind, {1, 3}, {"Second 3.5", "Third"}, "",
                                {bill, letter, Docfacts()}));
      QCOMPARE(model->rowCount(repo_ind), rows + 2);
      QCOMPARE(model->facts(model->index(orig, repo_ind)), bill);
      QCOMPARE(model->facts(model->index(second, repo_ind)), letter);
      QVERIFY(model->facts(model->index("Third" + ext, repo_ind)).isEmpty());

      // one undo puts the stack back with no facts, as it was
      model->getUndoStack()->undo();
      QCOMPARE(model->rowCount(repo_ind), rows);
      QVERIFY(model->facts(model->index(orig, repo_ind)).isEmpty());
      QVERIFY(images(orig) == before);

      // and a redo gives them all again
      model->getUndoStack()->redo();
      QCOMPARE(model->facts(model->index(orig, repo_ind)), bill);
      QCOMPARE(model->facts(model->index(second, repo_ind)), letter);
      model->getUndoStack()->undo();
      QVERIFY(images(orig) == before);
   }
}

/* What is done to the stacks is written in the repository's record of
   activity, with each stack's path below the top, and so are undo and
   redo; things done as one step are under an entry for the whole */
void TestOps::testActivityRecorded()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   QString root = model->deskToDirname(repo_ind);
   ActivityLog log(root);
   QList<Activity> acts;
   auto read = [&]() {
      QList<Activity> list;

      return log.read(list) ? QList<Activity>() : list;
   };

   QCOMPARE(QDir(model->activityRoot(root)), QDir(root));
   QVERIFY(read().isEmpty());

   // one stack renamed is one entry, by the user
   model->renameStack(model->index("testfile.max", repo_ind), "letter");
   acts = read();
   QCOMPARE(int(acts.size()), 1);
   QCOMPARE(acts[0].action, Activity::A_rename);
   QCOMPARE(acts[0].who, Activity::W_user);
   QCOMPARE(acts[0].from, QString("testfile.max"));
   QCOMPARE(acts[0].path, QString("letter.max"));
   QCOMPARE(acts[0].group, qint64(0));
   QCOMPARE(acts[0].describe(),
            QString("Renamed 'testfile' to 'letter'"));

   // a page is turned
   model->transformPage(model->index("letter.max", repo_ind), 1,
                        File::Transform_rotate90);
   acts = read();
   QCOMPARE(int(acts.size()), 2);
   QCOMPARE(acts[1].action, Activity::A_turn);
   QCOMPARE(acts[1].path, QString("letter.max"));
   QCOMPARE(acts[1].page, 1);

   // undoing it says which entry was undone, and redoing it likewise
   model->getUndoStack()->undo();
   acts = read();
   QCOMPARE(int(acts.size()), 3);
   QCOMPARE(acts[2].action, Activity::A_undo);
   QCOMPARE(acts[2].ref, acts[1].id);
   QCOMPARE(acts[2].describe(),
            QString("Undid: Turned page 2 of 'letter'"));
   model->getUndoStack()->redo();
   acts = read();
   QCOMPARE(int(acts.size()), 4);
   QCOMPARE(acts[3].action, Activity::A_redo);
   QCOMPARE(acts[3].ref, acts[1].id);

   // two stacks named together, by the assistant, are a group
   {
      ActivityBy by(model->getUndoStack(), Activity::W_assistant);
      QModelIndexList both{model->index("letter.max", repo_ind),
                           model->index("testpdf.pdf", repo_ind)};

      QCOMPARE(model->renameStacks(both, {"bank letter", "power bill"}), 2);
   }
   acts = read();
   QCOMPARE(int(acts.size()), 7);
   QCOMPARE(acts[4].action, Activity::A_group);
   QCOMPARE(acts[4].who, Activity::W_assistant);
   QCOMPARE(acts[4].note, QString("Name stacks (2)"));
   QCOMPARE(acts[5].action, Activity::A_rename);
   QCOMPARE(acts[5].group, acts[4].id);
   QCOMPARE(acts[5].who, Activity::W_assistant);
   QCOMPARE(acts[5].path, QString("bank letter.max"));
   QCOMPARE(acts[6].group, acts[4].id);
   QCOMPARE(acts[6].path, QString("power bill.pdf"));

   // whoever does the next thing is the user again
   QCOMPARE(model->getUndoStack()->who(), Activity::W_user);

   // the group is undone as one, naming the group
   model->getUndoStack()->undo();
   acts = read();
   QCOMPARE(int(acts.size()), 8);
   QCOMPARE(acts[7].action, Activity::A_undo);
   QCOMPARE(acts[7].ref, acts[4].id);
   QCOMPARE(acts[7].who, Activity::W_user);

   // from the first entry, the stack is followed to where it is now
   bool gone;
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("letter.max"));
   model->getUndoStack()->redo();
   acts = read();
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone),
            QString("bank letter.max"));
   QVERIFY(!gone);

   // moved to a folder, it is recorded with where it went
   QModelIndexList list{model->index("bank letter.max", repo_ind)};
   QString dest = root + "main/";
   QStringList made;
   int before = acts.size();

   model->moveToDir(list, repo_ind, dest, made);
   acts = read();
   QCOMPARE(int(acts.size()), before + 1);
   QCOMPARE(acts.last().action, Activity::A_move);
   QCOMPARE(acts.last().from, QString("bank letter.max"));
   QCOMPARE(acts.last().path, QString("main/bank letter.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone),
            QString("main/bank letter.max"));
   QVERIFY(QFile::exists(root + ActivityLog::whereNow(acts, 0, gone)));

   // a stack deleted is gone, until that is undone
   list = QModelIndexList{model->index("power bill.pdf", repo_ind)};
   model->trashStacks(list, repo_ind);
   acts = read();
   QCOMPARE(acts.last().action, Activity::A_trash);
   QCOMPARE(acts.last().path, QString("power bill.pdf"));
   ActivityLog::whereNow(acts, acts.size() - 1, gone);
   QVERIFY(gone);

   // the entry has the name it has in the trash, where it is
   QString kept = acts.last().kept;
   QString trash = root + ActivityLog::trashName() + "/";
   QCOMPARE(kept, QString("power bill.pdf"));
   QVERIFY(QFile::exists(trash + kept));
   model->getUndoStack()->undo();
   acts = read();
   QCOMPARE(ActivityLog::whereNow(acts, acts.size() - 2, gone),
            QString("power bill.pdf"));
   QVERIFY(!gone);

   /* deleted again, it can be brought back from the trash by that name,
      which is recorded against the entry which deleted it; and undoing
      that deletes it again */
   list = QModelIndexList{model->index("power bill.pdf", repo_ind)};
   model->trashStacks(list, repo_ind);
   acts = read();
   int deleted = acts.size() - 1;
   QCOMPARE(acts[deleted].action, Activity::A_trash);
   QCOMPARE(model->restoreStack(repo_ind, acts[deleted].kept,
                                "power bill.pdf", acts[deleted].id),
            QString("power bill.pdf"));
   QVERIFY(QFile::exists(root + "power bill.pdf"));
   QVERIFY(!QFile::exists(trash + kept));
   acts = read();
   QCOMPARE(acts.last().action, Activity::A_restore);
   QCOMPARE(acts.last().path, QString("power bill.pdf"));
   QCOMPARE(acts.last().ref, acts[deleted].id);
   ActivityLog::whereNow(acts, deleted, gone);
   QVERIFY(!gone);
   model->getUndoStack()->undo();
   QVERIFY(!QFile::exists(root + "power bill.pdf"));
   QVERIFY(QFile::exists(trash + kept));
   acts = read();
   ActivityLog::whereNow(acts, deleted, gone);
   QVERIFY(gone);
   model->getUndoStack()->redo();
   QVERIFY(QFile::exists(root + "power bill.pdf"));
   acts = read();
   ActivityLog::whereNow(acts, deleted, gone);
   QVERIFY(!gone);

   // dividing a stack gives the stack and each one taken out of it
   QModelIndex ind = model->index("power bill.pdf", repo_ind);
   before = acts.size();
   QVERIFY(model->splitStack(ind, {1, 3}, {"second", "third"}, "first"));
   acts = read();
   QCOMPARE(int(acts.size()), before + 5);
   QCOMPARE(acts[before].action, Activity::A_group);
   QCOMPARE(acts[before].note, QString("Split stack"));
   QCOMPARE(acts[before + 1].action, Activity::A_split);
   QCOMPARE(acts[before + 1].count, 3);
   QCOMPARE(acts[before + 2].action, Activity::A_unstack);
   QCOMPARE(acts[before + 2].path, QString("second.pdf"));
   QCOMPARE(acts[before + 2].from, QString("power bill.pdf"));
   QCOMPARE(acts[before + 3].path, QString("third.pdf"));
   QCOMPARE(acts[before + 4].action, Activity::A_rename);
   QCOMPARE(acts[before + 4].path, QString("first.pdf"));
   for (int i = 1; i <= 4; i++)
      QCOMPARE(acts[before + i].group, acts[before].id);

   // a small move about the desk is not recorded
   before = acts.size();
   list = QModelIndexList{model->index("first.pdf", repo_ind)};
   QList<QPoint> from{QPoint(10, 10)}, to{QPoint(30, 10)};
   model->move(list, repo_ind, from, to);
   QCOMPARE(int(read().size()), before);

   // every entry has a time, and numbers which only go up
   acts = read();
   for (int i = 0; i < acts.size(); i++) {
      QVERIFY(acts[i].when.isValid());
      QVERIFY(!i || acts[i].id > acts[i - 1].id);
      QVERIFY2(!acts[i].describe().isEmpty(),
               qPrintable(QString::number(i)));
   }
}

/* A page which is turned or cropped loses its words, since they are no
   longer where they were, and is read again by itself */
void TestOps::testTurnedPageReadAgain()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   QModelIndex ind = model->index("testfile.max", repo_ind);
   File *f = model->getFile(ind);
   QVERIFY(f && !f->load());

   // every page has words, as after a scan has been read
   auto words = [&](int pagenum) {
      OcrPage ocr;

      return f->getPageOcr(pagenum, ocr) ? -1 : int(ocr.words.size());
   };
   for (int i = 0; i < f->pagecount(); i++) {
      OcrPage ocr;

      ocr.size = QSize(100, 100);
      ocr.words << OcrWord{QRect(1, 2, 3, 4), "before", 90, 0, 0};
      QVERIFY(!f->putPageOcr(i, ocr));
   }

   // a reader which says what it was asked to read
   OcrReader *reader = Mainwidget::singleton()->ocrReader();
   QAtomicInt calls;

   reader->setEngine([&calls](QImage &image, OcrPage &page) {
      calls++;
      page = OcrPage();
      page.size = image.size();
      page.words << OcrWord{QRect(1, 2, 3, 4), "again", 90, 0, 0}
                 << OcrWord{QRect(5, 2, 3, 4), "read", 90, 0, 0};
      return QString();
   });
   OcrReader::allowRereading(true);
   auto forbid = qScopeGuard([] { OcrReader::allowRereading(false); });

   // turning a page drops its words, and it alone is read again
   model->transformPage(ind, 1, File::Transform_rotate90);
   QTRY_COMPARE(words(1), 2);
   QTRY_VERIFY(!reader->isBusy());
   QCOMPARE(int(calls), 1);
   QCOMPARE(words(0), 1);
   QCOMPARE(words(2), 1);

   // undo turns it back, which is another change, read again too
   model->getUndoStack()->undo();
   QTRY_VERIFY(!reader->isBusy());
   QTRY_COMPARE(int(calls), 2);
   QCOMPARE(words(1), 2);

   // with the reading of scanned pages turned off, nothing is read
   OcrReader::setEnabled(false);
   auto restore = qScopeGuard([] { QSettings().remove("ocr/background"); });
   model->transformPage(ind, 2, File::Transform_rotate180);
   QTest::qWait(100);
   QVERIFY(!reader->isBusy());
   QCOMPARE(int(calls), 2);
   QCOMPARE(words(2), 0);
}

void TestOps::testReversePages()
{
   QModelIndex repo_ind;
   Desktopmodel *model;
   Mainwindow me;

   getTestRepo(&me, model, repo_ind);

   // the image of each page of a stack
   auto images = [&](const QString &name) {
      File *f = model->getFile(model->index(name, repo_ind));
      QList<QImage> list;

      for (int i = 0; i < f->pagecount(); i++) {
         QImage image;
         QSize size, trueSize;
         int bpp;

         if (!f->getImage(i, false, image, size, trueSize, bpp, false))
            list << image;
      }
      return list;
   };

   const QStringList names = {"testfile.max", "testpdf.pdf"};
   QModelIndexList list;
   QList<QList<QImage>> before;

   for (const QString &name : names) {
      QModelIndex ind = model->index(name, repo_ind);

      QVERIFY(!model->getFile(ind)->load());
      list << ind;
      before << images(name);
      QVERIFY(before.last().size() > 1);
      QVERIFY(before.last().first() != before.last().last());
   }

   // each stack has its last page first
   QCOMPARE(model->reversePages(list, repo_ind), 2);
   for (int s = 0; s < names.size(); s++) {
      QList<QImage> after = images(names[s]);
      QList<QImage> expect = before[s];

      std::reverse(expect.begin(), expect.end());
      QVERIFY2(after == expect, qPrintable(names[s]));
   }

   // and both go back as they were, with one undo
   model->getUndoStack()->undo();
   for (int s = 0; s < names.size(); s++)
      QVERIFY2(images(names[s]) == before[s], qPrintable(names[s]));
}

/* A stack which moves gets another File, so the model says where it
   went, for anything which knows the stack by where it is */
void TestOps::testStackMoved()
{
   Mainwindow me;

   auto path = QDir::cleanPath(setupRepoWithExtra());
   Desktopwidget *desktop = me.getDesktop();
   QVERIFY(!desktop->addDir(path));

   Desktopmodel *model = desktop->getModel();
   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   QVERIFY(repo_ind.isValid());

   QList<QPair<QString, QString>> moves;
   connect(model, &Desktopmodel::stackMoved, model,
           [&moves](const QString &from, const QString &to) {
      moves << qMakePair(QDir::cleanPath(from), QDir::cleanPath(to));
   });

   QString sub = path + "/moved";
   QModelIndex dirIndex;
   QVERIFY(desktop->newDir(sub, dirIndex));

   // a stack's File is found by where the stack is
   const QString here = path + "/movefile.max";
   const QString there = sub + "/movefile.max";
   QModelIndex ind = model->index("movefile.max", repo_ind);
   QVERIFY(ind.isValid());
   QCOMPARE(model->fileForPath(here), model->getFile(ind));
   QVERIFY(!model->fileForPath(there));

   // moving it to another folder says so, and so does undoing that
   QModelIndexList list;
   QString dest = sub + "/";
   QStringList trash;
   list << ind;
   model->moveToDir(list, repo_ind, dest, trash);
   QTRY_COMPARE(int(moves.size()), 1);
   QCOMPARE(moves[0].first, here);
   QCOMPARE(moves[0].second, there);
   QVERIFY(QFile::exists(there));
   QVERIFY(!model->fileForPath(here));

   model->getUndoStack()->undo();
   QTRY_COMPARE(int(moves.size()), 2);
   QCOMPARE(moves[1].first, there);
   QCOMPARE(moves[1].second, here);
   QVERIFY(QFile::exists(here));
   QVERIFY(model->fileForPath(here));

   // deleting it moves it to the trash, and undoing brings it back
   list.clear();
   list << model->index("movefile.max", repo_ind);
   QVERIFY(list[0].isValid());
   model->trashStacks(list, repo_ind);
   QTRY_COMPARE(int(moves.size()), 3);
   QCOMPARE(moves[2].first, here);
   QString trashed = moves[2].second;
   QVERIFY(trashed != here);
   QVERIFY(QFile::exists(trashed));
   QVERIFY(!QFile::exists(here));

   model->getUndoStack()->undo();
   QTRY_COMPARE(int(moves.size()), 4);
   QCOMPARE(moves[3].first, trashed);
   QCOMPARE(moves[3].second, here);
   QVERIFY(QFile::exists(here));

   // a copy leaves the stack where it is, so nothing is said
   list.clear();
   list << model->index("movefile.max", repo_ind);
   model->moveToDir(list, repo_ind, dest, trash, true);
   QTRY_VERIFY(QFile::exists(there));
   QCOMPARE(int(moves.size()), 4);
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

void TestOps::testMoveToShownDir()
{
   Mainwindow me;

   auto path = setupRepoWithExtra();
   Desktopwidget *desktop = me.getDesktop();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);

   Desktopmodel *model = desktop->getModel();
   QString destPath = path + "/shown";
   QModelIndex dirIndex;
   QVERIFY(desktop->newDir(destPath, dirIndex));

   // Show the folder, so that the model holds what is in it, then go back
   QModelIndex dest_ind = model->showDir(destPath, path, nullptr);
   QCOMPARE(model->rowCount(dest_ind), 0);
   QModelIndex repo_ind = model->showDir(path, path, nullptr);
   QCOMPARE(model->rowCount(repo_ind), 4);

   // Move two stacks there, with the path as the move-to-folder dialog
   // gives it, which has no slash at its end
   QModelIndexList list;
   list << model->index(0, 0, repo_ind) << model->index(1, 0, repo_ind);
   QStringList names = model->listToFilenames(list);
   QString dest = destPath;
   QStringList sl;
   model->moveToDir(list, repo_ind, dest, sl);
   for (const QString &name : names)
      QVERIFY(QFile::exists(destPath + "/" + name));

   // They have gone from the one folder and are seen in the other
   repo_ind = model->deskFromDirname(path);
   QCOMPARE(model->rowCount(repo_ind), 2);
   dest_ind = model->deskFromDirname(destPath);
   QVERIFY(dest_ind.isValid());
   QCOMPARE(model->rowCount(dest_ind), 2);
   for (const QString &name : names)
      QVERIFY(model->index(name, dest_ind).isValid());

   // Undo puts them back, and takes them out of the folder they went to
   model->getUndoStack()->undo();
   QCOMPARE(model->rowCount(repo_ind), 4);
   QCOMPARE(model->rowCount(dest_ind), 0);
   for (const QString &name : names)
      QVERIFY(!QFile::exists(destPath + "/" + name));
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
   /* the same name the code makes: the short month name is cut to three
      letters, which only shows in a month with a longer one (September) */
   return date.toString("MM") + date.toString("MMM").toLower().left(3);
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
