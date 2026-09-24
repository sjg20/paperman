#include <QtTest/QtTest>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <sane/saneopts.h>

#ifdef Q_OS_LINUX
#include <dlfcn.h>
#endif

#include "desktopmodel.h"
#include "desktopwidget.h"
#include "filemax.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "paperstack.h"
#include "qscanner.h"
#include "utils.h"

#include <functional>

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
   int (*addFault) (const struct fakescan_fault *);
   void (*clear) (void);
   void (*setSideTime) (int);
   int (*press) (const char *);
   const char *(*log) (void);
   int (*count) (const char *);
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
   backdoor.addFault = (int (*) (const struct fakescan_fault *))
      dlsym (handle, "fakescan_add_fault");
   backdoor.clear = (void (*) (void))dlsym (handle, "fakescan_clear");
   backdoor.setSideTime = (void (*) (int))
      dlsym (handle, "fakescan_set_side_time");
   backdoor.press = (int (*) (const char *))dlsym (handle, "fakescan_press");
   backdoor.log = (const char *(*) (void))dlsym (handle, "fakescan_log");
   backdoor.count = (int (*) (const char *))dlsym (handle, "fakescan_count");
   ready = backdoor.reset && backdoor.loadSheet && backdoor.sheetsLeft
         && backdoor.setBacking && backdoor.addFault && backdoor.clear
         && backdoor.setSideTime && backdoor.press && backdoor.log
         && backdoor.count;

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


void Fakescan::addFault (enum fakescan_fault_kind kind, int sheet, int line,
                         int arg, int side)
{
   struct fakescan_fault fault = { kind, sheet, side, line, arg };

   QCOMPARE (backdoor.addFault (&fault), 0);
}


void Fakescan::clear (void)
{
   backdoor.clear ();
}


void Fakescan::setSideTime (int ms)
{
   backdoor.setSideTime (ms);
}


void Fakescan::press (const char *name)
{
   QCOMPARE (backdoor.press (name), 0);
}


QStringList Fakescan::log (void)
{
   return QString::fromUtf8 (backdoor.log ()).split ('\n');
}


int Fakescan::count (const char *call)
{
   return backdoor.count (call);
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

   /* a person has half a minute to clear a jam; a test has a second and
      a half */
   Paperscan::setTimeScale (0.05);
}


void TestFakescan::cleanup ()
{
   Paperscan::setTimeScale (1);
}


void TestFakescan::testDevice ()
{
   QScanner scanner;

   QVERIFY (scanner.initScanner ());
   QVERIFY (scanner.getDeviceList (false));

   // the only scanner libsane knows about
   QStringList names;

   for (int i = 0; i < scanner.deviceCount (); i++)
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


namespace {

/** a scan the whole way through paperman, and what it made */
struct ScanRun
   {
   /** scanner options to set first, by name, as --set does */
   QMap<QString, QString> options;

   /** if set, called every few milliseconds while the scan goes on, until
       it returns true: this is the person at the scanner */
   std::function<bool (Mainwidget *)> during;

   /** if set, called before the scan to change paperman's own settings,
       which are put back afterwards */
   std::function<void ()> configure;

   QList<QImage> pages;       //!< the pages of the stack, if one was made
   QStringList titles;        //!< and their titles
   QList<bool> blank;         //!< which of them paperman found blank
   };

}


/* Scan what is in the hopper into a new stack, the whole way through
   paperman as the user would */
static void scanStack (ScanRun &run)
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

   if (run.configure)
      run.configure ();
   QVERIFY (main->ensureScanner ());
   main->setScanOptions (run.options);

   QObject::connect (desktop->getModel (), &Desktopmodel::newScannedPage,
                     [&run] (const QString &, bool blank)
      {
      run.blank << blank;
      });

   QTimer person;

   if (run.during)
      {
      person.setInterval (5);
      QObject::connect (&person, &QTimer::timeout, [&] ()
         {
         if (run.during (main))
            person.stop ();
         });
      person.start ();
      }
   main->scanInto (repo_ind);
   person.stop ();

   // the scanner is only ever driven from one thread at a time
   QVERIFY2 (!Fakescan::log ().join (" ").contains ("concurrent"),
             qPrintable (Fakescan::log ().join ("\n")));

   QStringList stacks = QDir (path).entryList (QStringList () << "*.max",
                                               QDir::Files);
   run.pages.clear ();
   run.titles.clear ();
   QVERIFY (stacks.size () <= 1);
   if (stacks.isEmpty ())
      return;
   Filemax max (path + "/", stacks [0], nullptr);
   QVERIFY (!max.load ());

   for (int pagenum = 0; pagenum < max.pagecount (); pagenum++)
      {
      QImage image;
      QSize size, true_size;
      int bpp;
      QString title;

      QVERIFY (!max.getImage (pagenum, false, image, size, true_size, bpp,
                              false));
      QVERIFY (!max.getPageTitle (pagenum, title));
      run.pages << image;
      run.titles << title;
      }
}


