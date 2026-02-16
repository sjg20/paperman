#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "test_utils.h"

void TestUtils::testDetectYear()
{
   int pos;

   QCOMPARE(utilDetectYear("bills/2024/03mar/fred.max", pos), 2024);
   QCOMPARE(pos, 6);
   QCOMPARE(utilDetectYear("bills/tax/2023/fred.max", pos), 2023);
   QCOMPARE(pos, 10);
   QCOMPARE(utilDetectYear("bills/tax/1899/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/20201/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/12020/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/a2020/fred.max", pos), 2020);
   QCOMPARE(pos, 11);
   QCOMPARE(utilDetectYear("bills/tax/12020b/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/a2020b/fred.max", pos), 2020);
   QCOMPARE(pos, 11);
   QCOMPARE(utilDetectYear("2024/fred.max", pos), 2024);
   QCOMPARE(pos, 0);
   QCOMPARE(utilDetectYear("2023", pos), 2023);
   QCOMPARE(pos, 0);
   QCOMPARE(utilDetectYear("01jan", pos), 0);
}

void TestUtils::testDetectMonth()
{
   int pos;

   QCOMPARE(utilDetectMonth("bills/2024/03mar/fred.max", pos), 3);
   QCOMPARE(pos, 11);
   QCOMPARE(utilDetectMonth("bills/2024/04-mar/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("bills/2024/07mar/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("bills/2024/07-marc/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("bills/2024/07-mmar/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("03mar/fred.max", pos), 3);
   QCOMPARE(pos, 0);
   QCOMPARE(utilDetectMonth("08aug", pos), 8);
   QCOMPARE(pos, 0);
}

void TestUtils::testDetectMatches()
{
   QStringList matches, final, missing;
   QDate date = QDate(2024, 9, 1);

   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 0);

   // Should ignore a month if it isn't preceeded by a year
   matches << "bills/03mar";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 0);

   matches << "bills/2024/03mar";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 0);

   // Check it can detect a month
   matches << "bills/2024/09sep";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2024/09sep");
   QCOMPARE(missing.size(), 0);

   // Check it can detect a year
   matches << "bills/2024";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 2);
   QCOMPARE(final[0], "bills/2024/09sep");
   QCOMPARE(final[1], "bills/2024");
   QCOMPARE(missing.size(), 0);

   // Advance to Oct 2024; check it can suggest adding a month
   date = QDate(2024, 10, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2024");
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2024/10oct");

   // Make sure it only suggests this if the year is right
   date = QDate(2023, 10, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);

   // Advance to Jan 2025; check it doesn't suggest adding January, since Dec
   // isn't there. But it should suggest adding 2025
   date = QDate(2025, 1, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025");

   // Advance to Feb 2025; check that it suggests adding 2025
   date = QDate(2025, 2, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025");

   // Make to Jan 2025; now add Dec24 and check that it suggests adding Jan25
   date = QDate(2025, 1, 15);
   matches << "bills/2024/12dec";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025/01jan");

   // ...but not if Jan25 already exists
   matches << "bills/2025/01jan";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2025/01jan");
   QCOMPARE(missing.size(), 0);
if (0) {
   // See that it suggests Dec24 as well
   date = QDate(2025, 1, 15);
   matches << "bills/2024/12dec";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025/01jan");
}
   // Now move to 2026 and make sure it suggests to add that
   matches << "bills/2025";
   date = QDate(2026, 1, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2026");
}

void TestUtils::compare_trees(TreeItem *node1, TreeItem *node2)
{
   if (!node1 || !node2) {
      QCOMPARE(node1, node2);
      return;
   }
   QCOMPARE(node1->dirName(), node2->dirName());
   QCOMPARE(node1->childCount(), node2->childCount());
   for (int i = 0; i < node1->childCount(); i++)
      compare_trees(node1->child(i), node2->child(i));
}

void TestUtils::touch(const QString& dirpath, QString fname)
{
   QFile file(dirpath + "/" + fname);
   file.open(QIODevice::WriteOnly);
}

void TestUtils::createDirStructure(QTemporaryDir& tmp)
{
   QVERIFY(tmp.isValid());

   QString root = tmp.path();
   QDir dir(root);
   dir.mkpath(root + "/dir2");
   dir.mkpath(root + "/somedir/more-subdir");
   touch(root, "1");
   touch(root, "2");
   touch(root, "3");
   touch(root, "asc2");
   touch(root + "/somedir", "somefile");
   touch(root + "/somedir/more-subdir", "another-file");
   touch(root + "/dir2", "4");
}

void TestUtils::testScanDir()
{
   QTemporaryDir tmp;
   TreeItem *root, *chk;

   createDirStructure(tmp);
   root = utilScanDir(tmp.path(), nullptr);
   QString fname = cacheFile(tmp.path());
   utilWriteTree(fname, root);
   chk = utilReadTree(fname, tmp.path());
   compare_trees(root, chk);
   TreeItem::freeTree(chk);
   QCOMPARE(root->dirName(), tmp.path());
   QCOMPARE(root->childCount(), 6);
   TreeItem *child = root->child("2");
   QCOMPARE(child->dirName(), "2");

   QByteArray ba;
   QTextStream stream(&ba);
   root->write(stream, 0);
   stream.flush();
   stream.seek(0);
   QCOMPARE(stream.readLine(), " - 1");
   QCOMPARE(stream.readLine(), " - 2");
   QCOMPARE(stream.readLine(), " - 3");
   QCOMPARE(stream.readLine(), " - asc2");
   QCOMPARE(stream.readLine(), " + dir2");
   QCOMPARE(stream.readLine(), "  - 4");
   QCOMPARE(stream.readLine(), " + somedir");
   QCOMPARE(stream.readLine(), "  + more-subdir");
   QCOMPARE(stream.readLine(), "   - another-file");
   QCOMPARE(stream.readLine(), "  - somefile");
   QVERIFY(stream.atEnd());

   TreeItem::freeTree(root);
}

void TestUtils::testAdopt()
{
    QTemporaryDir tmp;
    TreeItem *root, *chk;

    createDirStructure(tmp);
    root = utilScanDir(tmp.path(), nullptr);

    QCOMPARE(root->childCount(), 6);

    chk = new TreeItem({"wibble"});
    chk->adopt(root);

    QCOMPARE(root->childCount(), 0);
    delete root;

    QCOMPARE(chk->childCount(), 6);
    QCOMPARE(chk->child(0)->data(0).toString(), "1");
    QCOMPARE(chk->child(1)->data(0).toString(), "2");
    QCOMPARE(chk->child(2)->data(0).toString(), "3");
    QCOMPARE(chk->child(3)->data(0).toString(), "asc2");
    QCOMPARE(chk->child(4)->data(0).toString(), "dir2");
    QCOMPARE(chk->child(5)->data(0).toString(), "somedir");

    QCOMPARE(chk->child(0)->childCount(), 0);
    QCOMPARE(chk->child(1)->childCount(), 0);
    QCOMPARE(chk->child(2)->childCount(), 0);
    QCOMPARE(chk->child(3)->childCount(), 0);
    QCOMPARE(chk->child(4)->childCount(), 1);
    QCOMPARE(chk->child(5)->childCount(), 2);
}

void TestUtils::testImageDepth()
{
   // Pure black image should be detected as 1bpp
   QImage black(10, 10, QImage::Format_ARGB32);
   black.fill(QColor(0, 0, 0));
   QCOMPARE(utilImageDepth(black), 1);

   // Mid-grey image should be detected as 8bpp
   QImage grey(10, 10, QImage::Format_ARGB32);
   grey.fill(QColor(128, 128, 128));
   QCOMPARE(utilImageDepth(grey), 8);

   // Colour image should be detected as 24bpp
   QImage colour(10, 10, QImage::Format_ARGB32);
   colour.fill(QColor(255, 0, 0));
   QCOMPARE(utilImageDepth(colour), 24);

   // Mostly black with one grey pixel should be 8bpp
   QImage mixGrey(10, 10, QImage::Format_ARGB32);
   mixGrey.fill(QColor(0, 0, 0));
   mixGrey.setPixelColor(5, 5, QColor(128, 128, 128));
   QCOMPARE(utilImageDepth(mixGrey), 8);

   // Mostly grey with one colour pixel should be 24bpp
   QImage mixColour(10, 10, QImage::Format_ARGB32);
   mixColour.fill(QColor(128, 128, 128));
   mixColour.setPixelColor(5, 5, QColor(255, 0, 0));
   QCOMPARE(utilImageDepth(mixColour), 24);
}

void TestUtils::testFindItem()
{
    const TreeItem *chk;
    QTemporaryDir tmp;
    TreeItem *root, *chkw;

    createDirStructure(tmp);
    root = utilScanDir(tmp.path(), nullptr);

    chk = root->findItem("");
    Q_ASSERT(chk != nullptr);
    QCOMPARE(chk, root);

    chk = root->findItem("3");
    Q_ASSERT(chk != nullptr);
    Q_ASSERT(chk != root);
    QCOMPARE(chk->data(0).toString(), "3");
    chkw = root->findItemW("3");
    QCOMPARE(chk, chkw);

    chk = root->findItem("dir2/4");
    Q_ASSERT(chk != nullptr);
    Q_ASSERT(chk != root);
    QCOMPARE(chk->data(0).toString(), "4");
    chkw = root->findItemW("dir2/4");
    QCOMPARE(chk, chkw);

    chk = root->findItem("somedir/more-subdir/another-file");
    Q_ASSERT(chk != nullptr);
    Q_ASSERT(chk != root);
    QCOMPARE(chk->data(0).toString(), "another-file");
    chkw = root->findItemW("somedir/more-subdir/another-file");
    QCOMPARE(chk, chkw);
}
