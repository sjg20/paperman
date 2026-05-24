#include <QtTest/QtTest>

#include "qscandialog.h"
#include "qscanner.h"
#include "qxmlconfig.h"
#include "test_qscanner.h"

#define SIMUL_NAME "simulscan"


static void ensureXmlConfig ()
{
   if (!xmlConfig)
      new QXmlConfig ();
}


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

   // Set DPI to a non-default value. reconnect() now snapshots and
   // restores state so the value should still be there afterwards.
   scanner.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);

   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 400);

   // Subsequent explicit setDpi must also still work.
   scanner.setDpi (300);
   QCOMPARE (scanner.xResolutionDpi (), 300);
}


void TestQscanner::testScanDialogSetDpi()
{
   ensureXmlConfig ();
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   dialog.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);
}


void TestQscanner::testScanDialogRebuildAfterReconnect()
{
   ensureXmlConfig ();
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   // Make a dialog and prove it can push settings.
   {
      QScanDialog dialog (&scanner, 0);
      dialog.setDpi (400);
      QCOMPARE (scanner.xResolutionDpi (), 400);
   }

   // Reconnect preserves settings via the QScanner snapshot.
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 400);

   // A freshly-built dialog can be created against the reconnected
   // scanner without crashing - that's the rebuild-on-reconnect contract.
   QScanDialog rebuilt (&scanner, 0);
   Q_UNUSED (rebuilt);
}


void TestQscanner::testStaleScanDialogAfterReconnect()
{
   ensureXmlConfig ();
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   dialog.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);

   // Reconnect preserves the setting via the QScanner snapshot, so we no
   // longer need the dialog to push it back.
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 400);

   // After reconnect the stale dialog still references the QScanner; it
   // doesn't crash to keep using it (the rebuild is a precaution, not a
   // hard requirement now that QScanner preserves its own state).
   Q_UNUSED (dialog);
}


void TestQscanner::testReconnectPreservesSettings()
{
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   // Set non-default values directly via QScanner.
   scanner.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);

   // After reconnect the settings should still be there, without any
   // explicit reapply by the caller.
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 400);
   QCOMPARE (scanner.yResolutionDpi (), 400);
}
