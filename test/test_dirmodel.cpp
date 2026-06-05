#include <QtTest/QtTest>

#include "test.h"
#include "utils.h"

#include "dirmodel.h"
#include "searchserver.h"
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

   QCOMPARE(getPaperTree(main_path), " + one|  + a|  + b| + two|");

   Q_ASSERT(main.isValid());

   QDir dira(_tempDir->path() + "/main/one/new-dira");
   QModelIndex dir_one = model->index(_tempDir->path() + "/main/one");
   Q_ASSERT(dir_one.isValid());
   Q_ASSERT(!dira.exists());

   QModelIndex new_a = model->mkdir(dir_one, "new-dira", 0);
   Q_ASSERT(new_a.isValid());
   Q_ASSERT(dira.exists());
   QCOMPARE(model->data(new_a, Qt::DisplayRole).toString(), "new-dira");

   QCOMPARE(getPaperTree(main_path), " + one|  + a|  + b|  + new-dira| + two|");

   QDir dirb(_tempDir->path() + "/main/one/new-dirb");
   QModelIndex new_b = model->mkdir(dir_one, "new-dirb", 0);
   Q_ASSERT(new_b.isValid());
   Q_ASSERT(dirb.exists());

   QCOMPARE(getPaperTree(main_path),
            " + one|  + a|  + b|  + new-dira|  + new-dirb| + two|");

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

   QCOMPARE(getPaperTree(main_path),
            " + one|  + a|  + b|  - ofile|  - ofile2| + two|");
}

void TestDirmodel::testRefreshKeepsSubdirs()
{
   Dirmodel *model = setupModel();

   // Trigger population of main/one
   QString one_path = _tempDir->path() + "/main/one";
   QModelIndex one = model->index(one_path);
   QVERIFY(one.isValid());
   QCOMPARE(model->rowCount(one), 2);   // "a" and "b" exist on disk

   // Capture child names so we can compare after refresh
   QStringList before;
   for (int i = 0; i < model->rowCount(one); i++)
      before << model->data(model->index(i, 0, one), Qt::DisplayRole).toString();
   before.sort();
   QCOMPARE(before, QStringList({"a", "b"}));

   // refresh() must not silently drop the existing subdirectories.
   // After a refresh the view should still report the same children.
   model->refresh(one);

   QModelIndex one_after = model->index(one_path);
   QVERIFY(one_after.isValid());
   QCOMPARE(model->rowCount(one_after), 2);

   QStringList after;
   for (int i = 0; i < model->rowCount(one_after); i++)
      after << model->data(model->index(i, 0, one_after), Qt::DisplayRole).toString();
   after.sort();
   QCOMPARE(after, QStringList({"a", "b"}));
}

void TestDirmodel::testBackendErrorSurfacing()
{
   /* Point the model at a directory that doesn't exist on disk; the
    * Backend should reject the populate, Dirmodel should mark the
    * node as failed (without trapping itself in a populated-with-no-
    * children state), and emit backendError. */
   Dirmodel *model = new Dirmodel();
   QString bogus = _tempDir->path() + "/never-existed";
   /* addDir returns false for an invalid path even with ignore_error=true,
    * but the Diritem is still appended.  That's the scenario we want. */
   model->addDir(bogus, /*ignore_error=*/true);
   QCOMPARE(model->rowCount(QModelIndex()), 1);

   QSignalSpy spy(model, &Dirmodel::backendError);

   QModelIndex root = model->index(0, 0, QModelIndex());
   QVERIFY(root.isValid());

   /* rowCount triggers populateNode(); the underlying call fails. */
   QCOMPARE(model->rowCount(root), 0);

   /* The failure was surfaced as a signal and a tooltip. */
   QCOMPARE(spy.count(), 1);
   QString tooltip = model->data(root, Qt::ToolTipRole).toString();
   QVERIFY2(tooltip.contains("Could not list contents"),
            qPrintable(tooltip));

   /* A second rowCount() does NOT trigger a retry — the failure
    * state is sticky until refresh().  Use spy.count() to verify
    * no additional signal was emitted. */
   (void)model->rowCount(root);
   QCOMPARE(spy.count(), 1);

   /* refresh() clears the failure so a subsequent populate retries. */
   model->refresh(root);
   QCOMPARE(model->rowCount(root), 0);
   QCOMPARE(spy.count(), 2);

   delete model;
}

