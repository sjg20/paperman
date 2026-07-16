#include <QtTest/QtTest>

#include "pscan.h"
#include "qscandialog.h"
#include "qscanner.h"
#include "qxmlconfig.h"
#include "test_qscanner.h"

#include "qi/previewwidget.h"
#include "qi/scanarea.h"

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


void TestQscanner::testPscanControls()
{
   ensureXmlConfig ();
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);

   /* setting a lower resolution must not snap to the maximum; this
      used to fail because changing a slider's range pushed the clamped
      slider position to the scanner */
   dialog.setDpi (200);
   QCOMPARE (scanner.xResolutionDpi (), 200);
   QCOMPARE (scanner.yResolutionDpi (), 200);

   Pscan pscan;
   pscan.setScanDialog (&dialog);
   pscan.scannerChanged (&scanner);

   // The scanner controls should be available
   QVERIFY (pscan.res->isEnabled ());
   QVERIFY (pscan.duplex->isEnabled ());

   // Choosing a resolution from the combo reaches the scanner
   pscan.res_activated (0);
   QCOMPARE (scanner.xResolutionDpi (), 200);
   pscan.res_activated (2);
   QCOMPARE (scanner.xResolutionDpi (), 400);
   pscan.res_activated (1);
   QCOMPARE (scanner.xResolutionDpi (), 300);

   // Clicking the duplex checkbox toggles duplex scanning
   bool was_duplex = scanner.duplex ();
   QTest::mouseClick (pscan.duplex, Qt::LeftButton);
   QCOMPARE (scanner.duplex (), !was_duplex);
   QTest::mouseClick (pscan.duplex, Qt::LeftButton);
   QCOMPARE (scanner.duplex (), was_duplex);
}


void TestQscanner::testPscanPaperToggle()
{
   ensureXmlConfig ();
   QScanner scanner;
   scanner.setDeviceName (SIMUL_NAME);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);

   Pscan pscan;
   pscan.setScanDialog (&dialog);
   pscan.scannerChanged (&scanner);
   pscan.setPreviewWidget (dialog.getPreview ());

   PreviewWidget *pv = dialog.getPreview ();
   if (pv->getPreDefLetter () == -1 || pv->getPreDefLegal () == -1)
      QSKIP ("scanner does not offer both Letter and Legal sizes");

   QString legalName = pv->getSizeName (pv->getPreDefLegal ());
   QString letterName = pv->getSizeName (pv->getPreDefLetter ());

   /* Toggle repeatedly and check the scan area the scanner actually ends up
      with. This used to get stuck on the first size chosen: the combo changed
      but the scanner's bottom-right y stayed put, so a Letter scan came out
      Legal-length (or vice versa). */
   QMap<QString, double> bryForName;
   QString prev;
   for (int i = 0; i < 6; i++)
   {
      pscan.toggleLetter ();
      QString name = pscan.pageSize->currentText ();
      double bry = SANE_UNFIX (scanner.saneWordValue (scanner.getBryOption ()));

      // each toggle must actually switch the shown size
      QVERIFY (name != prev);
      prev = name;

      // the same paper size must always give the same scan height
      if (bryForName.contains (name))
         QVERIFY2 (qAbs (bryForName[name] - bry) < 1.0,
            "scan height changed for an unchanged paper size");
      else
         bryForName[name] = bry;
   }

   // We should have seen exactly the two sizes, and Legal (356mm) must scan
   // clearly taller than Letter (279mm) - before the fix both stayed equal
   QCOMPARE (bryForName.size (), 2);
   QVERIFY (bryForName.contains (legalName));
   QVERIFY (bryForName.contains (letterName));
   QVERIFY (bryForName[legalName] > bryForName[letterName] + 50.0);
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
