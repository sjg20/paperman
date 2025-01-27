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

QString TestDirmodel::getPaperTree(QString &path)
{
    QFile file(cacheFile(path));
    Q_ASSERT(file.open(QIODevice::ReadOnly));

    QString lines;
    QTextStream inf(&file);

    while (!inf.atEnd())
        lines.append(inf.readLine() + "|");

    return lines;
}

void TestDirmodel::testAddDir()
{
   Dirmodel *model;

   model = setupModel();

   QString main_path = _tempDir->path() + "/main";

   QModelIndex main = model->index(main_path);
   model->buildCache(main, 0);

   QCOMPARE(getPaperTree(main_path), " one|  a|  b| two|");

   Q_ASSERT(main.isValid());

   QDir dira(_tempDir->path() + "/main/one/new-dira");
   QModelIndex dir_one = model->index(_tempDir->path() + "/main/one");
   Q_ASSERT(dir_one.isValid());
   Q_ASSERT(!dira.exists());

   QModelIndex new_a = model->mkdir(dir_one, "new-dira", 0);
   Q_ASSERT(new_a.isValid());
   Q_ASSERT(dira.exists());
   QCOMPARE(model->data(new_a, Qt::DisplayRole).toString(), "new-dira");

   QCOMPARE(getPaperTree(main_path), " one|  a|  b|  new-dira| two|");

   QDir dirb(_tempDir->path() + "/main/one/new-dirb");
   QModelIndex new_b = model->mkdir(dir_one, "new-dirb", 0);
   Q_ASSERT(new_b.isValid());
   Q_ASSERT(dirb.exists());

   QCOMPARE(getPaperTree(main_path),
            " one|  a|  b|  new-dira|  new-dirb| two|");

   // Since we added something to the model, the old index isn't valid, so get
   // a new one
   QModelIndex chk_a = model->index(_tempDir->path() + "/main/one/new-dira");
   QCOMPARE(model->data(chk_a, Qt::DisplayRole).toString(), "new-dira");
   QCOMPARE(model->data(new_b, Qt::DisplayRole).toString(), "new-dirb");
}

void TestDirmodel::testCacheFiles()
{
   Dirmodel *model;

   model = setupModel(true);

   QString main_path = _tempDir->path() + "/main";

   QModelIndex main = model->index(main_path);
   model->buildCache(main, 0);

   QCOMPARE(getPaperTree(main_path), " one|  a|  b|  ofile|  ofile2| two|");
}

void TestDirmodel::testAddFiles()
{
   Dirmodel *model;

   model = setupModel(true);

   QString main_path = _tempDir->path() + "/main";

   QModelIndex main = model->index(main_path);
   model->buildCache(main, 0);

   QCOMPARE(getPaperTree(main_path), " one|  a|  b|  ofile|  ofile2| two|");

   Q_ASSERT(main.isValid());

   QString dst = _tempDir->path();

   touch(dst + "/main/one/newfile");

   // Refreshing 'a' should do nothing
   QModelIndex dir_a = model->index(main_path + "/one/a");
   model->refreshCacheFrom(dir_a, nullptr);

   QCOMPARE(getPaperTree(main_path), " one|  a|  b|  ofile|  ofile2| two|");

   // Refreshing 'one' should update
   QModelIndex one = model->index(main_path + "/one");
   model->refreshCacheFrom(one, nullptr);

   QCOMPARE(getPaperTree(main_path),
            " one|  a|  b|  newfile|  ofile|  ofile2| two|");

   QDir dir;
   Q_ASSERT(dir.remove(dst + "/main/one/newfile"));

   // Refreshing 'two' should do nothing
   QModelIndex two = model->index(main_path + "/two/a");
   model->refreshCacheFrom(two, nullptr);

   // Refreshing 'one' should update
   model->refreshCacheFrom(one, nullptr);

   QCOMPARE(getPaperTree(main_path), " one|  a|  b|  ofile|  ofile2| two|");
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
      QModelIndex src_ind = dirmodel->index(_tempDir->path() + "/main");
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

   QCOMPARE(model->data(ind, Qt::DisplayRole).toString(), "main");

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

/**
 * @brief Set up a model suiotable for testing
 *
 * Thus creates a model with two top-level Diritems:
 *
 *    main/
 *    other/
 *
 * @return
 */
Dirmodel *TestDirmodel::setupModel(bool add_files)
{
   Dirmodel *model = new Dirmodel();

   auto path = setupRepo(add_files);

   QString newpath = path + "/main";
   model->addDir(newpath);
   newpath = path + "/other";
   model->addDir(newpath);

   return model;
}
