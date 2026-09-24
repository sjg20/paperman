#ifndef TEST_FAKESCAN_H
#define TEST_FAKESCAN_H

#include <QImage>
#include <QObject>

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
       \returns true if OK */
   static bool loadSheet (const QImage &front, const QImage &back = QImage (),
                          int dpi = 100);

   /** \returns how many sheets are still in the hopper */
   static int sheetsLeft (void);

   /** set the colour scanned wherever the window reaches past the sheet */
   static void setBacking (QRgb rgb);
};

class TestFakescan : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   void init ();

   //! The fake scanner is the one scanner libsane offers
   void testDevice ();

   //! A duplex scan sends front then back of each sheet, then runs dry
   void testDuplex ();

   //! A sheet narrower than the window has the backing either side
   void testSheetOnBacking ();

   //! Paperman scans a hopper of sheets into a stack, in order
   void testScanIntoStack ();
};

#endif // TEST_FAKESCAN_H