/* the same, for the tests which need only the pages

   \param options   scanner options to set first, by name, as --set does
   \param pages     set to the pages of the stack, which is none if the scan
                    made no stack
   \param during    see ScanRun
   \param titles    if not null, set to the titles of the pages */
static void scanStack (const QMap<QString, QString> &options,
                       QList<QImage> &pages,
                       std::function<bool (Mainwidget *)> during = nullptr,
                       QStringList *titles = nullptr)
{
   ScanRun run;

   run.options = options;
   run.during = during;
   scanStack (run);
   pages = run.pages;
   if (titles)
      *titles = run.titles;
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

   /* but holds only the sheet: the scanner's rows of blocks are 8 lines
      high, with a restart marker between each pair */
   QCOMPARE (restartCount (data), (200 + 7) / 8 - 1);
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


/* A jam stops the scanner until the paper path is cleared. The sheet
   which jammed is in the paper path, not the hopper, so clearing it
   takes it away; the next sheet goes through as normal */
void TestFakescan::testJam ()
{
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0xff, 0, 0)), QImage (),
                                 10));
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0, 0, 0xff)), QImage (),
                                 10));
   Fakescan::addFault (FAKESCAN_JAM, 1);

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_COLOR, 50);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_JAMMED);
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_JAMMED);
   QCOMPARE (Fakescan::sheetsLeft (), 1);

   Fakescan::clear ();
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE ((uchar)data [data.size () / 2 / 3 * 3 + 2], 0xff);   // blue
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_NO_DOCS);
   QVERIFY (Fakescan::log ().contains ("start: Jammed"));

   // a jam part-way down a page stops the frame there
   Fakescan::clear ();
   QVERIFY (Fakescan::loadSheet (shortSheet (), QImage (), 10));
   Fakescan::addFault (FAKESCAN_JAM, 3, 100);
   scanner.cancel ();
   setString (scanner, SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_GRAY);
   if (QTest::currentTestFailed ())
      return;
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_JAMMED);
   QCOMPARE (data.size (), 100 * params.bytes_per_line);
}


/* A frame can end short of what it promised, or have nothing in it */
void TestFakescan::testShortFrames ()
{
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0xff, 0xff, 0xff)),
                                 QImage (), 10));
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0xff, 0xff, 0xff)),
                                 QImage (), 10));
   Fakescan::addFault (FAKESCAN_SHORT, 1, -1, 8);
   Fakescan::addFault (FAKESCAN_EOF_EMPTY, 2);

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_GRAY, 50);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (params.lines, 550);
   QCOMPARE (data.size (), 8 * params.bytes_per_line);

   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (data.size (), 0);
}


/* A press of a button on the scanner waits until the front end has
   looked, and the double-feed sensor says what happened until the
   paper path is cleared */
void TestFakescan::testButtons ()
{
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0xff, 0xff, 0xff)),
                                 QImage (), 10));
   Fakescan::addFault (FAKESCAN_DOUBLE_FEED, 1);

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_GRAY, 50);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (scanner.checkButtons (), 0);
   Fakescan::press ("scan");
   QCOMPARE (scanner.checkButtons (), 1 << QScanner::BUT_scan);
   QCOMPARE (scanner.checkButtons (), 0);

   SANE_Parameters params;
   QByteArray data;

   QVERIFY (!scanner.checkDoubleFeed ());
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_JAMMED);
   QVERIFY (scanner.checkDoubleFeed ());
   Fakescan::clear ();
   QVERIFY (!scanner.checkDoubleFeed ());
}


