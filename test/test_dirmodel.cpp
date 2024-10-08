#include <QtTest/QtTest>

#include "test.h"
#include "utils.h"

#include "dirmodel.h"
#include "test_dirmodel.h"

void TestDirmodel::testBase()
{
   Dirmodel *model;

   model = setupModel();
   checkModel(model);
}

void TestDirmodel::checkModel(const QAbstractItemModel *model)
{
   QStringList dirs{"one", "two", "three"};
   QModelIndex parent, ind;

   parent = QModelIndex();
//   qDebug() << model->data(parent, Qt::DisplayRole).toString();
   int rows = model->rowCount(parent);
   QCOMPARE(rows, 2);

   ind = model->index(0, 0, parent);
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "dir");

   ind = model->index(1, 0, parent);
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "other");

   for (int i = 0; i < rows; i++) {
      ind = model->index(i, 0, parent);

      QCOMPARE(model->parent(ind), QModelIndex());
      QCOMPARE(ind.row(), i);
      int rows2 = model->rowCount(ind);
      QCOMPARE(rows2, i ? 1 : 2);
      for (int j = 0; j < rows2; j++) {
         QModelIndex ind2 = model->index(j, 0, ind);
         QString disp = model->data(ind2, Qt::DisplayRole).toString();

         QCOMPARE(disp, dirs[i * 2 + j]);
         QCOMPARE(model->parent(ind2), ind);
         QCOMPARE(ind2.row(), j);
      }
   }
}

Dirmodel *TestDirmodel::setupModel()
{
   Dirmodel *model = new Dirmodel();

   auto path = setupRepo();

   QString newpath = path + "/dir";
   model->addDir(newpath);
   newpath = path + "/other";
   model->addDir(newpath);

   return model;
}
