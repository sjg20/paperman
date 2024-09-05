#include <QtTest/QtTest>

#include "../utils.h"

class TestPaperman: public QObject
{
   Q_OBJECT
private slots:
   void testDetectYear();
   void testDetectMonth();
   void testDetectMatches();
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
}

void TestPaperman::testDetectMatches()
{
   QStringList matches, final;
   QDate date = QDate(2024, 9, 1);

   final = utilDetectMatches(date, matches);
   QCOMPARE(final.size(), 0);

   // Should ignore a month if it isn't preceeded by a year
   matches << "bills/03mar";
   final = utilDetectMatches(date, matches);
   QCOMPARE(final.size(), 0);

   matches << "bills/2024/03mar";
   final = utilDetectMatches(date, matches);
   QCOMPARE(final.size(), 0);

   // Check it can detect a month
   matches << "bills/2024/09sep";
   final = utilDetectMatches(date, matches);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2024/09sep");

   // Check it can detect a year
   matches << "bills/2024";
   final = utilDetectMatches(date, matches);
   QCOMPARE(final.size(), 2);
   QCOMPARE(final[0], "bills/2024/09sep");  // should be in other order
   QCOMPARE(final[1], "bills/2024");
}

QTEST_MAIN(TestPaperman)
#include "testpaperman.moc"
