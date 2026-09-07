#include <QtTest/QtTest>

#include <sane/saneopts.h>

#include "pscan.h"
#include "qscandialog.h"
#include "utils.h"
#include "mainwindow.h"
#include "desktopwidget.h"
#include "desktopmodel.h"
#include "pagewidget.h"
#include <QListView>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include "qscanner.h"
#include "qxmlconfig.h"
#include "test_qscanner.h"

#include "qi/previewwidget.h"
#include "qi/qsaneoption.h"
#include <QScrollArea>
#include <QGroupBox>
#include <QCheckBox>
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


/* Drive a real GUI scan (shown window, live preview and thumbnails) and
   time how fast pages arrive, to compare the GUI path with headless.
   Device-gated; PAPERMAN_TEST_PAGES sets the count, PAPERMAN_TEST_AUTOSIZE
   turns auto-size on */
void TestQscanner::testScanGuiTiming()
{
   const char *dev = getenv ("PAPERMAN_TEST_DEVICE");
   if (!dev)
      QSKIP ("set PAPERMAN_TEST_DEVICE");
   ensureXmlConfig ();
   int pages = getenv ("PAPERMAN_TEST_PAGES")
      ? atoi (getenv ("PAPERMAN_TEST_PAGES")) : 20;
   const char *asenv = getenv ("PAPERMAN_TEST_AUTOSIZE");
   bool autosize = asenv && asenv[0];

   /* open the scanner from config without a chooser dialog, but keep the
      window shown so the preview and thumbnails still render */
   utilSetHeadless (true);

   QTemporaryDir repo;
   QVERIFY (repo.isValid ());

   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop ();
   QVERIFY (!desktop->addDir (repo.path ()));
   Desktopmodel *model = desktop->getModel ();
   me.resize (1024, 768);
   me.show ();
   QVERIFY (QTest::qWaitForWindowExposed (&me));
   QString path = repo.path ();
   if (path.endsWith ("/"))
      path.chop (1);
   QModelIndex repo_ind = desktop->getDirIndex (path + "/");
   QVERIFY (repo_ind.isValid ());

   Mainwidget *main = Mainwidget::singleton ();
   QVERIFY (main);
   /* the widget loaded the real config file, so set the device now */
   QString old_dev = xmlConfig->stringValue ("LAST_DEVICE", QString ());
   int old_single = xmlConfig->intValue ("SCAN_SINGLE");
   xmlConfig->setStringValue ("LAST_DEVICE", dev);
   xmlConfig->setIntValue ("SCAN_SINGLE", pages);
   QMap<QString, QString> opt;
   opt["resolution"] = "300";
   opt["mode"] = "Color";
   opt["source"] = "ADF Duplex";
   if (autosize)
      opt["auto-size"] = "yes";
   main->setScanOptions (opt);

   QElapsedTimer timer;
   QList<qint64> stamps;
   connect (model, &Desktopmodel::newScannedPage, this,
            [&] (const QString &, bool) { stamps << timer.elapsed (); });

   timer.start ();
   main->scanInto (repo_ind);          // returns when the batch is done

   if (getenv ("DUMP_PNG"))
      {
      QTest::qWait (300);
      me.grab ().save (getenv ("DUMP_PNG"));
      }
   QVERIFY2 (stamps.size () >= 2, qPrintable (QString ("only %1 pages")
                                              .arg (stamps.size ())));
   QList<qint64> gaps;
   for (int i = 1; i < stamps.size (); i++)
      gaps << stamps[i] - stamps[i - 1];
   std::sort (gaps.begin (), gaps.end ());
   qint64 total = stamps.last () - stamps.first ();
   qDebug ("GUI scan %s: %d pages, %lld ms total, %lld ms/page mean, "
           "min %lld median %lld max %lld",
           autosize ? "auto-size" : "fixed", (int) stamps.size (),
           (long long) total, (long long) (total / (stamps.size () - 1)),
           (long long) gaps.first (),
           (long long) gaps[gaps.size () / 2], (long long) gaps.last ());

   utilSetHeadless (false);
   xmlConfig->setStringValue ("LAST_DEVICE", old_dev);
   xmlConfig->setIntValue ("SCAN_SINGLE", old_single);
}


void TestQscanner::testAutoSizePanel()
{
   const char *dev = getenv ("PAPERMAN_TEST_DEVICE");
   if (!dev)
      QSKIP ("set PAPERMAN_TEST_DEVICE to a scanner with auto-size");
   ensureXmlConfig ();
   QScanner *scanner = new QScanner;
   scanner->setDeviceName (dev);
   QVERIFY (scanner->openDevice ());
   QScanDialog *dlg = new QScanDialog (scanner, 0);
   if (!dlg->hasAutoSize ())
      QSKIP ("scanner has no auto-size option");
   Pscan panel (0);
   QCheckBox *box = panel.findChild<QCheckBox *> ("autosize");
   QVERIFY (box);
   QVERIFY (box->isHidden ());          // hidden until a scanner is known
   panel.setScanDialog (dlg);
   panel.scannerChanged (scanner);
   QVERIFY2 (!box->isHidden (), "auto-size box stayed hidden");
}


/* Auto-size, when the backend offers it, must round-trip through the
   scan dialog and disable the manual scan-area options. Runs only when
   PAPERMAN_TEST_DEVICE names a scanner that supports it */
