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

   model = setupModel();
   QAbstractItemModelTester modelTester(model); // Default Fatal mode
}

void TestDirmodel::testAddDir()
{
   Dirmodel *model;

   model = setupModel();

   QDir dira(_tempDir->path() + "/dir/one/new-dira");
   QModelIndex dir_one = model->index(_tempDir->path() + "/dir/one");
   Q_ASSERT(dir_one.isValid());
   Q_ASSERT(!dira.exists());

   QModelIndex new_a = model->mkdir(dir_one, "new-dira", 0);
   Q_ASSERT(new_a.isValid());
   Q_ASSERT(dira.exists());
   QCOMPARE(model->data(new_a, Qt::DisplayRole).toString(), "new-dira");

   QDir dirb(_tempDir->path() + "/dir/one/new-dirb");
   QModelIndex new_b = model->mkdir(dir_one, "new-dirb", 0);
   Q_ASSERT(new_b.isValid());
   Q_ASSERT(dirb.exists());

   // Since we added something to the model, the old index isn't valid, so get
   // a new one
   QModelIndex chk_a = model->index(_tempDir->path() + "/dir/one/new-dira");
   QCOMPARE(model->data(chk_a, Qt::DisplayRole).toString(), "new-dira");
   QCOMPARE(model->data(new_b, Qt::DisplayRole).toString(), "new-dirb");
}

void TestDirmodel::checkModel(const QAbstractItemModel *model,
                              const Dirmodel *dirmodel,
                              const QAbstractProxyModel *proxy)
{
   QStringList dirs{"one", "two", "three"};
   QModelIndex parent, ind;

   parent = QModelIndex();
//   qDebug() << model->data(parent, Qt::DisplayRole).toString();
   int rows = model->rowCount(parent);
   QCOMPARE(rows, 2);
   QCOMPARE(model->columnCount(parent), 1);
   QCOMPARE(model->parent(parent), QModelIndex());
   QCOMPARE(model->data(parent, Qt::DisplayRole).toString(), "");

   if (dirmodel) {
      QModelIndex src_ind = dirmodel->index(_tempDir->path() + "/dir");
      Q_ASSERT(src_ind.isValid());
      if (proxy) {
         QModelIndex proxy_ind = proxy->mapFromSource(src_ind);
         QModelIndex src_ind2 = proxy->mapToSource(proxy_ind);
         QCOMPARE(src_ind2, src_ind);
      }
   }

   ind = model->index(0, 0, parent);
   QCOMPARE(ind.row(), 0);
   QCOMPARE(ind.column(), 0);
   QCOMPARE(ind.model(), model);
   QCOMPARE(ind.parent(), QModelIndex());

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "dir");

   ind = model->index(1, 0, parent);
   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "other");
   QCOMPARE(ind.row(), 1);
   QCOMPARE(ind.column(), 0);
   QCOMPARE(ind.model(), model);
/*  Use if needed
   ind = model->index(0, 0, parent);
   QModelIndex ind2 = model->index(0, 0, ind);
   if (dirmodel) {
      QCOMPARE(dirmodel->_map.size(), 1);
      QCOMPARE(dirmodel->_map.value(ind2).first, dirmodel->_item[0]);
   }
*/
   int count = 0;  // number of indexes issued by the model

   for (int i = 0; i < rows; i++) {
      ind = model->index(i, 0, parent);

      QCOMPARE(model->parent(ind), QModelIndex());
      QCOMPARE(ind.row(), i);
      QCOMPARE(ind.model(), model);
      int rows2 = model->rowCount(ind);
      QCOMPARE(rows2, i ? 1 : 2);
      for (int j = 0; j < rows2; j++) {
         QModelIndex ind2 = model->index(j, 0, ind);
         count++;

         QString disp = model->data(ind2, Qt::DisplayRole).toString();

         QCOMPARE(disp, dirs[i * 2 + j]);
         QCOMPARE(model->parent(ind2), ind);
         QCOMPARE(ind2.row(), j);
         QCOMPARE(ind2.model(), model);
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
