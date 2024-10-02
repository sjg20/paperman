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

   model.addDir (dir);
   model.addDir (dir2);
   parent = QModelIndex ();
   printf ("%s\n", model.data (parent, Qt::DisplayRole).toString ().
           toLatin1 ().constData());
   int rows = model.rowCount (parent);
   for (int i = 0; i < rows; i++)
      {
      ind = model.index (i, 0, parent);
      printf ("   %s %p\n", model.data (ind, Qt::DisplayRole).toString ().
              toLatin1 ().constData(), ind.internalPointer ());
      if (model.parent (ind) != QModelIndex ())
         {
         printf ("   - root parent bad\n");
         return;
         }
      if (ind.row () != i)
         qDebug () << "row is" << ind.row () << "should be" << i;
      int rows2 = model.rowCount (ind);
      for (int j = 0; j < rows2; j++)
         {
         QModelIndex ind2 = model.index (j, 0, ind);
         printf ("       %s %p\n", model.data (ind2, Qt::DisplayRole).
                 toString ().toLatin1 ().constData(),
            ind2.internalPointer ());
         if (model.parent (ind2) != ind)
            {
            qDebug () << "   - parent bad, is" << model.parent (ind2) << "should be" << ind;
            return;
            }
         if (ind2.row () != j)
            qDebug () << "row is" << ind2.row () << "should be" << j;
         }
      }
   }
