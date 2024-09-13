#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

class TestPaperman: public QObject
{
   Q_OBJECT
private slots:
   void testDetectYear();
   void testDetectMonth();
   void testDetectMatches();
   void testScanDir();
private:
   // Create files in a temporary directory structure used for testing
   void createDirStructure(QTemporaryDir& tmp);

   // Create an empty file in a directory
   void touch(const QString& dirpath, QString fname);
};

void TestPaperman::testDetectYear()
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

void TestPaperman::testDetectMonth()
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

void TestPaperman::testDetectMatches()
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
   date = QDate(2025, 1, 1);
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

   // Now move to 2026 and make sure it suggests to add that
   matches << "bills/2025";
   date = QDate(2026, 1, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2026");
}

void TestPaperman::touch(const QString& dirpath, QString fname)
{
   QFile file(dirpath + "/" + fname);
   file.open(QIODevice::WriteOnly);
}

void TestPaperman::createDirStructure(QTemporaryDir& tmp)
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

void TestPaperman::testScanDir()
{
   QTemporaryDir tmp;
   TreeItem *root, *chk;

   createDirStructure(tmp);
   root = utilScanDir(tmp.path());
   QString fname = tmp.path() + "/.papertree";
   QCOMPARE(root->dirName(), tmp.path());
   QCOMPARE(root->childCount(), 6);
}

//QTEST_MAIN(TestPaperman)
#include "testpaperman.moc"

int test_run(int argc, char **argv, QApplication *app)
{
   TESTLIB_SELFCOVERAGE_START(#TestPaperman)
   QT_PREPEND_NAMESPACE(QTest::Internal::callInitMain)<TestPaperman>();
   app->setAttribute(Qt::AA_Use96Dpi, true);
   QTEST_DISABLE_KEYPAD_NAVIGATION
   TestPaperman tc;
   QTEST_SET_MAIN_SOURCE_PATH
   return QTest::qExec(&tc, argc, argv);
}
