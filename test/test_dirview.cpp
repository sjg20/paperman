#include <QSignalSpy>
#include <QtTest/QtTest>

#include "dirmodel.h"
#include "dirview.h"
#include "test_dirview.h"

Dirview *TestDirview::setupView(Dirmodel *&model, Dirproxy *&proxy)
{
   model = new Dirmodel();

   // First repo: uses the standard setupRepo() layout
   auto path = setupRepo();
   QString repo1 = path + "/main";
   model->addDir(repo1);

   // Second repo: separate temp directory with a different structure
   _tempDir2 = new QTemporaryDir();
   QDir dir2(_tempDir2->path());
   Q_ASSERT(dir2.mkdir("photos"));
   Q_ASSERT(dir2.mkdir("photos/holiday"));
   Q_ASSERT(dir2.mkdir("photos/family"));
   Q_ASSERT(dir2.mkdir("photos/family/kids"));

   // Add files alongside the directories
   touch(path + "/main/one/scan1.pdf");
   touch(path + "/main/one/scan2.pdf");
   touch(path + "/main/two/receipt.pdf");
   touch(_tempDir2->path() + "/photos/holiday/beach.jpg");
   touch(_tempDir2->path() + "/photos/holiday/sunset.jpg");
   touch(_tempDir2->path() + "/photos/family/portrait.jpg");
   touch(_tempDir2->path() + "/photos/family/kids/play.jpg");

   QString repo2 = _tempDir2->path() + "/photos";
   model->addDir(repo2);

   proxy = new Dirproxy();
   proxy->setSourceModel(model);

   auto *view = new Dirview();
   view->setModel(proxy);

   return view;
}

void TestDirview::testSelectContext()
{
   Dirmodel *model;
   Dirproxy *proxy;
   Dirview *view = setupView(model, proxy);

   // Select the first repo ("main")
   QModelIndex main_src = model->index(0, 0, QModelIndex());
   Q_ASSERT(main_src.isValid());
   QModelIndex main_proxy = proxy->mapFromSource(main_src);
   Q_ASSERT(main_proxy.isValid());

   view->selectContextItem(main_proxy);
   QCOMPARE(view->menuGetModelIndex(), main_proxy);

   // Select the second repo ("photos")
   QModelIndex photos_src = model->index(1, 0, QModelIndex());
   QModelIndex photos_proxy = proxy->mapFromSource(photos_src);

   view->selectContextItem(photos_proxy);
   QCOMPARE(view->menuGetModelIndex(), photos_proxy);
}

void TestDirview::testMenuGetters()
{
   Dirmodel *model;
   Dirproxy *proxy;
   Dirview *view = setupView(model, proxy);

   // Top-level items are virtual roots; check display name
   QModelIndex main_src = model->index(0, 0, QModelIndex());
   QModelIndex main_proxy = proxy->mapFromSource(main_src);
   view->selectContextItem(main_proxy);

   QCOMPARE(view->menuGetName(), QString("main"));

   // Check a child of the first repo
   QModelIndex one_src = model->index(0, 0, main_src);
   QModelIndex one_proxy = proxy->mapFromSource(one_src);
   view->selectContextItem(one_proxy);

   QCOMPARE(view->menuGetName(), QString("one"));
   QVERIFY(view->menuGetPath().contains("main/one"));

   // Check the second repo top-level
   QModelIndex photos_src = model->index(1, 0, QModelIndex());
   QModelIndex photos_proxy = proxy->mapFromSource(photos_src);
   view->selectContextItem(photos_proxy);

   QCOMPARE(view->menuGetName(), QString("photos"));

   // Children are sorted alphabetically: family, holiday
   QModelIndex family_src = model->index(0, 0, photos_src);
   QModelIndex family_proxy = proxy->mapFromSource(family_src);
   view->selectContextItem(family_proxy);

   QCOMPARE(view->menuGetName(), QString("family"));
   QVERIFY(view->menuGetPath().contains("photos/family"));

   QModelIndex holiday_src = model->index(1, 0, photos_src);
   QModelIndex holiday_proxy = proxy->mapFromSource(holiday_src);
   view->selectContextItem(holiday_proxy);

   QCOMPARE(view->menuGetName(), QString("holiday"));
   QVERIFY(view->menuGetPath().contains("photos/holiday"));
}