void TestDirmodel::testRemoteRepository()
{
   /* Spin up a real SearchServer pointing at a freshly seeded
    * repository, then ask a fresh Dirmodel to consume it remotely.
    * The model's only knowledge of the data is the HTTP URL. */
   QTemporaryDir serverDir;
   QVERIFY(serverDir.isValid());
   QString repoRoot = serverDir.path() + "/photos";
   QVERIFY(QDir().mkpath(repoRoot));
   QVERIFY(QDir().mkpath(repoRoot + "/2025"));
   QVERIFY(QDir().mkpath(repoRoot + "/2026"));

   const quint16 port = 9877;
   SearchServer server(repoRoot, port);
   QVERIFY(server.start());
   QTest::qWait(100);

   Dirmodel model;
   QString err;
   QVERIFY2(model.addRemoteRepository(
                QUrl(QString("http://localhost:%1").arg(port)), &err),
            qPrintable(err));

   /* The server has one repository — "photos" — and it should now
    * appear as a top-level node. */
   QCOMPARE(model.rowCount(QModelIndex()), 1);
   QModelIndex root = model.index(0, 0, QModelIndex());
   QCOMPARE(model.data(root, Qt::DisplayRole).toString(),
            QString("photos"));

   /* First rowCount triggers async populate; the placeholder
    * "Loading…" child appears immediately. */
   QCOMPARE(model.rowCount(root), 1);
   QCOMPARE(model.data(model.index(0, 0, root), Qt::DisplayRole).toString(),
            QString("Loading…"));

   /* Spin the event loop until the async response arrives and the
    * placeholder is replaced by the real children. */
   QTRY_COMPARE(model.rowCount(root), 2);
   QStringList names;
   for (int i = 0; i < model.rowCount(root); i++)
      names << model.data(model.index(i, 0, root),
                          Qt::DisplayRole).toString();
   names.sort();
   QCOMPARE(names, QStringList({"2025", "2026"}));

   server.stop();
}


void TestDirmodel::testRemoteRepositoryExpandChild()
{
   /* The reported bug: expanding a top-level remote repo fires
    * /browse, but expanding a child sub-folder doesn't fire a
    * second /browse — counters stay flat.  Recreate the scenario
    * end-to-end: seed nested directories on the server, walk down
    * one level at a time, and confirm a /browse request fires at
    * every level. */
   QTemporaryDir serverDir;
   QVERIFY(serverDir.isValid());
   QString repoRoot = serverDir.path() + "/photos";
   QVERIFY(QDir().mkpath(repoRoot + "/2025/a"));
   QVERIFY(QDir().mkpath(repoRoot + "/2025/b"));
   QVERIFY(QDir().mkpath(repoRoot + "/2026"));

   const quint16 port = 9878;
   SearchServer server(repoRoot, port);
   QVERIFY(server.start());
   QTest::qWait(100);

   Dirmodel model;
   QString err;
   QVERIFY2(model.addRemoteRepository(
                QUrl(QString("http://localhost:%1").arg(port)), &err),
            qPrintable(err));

   /* Level 0: repo. */
   QModelIndex repo = model.index(0, 0, QModelIndex());
   QVERIFY(repo.isValid());

   /* Level 1: expand the repo.  The first rowCount inserts a
    * "Loading…" placeholder while the async /browse runs. */
   QCOMPARE(model.rowCount(repo), 1);
   QTRY_COMPARE(model.rowCount(repo), 2);  // 2025 + 2026

   /* Find the "2025" child. */
   QModelIndex y2025;
   for (int i = 0; i < model.rowCount(repo); i++) {
      QModelIndex idx = model.index(i, 0, repo);
      if (model.data(idx, Qt::DisplayRole).toString() == "2025") {
         y2025 = idx;
         break;
      }
   }
   QVERIFY(y2025.isValid());

   /* Level 2: expand "2025".  This is the case the user reports
    * silently producing no traffic.  Same pattern: first rowCount
    * is the placeholder, then async fills in the real two children
    * ("a" and "b"). */
   QCOMPARE(model.rowCount(y2025), 1);   // placeholder
   QTRY_COMPARE(model.rowCount(y2025), 2);  // a + b

   QStringList names;
   for (int i = 0; i < model.rowCount(y2025); i++)
      names << model.data(model.index(i, 0, y2025),
                          Qt::DisplayRole).toString();
   names.sort();
   QCOMPARE(names, QStringList({"a", "b"}));

   server.stop();
}


