/* The back door into the fake scanner

   fakescan.cpp is a SANE back end which answers as the fujitsu back end
   does for an fi-8170. A front end drives it through the ordinary SANE
   calls; a test also reaches in through these, to do what a person
   standing at the scanner would: put paper in the hopper, and so on.

   libsane's dll back end loads the library with dlopen(), inside the
   front end's own process. Opening the same file again hands back the
   same copy of it, so a test which does that and looks these up with
   dlsym() is talking to the scanner the front end has open.

   A person can drive it too, through a directory named by FAKESCAN_DIR,
   which scripts/fakescan.sh sets up:

      hopper/     pictures of sheets, fed in order of name: page.png is
                  the front of a sheet and page.back.png, if there is one,
                  its back. Each goes to fed/ as it is taken
      press-scan  press the Scan button, and likewise press-email

   A picture is taken to be at the resolution it says, or at 300dpi if
   it says less than 100dpi, which is usually a default */

#ifndef FAKESCAN_H
#define FAKESCAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* the file the dll back end looks for, and the name the front end sees */
#define FAKESCAN_LIB      "libsane-fakefujitsu.so.1"
#define FAKESCAN_BACKEND  "fakefujitsu"
#define FAKESCAN_DEVICE   FAKESCAN_BACKEND ":fi-8170:00001"

/** a sheet of paper, as it goes into the hopper */
struct fakescan_sheet
   {
   const char *front;   /**< image of the front, or NULL for plain paper */
   const char *back;    /**< image of the back, or NULL for plain paper */
   double width_mm;     /**< size of the sheet, or 0 to go by the images */
   double height_mm;
   int dpi;             /**< resolution of the images, or 0 for 300 */
   double skew;         /**< how far askew it goes through, in degrees
                             clockwise */
   };

/** Take all the paper out of the hopper and put everything which is not
    set through SANE back as it was when the library was loaded */
void fakescan_reset (void);

/** Put a sheet in the hopper, behind any already there

    \param sheet   the sheet; the images are read before this returns
    \returns 0 if OK, -1 if an image cannot be read, -2 if the sheet has
             no size, since neither it nor an image gives one */
int fakescan_load_sheet (const struct fakescan_sheet *sheet);

/** \returns how many sheets are still in the hopper. With buffermode on,
             the feeder takes a few from it as soon as a batch starts */
int fakescan_sheets_left (void);

/** Set how long each side takes to scan, so that a test has time to do
    something while a batch is going through. It starts at 0

    \param ms   time in milliseconds */
void fakescan_set_side_time (int ms);

/** Set the colour of the backing, which is what is scanned wherever the
    window reaches beyond the sheet

    \param rgb   colour as 0xRRGGBB */
void fakescan_set_backing (unsigned int rgb);

/** things which go wrong at a scanner */
enum fakescan_fault_kind
   {
   /** the sheet jams: SANE_STATUS_JAMMED, and again from every
       sane_start() until the paper path is cleared */
   FAKESCAN_JAM,
   /** two sheets go through together: as a jam, with the double-feed
       sensor set */
   FAKESCAN_DOUBLE_FEED,
   /** the cover is opened: SANE_STATUS_COVER_OPEN until it is shut */
   FAKESCAN_COVER_OPEN,
   /** the frame ends at once, with no data */
   FAKESCAN_EOF_EMPTY,
   /** the frame ends after 'arg' lines, having promised the whole page */
   FAKESCAN_SHORT,
   /** some of the JPEG's data is spoilt */
   FAKESCAN_CORRUPT_JPEG,
   /** the scanner says it is busy, taking 'arg' ms each time, until the
       paper path is cleared */
   FAKESCAN_BUSY,
   /** the scanner stops answering: SANE_STATUS_IO_ERROR from every call
       until it is cleared, as though turned off and on again */
   FAKESCAN_IO_ERROR,
   };

/** something to go wrong, and when */
struct fakescan_fault
   {
   enum fakescan_fault_kind kind;
   int sheet;           /**< which sheet fed, counting from 1 */
   int side;            /**< 0 for the front, 1 for the back */
   int line;            /**< how far into the side, or -1 at sane_start() */
   int arg;             /**< as the kind says */
   };

/** Arrange for something to go wrong. It happens once, when that sheet
    reaches that point, and the scanner then stays as the kind says

    \returns 0 if OK, -1 if the fault makes no sense */
int fakescan_add_fault (const struct fakescan_fault *fault);

/** Clear the paper path and shut the cover, as a person does after a
    jam, and bring back a scanner which stopped answering */
void fakescan_clear (void);

/** Press a button on the scanner. It stays pressed until the front end
    has seen it, so a front end which looks from time to time does not
    miss it

    \param name   name of the button's option, e.g. "scan"
    \returns 0 if OK, -1 if the scanner has no such button */
int fakescan_press (const char *name);

/** What the front end has asked of the scanner since the last reset, a
    line for each call which changed something or which failed, oldest
    first. Two calls made at once on one handle, which a real back end
    does not expect, are given as "concurrent <call>"

    \returns the log, valid until the next call of this */
const char *fakescan_log (void);

/** How many times the front end has made a call on the scanner since the
    last reset, whatever it asked

    \param call   name of the call, without sane_, e.g. "start" or
                  "get_option_descriptor"
    \returns the count */
int fakescan_count (const char *call);

#ifdef __cplusplus
}
#endif

#endif /* FAKESCAN_H */
