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

   // Reconnect: handle is fresh, options reset to defaults.
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 300);

   // Mainwidget's contract is to throw away the old QScanDialog and build
   // a fresh one after reconnect. A fresh dialog must be able to push
   // values to the new handle.
   QScanDialog rebuilt (&scanner, 0);
   rebuilt.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);
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

   // Reconnect without rebuilding the dialog. The dialog still references
   // the QScanner, whose mDeviceHandle is now fresh.
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 300);

   // Try to push a value through the stale dialog. If it works the simul
   // scanner is too forgiving to demonstrate the bug we observed on real
   // hardware; if it fails this captures why mainwidget needs the rebuild.
   dialog.setDpi (400);
   int dpi_after_stale = scanner.xResolutionDpi ();
   qDebug () << "stale-dialog setDpi result:" << dpi_after_stale;
   // Don't QCOMPARE - just record. The real-hardware bug shows up here.
   QVERIFY (dpi_after_stale == 300 || dpi_after_stale == 400);
}