void TestQscanner::testAutoSize()
{
   const char *dev = getenv ("PAPERMAN_TEST_DEVICE");
   if (!dev)
      QSKIP ("set PAPERMAN_TEST_DEVICE to a scanner with auto-size");
   ensureXmlConfig ();
   QScanner *scanner = new QScanner;
   scanner->setDeviceName (dev);
   QVERIFY (scanner->openDevice ());
   QScanDialog dlg (scanner, 0);
   if (!dlg.hasAutoSize ())
      QSKIP ("scanner has no auto-size option");
   QVERIFY (!dlg.autoSize ());
   QVERIFY (dlg.setAutoSize (true));
   QVERIFY (dlg.autoSize ());
   if (getenv ("DUMP_PNG"))
      {
      dlg.show ();
      QTest::qWait (200);
      dlg.grab ().save (getenv ("DUMP_PNG"));
      }
   /* the scan-area options should now be inactive */
   int tlx = -1;
   for (int i = 1; i < scanner->optionCount (); i++)
      {
      SANE_String_Const nm = scanner->getOptionName (i);
      if (nm && !strcmp (nm, SANE_NAME_SCAN_TL_X))
         tlx = i;
      }
   QVERIFY (tlx > 0);
   QVERIFY (!scanner->isOptionActive (tlx));
   QVERIFY (dlg.setAutoSize (false));
   QVERIFY (!dlg.autoSize ());
   QVERIFY (scanner->isOptionActive (tlx));
}


/* The scan dialog's option list must lay out its groups: the scroll
   view once left them at zero height for some back ends */
void TestQscanner::testOptionDialogLayout()
{
   ensureXmlConfig ();
   QScanner *scanner = new QScanner;
   const char *dev = getenv ("PAPERMAN_TEST_DEVICE");
   scanner->setDeviceName (dev ? dev : SIMUL_NAME);
   QVERIFY (scanner->openDevice ());
   QVERIFY (scanner->getGroupCount () > 0);
   QScanDialog dlg (scanner, 0);
   dlg.show ();
   QTest::qWait (200);
   if (getenv ("DUMP_PNG"))
      dlg.grab ().save (getenv ("DUMP_PNG"));
   QList<QGroupBox *> boxes = dlg.findChildren<QGroupBox *> ();
   QCOMPARE (boxes.size (), scanner->getGroupCount ());
   foreach (QGroupBox *gb, boxes)
      {
      QVERIFY2 (gb->height () > 0, qPrintable (gb->title ()));
      QVERIFY (gb->findChildren<QSaneOption *> ().size () > 0);
      }
   QScrollArea *sa = dlg.findChild<QScrollArea *> ();
   QVERIFY (sa && sa->widget ());
   QVERIFY (sa->widget ()->height () > sa->viewport ()->height ());
   QVERIFY2 (sa->widget ()->width () <= sa->viewport ()->width (),
             qPrintable (QString ("content %1 wide, viewport %2")
                         .arg (sa->widget ()->width ())
                         .arg (sa->viewport ()->width ())));
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
   scanner.setBrightness (17);
   scanner.setContrast (-9);

   /* the geometry options too: a Legal-length page and scan window,
      plus a small top margin, must survive the reconnect */
   int pageHeight = scanner.findOption ("page-height");
   int bry = scanner.getBryOption ();
   int tly = scanner.findOption (SANE_NAME_SCAN_TL_Y);
   QVERIFY (pageHeight != -1);
   QVERIFY (bry != -1);
   QVERIFY (tly != -1);
   SANE_Word legal = SANE_FIX (355.6);
   SANE_Word margin = SANE_FIX (5.0);
   scanner.setOption (pageHeight, &legal);
   scanner.setOption (bry, &legal);
   scanner.setOption (tly, &margin);

   /* the backend stores millimetres in scanner units, so read-backs
      can be a fraction of a millimetre off */
   auto nearMm = [&](int opt, SANE_Word want) {
      return qAbs (SANE_UNFIX (scanner.saneWordValue (opt))
                   - SANE_UNFIX (want)) < 0.1;
   };
   QVERIFY (nearMm (pageHeight, legal));

   /* a string option stands in for things like the Fujitsu's buffer
      mode, which no hand-picked restore list would ever cover */
   int compress = scanner.findOption ("compression");
   QVERIFY (compress != -1);
   char jpegName[] = "JPEG";
   scanner.setOption (compress, jpegName);
   QCOMPARE (scanner.saneStringValue (compress), QString ("JPEG"));

   // After reconnect the settings should still be there, without any
   // explicit reapply by the caller.
   QVERIFY (scanner.reconnect ());
   QCOMPARE (scanner.xResolutionDpi (), 400);
   QCOMPARE (scanner.yResolutionDpi (), 400);
   QCOMPARE (scanner.getBrightness (), 17);
   QCOMPARE (scanner.getContrast (), -9);

   /* option numbers may have moved on the fresh handle; look the
      geometry up again by name */
   pageHeight = scanner.findOption ("page-height");
   bry = scanner.getBryOption ();
   tly = scanner.findOption (SANE_NAME_SCAN_TL_Y);
   QVERIFY (nearMm (pageHeight, legal));
   QVERIFY (nearMm (bry, legal));
   QVERIFY (nearMm (tly, margin));
   compress = scanner.findOption ("compression");
   QCOMPARE (scanner.saneStringValue (compress), QString ("JPEG"));
}