/* A scanner which has stopped handing anything over says it is busy,
   and takes its time about it, until it is set going again */
void TestFakescan::testBusy ()
{
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0xff, 0xff, 0xff)),
                                 QImage (), 10));
   Fakescan::addFault (FAKESCAN_BUSY, 1, -1, 100);

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_GRAY, 50);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;
   QElapsedTimer timer;

   timer.start ();
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_DEVICE_BUSY);
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_DEVICE_BUSY);
   QVERIFY (timer.elapsed () >= 200);

   Fakescan::clear ();
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
}


/* a white sheet of US letter at 10dpi with a black band a fifth of the
   way down it, in the middle, or four fifths down */
static QImage bandSheet (int where)
{
   QImage image = plainSheet (qRgb (0xff, 0xff, 0xff));
   int top = 12 + where * 38;

   for (int y = top; y < top + 10; y++)
      for (int x = 0; x < image.width (); x++)
         image.setPixel (x, y, qRgb (0, 0, 0));

   return image;
}


/* which third of a page is darkest, to tell bandSheet()s apart */
static int darkThird (const QImage &page)
{
   QImage grey = page.convertToFormat (QImage::Format_Grayscale8);
   int dark [3] = { 0, 0, 0 };

   for (int y = 0; y < grey.height (); y++)
      {
      const uchar *line = grey.constScanLine (y);

      for (int x = 0; x < grey.width (); x++)
         if (line [x] < 0x80)
            dark [y * 3 / grey.height ()]++;
      }

   return dark [0] > dark [1] && dark [0] > dark [2] ? 0
      : dark [1] > dark [2] ? 1 : 2;
}


/* Three sheets, the second of which jams

   The feeder is kept from running ahead, which would take the third
   sheet into the scanner before the jam, where what becomes of it
   depends on what a real scanner does with such a sheet on a jam, which
   is not known */
static QMap<QString, QString> loadJamming (void)
{
   QMap<QString, QString> options;

   for (int i = 0; i < 3; i++)
      if (!Fakescan::loadSheet (bandSheet (i), QImage (), 10))
         QTest::qFail ("cannot load a sheet", __FILE__, __LINE__);
   Fakescan::addFault (FAKESCAN_JAM, 2);
   options ["buffermode"] = "Off";

   return options;
}


/* The scan waits while the user clears a jam, and Scan in the panel
   says to carry on */
void TestFakescan::testResumeFromPanel ()
{
   QList<QImage> pages;
   bool resumed = false;

   scanStack (loadJamming (), pages, [&] (Mainwidget *main)
      {
      if (!main->scanWaiting ())
         return false;
      Fakescan::clear ();
      main->resumeScan ();
      resumed = true;
      return true;
      });
   if (QTest::currentTestFailed ())
      return;

   // the jammed sheet was taken away, and the batch went on after it
   QVERIFY (resumed);
   QCOMPARE (pages.size (), 2);
   QCOMPARE (darkThird (pages [0]), 0);
   QCOMPARE (darkThird (pages [1]), 2);
   QCOMPARE (Fakescan::sheetsLeft (), 0);
}


/* The same, with the user pressing Scan on the scanner instead */
void TestFakescan::testResumeFromScanner ()
{
   QList<QImage> pages;
   bool pressed = false;

   scanStack (loadJamming (), pages, [&] (Mainwidget *main)
      {
      if (!main->scanWaiting ())
         return false;
      Fakescan::clear ();
      Fakescan::press ("scan");
      pressed = true;
      return true;
      });
   if (QTest::currentTestFailed ())
      return;

   QVERIFY (pressed);
   QCOMPARE (pages.size (), 2);
   QCOMPARE (darkThird (pages [1]), 2);
   QCOMPARE (Fakescan::sheetsLeft (), 0);
}


/* With nobody to clear the jam the scan gives up waiting, and the page
   scanned before it is kept */
void TestFakescan::testJamNotCleared ()
{
   QList<QImage> pages;

   scanStack (loadJamming (), pages);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (pages.size (), 1);
   QCOMPARE (darkThird (pages [0]), 0);
   QCOMPARE (Fakescan::sheetsLeft (), 1);
}


