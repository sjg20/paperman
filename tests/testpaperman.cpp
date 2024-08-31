#include <QtTest/QtTest>

#include "../utils.h"

class TestPaperman: public QObject
{
   Q_OBJECT
};


QTEST_MAIN(TestPaperman)
#include "testpaperman.moc"
