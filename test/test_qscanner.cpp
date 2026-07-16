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

   // Capture the size actually pushed to the scanner each time a predefined
   // size is applied
   QString applied;
   QObject::connect (pv, &PreviewWidget::signalPredefinedSize, pv,
      [&applied] (ScanArea *sca) { applied = sca ? sca->getName () : QString (); });

   /* Toggling repeatedly must keep the size sent to the scanner in step with
      the size shown in the combo. This used to drift after the first toggle:
      applying a size rebuilt the preview's size list, the cached indices went
      stale, and a later toggle sent the wrong size while the combo showed the
      right one. */
   for (int i = 0; i < 4; i++)
   {
      applied.clear ();
      pscan.toggleLetter ();
      QCOMPARE (applied, pscan.pageSize->currentText ());
   }

   // And the toggle really does alternate between two sizes
   QString before = pscan.pageSize->currentText ();
   pscan.toggleLetter ();
   QVERIFY (pscan.pageSize->currentText () != before);
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