/* A scanner which ends a page early, as one does when a sheet jams and
   is fed again, sends fewer lines than it said. Such a page is stored at
   the length which arrived: a hundred lines of one used to fill the
   terminal with "Application transferred too few scanlines", and eight
   lines of another failed with "Out of memory (-720 bytes)" */
void TestFakescan::testShortPagesStored ()
{
   QVERIFY (Fakescan::loadSheet (bandSheet (0), QImage (), 10));
   QVERIFY (Fakescan::loadSheet (bandSheet (0), QImage (), 10));
   Fakescan::addFault (FAKESCAN_SHORT, 1, -1, 100);
   Fakescan::addFault (FAKESCAN_SHORT, 2, -1, 8);

   QMap<QString, QString> options;
   QList<QImage> pages;

   options ["mode"] = SANE_VALUE_SCAN_MODE_GRAY;
   options ["resolution"] = "100";
   scanStack (options, pages);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (pages.size (), 2);
   QCOMPARE (pages [0].size (), QSize (850, 100));
   QCOMPARE (pages [1].size (), QSize (850, 8));
}


/* A scanner which stays busy, taking a while to say so each time, is
   given up on after a few seconds rather than asked thirty times over */
void TestFakescan::testBusyScanner ()
{
   QVERIFY (Fakescan::loadSheet (bandSheet (0), QImage (), 10));
   Fakescan::addFault (FAKESCAN_BUSY, 1, -1, 200);

   QList<QImage> pages;

   scanStack (QMap<QString, QString> (), pages);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (pages.size (), 0);

   int tries = Fakescan::log ().filter ("start: Device busy").size ();

   QVERIFY2 (tries >= 1 && tries <= 3,
             qPrintable (QString ("the scanner was asked %1 times")
                         .arg (tries)));
}


/* A scanner which stops answering part-way through a batch leaves the
   pages it did scan */
void TestFakescan::testStopsAnswering ()
{
   for (int i = 0; i < 3; i++)
      QVERIFY (Fakescan::loadSheet (bandSheet (i), QImage (), 10));
   Fakescan::addFault (FAKESCAN_IO_ERROR, 3);

   QList<QImage> pages;

   scanStack (QMap<QString, QString> (), pages);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (pages.size (), 2);
   QCOMPARE (darkThird (pages [1]), 1);
}


/* A JPEG with some of its data spoilt still makes a page */
void TestFakescan::testCorruptJpegStored ()
{
   QVERIFY (Fakescan::loadSheet (bandSheet (1), QImage (), 10));
   Fakescan::addFault (FAKESCAN_CORRUPT_JPEG, 1);

   QMap<QString, QString> options;
   QList<QImage> pages;

   options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   options ["resolution"] = "100";
   scanStack (options, pages);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (pages.size (), 1);
   QCOMPARE (pages [0].size (), QSize (850, 1100));
}


/* With buffermode on the feeder takes sheets ahead of the one being
   read. Stopping the feed keeps them, and the batch ends once they have
   been read; cancelling sends them through unread */
void TestFakescan::testFeeder ()
{
   for (int i = 0; i < 8; i++)
      QVERIFY (Fakescan::loadSheet (bandSheet (0), QImage (), 10));

   QScanner scanner;

   openScanner (scanner, "ADF Front", SANE_VALUE_SCAN_MODE_GRAY, 50);
   setString (scanner, "buffermode", "On");
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   // reading one sheet leaves four more in the scanner
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (Fakescan::sheetsLeft (), 3);

   // there is nothing to stop between batches
   int stop = scanner.findOption ("stop-feed");

   QVERIFY (stop != -1);
   QCOMPARE (scanner.setOption (stop, nullptr), SANE_STATUS_GOOD);

   // the four it took are read, then no more
   for (int i = 0; i < 4; i++)
      QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_NO_DOCS);
   QCOMPARE (Fakescan::sheetsLeft (), 3);

   // a new batch takes some more, which a cancel sends through unread
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);
   QCOMPARE (Fakescan::sheetsLeft (), 0);
   scanner.cancel ();
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_NO_DOCS);
}


