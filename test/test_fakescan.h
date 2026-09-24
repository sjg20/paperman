#ifndef TEST_FAKESCAN_H
#define TEST_FAKESCAN_H

#include <QImage>
#include <QObject>

#include "fakescan/fakescan.h"
#include "suite.h"

/** The tests' way in to the fake scanner in test/fakescan

    setup() points libsane at the fake scanner, so that it is the only
    scanner a test can find, and opens the back door into it. The rest do
    what a person at the scanner would: see fakescan/fakescan.h */
class Fakescan
{
public:
   /** Point libsane at the fake scanner and open the back door. This
       must come before anything calls sane_init()

       \returns true if the fake scanner is ready, false if it is not
                built here or a real scanner is being tested instead */
   static bool setup (void);

   /** \returns true if setup() found the fake scanner */
   static bool available (void);

   /** empty the hopper and put the scanner back as it started */
   static void reset (void);

   /** Put a sheet in the hopper, behind any already there

       \param front   picture of the front, or a null image for plain paper
       \param back    picture of the back, likewise
       \param dpi     resolution of the pictures, which with their size
                      says how big the sheet is
       \param skew    how far askew it goes through, in degrees clockwise
       \returns true if OK */
   static bool loadSheet (const QImage &front, const QImage &back = QImage (),
                          int dpi = 100, double skew = 0);

   /** \returns how many sheets are still in the hopper */
   static int sheetsLeft (void);

   /** set the colour scanned wherever the window reaches past the sheet */
   static void setBacking (QRgb rgb);

   /** Arrange for something to go wrong, see fakescan_add_fault()

       \param kind    what goes wrong
       \param sheet   which sheet fed, counting from 1
       \param line    how far into the side, or -1 at sane_start()
       \param arg     as the kind says
       \param side    0 for the front, 1 for the back */
   static void addFault (enum fakescan_fault_kind kind, int sheet,
                         int line = -1, int arg = 0, int side = 0);

   /** clear the paper path, as a person does after a jam */
   static void clear (void);

   /** set how long each side takes to scan, see fakescan_set_side_time() */
   static void setSideTime (int ms);

   /** press a button on the scanner, e.g. "scan" */
   static void press (const char *name);

   /** \returns what the front end asked of the scanner, a call a line */
   static QStringList log (void);

   /** \returns how many times the front end made a call, see
                fakescan_count() */
   static int count (const char *call);
};

class TestFakescan : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   void init ();
   void cleanup ();

   //! The fake scanner is the one scanner libsane offers
   void testDevice ();

   //! A duplex scan sends front then back of each sheet, then runs dry
   void testDuplex ();

   //! A sheet narrower than the window has the backing either side
   void testSheetOnBacking ();

   //! Paperman scans a hopper of sheets into a stack, in order
   void testScanIntoStack ();

   //! With ald the page ends at the foot of the sheet, length unknown
   void testAld ();

   //! A JPEG ended at the foot of the sheet still promises the window
   void testAldJpeg ();

   //! A straightened sheet is its own size, but its JPEG is bigger
   void testDeskewCrop ();

   //! A receipt scanned with ald is stored at its own size
   void testReceiptStored ();

   //! A sheet which went through askew is stored straightened
   void testStraightenedSheetStored ();

   //! A jam holds the scanner up until the paper path is cleared
   void testJam ();

   //! A frame can stop short or come to nothing
   void testShortFrames ();

   //! A button press waits to be seen, and the sensors say what happened
   void testButtons ();

   //! A busy scanner takes its time saying so, until it is cleared
   void testBusy ();

   //! After a jam, Scan in the panel carries the batch on
   void testResumeFromPanel ();

   //! After a jam, Scan on the scanner carries the batch on
   void testResumeFromScanner ();

   //! A jam nobody clears ends the batch, keeping the pages before it
   void testJamNotCleared ();

   //! Pages the scanner cuts short are stored at their real length
   void testShortPagesStored ();

   //! A scanner which stays busy is given up on quickly
   void testBusyScanner ();

   //! A scanner which stops answering keeps the pages already scanned
   void testStopsAnswering ();

   //! A spoilt JPEG is still stored
   void testCorruptJpegStored ();

   //! The feeder runs ahead, and stop-feed keeps what it has taken
   void testFeeder ();

   //! Stop part-way through a batch keeps every sheet the feeder took
   void testStopKeepsSheets ();

   //! A person can drive the scanner through a directory
   void testControlDir ();

   //! The pages of a stack are numbered after the first
   void testPageNames ();

   //! A page with a note in blue ink stays colour
   void testPenNoteKept ();

   //! A blank page scanned in colour is found blank
   void testBlankColourPage ();

   //! A colour page cut short reads back without a fuss
   void testShortColourPageRead ();

   //! A sheet which went through askew keeps its corners
   void testSkewedCornersKept ();

   //! A page in a window much longer than itself is stored whole
   void testLongWindow ();

   //! A scan does not ask the scanner for every option over and over
   void testOptionLookups ();
};

#endif // TEST_FAKESCAN_H