void TestDirmodel::testRemoteRepositoryExpandChildThroughProxy()
{
   /* Same shape as testRemoteRepositoryExpandChild but every model
    * access goes through a Dirproxy, since that's the path the GUI
    * uses.  Use folder names that don't trip Dirproxy's year/month
    * filter (it's on by default) so the test exercises the
    * expansion mechanics rather than the filtering rules. */
   QTemporaryDir serverDir;
   QVERIFY(serverDir.isValid());
   QString repoRoot = serverDir.path() + "/papers";
   QVERIFY(QDir().mkpath(repoRoot + "/banks/Bank-Direct"));
   QVERIFY(QDir().mkpath(repoRoot + "/banks/Bradley"));

   const quint16 port = 9879;
   SearchServer server(repoRoot, port);
   QVERIFY(server.start());
   QTest::qWait(100);

   Dirmodel model;
   QString err;
   QVERIFY2(model.addRemoteRepository(
                QUrl(QString("http://localhost:%1").arg(port)), &err),
            qPrintable(err));

   Dirproxy proxy;
   proxy.setSourceModel(&model);

   /* Level 0: the proxy sees the single repo. */
   QCOMPARE(proxy.rowCount(QModelIndex()), 1);
   QModelIndex repo = proxy.index(0, 0, QModelIndex());
   QVERIFY(repo.isValid());

   /* Level 1: expand the repo via the proxy.  Wait for the async
    * to swap the placeholder for the real "banks" child. */
   QCOMPARE(proxy.rowCount(repo), 1);   // placeholder
   QTRY_COMPARE(proxy.data(proxy.index(0, 0, repo),
                           Qt::DisplayRole).toString(),
                QString("banks"));

   QModelIndex banks = proxy.index(0, 0, repo);
   QVERIFY(banks.isValid());

   /* Level 2: expand "banks" via the proxy.  If the proxy short-
    * circuits and doesn't ask the source, this never reaches
    * populateNode and no /browse fires — the bug.  Wait for the
    * real children to swap in. */
   QCOMPARE(proxy.rowCount(banks), 1);   // placeholder
   QTRY_VERIFY(proxy.data(proxy.index(0, 0, banks),
                          Qt::DisplayRole).toString() != "Loading…");
   QCOMPARE(proxy.rowCount(banks), 2);  // Bank-Direct + Bradley

   server.stop();
}


void TestDirmodel::testAddFiles()
{
   Dirmodel *model;

   model = setupModel(true);

   QString main_path = _tempDir->path() + "/main";

   QModelIndex main = model->index(main_path);
   model->buildCache(main, 0);

   QCOMPARE(getPaperTree(main_path),
            " + one|  + a|  + b|  - ofile|  - ofile2| + two|");

   Q_ASSERT(main.isValid());

   QString dst = _tempDir->path();

   touch(dst + "/main/one/newfile");

   // Refreshing 'a' should do nothing
   QModelIndex dir_a = model->index(main_path + "/one/a");
   model->refreshCacheFrom(dir_a, nullptr);

   QCOMPARE(getPaperTree(main_path),
            " + one|  + a|  + b|  - ofile|  - ofile2| + two|");

   // Refreshing 'one' should update
   QModelIndex one = model->index(main_path + "/one");
   model->refreshCacheFrom(one, nullptr);

   QCOMPARE(getPaperTree(main_path),
            " + one|  + a|  + b|  - newfile|  - ofile|  - ofile2| + two|");

   QDir dir;
   Q_ASSERT(dir.remove(dst + "/main/one/newfile"));

   // Refreshing 'two' should do nothing
   QModelIndex two = model->index(main_path + "/two");
   Q_ASSERT(two.isValid());
   model->refreshCacheFrom(two, nullptr);

   // Refreshing 'one' should update
   Q_ASSERT(one.isValid());
   model->refreshCacheFrom(one, nullptr);

   QCOMPARE(getPaperTree(main_path),
            " + one|  + a|  + b|  - ofile|  - ofile2| + two|");
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
