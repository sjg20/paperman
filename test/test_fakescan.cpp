#include <QtTest/QtTest>
#include <QTemporaryDir>

#include <sane/saneopts.h>

#ifdef Q_OS_LINUX
#include <dlfcn.h>
#endif

#include "desktopwidget.h"
#include "filemax.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "qscanner.h"
#include "utils.h"

#include "fakescan/fakescan.h"
#include "scansettings.h"
#include "test_fakescan.h"

namespace {

/** the back door, looked up in the copy of the library libsane loads */
struct Backdoor
   {
   void (*reset) (void);
   int (*loadSheet) (const struct fakescan_sheet *);
   int (*sheetsLeft) (void);
   void (*setBacking) (unsigned int);
   };

Backdoor backdoor;
bool ready;

/* libsane reads its configuration on every sane_init(), and the scanner
   reads each sheet when it goes in the hopper, so these last as long as
   the tests do */
QTemporaryDir *conf_dir;
QTemporaryDir *sheet_dir;
int sheet_count;

}


bool Fakescan::setup (void)
{
#ifdef Q_OS_LINUX
   // a real scanner is wanted instead, so leave libsane as it is
   if (qEnvironmentVariableIsSet ("PAPERMAN_TEST_DEVICE"))
      return false;

   QString dir = QCoreApplication::applicationDirPath () + "/test/fakescan";
   QString lib = dir + "/" FAKESCAN_LIB;

   if (!QFile::exists (lib))
      return false;

   /* Give libsane a configuration of its own which lists only the
      fake scanner. It then loads no other back end, so a test cannot
      reach a real scanner, nor wait while the others look for theirs */
   conf_dir = new QTemporaryDir;
   sheet_dir = new QTemporaryDir;
   if (!conf_dir->isValid () || !sheet_dir->isValid ())
      return false;

   QFile conf (conf_dir->path () + "/dll.conf");

   if (!conf.open (QIODevice::WriteOnly))
      return false;
   conf.write (FAKESCAN_BACKEND "\n");
   conf.close ();
   qputenv ("SANE_CONFIG_DIR", conf_dir->path ().toLocal8Bit ());

   // the dll back end looks along LD_LIBRARY_PATH for back ends
   QByteArray path = qgetenv ("LD_LIBRARY_PATH");

   qputenv ("LD_LIBRARY_PATH", dir.toLocal8Bit ()
                               + (path.isEmpty () ? "" : ":" + path));

   /* Opening the file the dll back end will open gives the same copy of
      it, so the back door leads to the scanner a test has open. Holding
      it open also keeps the paper in the hopper when sane_exit() lets
      go of the back end, as a real scanner would */
   void *handle = dlopen (qPrintable (lib), RTLD_NOW);

   if (!handle)
      {
      qWarning () << "Cannot open the fake scanner:" << dlerror ();
      return false;
      }
   backdoor.reset = (void (*) (void))dlsym (handle, "fakescan_reset");
   backdoor.loadSheet = (int (*) (const struct fakescan_sheet *))
      dlsym (handle, "fakescan_load_sheet");
   backdoor.sheetsLeft = (int (*) (void))dlsym (handle, "fakescan_sheets_left");
   backdoor.setBacking = (void (*) (unsigned int))
      dlsym (handle, "fakescan_set_backing");
   ready = backdoor.reset && backdoor.loadSheet && backdoor.sheetsLeft
         && backdoor.setBacking;

   return ready;
#else
   return false;
#endif
}


bool Fakescan::available (void)
{
   return ready;
}


void Fakescan::reset (void)
{
   backdoor.reset ();
}


bool Fakescan::loadSheet (const QImage &front, const QImage &back, int dpi)
{
   QByteArray path [2];
   const QImage *side [2] = { &front, &back };
   struct fakescan_sheet sheet = {};

   // the scanner reads each side from a file, as a person would feed it
   for (int i = 0; i < 2; i++)
      if (!side [i]->isNull ())
         {
         QString fname = QString ("%1/sheet%2-%3.png").arg (sheet_dir->path ())
                         .arg (sheet_count).arg (i);

         if (!side [i]->save (fname, "PNG"))
            return false;
         path [i] = fname.toUtf8 ();
         }
   sheet_count++;

   sheet.front = path [0].isEmpty () ? nullptr : path [0].constData ();
   sheet.back = path [1].isEmpty () ? nullptr : path [1].constData ();
   sheet.dpi = dpi;

   return !backdoor.loadSheet (&sheet);
}


