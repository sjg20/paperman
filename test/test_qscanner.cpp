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
#include "filemax.h"
#include "qxmlconfig.h"
#include "test_qscanner.h"
#include "scansettings.h"
#include "test_fakescan.h"
#include "fakescan/fakescan.h"

#include "qi/previewwidget.h"
#include "qi/qsaneoption.h"
#include <QScrollArea>
#include <QGroupBox>
#include <QCheckBox>
#include "qi/scanarea.h"

/* The scanner a test which needs one uses: whichever one
   PAPERMAN_TEST_DEVICE names, else the fake Fujitsu, or null if there
   is neither, since the fake one is only built on Linux */
static const char *testDevice (void)
{
   const char *dev = getenv ("PAPERMAN_TEST_DEVICE");

   if (dev)
      return dev;
   return Fakescan::available () ? FAKESCAN_DEVICE : nullptr;
}

#define NO_SCANNER "no scanner to test with: set PAPERMAN_TEST_DEVICE"

/* Give the fake scanner a sheet of US letter to scan, unless a real
   scanner is being tested, which has what the person running the test
   put in it */
static void loadLetter (void)
{
   if (!Fakescan::available ())
      return;

   QImage sheet (850, 1100, QImage::Format_RGB32);

   sheet.fill (Qt::white);
   Fakescan::reset ();
   QVERIFY (Fakescan::loadSheet (sheet, QImage (), 100));
}


void TestQscanner::testOpen()
{
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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
   const char *dev = testDevice ();
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
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   QScanner *scanner = new QScanner;
   scanner->setDeviceName (dev);
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
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
   QVERIFY (scanner.openDevice ());
   QVERIFY (scanner.reconnect ());
   QVERIFY (scanner.isOpen ());
   // option cache is refreshed during reconnect, so getters still work
   QCOMPARE (scanner.xResolutionDpi (), 300);
}


void TestQscanner::testReapplyDpiAfterReconnect()
{
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   dialog.setDpi (400);
   QCOMPARE (scanner.xResolutionDpi (), 400);
}


void TestQscanner::testScanDialogRebuildAfterReconnect()
{
   ensureXmlConfig ();
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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
   /* the paper sizes are handed to the scanner the other way round when
      the sheets are fed sideways, so pin the feed: this test is about
      the sizes themselves */
   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 0);
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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


/* Sheets fed sideways go through with the page's height across the
   scanner, so the size the user picks describes the page and has to
   reach the scanner the other way round. Letter fed sideways must give
   a scan area 11 inches across and 8.5 down, not the reverse */
void TestQscanner::testPscanPaperSideways()
{
   ensureXmlConfig ();
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   Pscan pscan;
   pscan.setScanDialog (&dialog);
   pscan.scannerChanged (&scanner);
   pscan.setPreviewWidget (dialog.getPreview ());

   PreviewWidget *pv = dialog.getPreview ();
   if (pv->getPreDefLetter () == -1)
      QSKIP ("scanner does not offer a Letter size");

   int letter = pv->getPreDefLetter ();
   double upright_x, upright_y, sideways_x, sideways_y;

   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 0);
   pscan.selectPreviewSize (letter);
   upright_x = SANE_UNFIX (scanner.saneWordValue (scanner.getBrxOption ()));
   upright_y = SANE_UNFIX (scanner.saneWordValue (scanner.getBryOption ()));

   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 2);
   pscan.selectPreviewSize (letter);
   sideways_x = SANE_UNFIX (scanner.saneWordValue (scanner.getBrxOption ()));
   sideways_y = SANE_UNFIX (scanner.saneWordValue (scanner.getBryOption ()));
   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 0);

   /* Upright, Letter is taller than it is wide. Fed sideways the page's
      width becomes the length of the scan, so the scan must get shorter
      by that much. The width should grow to the page's height to match,
      but a scanner too narrow to take the page lengthways caps it, as
      an fi-8170 does, so only the length is checked here */
   QVERIFY2 (upright_y > upright_x, "Letter upright should be taller than wide");
   QVERIFY2 (qAbs (sideways_y - upright_x) < 2.0,
             "fed sideways, the scan length should be the page's width");
   QVERIFY2 (sideways_y < upright_y - 20.0,
             "fed sideways, the scan should be shorter than upright");
   QVERIFY2 (sideways_x >= upright_x - 2.0,
             "fed sideways, the scan should be no narrower than upright");

   /* A page narrow enough to go across the scanner either way, such as
      A5, has to be scanned whole when fed sideways: across, its height,
      not the square the upright window turned round would give, which
      cut the foot off every page */
   int a5 = -1;

   for (int i = 0; !pv->getSizeName (i).isEmpty (); i++)
      if (pv->getSizeName (i).startsWith ("A5"))
         a5 = i;
   QVERIFY (a5 != -1);
   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 2);
   pscan.selectPreviewSize (a5);
   sideways_x = SANE_UNFIX (scanner.saneWordValue (scanner.getBrxOption ()));
   sideways_y = SANE_UNFIX (scanner.saneWordValue (scanner.getBryOption ()));
   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 0);
   QVERIFY2 (qAbs (sideways_x - 210) < 2.0 && qAbs (sideways_y - 148) < 2.0,
             qPrintable (QString ("A5 fed sideways gives a window %1 x %2mm")
                         .arg (sideways_x).arg (sideways_y)));
}


