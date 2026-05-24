#include <QtTest/QtTest>

#include "qscanner.h"
#include "test_qscanner.h"

#define SIMUL_NAME "simulscan"


void TestQscanner::testOpenSimul()
{
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());
   QVERIFY (scanner.isOpen ());
   QCOMPARE (scanner.xResolutionDpi (), 300);
   QCOMPARE (scanner.yResolutionDpi (), 300);
}


void TestQscanner::testReconnectKeepsOpen()
{
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());
   QVERIFY (scanner.reconnect ());
   QVERIFY (scanner.isOpen ());
   // option cache is refreshed during reconnect, so getters still work
   QCOMPARE (scanner.xResolutionDpi (), 300);
}


void TestQscanner::testReapplyDpiAfterReconnect()
{
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   // Set DPI to a non-default value
   scanner.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);

   // Reconnect resets the scanner to defaults (same as a real device
   // returning to defaults after sane_exit/sane_init)
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 300);

   // Re-apply the preset value through QScanner. This must reach the
   // freshly-opened SANE handle.
   scanner.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);
   QCOMPARE (scanner.yResolutionDpi (), 400);
}
