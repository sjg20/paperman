#include <QtTest/QtTest>

#include "../utils.h"

class TestPaperman: public QObject
{
   Q_OBJECT
private slots:
   void testDetectYear();
   void testDetectMonth();
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
   QCOMPARE(utilDetectMonth("/vid/bills/2024/04-mar/fred.max"), 3);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/07-mar/fred.max"), 3);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/07-marc/fred.max"), 0);
   QCOMPARE(utilDetectMonth("/vid/bills/2024/07-mmar/fred.max"), 0);
}

QTEST_MAIN(TestPaperman)
#include "testpaperman.moc"
