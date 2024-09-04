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
   QCOMPARE(utilDetectYear("/vid/bills/2024/03mar/fred.max"), 2024);
   QCOMPARE(utilDetectYear("/vid/bills/tax/2023/fred.max"), 2023);
   QCOMPARE(utilDetectYear("/vid/bills/tax/1899/fred.max"), 0);
   QCOMPARE(utilDetectYear("/vid/bills/tax/20201/fred.max"), 0);
   QCOMPARE(utilDetectYear("/vid/bills/tax/12020/fred.max"), 0);
   QCOMPARE(utilDetectYear("/vid/bills/tax/a2020/fred.max"), 2020);
   QCOMPARE(utilDetectYear("/vid/bills/tax/12020b/fred.max"), 0);
   QCOMPARE(utilDetectYear("/vid/bills/tax/a2020b/fred.max"), 2020);
}

void TestPaperman::testDetectMonth()
{
   QCOMPARE(utilDetectMonth("/vid/bills/2024/03mar/fred.max"), 3);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/04-mar/fred.max"), 0);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/07-mar/fred.max"), 0);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/07-marc/fred.max"), 0);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/07-mmar/fred.max"), 0);
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
