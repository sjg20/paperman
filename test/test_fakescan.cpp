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


bool Fakescan::loadSheet (const QImage &front, const QImage &back, int dpi,
                          double skew)
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
   sheet.skew = skew;

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


static void setBool (QScanner &scanner, const char *name, bool on)
{
   SANE_Word word = on;

   QCOMPARE (scanner.setOption (scanner.findOption (name), &word),
             SANE_STATUS_GOOD);
}


static void setString (QScanner &scanner, const char *name, const char *val)
{
   QByteArray str (val);

   QCOMPARE (scanner.setOption (scanner.findOption (name), str.data ()),
             SANE_STATUS_GOOD);
}


/* the size of picture a JPEG says it holds, from its start of frame */
static QSize jpegSize (const QByteArray &jpeg)
{
   const uchar *data = (const uchar *)jpeg.constData ();

   for (int pos = 2; pos + 9 <= jpeg.size () && data [pos] == 0xff;
        pos += 2 + (data [pos + 2] << 8 | data [pos + 3]))
      if (data [pos + 1] >= 0xc0 && data [pos + 1] <= 0xc3)
         return QSize (data [pos + 7] << 8 | data [pos + 8],
                       data [pos + 5] << 8 | data [pos + 6]);

   return QSize ();
}


/* How many restart markers a JPEG holds. The fake scanner puts one at
   the end of each row of blocks, as a real one does, so this says how
   many rows of blocks it sent whatever height it promised */
static int restartCount (const QByteArray &jpeg)
{
   int count = 0;

   for (int i = 0; i + 1 < jpeg.size (); i++)
      if ((uchar)jpeg [i] == 0xff && ((uchar)jpeg [i + 1] & 0xf8) == 0xd0)
         count++;

   return count;
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


/* Scan what is in the hopper into a new stack, the whole way through
   paperman as the user would, and hand back its pages

   \param options   scanner options to set first, by name, as --set does
   \param pages     set to the pages of the stack */
static void scanStack (const QMap<QString, QString> &options,
                       QList<QImage> &pages)
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

   Scansettings settings (0, FAKESCAN_DEVICE);

   QVERIFY (main->ensureScanner ());
   main->setScanOptions (options);
   main->scanInto (repo_ind);

   QStringList stacks = QDir (path).entryList (QStringList () << "*.max",
                                               QDir::Files);
   QCOMPARE (stacks.size (), 1);
   Filemax max (path + "/", stacks [0], nullptr);
   QVERIFY (!max.load ());

   pages.clear ();
   for (int pagenum = 0; pagenum < max.pagecount (); pagenum++)
      {
      QImage image;
      QSize size, true_size;
      int bpp;

      QVERIFY (!max.getImage (pagenum, false, image, size, true_size, bpp,
                              false));
      pages << image;
      }
}


/* The whole way through: sheets in the hopper become pages of a stack,
   each where it belongs */
void TestFakescan::testScanIntoStack ()
{
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

   QList<QImage> pages;

   scanStack (QMap<QString, QString> (), pages);
   if (QTest::currentTestFailed ())
      return;

   // every sheet went through, and made a page each
   QCOMPARE (Fakescan::sheetsLeft (), 0);
   QCOMPARE (pages.size (), 2);

   // with the band in the half of each page it was put in
   for (int pagenum = 0; pagenum < 2; pagenum++)
      {
      QImage image = pages [pagenum].convertToFormat
         (QImage::Format_Grayscale8);

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


/* a sheet at 10dpi which is US letter wide and 101.6mm long, which at
   50dpi is 425 pixels across and 200 lines down */
static QImage shortSheet ()
{
   QImage image (85, 40, QImage::Format_RGB32);

   image.fill (Qt::white);
   return image;
}


/* A scanner asked to find the foot of the sheet cannot say how long the
   page will be, and ends it there */
void TestFakescan::testAld ()
{
   QVERIFY (Fakescan::loadSheet (shortSheet (), QImage (), 10));

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_GRAY, 50);
   setBool (scanner, "ald", true);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   scanner.getParameters (&params);
   QCOMPARE (params.lines, -1);

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (params.lines, -1);
   QCOMPARE (params.pixels_per_line, 425);

   // the sheet is 200 lines long, in a window of 550
   int lines = data.size () / params.bytes_per_line;

   QVERIFY2 (qAbs (lines - 200) <= 1, qPrintable (QString::number (lines)));
}


/* A scanner which finds the foot of the sheet while sending a JPEG has
   already said in the JPEG's header how long the picture is, and says
   the whole window. It then ends the picture at the foot of the sheet
   and finishes the file off properly */
void TestFakescan::testAldJpeg ()
{
   QVERIFY (Fakescan::loadSheet (shortSheet (), QImage (), 10));

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_COLOR, 50);
   setString (scanner, "compression", "JPEG");
   setBool (scanner, "ald", true);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE ((int)params.format, 0x0b);   // SANE_FRAME_JPEG
   QCOMPARE (params.lines, -1);

   // a whole file, which promises the whole window
   QVERIFY (data.startsWith ("\xff\xd8"));
   QVERIFY (data.endsWith ("\xff\xd9"));
   QCOMPARE (jpegSize (data), QSize (425, 550));

   /* but holds only the sheet: a colour JPEG's rows of blocks are 16
      lines high, with a restart marker between each pair */
   QCOMPARE (restartCount (data), (200 + 15) / 16 - 1);
}