int Fakescan::sheetsLeft (void)
{
   return backdoor.sheetsLeft ();
}


void Fakescan::setBacking (QRgb rgb)
{
   backdoor.setBacking (rgb & 0xffffff);
}


/* a sheet of US letter at 10dpi, one colour all over */
static QImage plainSheet (QRgb colour)
{
   QImage image (85, 110, QImage::Format_RGB32);

   image.fill (colour);
   return image;
}


/* Open the fake scanner as paperman does, and set it up for a test.
   Anything which goes wrong fails the test */
static void openScanner (QScanner &scanner, const char *source,
                         const char *mode, int dpi)
{
   QVERIFY (scanner.initScanner ());
   QVERIFY (scanner.getDeviceList (false));
   scanner.setDeviceName (FAKESCAN_DEVICE);
   QVERIFY (scanner.openDevice ());

   QByteArray src (source), mod (mode);
   SANE_Word res = dpi;

   QCOMPARE (scanner.setOption (scanner.findOption (SANE_NAME_SCAN_SOURCE),
                                src.data ()), SANE_STATUS_GOOD);
   QCOMPARE (scanner.setOption (scanner.findOption (SANE_NAME_SCAN_MODE),
                                mod.data ()), SANE_STATUS_GOOD);
   QCOMPARE (scanner.setOption (scanner.findOption
                                (SANE_NAME_SCAN_RESOLUTION), &res),
             SANE_STATUS_GOOD);
}


/* Start a frame and read all of it, as a front end does

   \returns what sane_start() said if it failed, else what the reading
            ended with other than the end of the frame */
static SANE_Status readFrame (QScanner &scanner, SANE_Parameters &params,
                              QByteArray &data)
{
   SANE_Byte buf [65536];
   SANE_Int len;
   SANE_Status status = scanner.start ();

   data.clear ();
   if (status != SANE_STATUS_GOOD)
      return status;
   scanner.getParameters (&params);
   while ((status = scanner.read (buf, sizeof (buf), &len))
          == SANE_STATUS_GOOD)
      data.append ((const char *)buf, len);

   return status == SANE_STATUS_EOF ? SANE_STATUS_GOOD : status;
}


void TestFakescan::init ()
{
   if (!Fakescan::available ())
      QSKIP ("the fake scanner is not built here, or a real one is "
             "being tested");
   Fakescan::reset ();
}


void TestFakescan::testDevice ()
{
   QScanner scanner;

   QVERIFY (scanner.initScanner ());
   QVERIFY (scanner.getDeviceList (false));

   // the only scanner libsane knows about, besides paperman's own
   QStringList names;

   for (int i = 0; i < scanner.deviceCount (); i++)
      if (strcmp (scanner.name (i), SIMUL_NAME))
         names << scanner.name (i);
   QCOMPARE (names, QStringList () << FAKESCAN_DEVICE);

   scanner.setDeviceName (FAKESCAN_DEVICE);
   QVERIFY (scanner.openDevice ());

   // what paperman looks for by name
   for (const char *name : { SANE_NAME_SCAN_SOURCE, SANE_NAME_SCAN_MODE,
                             SANE_NAME_SCAN_RESOLUTION, SANE_NAME_SCAN_TL_X,
                             SANE_NAME_SCAN_TL_Y, SANE_NAME_SCAN_BR_X,
                             SANE_NAME_SCAN_BR_Y, SANE_NAME_PAGE_WIDTH,
                             SANE_NAME_PAGE_HEIGHT })
      QVERIFY2 (scanner.findOption (name) != -1, name);
}


/* The front of each sheet and then its back, each in a frame of its own,
   and once the hopper is empty the scanner says so */