/* Stop means finish the batch rather than abandon it. The feeder has
   taken sheets which have not been read, and on a cancel they would go
   through to the output tray unscanned, to be found and scanned again.
   Stopping the feed instead has them read, so every sheet is either a
   page or still in the hopper */
void TestFakescan::testStopKeepsSheets ()
{
   for (int i = 0; i < 8; i++)
      QVERIFY (Fakescan::loadSheet (bandSheet (i % 3), QImage (), 10));

   // slow enough to press Stop part-way through
   Fakescan::setSideTime (100);

   QList<QImage> pages;

   scanStack (QMap<QString, QString> (), pages, [&] (Mainwidget *main)
      {
      if (!Fakescan::log ().contains ("read: EOF"))
         return false;
      main->stopScan (false);
      return true;
      });
   if (QTest::currentTestFailed ())
      return;

   QVERIFY2 (pages.size () + Fakescan::sheetsLeft () == 8,
             qPrintable (QString ("%1 pages scanned and %2 sheets left in "
                                  "the hopper, of 8")
                         .arg (pages.size ()).arg (Fakescan::sheetsLeft ())));
   QVERIFY2 (pages.size () < 8, "the scan did not stop");
   QVERIFY (Fakescan::log ().contains ("set stop-feed"));
}


/* A person can drive the scanner by hand through the directory named by
   FAKESCAN_DIR, putting pictures of sheets in its hopper and pressing
   buttons by making files */
void TestFakescan::testControlDir ()
{
   QTemporaryDir dir;

   QVERIFY (dir.isValid ());
   QVERIFY (QDir (dir.path ()).mkpath ("hopper"));

   // US letter at 100dpi, which the pictures say they are at
   QImage side [2] = { QImage (850, 1100, QImage::Format_RGB32),
                       QImage (850, 1100, QImage::Format_RGB32) };

   side [0].fill (qRgb (0xff, 0, 0));
   side [1].fill (qRgb (0, 0, 0xff));
   for (QImage &image : side)
      {
      image.setDotsPerMeterX (qRound (100 / 0.0254));
      image.setDotsPerMeterY (qRound (100 / 0.0254));
      }
   QVERIFY (side [0].save (dir.path () + "/hopper/page.png"));
   QVERIFY (side [1].save (dir.path () + "/hopper/page.back.png"));

   // named for this test only, whether it passes or not
   qputenv ("FAKESCAN_DIR", dir.path ().toLocal8Bit ());
   auto unset = qScopeGuard ([] { qunsetenv ("FAKESCAN_DIR"); });

   QScanner scanner;

   openScanner (scanner, "ADF Duplex", SANE_VALUE_SCAN_MODE_COLOR, 50);
   if (QTest::currentTestFailed ())
      return;

   SANE_Parameters params;
   QByteArray data;

   for (QRgb colour : { qRgb (0xff, 0, 0), qRgb (0, 0, 0xff) })
      {
      QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_GOOD);

      const uchar *mid = (const uchar *)data.constData ()
         + params.lines / 2 * params.bytes_per_line
         + params.pixels_per_line / 2 * 3;

      QCOMPARE (qRgb (mid [0], mid [1], mid [2]), colour);
      }
   QCOMPARE (readFrame (scanner, params, data), SANE_STATUS_NO_DOCS);

   // what went through is put aside
   QVERIFY (QDir (dir.path () + "/hopper").isEmpty ());
   QVERIFY (QFile::exists (dir.path () + "/fed/page.back.png"));

   QFile press (dir.path () + "/press-scan");

   QVERIFY (press.open (QIODevice::WriteOnly));
   press.close ();
   QCOMPARE (scanner.checkButtons (), 1 << QScanner::BUT_scan);
   QVERIFY (!press.exists ());
}


/* The pages of a stack are named for the day they were scanned, and
   numbered after the first. They were numbered by the year in the date,
   so that each page was a year after the one before */
void TestFakescan::testPageNames ()
{
   for (int i = 0; i < 3; i++)
      QVERIFY (Fakescan::loadSheet (bandSheet (i), QImage (), 10));

   QList<QImage> pages;
   QStringList titles;
   QString today = QDate::currentDate ().toString ("d_MMMM_yyyy");

   scanStack (QMap<QString, QString> (), pages, nullptr, &titles);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (titles, QStringList () << today << today + "_2"
                                    << today + "_3");
}


