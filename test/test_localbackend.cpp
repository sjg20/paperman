#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../localbackend.h"

#include "test_localbackend.h"


/* create a small file with known content */
static void makeFile(const QString &path, const QByteArray &content)
{
   QFile f(path);
   QVERIFY(f.open(QIODevice::WriteOnly));
   f.write(content);
}


/* build a little repository:
      a.max, b.pdf, .paperdesk (hidden), sub/c.jpg */
static void makeRepo(const QString &root)
{
   makeFile(root + "/a.max", "max content");
   makeFile(root + "/b.pdf", "pdf content");
   makeFile(root + "/.paperdesk", "[Files]\n");
   QVERIFY(QDir().mkpath(root + "/sub"));
   makeFile(root + "/sub/c.jpg", "jpg content");
}


void TestLocalBackend::testRepositories()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   makeRepo(tmp.path());

   LocalBackend be(QStringList() << tmp.path()
                                 << tmp.path() + "/nosuch");
   QList<RepositoryInfo> repos = be.listRepositories();
   QCOMPARE(repos.size(), 2);
   QCOMPARE(repos[0].name, QFileInfo(tmp.path()).fileName());
   QCOMPARE(repos[0].path, tmp.path());
   QVERIFY(repos[0].exists);
   QVERIFY(!repos[1].exists);

   QCOMPARE(be.rootPathForName(repos[0].name), tmp.path());
   QCOMPARE(be.rootPathForName("nonexistent"), QString());
}


void TestLocalBackend::testBrowse()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   makeRepo(tmp.path());
   QString repo = QFileInfo(tmp.path()).fileName();

   LocalBackend be(QStringList() << tmp.path());

   /* the recursive per-directory counts come from the file cache,
      which the server builds at startup */
   be.loadCacheForRepo(tmp.path());

   // the root: the subdirectory first, then the files, dotfiles hidden
   DirectoryListing root = be.browseDirectory(repo, "");
   QVERIFY2(root.ok, qPrintable(root.error));
   QCOMPARE(root.entries.size(), 3);
   QCOMPARE(root.entries[0].name, QString("sub"));
   QVERIFY(root.entries[0].isDir);
   QCOMPARE(root.entries[0].count, 1);
   QCOMPARE(root.entries[1].name, QString("a.max"));
   QCOMPARE(root.entries[1].size, (qint64)11);
   QVERIFY(!root.entries[1].isDir);
   QCOMPARE(root.entries[2].name, QString("b.pdf"));

   // a subdirectory
   DirectoryListing sub = be.browseDirectory(repo, "sub");
   QVERIFY(sub.ok);
   QCOMPARE(sub.entries.size(), 1);
   QCOMPARE(sub.entries[0].name, QString("c.jpg"));
   QCOMPARE(sub.entries[0].path, QString("sub/c.jpg"));

   // a missing directory and a missing repository report not-found
   DirectoryListing bad = be.browseDirectory(repo, "nosuchdir");
   QVERIFY(!bad.ok);
   QVERIFY(bad.notFound);
   DirectoryListing norepo = be.browseDirectory("nosuchrepo", "");
   QVERIFY(!norepo.ok);
}


void TestLocalBackend::testReadFile()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   makeRepo(tmp.path());
   QString repo = QFileInfo(tmp.path()).fileName();

   LocalBackend be(QStringList() << tmp.path());

   FileFetch got = be.readFile(repo, "a.max");
   QVERIFY2(got.ok, qPrintable(got.error));
   QCOMPARE(got.bytes, QByteArray("max content"));

   FileFetch subFile = be.readFile(repo, "sub/c.jpg");
   QVERIFY(subFile.ok);
   QCOMPARE(subFile.bytes, QByteArray("jpg content"));

   FileFetch missing = be.readFile(repo, "nosuch.max");
   QVERIFY(!missing.ok);
   QVERIFY(missing.notFound);
}


void TestLocalBackend::testContentType()
{
   QCOMPARE(Backend::contentTypeForPath("a.pdf"),
            QString("application/pdf"));
   QCOMPARE(Backend::contentTypeForPath("a.jpg"), QString("image/jpeg"));
   QCOMPARE(Backend::contentTypeForPath("a.jpeg"), QString("image/jpeg"));
   QCOMPARE(Backend::contentTypeForPath("a.max"),
            QString("application/octet-stream"));
   QCOMPARE(Backend::contentTypeForPath("noext"),
            QString("application/octet-stream"));
}


void TestLocalBackend::testFileCache()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   makeRepo(tmp.path());

   LocalBackend be(QStringList() << tmp.path());
   int count = be.loadCacheForRepo(tmp.path());
   QCOMPARE(count, 3);
   QCOMPARE(be.cacheCount(tmp.path()), 3);
   QVERIFY(be.cacheBytes(tmp.path()) > 0);

   const QList<CachedFile> &files = be.fileCacheFor(tmp.path());
   QCOMPARE(files.size(), 3);
   QStringList paths;
   for (const CachedFile &f : files)
      paths << f.path;
   QVERIFY(paths.contains("a.max"));
   QVERIFY(paths.contains("b.pdf"));
   QVERIFY(paths.contains("sub/c.jpg"));
}