void TestFakescan::testDuplex ()
{
   const QRgb colour [] = { qRgb (0xff, 0, 0), qRgb (0, 0xff, 0),
                            qRgb (0, 0, 0xff), qRgb (0xff, 0xff, 0) };

   QVERIFY (Fakescan::loadSheet (plainSheet (colour [0]),
                                 plainSheet (colour [1]), 10));
   QVERIFY (Fakescan::loadSheet (plainSheet (colour [2]),
                                 plainSheet (colour [3]), 10));
   QCOMPARE (Fakescan::sheetsLeft (), 2);

   QScanner scanner;

   openScanner (scanner, "ADF Duplex", SANE_VALUE_SCAN_MODE_COLOR, 50);
   if (QTest::currentTestFailed ())
      return;

   for (int side = 0; side < 4; side++)
      {
      SANE_Parameters params;
      QByteArray data;

      QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);

      // US letter at 50dpi, which the sheet fills
      QCOMPARE (params.format, SANE_FRAME_RGB);
      QCOMPARE (params.pixels_per_line, 425);
      QCOMPARE (params.lines, 550);
      QCOMPARE (data.size (), params.bytes_per_line * params.lines);

      const uchar *mid = (const uchar *)data.constData ()
         + params.lines / 2 * params.bytes_per_line
         + params.pixels_per_line / 2 * 3;

      QCOMPARE (qRgb (mid [0], mid [1], mid [2]), colour [side]);
      }

   SANE_Parameters params;
   QByteArray data;

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_NO_DOCS);
   QCOMPARE (Fakescan::sheetsLeft (), 0);
}


/* The feeder centres a sheet, and the window reaches past it to either
   side and past its foot, where the scanner sees its own backing */
void TestFakescan::testSheetOnBacking ()
{
   QImage narrow (40, 80, QImage::Format_RGB32);   // 100 x 200mm, roughly

   narrow.fill (Qt::white);
   Fakescan::setBacking (qRgb (0, 0, 0));
   QVERIFY (Fakescan::loadSheet (narrow, QImage (), 10));

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_GRAY, 50);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (params.format, SANE_FRAME_GRAY);
   QCOMPARE (params.depth, 8);

   auto at = [&] (int x, int y)
      {
      return (uchar)data [y * params.bytes_per_line + x];
      };
   int mid = params.pixels_per_line / 2;

   // the sheet is 200 of the window's 425 pixels across, in the middle
   QCOMPARE (at (mid, 100), 0xff);
   QCOMPARE (at (mid - 90, 100), 0xff);
   QCOMPARE (at (mid + 90, 100), 0xff);
   QCOMPARE (at (mid - 110, 100), 0);
   QCOMPARE (at (mid + 110, 100), 0);

   // and 400 of its 550 lines down
   QCOMPARE (at (mid, 390), 0xff);
   QCOMPARE (at (mid, 410), 0);
}


/* The whole way through: sheets in the hopper become pages of a stack,
   each where it belongs */
void TestFakescan::testScanIntoStack ()
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

   // a black band across the top of one sheet and the foot of the other
   QImage top = plainSheet (qRgb (0xff, 0xff, 0xff));
   QImage foot = top.copy ();

   for (int y = 0; y < 20; y++)
      for (int x = 0; x < top.width (); x++)
         {
         top.setPixel (x, 10 + y, qRgb (0, 0, 0));
         foot.setPixel (x, foot.height () - 30 + y, qRgb (0, 0, 0));
         }
   QVERIFY (Fakescan::loadSheet (top, QImage (), 10));
   QVERIFY (Fakescan::loadSheet (foot, QImage (), 10));

   Scansettings settings (0, FAKESCAN_DEVICE);

   QVERIFY (main->ensureScanner ());
   main->scanInto (repo_ind);

   // every sheet went through, and made a page each
   QCOMPARE (Fakescan::sheetsLeft (), 0);

   QStringList stacks = QDir (path).entryList (QStringList () << "*.max",
                                               QDir::Files);
   QCOMPARE (stacks.size (), 1);
   Filemax max (path + "/", stacks [0], nullptr);
   QVERIFY (!max.load ());
   QCOMPARE (max.pagecount (), 2);

   // with the band in the half of each page it was put in
   for (int pagenum = 0; pagenum < 2; pagenum++)
      {
      QImage image;
      QSize size, true_size;
      int bpp;

      QVERIFY (!max.getImage (pagenum, false, image, size, true_size, bpp,
                              false));
      image = image.convertToFormat (QImage::Format_Grayscale8);

      int dark [2] = { 0, 0 };

      for (int y = 0; y < image.height (); y++)
         {
         const uchar *line = image.constScanLine (y);

         for (int x = 0; x < image.width (); x++)
            if (line [x] < 0x80)
               dark [y >= image.height () / 2]++;
         }
      QVERIFY2 (dark [pagenum] > 10 * dark [!pagenum],
                qPrintable (QString ("page %1 has %2 dark pixels in its top "
                                     "half and %3 in its foot")
                            .arg (pagenum + 1).arg (dark [0])
                            .arg (dark [1])));
      }
}
