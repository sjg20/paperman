#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "desktopmodel.h"
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

   Desktopwidget *desktop = me.getDesktop ();

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

   QModelIndex repo_ind = model->index(0, 0, QModelIndex());
   Q_ASSERT(repo_ind.isValid());

   // We expect two files
   int files = model->rowCount(repo_ind);
   QCOMPARE(files, 2);

   QModelIndex max_ind = model->index(0, 0, repo_ind);
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

   // Duplicate the max file
   desktop->duplicate();

   files = model->rowCount(repo_ind);
   QCOMPARE(files, 3);

   QModelIndex dup_ind = model->index(2, 0, repo_ind);
   Desktopmodelconv *modelconv = desktop->getModelconv();

   // Make sure that the new stack is selected
   ind = dup_ind;
   has_current = desktop->getCurrentFile(ind);
   QCOMPARE(has_current, true);
   Q_ASSERT(ind.isValid());
   modelconv->indexToSource(desktop->_contents_proxy, ind);

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "testfile_copy");

   File *max2 = model->getFile(max_ind);
   Q_ASSERT(max2);
   QCOMPARE(max2->typeName(), "Max");
}
