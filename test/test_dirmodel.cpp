#include <QtTest/QtTest>

#include "test.h"
#include "utils.h"

#include "dirmodel.h"
#include "test_dirmodel.h"

void TestDirmodel::testBase()
{
   QString dir = "desktopl";
   QString dir2 = "test";
   Dirmodel model;
   QModelIndex parent, ind;

   model.addDir(dir);
   model.addDir(dir2);
   parent = QModelIndex();
   qDebug() << model.data(parent, Qt::DisplayRole).toString();
   int rows = model.rowCount(parent);
   QCOMPARE(rows, 2);
   for (int i = 0; i < rows; i++) {
      ind = model.index(i, 0, parent);

      QCOMPARE(model.data(ind, Qt::DisplayRole).toString(), "test");
      qDebug() << "   " << model.data(ind, Qt::DisplayRole).toString()
               << ind.internalPointer();
      QCOMPARE(model.parent(ind), QModelIndex());
      QCOMPARE(ind.row(), i);
      int rows2 = model.rowCount(ind);
      qDebug() << "rows2" << rows2;
      for (int j = 0; j < rows2; j++) {
         QModelIndex ind2 = model.index(j, 0, ind);
         qDebug() << "       " << model.data(ind2, Qt::DisplayRole).toString()
                  << ind2.internalPointer();
         QCOMPARE(model.parent(ind2), ind);
         QCOMPARE(ind2.row(), j);
      }
   }
}