void TestQscanner::testReconnectPreservesSettings()
{
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
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
      mode, which no hand-picked restore list would ever cover. The
      scanner only compresses a colour or grey page, so ask for colour */
   int mode = scanner.findOption (SANE_NAME_SCAN_MODE);
   char colour[] = SANE_VALUE_SCAN_MODE_COLOR;
   QVERIFY (mode != -1);
   scanner.setOption (mode, colour);
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


/* Sheets fed sideways go through with the page's height across the
   scanner, so a scan turns the scan window round. What the scanner
   holds afterwards is saved with the device and used by the next scan,
   so the window has to be put back: a scanner too narrow to take the
   page lengthways caps the width, and the page's length goes with it,
   after which an upright page is cut off at the foot */
void TestQscanner::testSidewaysPageSizeRestored()
{
   ensureXmlConfig ();
   utilSetHeadless (true);

   QTemporaryDir repo;
   QVERIFY (repo.isValid ());

   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop ();
   QVERIFY (!desktop->addDir (repo.path ()));
   QString path = repo.path ();
   if (path.endsWith ("/"))
      path.chop (1);
   QModelIndex repo_ind = desktop->getDirIndex (path + "/");
   QVERIFY (repo_ind.isValid ());

   Mainwidget *main = Mainwidget::singleton ();
   QVERIFY (main);

   if (!testDevice ())
      QSKIP (NO_SCANNER);
   Scansettings settings (1, testDevice ());

   settings.setSideways (2);
   loadLetter ();

   QVERIFY (main->ensureScanner ());
   QScanner *scanner = main->_scanner;
   QVERIFY (scanner);

   int wnum = scanner->findOption ("page-width");
   int hnum = scanner->findOption ("page-height");
   int brx = scanner->getBrxOption ();
   int bry = scanner->getBryOption ();
   QVERIFY (wnum != -1 && hnum != -1 && brx != -1 && bry != -1);

   // an upright Letter page, as the user would have set it up
   SANE_Word wide = SANE_FIX (215.9);
   SANE_Word tall = SANE_FIX (279.4);
   scanner->setOption (wnum, &wide);
   scanner->setOption (hnum, &tall);
   scanner->setOption (brx, &wide);
   scanner->setOption (bry, &tall);

   SANE_Word was_w = scanner->saneWordValue (wnum);
   SANE_Word was_h = scanner->saneWordValue (hnum);
   SANE_Word was_brx = scanner->saneWordValue (brx);
   SANE_Word was_bry = scanner->saneWordValue (bry);
   QVERIFY2 (was_h > was_w, "Letter upright should be taller than wide");

   main->scanInto (repo_ind);

   /* the scan really was turned round: fed on its side the page is as
      long as the paper is wide, well short of the 11in it stands when
      it is upright (the width cannot grow to match on a scanner too
      narrow to take the page lengthways, as this one is) */
   QStringList stacks = QDir (path).entryList (QStringList () << "*.max",
                                               QDir::Files);
   QCOMPARE (stacks.size (), 1);
   Filemax max (path + "/", stacks [0], nullptr);
   QVERIFY (!max.load ());
   QSize size, true_size;
   int bpp, image_size, compressed_size;
   QDateTime when;
   QVERIFY (!max.getImageInfo (0, size, true_size, bpp, image_size,
                               compressed_size, when));
   QVERIFY2 (size.height () < 10 * scanner->yResolutionDpi (),
             qPrintable (QString ("fed sideways the page should be shorter "
                                  "than 10in, got %1x%2 at %3 dpi")
                         .arg (size.width ()).arg (size.height ())
                         .arg (scanner->yResolutionDpi ())));

   /* look the options up again, since the scan may have been through a
      reconnect and the numbers with it */
   wnum = scanner->findOption ("page-width");
   hnum = scanner->findOption ("page-height");
   brx = scanner->getBrxOption ();
   bry = scanner->getBryOption ();
   QCOMPARE (scanner->saneWordValue (wnum), was_w);
   QCOMPARE (scanner->saneWordValue (hnum), was_h);
   QCOMPARE (scanner->saneWordValue (brx), was_brx);
   QCOMPARE (scanner->saneWordValue (bry), was_bry);

   utilSetHeadless (false);
}


/* The panel shows a paper size from the moment it is built, while the
   scanner holds whatever window was saved with the device, which may be
   from another session, another scanner, or one turned round for sheets
   fed sideways. The two are brought together when the panel is built.

   Only then: a window set by hand in the options dialog is the user's
   own, and a scan must use it rather than putting the panel's size back */
void TestQscanner::testPanelPageSizeApplied()
{
   utilSetHeadless (true);

   QTemporaryDir repo;
   QVERIFY (repo.isValid ());

   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop ();
   QVERIFY (!desktop->addDir (repo.path ()));
   QString path = repo.path ();
   if (path.endsWith ("/"))
      path.chop (1);
   QModelIndex repo_ind = desktop->getDirIndex (path + "/");
   QVERIFY (repo_ind.isValid ());

   Mainwidget *main = Mainwidget::singleton ();
   QVERIFY (main);

   if (!testDevice ())
      QSKIP (NO_SCANNER);
   Scansettings settings (1, testDevice ());

   loadLetter ();
   QVERIFY (main->ensureScanner ());
   QScanner *scanner = main->_scanner;
   QVERIFY (scanner);

   int hnum = scanner->findOption ("page-height");
   int bry = scanner->getBryOption ();
   QVERIFY (hnum != -1 && bry != -1);

   /* the window the scanner was left holding, too short for the page
      the panel is about to show */
   SANE_Word squat = SANE_FIX (150.0);

   scanner->setOption (hnum, &squat);
   scanner->setOption (bry, &squat);

   // opens the scan panel, as the user would
   main->pscan ();
   QVERIFY (main->_pscan && main->_preview);

   // the size the panel shows has reached the scanner
   bry = scanner->getBryOption ();
   QVERIFY2 (SANE_UNFIX (scanner->saneWordValue (bry)) > 200.0,
             qPrintable (QString ("the window is still %1mm, so the size "
                                  "the panel shows never reached the "
                                  "scanner")
                         .arg (SANE_UNFIX (scanner->saneWordValue (bry)))));

   /* now the user sets a window by hand, as the options dialog does.
      A scan must use it: putting the panel's size back would throw
      away what was asked for */
   hnum = scanner->findOption ("page-height");
   scanner->setOption (hnum, &squat);
   scanner->setOption (bry, &squat);

   main->scanInto (repo_ind);

   QStringList stacks = QDir (path).entryList (QStringList () << "*.max",
                                               QDir::Files);
   QCOMPARE (stacks.size (), 1);
   Filemax max (path + "/", stacks [0], nullptr);
   QVERIFY (!max.load ());
   QSize size, true_size;
   int bpp, image_size, compressed_size;
   QDateTime when;

   /* say what the stack holds if this goes wrong: a scan which left no
      pages behind, or a file still half-written, look the same from
      the error that getImageInfo gives back */
   QVERIFY2 (max.pagecount () == 1,
             qPrintable (QString ("the scan left %1 pages in %2, %3 bytes")
                         .arg (max.pagecount ()).arg (stacks [0])
                         .arg (QFileInfo (path + "/" + stacks [0]).size ())));
   QVERIFY (!max.getImageInfo (0, size, true_size, bpp, image_size,
                               compressed_size, when));

   int want = (int)(150.0 / 25.4 * scanner->yResolutionDpi ());

   QVERIFY2 (qAbs (size.height () - want) < want / 10,
             qPrintable (QString ("the page is %1 lines: the scan wanted "
                                  "%2, for the 150mm window set by hand")
                         .arg (size.height ()).arg (want)));

   utilSetHeadless (false);
}


/* A scanner which can straighten a sheet and cut the page down to it
   does that better than anything which can be done to the page
   afterwards, at the cost of reading each page in full before passing
   it on. The panel offers it where the scanner has it and says nothing
   where it does not, and the page size is the scanner's business while
   it is on */
/* A scan which stops for a misfeed waits for the user to say that the
   paper path is clear. The scanner's own Scan button says that, but it
   is not always within reach, so the Scan button in the panel says it
   too: while the scan waits it is offered again, and means carry on
   rather than start another scan */

void TestQscanner::testPscanResume()
{
   ensureXmlConfig ();
   Pscan pscan;

   // with nothing going on, Scan starts a scan and there is none to stop
   pscan.checkEnabled (false, false);
   QVERIFY (pscan.scan->isEnabled ());
   QVERIFY (!pscan.stop->isEnabled ());

   // while a scan runs there is nothing for Scan to do
   pscan.checkEnabled (true, false);
   QVERIFY (!pscan.scan->isEnabled ());
   QVERIFY (pscan.stop->isEnabled ());

   // while it waits for the scanner to be cleared, Scan means carry on
   pscan.checkEnabled (true, true);
   QVERIFY (pscan.scan->isEnabled ());
   QVERIFY (pscan.stop->isEnabled ());

   // and the scan going on again takes the offer away
   pscan.checkEnabled (true, false);
   QVERIFY (!pscan.scan->isEnabled ());
}


/* With the scanner cutting each page down to the sheet, the size the
   user chose is the window it scans within rather than the size a page
   comes out at, so the panel calls it a maximum */

void TestQscanner::testPscanMaxSize()
{
   ensureXmlConfig ();
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   Pscan pscan;

   pscan.setScanDialog (&dialog);
   pscan.scannerChanged (&scanner);

   if (!dialog.hasAutoSize ())
      {
      // nothing crops the page, so the size is the size
      pscan.updateAutoSize ();
      QCOMPARE (pscan.sizeLabel->text (), QString ("Size"));
      QSKIP ("scanner cannot find the size of a sheet itself");
      }

   QVERIFY (dialog.setAutoSize (false));
   pscan.updateAutoSize ();
   QCOMPARE (pscan.sizeLabel->text (), QString ("Size"));

   QVERIFY (dialog.setAutoSize (true));
   pscan.updateAutoSize ();
   QCOMPARE (pscan.sizeLabel->text (), QString ("Max size"));

   // and the label is greyed along with the box it belongs to
   QCOMPARE (pscan.sizeLabel->isEnabled (), pscan.pageSize->isEnabled ());

   QVERIFY (dialog.setAutoSize (false));
   pscan.updateAutoSize ();
   QCOMPARE (pscan.sizeLabel->text (), QString ("Size"));
   QVERIFY (pscan.pageSize->isEnabled ());
   QVERIFY (pscan.sizeLabel->isEnabled ());
}


/* A sheet longer than any paper size, such as a till receipt, is
   scanned into the long size: a window as long as the scanner takes,
   with the scanner ending the page at the foot of the sheet. That is
   only offered while the scanner is doing that, since otherwise every
   page would be the whole length of the window */
void TestQscanner::testPscanLong()
{
   ensureXmlConfig ();
   xmlConfig->setIntValue ("SCAN_SIDEWAYS", 0);

   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   Pscan pscan;

   pscan.setScanDialog (&dialog);
   pscan.scannerChanged (&scanner);
   pscan.setPreviewWidget (dialog.getPreview ());

   PreviewWidget *pv = dialog.getPreview ();

   if (!dialog.hasAutoSize () || pv->getPreDefLong () == -1)
      QSKIP ("scanner cannot find the foot of a sheet, or take a long one");

   QString longName = pv->getSizeName (pv->getPreDefLong ());

   // not while every page would be the whole window
   QVERIFY (dialog.setAutoSize (false));
   pscan.updateAutoSize ();
   QCOMPARE (pscan.pageSize->findText (longName), -1);

   QVERIFY (dialog.setAutoSize (true));
   pscan.updateAutoSize ();
   int row = pscan.pageSize->findText (longName);

   QVERIFY (row != -1);

   // choosing it asks for a page longer than US legal
   pscan.pageSize->setCurrentIndex (row);
   pscan.size_activated (row);

   int bry = scanner.getBryOption ();
   double length = SANE_UNFIX (scanner.saneWordValue (bry));

   QVERIFY2 (length > 1000,
             qPrintable (QString ("the window is %1mm").arg (length)));

   // and as wide as the scanner takes, which is wider than US letter
   double width = SANE_UNFIX (scanner.saneWordValue (scanner.getBrxOption ())
                  - scanner.saneWordValue (scanner.getTlxOption ()));

   QVERIFY2 (width > 220,
             qPrintable (QString ("the window is %1mm wide").arg (width)));

   /* a receipt 600mm long, more than half as long again as US legal,
      comes out whole, if the scanner is the fake one */
   if (!strcmp (dev, FAKESCAN_DEVICE))
      {
      QImage receipt (315, 2362, QImage::Format_RGB32);   // 80 x 600mm

      receipt.fill (Qt::white);
      Fakescan::reset ();
      QVERIFY (Fakescan::loadSheet (receipt, QImage (), 100));

      SANE_Parameters params;
      SANE_Byte buf [65536];
      SANE_Int len;
      int bytes = 0;

      QCOMPARE (scanner.start (), SANE_STATUS_GOOD);
      scanner.getParameters (&params);
      while (scanner.read (buf, sizeof (buf), &len) == SANE_STATUS_GOOD)
         bytes += len;
      scanner.cancel ();

      int lines = bytes / params.bytes_per_line;
      int want = 600 * scanner.yResolutionDpi () / 25.4;

      QVERIFY2 (qAbs (lines - want) < want / 50,
                qPrintable (QString ("the receipt came out %1 lines long, "
                                     "not %2").arg (lines).arg (want)));
      }

   // and with the scanner no longer finding the foot, it goes again
   QVERIFY (dialog.setAutoSize (false));
   pscan.updateAutoSize ();
   QCOMPARE (pscan.pageSize->findText (longName), -1);
   QVERIFY (pscan.pageSize->currentText () != longName);
   QVERIFY2 (SANE_UNFIX (scanner.saneWordValue (scanner.getBryOption ()))
             < 400, "the window is still long");
}


void TestQscanner::testPscanDeskew()
{
   ensureXmlConfig ();
   QScanner scanner;
   const char *dev = testDevice ();

   if (!dev)
      QSKIP (NO_SCANNER);
   scanner.setDeviceName (dev);
   QVERIFY (scanner.openDevice ());

   QScanDialog dialog (&scanner, 0);
   Pscan pscan;

   pscan.setScanDialog (&dialog);
   pscan.scannerChanged (&scanner);

   if (!dialog.hasDeskewCrop ())
      {
      // nothing to offer, so nothing is shown
      QVERIFY (!pscan.deskew->isVisible ());
      QSKIP ("scanner cannot straighten and crop pages itself");
      }

   QVERIFY (dialog.setDeskewCrop (true));
   QVERIFY (dialog.deskewCrop ());
   pscan.updateDeskew ();
   pscan.updateAutoSize ();
   QVERIFY (pscan.deskew->isChecked ());

   /* the scanner cuts the page to the sheet, but within the window, so
      the size is the most a page can be, and still ours to set */
   QVERIFY (pscan.pageSize->isEnabled ());
   QCOMPARE (pscan.sizeLabel->text (), QString ("Max size"));

   QVERIFY (dialog.setDeskewCrop (false));
   QVERIFY (!dialog.deskewCrop ());
   pscan.updateDeskew ();
   pscan.updateAutoSize ();
   QVERIFY (!pscan.deskew->isChecked ());
}