/* The problems found with a real scanner this week, tried again with the
   fake one */


/* A printed page with a note written on it in blue ink was stored as
   grey by auto-colour, which looked for colour over the whole page and
   found too little of it. The page from the corpus goes through the
   scanner, as a JPEG, beside a grey page which should stay grey */
void TestFakescan::testPenNoteKept ()
{
   QImage pen ("test/corpus/pen-colour.jpg");
   QImage grey ("test/corpus/lkd-chapter-grey.jpg");

   QVERIFY (!pen.isNull () && !grey.isNull ());
   QVERIFY (Fakescan::loadSheet (pen, QImage (), 200));
   QVERIFY (Fakescan::loadSheet (grey, QImage (),
                                 qRound (grey.dotsPerMeterX () * 0.0254)));

   ScanRun run;

   run.options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   run.options ["resolution"] = "200";
   run.configure = [] { xmlConfig->setBoolValue ("SCAN_AUTO_COLOUR", true); };
   scanStack (run);
   if (QTest::currentTestFailed ())
      return;

   /* a colour page comes back 24 or 32 bits deep, grey 8 and mono 1 */
   QCOMPARE (run.pages.size (), 2);
   QVERIFY2 (run.pages [0].depth () >= 24,
             qPrintable (QString ("the page with a note was stored %1 bits "
                                  "deep").arg (run.pages [0].depth ())));
   QCOMPARE (run.pages [1].depth (), 8);
}


/* A blank page scanned in colour was not found to be blank, so it was
   kept however the user asked for blank pages to be treated */
void TestFakescan::testBlankColourPage ()
{
   QVERIFY (Fakescan::loadSheet (plainSheet (qRgb (0xff, 0xff, 0xff)),
                                 QImage (), 10));
   QVERIFY (Fakescan::loadSheet (bandSheet (1), QImage (), 10));

   ScanRun run;

   run.options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   run.options ["resolution"] = "100";
   run.configure = []
      {
      xmlConfig->setIntValue ("SCAN_BLANK", Paperstack::ignore);
      };
   scanStack (run);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (run.blank, QList<bool> () << true << false);
}


/* collects the warnings given while it is alive */
namespace {
class Warnings
   {
public:
   Warnings ()
      {
      _self = this;
      _old = qInstallMessageHandler (handler);
      }

   ~Warnings ()
      {
      qInstallMessageHandler (_old);
      _self = nullptr;
      }

   QStringList seen;

private:
   static void handler (QtMsgType type, const QMessageLogContext &context,
                        const QString &msg)
      {
      if (_self && type == QtWarningMsg)
         _self->seen << msg;
      _old (type, context, msg);
      }

   static Warnings *_self;
   static QtMessageHandler _old;
   };

Warnings *Warnings::_self;
QtMessageHandler Warnings::_old;
}


/* A colour page cut short, as by a jam, is stored at the length which
   arrived. Reading it back, the decoder stopped before the height its
   JPEG promised and then asked the library to finish, which said
   "Application transferred too few scanlines", again and again */
void TestFakescan::testShortColourPageRead ()
{
   QVERIFY (Fakescan::loadSheet (bandSheet (0), QImage (), 10));
   Fakescan::addFault (FAKESCAN_SHORT, 1, -1, 100);

   Warnings warnings;
   ScanRun run;

   run.options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   run.options ["resolution"] = "100";
   scanStack (run);
   if (QTest::currentTestFailed ())
      return;

   /* the scanner's JPEG holds its lines eight at a time, so the last row
      of them is filled out and the page can be up to seven lines long */
   QCOMPARE (run.pages.size (), 1);
   QCOMPARE (run.pages [0].width (), 850);
   QVERIFY2 (run.pages [0].height () >= 100 && run.pages [0].height () < 108,
             qPrintable (QString::number (run.pages [0].height ())));
   QVERIFY2 (warnings.seen.filter ("scanlines").isEmpty (),
             qPrintable (warnings.seen.join ("\n")));
}


/* A sheet which went through a little askew had its corners cut off
   when the backing was cut away from its sides: the corners stand out
   beyond the rest of the sheet, in columns which hold only a little of
   it, and nothing but white margin */