void TestDirview::testExpandDir()
{
   Dirmodel *model;
   Dirproxy *proxy;
   Dirview *view = setupView(model, proxy);

   // Show the view so that visual rects are calculated
   view->resize(400, 300);
   view->show();

   // Expand "main" - should show only subdirs "one" and "two", not files
   QModelIndex main_src = model->index(0, 0, QModelIndex());
   QModelIndex main_proxy = proxy->mapFromSource(main_src);
   view->expand(main_proxy);

   int rows = proxy->rowCount(main_proxy);
   QCOMPARE(rows, 2);

   QModelIndex child0 = proxy->index(0, 0, main_proxy);
   QModelIndex child1 = proxy->index(1, 0, main_proxy);
   QCOMPARE(proxy->data(child0, Qt::DisplayRole).toString(), "one");
   QCOMPARE(proxy->data(child1, Qt::DisplayRole).toString(), "two");

   // Check that the expanded children are visible in the view
   QVERIFY(view->visualRect(child0).isValid());
   QVERIFY(view->visualRect(child1).isValid());

   // Expand "one" - has subdirs "a" and "b" plus files scan1.pdf, scan2.pdf
   // Only subdirs should appear since Dirmodel filters to directories
   view->expand(child0);
   QCOMPARE(proxy->rowCount(child0), 2);

   // Expand "photos" - children sorted alphabetically: family, holiday
   QModelIndex photos_src = model->index(1, 0, QModelIndex());
   QModelIndex photos_proxy = proxy->mapFromSource(photos_src);
   view->expand(photos_proxy);

   int rows2 = proxy->rowCount(photos_proxy);
   QCOMPARE(rows2, 2);

   QModelIndex pchild0 = proxy->index(0, 0, photos_proxy);
   QModelIndex pchild1 = proxy->index(1, 0, photos_proxy);
   QCOMPARE(proxy->data(pchild0, Qt::DisplayRole).toString(), "family");
   QCOMPARE(proxy->data(pchild1, Qt::DisplayRole).toString(), "holiday");

   QVERIFY(view->visualRect(pchild0).isValid());
   QVERIFY(view->visualRect(pchild1).isValid());
}

void TestDirview::testSelectEmitsClicked()
{
   Dirmodel *model;
   Dirproxy *proxy;
   Dirview *view = setupView(model, proxy);

   QSignalSpy spy(view, &Dirview::clicked);

   // Click on first repo
   QModelIndex main_src = model->index(0, 0, QModelIndex());
   QModelIndex main_proxy = proxy->mapFromSource(main_src);

   view->selectContextItem(main_proxy);

   QCOMPARE(spy.count(), 1);
   QCOMPARE(spy.at(0).at(0).toModelIndex(), main_proxy);

   // Click on second repo
   QModelIndex photos_src = model->index(1, 0, QModelIndex());
   QModelIndex photos_proxy = proxy->mapFromSource(photos_src);

   view->selectContextItem(photos_proxy);

   QCOMPARE(spy.count(), 2);
   QCOMPARE(spy.at(1).at(0).toModelIndex(), photos_proxy);
}


void TestDirview::testRefreshKeepsSubdirsInView()
{
   Dirmodel *model;
   Dirproxy *proxy;
   Dirview *view = setupView(model, proxy);

   view->resize(400, 300);
   view->show();

   // Expand "main" and then "main/one" so the view has actually rendered
   // its children. This mirrors the user action where they expand a
   // directory in the tree before refreshing it.
   QModelIndex main_src = model->index(0, 0, QModelIndex());
   QModelIndex main_proxy = proxy->mapFromSource(main_src);
   view->expand(main_proxy);

   QModelIndex one_proxy = proxy->index(0, 0, main_proxy);
   view->expand(one_proxy);
   QCOMPARE(proxy->rowCount(one_proxy), 2);   // "a" and "b"

   // Trigger the refresh path that Desktopwidget::refreshDir() uses.
   QModelIndex one_src = proxy->mapToSource(one_proxy);
   model->refresh(one_src);

   // Sanity: the source model must still report the children since the
   // directories still exist on disk.
   QModelIndex one_src_after = model->index(_tempDir->path() + "/main/one");
   QVERIFY(one_src_after.isValid());
   QCOMPARE(model->rowCount(one_src_after), 2);

   // The proxy must agree.
   one_proxy = proxy->mapFromSource(one_src_after);
   QVERIFY(one_proxy.isValid());
   QCOMPARE(proxy->rowCount(one_proxy), 2);

   QStringList names;
   for (int i = 0; i < proxy->rowCount(one_proxy); i++)
      names << proxy->data(proxy->index(i, 0, one_proxy),
                           Qt::DisplayRole).toString();
   names.sort();
   QCOMPARE(names, QStringList({"a", "b"}));
}