/* A scanner which straightens each sheet and crops it reads the whole
   sheet before sending any, so it can say the size of the sheet. The
   JPEG it sends is bigger, since it holds the box the sheet went through
   in, with the sheet upright in the middle */
void TestFakescan::testDeskewCrop ()
{
   // 101.6 x 152.4mm, which at 50dpi is 200 x 300
   QImage sheet (40, 60, QImage::Format_RGB32);

   sheet.fill (Qt::white);
   QVERIFY (Fakescan::loadSheet (sheet, QImage (), 10, 10));
   QVERIFY (Fakescan::loadSheet (sheet, QImage (), 10, 10));

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_COLOR, 50);
   setString (scanner, "compression", "JPEG");
   setBool (scanner, "hwdeskewcrop", true);
   setBool (scanner, "ald", true);
   if (QTest::currentTestFailed ())
      return;

   // until the sheet is read, all the scanner knows is the window
   SANE_Parameters params;
   QByteArray data;

   scanner.getParameters (&params);
   QCOMPARE (params.pixels_per_line, 425);
   QCOMPARE (params.lines, 550);

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (params.pixels_per_line, 200);
   QCOMPARE (params.lines, 300);

   QSize box = jpegSize (data);

   QVERIFY2 (box.width () > 200 && box.height () > 300,
             qPrintable (QString ("the JPEG is %1 x %2").arg (box.width ())
                         .arg (box.height ())));

   // sent as lines, it is the size it says
   scanner.cancel ();
   setString (scanner, "compression", "None");
   setString (scanner, SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_GRAY);
   if (QTest::currentTestFailed ())
      return;
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (params.format, SANE_FRAME_GRAY);
   QCOMPARE (data.size (), 200 * 300);
}


/* a till receipt, 80 x 150mm at 100dpi, with lines of print on it */
static QImage receipt ()
{
   QImage image (315, 591, QImage::Format_RGB32);

   image.fill (Qt::white);
   for (int y = 30; y < image.height () - 30; y += 40)
      for (int x = 20; x < image.width () - 20; x++)
         for (int i = 0; i < 8; i++)
            image.setPixel (x, y + i, qRgb (0, 0, 0));

   return image;
}


/* A receipt scanned in colour with ald comes back as a JPEG which says
   it is the length of the whole window, and is stored at the length of
   the receipt, cut down to its width. It came out a yard long with the
   receipt at the top and flat grey below it, and then kept the width of
   the window since the grey looked like ink */
void TestFakescan::testReceiptStored ()
{
   QVERIFY (Fakescan::loadSheet (receipt (), QImage (), 100));

   QMap<QString, QString> options;
   QList<QImage> pages;

   options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   options ["resolution"] = "100";
   options ["ald"] = "yes";
   scanStack (options, pages);
   if (QTest::currentTestFailed ())
      return;
   QCOMPARE (pages.size (), 1);

   // US letter at 100dpi is 850 x 1100
   QSize size = pages [0].size ();

   QVERIFY2 (qAbs (size.height () - 591) < 20 && size.width () > 300
             && size.width () < 400,
             qPrintable (QString ("the receipt is 315 x 591 but was stored "
                                  "%1 x %2").arg (size.width ())
                         .arg (size.height ())));
}


/* A sheet which goes through askew with the scanner straightening it
   comes back bigger than the scanner says, upright on a black ground.
   It crashed inside the JPEG library and later corrupted the heap, and
   was then kept at the full width of the ground */
void TestFakescan::testStraightenedSheetStored ()
{
   // 120 x 180mm at 100dpi, askew by 8 degrees
   QImage sheet = receipt ().scaled (472, 709);

   QVERIFY (Fakescan::loadSheet (sheet, QImage (), 100, 8));
   QVERIFY (Fakescan::loadSheet (sheet, QImage (), 100, -8));

   QMap<QString, QString> options;
   QList<QImage> pages;

   options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   options ["resolution"] = "100";
   options ["ald"] = "yes";
   options ["hwdeskewcrop"] = "yes";
   scanStack (options, pages);
   if (QTest::currentTestFailed ())
      return;
   QCOMPARE (pages.size (), 2);

   /* the box the scanner sends is about 566 wide; the page is cut to
      the sheet, and none of the sheet is lost */
   for (const QImage &page : pages)
      QVERIFY2 (page.width () > 450 && page.width () < 520
                && page.height () >= 709,
                qPrintable (QString ("the sheet is 472 x 709 but was stored "
                                     "%1 x %2").arg (page.width ())
                            .arg (page.height ())));
}