void TestFakescan::testSkewedCornersKept ()
{
   // 150 x 200mm at 100dpi, printed in the middle, 5 degrees askew
   QImage sheet (591, 787, QImage::Format_RGB32);

   sheet.fill (Qt::white);
   for (int y = 100; y < sheet.height () - 100; y += 30)
      for (int x = 80; x < sheet.width () - 80; x++)
         for (int i = 0; i < 6; i++)
            sheet.setPixel (x, y + i, qRgb (0, 0, 0));
   QVERIFY (Fakescan::loadSheet (sheet, QImage (), 100, 5));

   ScanRun run;

   run.options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   run.options ["resolution"] = "100";
   run.options ["ald"] = "yes";
   scanStack (run);
   if (QTest::currentTestFailed ())
      return;

   /* askew, the sheet spans 591 * cos 5 + 787 * sin 5 = 658 pixels, in
      a window of 850. Without following the corners out, the page is
      cut to about 606, taking some 25 pixels off each side of the
      sheet. Beyond about 7 degrees the corners reach further than
      PAPER_EDGE_MOST lets the edge be followed, so some is still lost */
   QCOMPARE (run.pages.size (), 1);
   QVERIFY2 (run.pages [0].width () >= 658 && run.pages [0].width () < 720,
             qPrintable (QString ("the sheet spans 658 pixels but the page "
                                  "is %1 wide")
                         .arg (run.pages [0].width ())));
}


/* A sheet in a window much longer than any page: the page cannot be
   made ready for all of the window, so it starts smaller and grows as
   the JPEG's header asks. Growing it moved the data out from under the
   decoder which was reading it, and the back of an 11x17 sheet came out
   grey from an inch down. What the decoder read was freed memory, which
   usually still holds what it did, so this passes either way in an
   ordinary build. Built with the address sanitiser and run with freed
   memory overwritten, as doc/develop.rst says, the page comes out eight
   lines long without the fix */
void TestFakescan::testLongWindow ()
{
   // 216 x 300mm at 100dpi, with a band near its foot on either side
   QImage side (850, 1181, QImage::Format_RGB32);

   side.fill (Qt::white);
   for (int y = 1100; y < 1120; y++)
      for (int x = 0; x < side.width (); x++)
         side.setPixel (x, y, qRgb (0, 0, 0));
   QVERIFY (Fakescan::loadSheet (side, side, 100));

   // in a window 600mm long
   ScanRun run;

   run.options ["mode"] = SANE_VALUE_SCAN_MODE_COLOR;
   run.options ["resolution"] = "100";
   run.options ["source"] = "ADF Duplex";
   run.options ["page-height"] = "600";
   run.options ["ald"] = "yes";
   scanStack (run);
   if (QTest::currentTestFailed ())
      return;

   // both sides whole, down to the band near the foot
   QCOMPARE (run.pages.size (), 2);
   for (const QImage &page : run.pages)
      {
      QVERIFY2 (qAbs (page.height () - 1181) < 10,
                qPrintable (QString ("the sheet is 1181 lines but the page "
                                     "is %1").arg (page.height ())));

      QImage grey = page.convertToFormat (QImage::Format_Grayscale8);

      QVERIFY2 (grey.pixelColor (grey.width () / 2, 1110).value () < 0x40,
                "the band near the foot of the page is missing");
      }
}


/* A scan which seemed to hang was spending its time asking the scanner
   the name of every one of its options, over and over, to find a few of
   them by name. With the back end's debugging on, each question was a
   line in the log */
void TestFakescan::testOptionLookups ()
{
   for (int i = 0; i < 5; i++)
      QVERIFY (Fakescan::loadSheet (bandSheet (i % 3), QImage (), 10));

   ScanRun run;

   scanStack (run);
   if (QTest::currentTestFailed ())
      return;

   QCOMPARE (run.pages.size (), 5);

   /* asking once for each option, and a few more for each page, is
      under a hundred; asking for all of them at each lookup is some
      eight hundred, on a scanner with a third of the fi-8170's options */
   int asked = Fakescan::count ("get_option_descriptor");

   QVERIFY2 (asked < 200,
             qPrintable (QString ("asked for %1 option descriptors to scan "
                                  "5 sheets").arg (asked)));
}
