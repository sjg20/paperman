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
   qDebug() << "rows" << rows;
   for (int i = 0; i < rows; i++) {
      ind = model.index(i, 0, parent);
      qDebug() << "   " << model.data(ind, Qt::DisplayRole).toString()
               << ind.internalPointer();
      if (model.parent(ind) != QModelIndex()) {
         qDebug() << "   - root parent bad";
         return;
      }
      if (ind.row() != i)
         qDebug() << "row is" << ind.row() << "should be" << i;
      int rows2 = model.rowCount(ind);
      qDebug() << "rows2" << rows2;
      for (int j = 0; j < rows2; j++) {
         QModelIndex ind2 = model.index(j, 0, ind);
         qDebug() << "       " << model.data(ind2, Qt::DisplayRole).toString()
                  << ind2.internalPointer();
         if (model.parent(ind2) != ind) {
            qDebug() << "   - parent bad, is" << model.parent(ind2)
                     << "should be" << ind;
            return;
         }
         if (ind2.row() != j)
            qDebug() << "row is" << ind2.row() << "should be" << j;
      }
   }
}
