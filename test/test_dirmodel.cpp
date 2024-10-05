#include <QAbstractItemModelTester>
#include <QtTest/QtTest>

#include "test.h"
#include "utils.h"

#include "dirmodel.h"
#include "test_dirmodel.h"

void TestDirmodel::testBase()
{
   Dirmodel *model;

   model = setupModel();
   checkModel(model, model, nullptr);
}

void TestDirmodel::testProxy()
{
   Dirmodel *model;

   model = setupModel();

   auto proxy = new Dirproxy();
   proxy->setSourceModel(model);
   checkModel(proxy, nullptr, proxy);
}

void TestDirmodel::testModel()
{
   Dirmodel *model;

//   qDebug() << "testModel";
   model = setupModel();
   QAbstractItemModelTester modelTester(model); // Default Fatal mode
}

void TestDirmodel::checkModel(const QAbstractItemModel *model,
                              const Dirmodel *dirmodel,
                              const QAbstractProxyModel *proxy)
{
   QStringList dirs{"one", "two", "three"};
   QModelIndex parent, ind;

   // We should have two things in the map: the two Diritem objects
   if (dirmodel)
      QCOMPARE(dirmodel->_map.size(), 0);

   parent = QModelIndex();
//   qDebug() << model->data(parent, Qt::DisplayRole).toString();
   int rows = model->rowCount(parent);
   QCOMPARE(rows, 2);
   QCOMPARE(model->columnCount(parent), 1);
   QCOMPARE(model->parent(parent), QModelIndex());
   QCOMPARE(model->data(parent, Qt::DisplayRole).toString(), "");

   if (dirmodel) {
      QCOMPARE(dirmodel->_map.size(), 0);

      QModelIndex src_ind = dirmodel->index(_tempDir->path() + "/dir");
      Q_ASSERT(src_ind.isValid());
      if (proxy) {
         QModelIndex proxy_ind = proxy->mapFromSource(src_ind);
         QModelIndex src_ind2 = proxy->mapToSource(proxy_ind);
         QCOMPARE(src_ind2, src_ind);
      }
   }
/*
   ind = model->index(0, 0, parent);
   QCOMPARE(ind.row(), 0);
   QCOMPARE(ind.column(), 0);
   QCOMPARE(ind.model(), model);
   QCOMPARE(ind.parent(), QModelIndex());
   if (dirmodel) {
//      QCOMPARE(ind.internalPointer(), dirmodel->_item[0]);
      QCOMPARE(dirmodel->_map.size(), 2);
      QCOMPARE(dirmodel->_map.value(ind).first, dirmodel->_item[0]);
   }

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "dir");

   ind = model->index(1, 0, parent);
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "other");
   QCOMPARE(ind.row(), 1);
   QCOMPARE(ind.column(), 0);
   QCOMPARE(ind.model(), model);
//   QCOMPARE(ind.internalPointer(), model->_item[1]);
   if (dirmodel) {
      QCOMPARE(dirmodel->_map.size(), 2);
      QCOMPARE(dirmodel->_map.value(ind).first, dirmodel->_item[1]);
   }

   ind = model->index(0, 0, parent);
   QModelIndex ind2 = model->index(0, 0, ind);
   if (dirmodel) {
      QCOMPARE(dirmodel->_map.size(), 3);
      QCOMPARE(dirmodel->_map.value(ind2).first, dirmodel->_item[0]);
   }
*/
   int count = 2;  // number of indexes issued by the model

   for (int i = 0; i < rows; i++) {
      ind = model->index(i, 0, parent);

      if (dirmodel) {
         QCOMPARE(dirmodel->_map.size(), count);
         QCOMPARE(dirmodel->_map.value(ind).first, dirmodel->_item[i]);
      }
      QCOMPARE(model->parent(ind), QModelIndex());
      QCOMPARE(ind.row(), i);
      QCOMPARE(ind.model(), model);
//      QCOMPARE(ind.internalPointer(), model->_item[i]);
      int rows2 = model->rowCount(ind);
      QCOMPARE(rows2, i ? 1 : 2);
      for (int j = 0; j < rows2; j++) {
         QModelIndex ind2 = model->index(j, 0, ind);
         count++;

         if (dirmodel) {
            QCOMPARE(dirmodel->_map.size(), count);
            QCOMPARE(dirmodel->_map.value(ind).first, dirmodel->_item[i]);
         }

         QString disp = model->data(ind2, Qt::DisplayRole).toString();

         QCOMPARE(disp, dirs[i * 2 + j]);
//         qDebug() << "ind" << ind;
//         qDebug() << "ind2" << model->parent(ind2);
         disp = model->data(ind2, Qt::DisplayRole).toString();
//         qDebug() << "disp" << disp;
         QModelIndex par = model->parent(ind2);
//         qDebug() << "par" << par;
         QCOMPARE(par, ind);
         QCOMPARE(ind2.row(), j);
      }
   }

   QModelIndex dir_ind = model->index(0, 0, parent);
//   qDebug() << "dir_ind" << dir_ind;
   QModelIndex one_ind = model->index(0, 0, dir_ind);
//   qDebug() << "one_ind" << one_ind;
   QModelIndex a_ind = model->index(0, 0, one_ind);
//   qDebug() << "a_ind" << a_ind;
   Q_ASSERT(a_ind.isValid());

}

Dirmodel *TestDirmodel::setupModel()
{
   QStringList dirs{"one", "two", "three"};
   Dirmodel *model = new Dirmodel();

   auto path = setupRepo();

   QString newpath = path + "/dir";
   model->addDir(newpath);
   newpath = path + "/other";
   model->addDir(newpath);

   return model;
}
