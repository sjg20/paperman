/* A fake Fujitsu scanner, as a SANE back end

   The tests have libsane's dll back end load this in place of a real
   scanner's back end, and paperman drives it through the same SANE
   calls it makes for any scanner. It answers as the fujitsu back end
   does for an fi-8170 in the ways paperman depends on: the same option
   names and values, a window placed on the sheet the same way, and a
   frame for each side of each sheet until the hopper is empty.

   What it scans is whatever a test put in the hopper through the back
   door in fakescan.h. Each sheet is a picture of the paper, which is
   laid on the backing inside the window and sent at the resolution and
   in the mode asked for, as raw lines or as a JPEG.

   It does what the fujitsu back end does with the settings which decide
   the size of a page. With automatic length detection (ald) the page
   ends at the foot of the sheet, and the scanner says the length is
   unknown until it gets there. With hardware deskew and crop
   (hwdeskewcrop) the sheet comes back straightened and cut down to
   itself, and its size is known only once it has been read in full.

   It is a model of what a front end sees, not of the scanner's firmware
   or of what passes over USB */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <jpeglib.h>

#include <QAtomicInt>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QSet>
#include <QStringList>
#include <QThread>

#include <sane/sane.h>
#include <sane/saneopts.h>

#include "fakescan.h"

/* libsane has had this since 1.1, but some copies of its header still
   leave it out */
#ifndef SANE_FRAME_JPEG
#define SANE_FRAME_JPEG ((SANE_Frame) 0x0b)
#endif

#define EXPORT extern "C" __attribute__ ((visibility ("default")))

/* The scanner works in 1/1200ths of an inch, as the fujitsu back end
   does, and gives sizes to the front end in mm */
#define UNITS_PER_INCH    1200
#define MM_TO_UNITS(mm)   ((int) ((mm) / 25.4 * UNITS_PER_INCH + 0.5))
#define UNITS_TO_MM(u)    ((u) * 25.4 / UNITS_PER_INCH)
#define UNITS_TO_FIXED(u) SANE_FIX (UNITS_TO_MM (u))
#define FIXED_TO_UNITS(f) MM_TO_UNITS (SANE_UNFIX (f))

/* The largest window an fi-8170 has: 9.15 x 108.30 inches, which is what
   one tells the fujitsu back end, for a sheet no wider than 216mm. It is
   wider than the sheet so that US letter, at 8.5in, fits with room to
   spare: a front end which is told any less can find US letter a hair
   too wide, after SANE's fixed point, and offer no US paper sizes at
   all. The smallest sheet it takes is 50.8 x 54mm */
#define MIN_X             MM_TO_UNITS (50.8)
#define MIN_Y             MM_TO_UNITS (54)
#define MAX_X             (UNITS_PER_INCH * 915 / 100)
#define MAX_Y             (UNITS_PER_INCH * 10830 / 100)
#define MIN_RES           50
#define MAX_RES           600

/* a light grey, darker than paper. This is not yet measured from a real
   fi-8170 */
#define BACKING_DEFAULT   qRgb (0xc8, 0xc8, 0xc8)

enum { SOURCE_ADF_FRONT, SOURCE_ADF_BACK, SOURCE_ADF_DUPLEX };
enum { MODE_LINEART, MODE_HALFTONE, MODE_GRAY, MODE_COLOR };
enum { COMPRESS_NONE, COMPRESS_JPEG };

/* the same strings the fujitsu back end uses */
static SANE_String_Const source_list[] =
   {
   "ADF Front", "ADF Back", "ADF Duplex", nullptr
   };

static SANE_String_Const mode_list[] =
   {
   SANE_VALUE_SCAN_MODE_LINEART, SANE_VALUE_SCAN_MODE_HALFTONE,
   SANE_VALUE_SCAN_MODE_GRAY, SANE_VALUE_SCAN_MODE_COLOR, nullptr
   };

static SANE_String_Const buffermode_list[] =
   {
   "Default", "Off", "On", nullptr
   };

enum { BUFFERMODE_DEFAULT, BUFFERMODE_OFF, BUFFERMODE_ON };

/* How many sheets the feeder takes ahead of the one being read, with
   buffermode on: an fi-8950 has been seen to run four ahead */
#define READ_AHEAD 4

static SANE_String_Const compress_list[] =
   {
   "None", "JPEG", nullptr
   };

enum
   {
   OPT_NUM_OPTS,
   OPT_STANDARD_GROUP,
   OPT_SOURCE,
   OPT_MODE,
   OPT_RES,
   OPT_GEOMETRY_GROUP,
   OPT_TL_X,
   OPT_TL_Y,
   OPT_BR_X,
   OPT_BR_Y,
   OPT_PAGE_WIDTH,
   OPT_PAGE_HEIGHT,
   OPT_ENHANCEMENT_GROUP,
   OPT_BRIGHTNESS,
   OPT_CONTRAST,
   OPT_THRESHOLD,
   OPT_ADVANCED_GROUP,
   OPT_ALD,
   OPT_COMPRESS,
   OPT_COMPRESS_ARG,
   OPT_HWDESKEWCROP,
   OPT_BUFFERMODE,
   OPT_STOP_FEED,
   OPT_SENSOR_GROUP,
   OPT_SCAN_SW,
   OPT_EMAIL_SW,
   OPT_DOUBLE_FEED,
   NUM_OPTIONS
   };

namespace {

/** a sheet of paper, in the hopper or going through the scanner */
struct Sheet
   {
   QImage side [2];   //!< front and back, or null for plain paper
   int width;         //!< size of the paper, in scanner units
   int height;
   double skew;       //!< degrees askew, clockwise, as it goes through
   };

/** What is at the scanner rather than behind a SANE handle: the paper
    and the state of the machine. It outlasts sane_close() and
    sane_exit(), as they do on a real scanner */
struct Machine
   {
   QMutex lock;
   QList<Sheet> sheets;               //!< in the hopper
   QRgb backing = BACKING_DEFAULT;
   QList<fakescan_fault> faults;      //!< still to happen
   QList<Sheet> taken;                //!< fed but not yet read, in order
   bool feed_stopped = false;         //!< taking no more from the hopper
   int side_ms = 0;                   //!< how long a side takes to scan
   int fed = 0;                       //!< sheets read since the reset
   bool jammed = false;               //!< until the paper path is cleared
   bool double_feed = false;          //!< what the sensor says
   bool cover_open = false;
   bool wedged = false;               //!< not answering at all
   int busy_ms = -1;                  //!< busy, taking this long to say so
   QSet<QString> pressed;             //!< buttons not yet seen
   QStringList log;                   //!< see fakescan_log()
   QByteArray log_out;
   };

/** what a SANE handle refers to */
struct Scanner
   {
   SANE_Option_Descriptor opt [NUM_OPTIONS];
   SANE_Range res_range;
   SANE_Range x_range;        //!< where the window can go across the page
   SANE_Range y_range;        //!< and down it
   SANE_Range width_range;    //!< sizes of paper the scanner takes
   SANE_Range height_range;
   SANE_Range compress_arg_range;
   SANE_Range enhance_range;          //!< brightness and contrast
   SANE_Range threshold_range;

   int source;
   int mode;
   int resolution;
   int tl_x, tl_y, br_x, br_y;      //!< the window, in scanner units
   int page_width, page_height;     //!< the paper, in scanner units
   int brightness;                  //!< kept, but not yet applied
   int contrast;                    //!< kept, but not yet applied
   int threshold;                   //!< line art: darker is black, 0 for 128
   bool ald;                        //!< end the page at the foot of the sheet
   int compress;
   int compress_arg;                //!< JPEG level, 1 small to 7 large
   bool hwdeskewcrop;               //!< straighten the sheet and cut to it
   int buffermode;

   /** where a scan is up to: IDLE before sane_start() and after
       sane_cancel(), SCANNING while a frame is being read and DONE
       once all of it has been */
   enum { IDLE, SCANNING, DONE } state;
   SANE_Parameters params;    //!< of the frame being read, as announced
   QByteArray frame;          //!< all of it, as it is sent
   int sent;                  //!< how much of it has been
   bool back_pending;         //!< a duplex sheet whose back is still to go
   Sheet sheet;               //!< the sheet going through
   int sheet_num;             //!< which sheet it is, counting from 1
   int side;                  //!< which side of it is being sent
   int stop_at;               //!< where the frame stops short, or -1
   SANE_Status stop_status;   //!< and what reading there says
   int stop_kind;             //!< and why, a fakescan_fault_kind
   QAtomicInt calls;          //!< calls in progress on this handle
   };

}

static Machine machine;

static const SANE_Device device =
   {
   "fi-8170:00001", "FUJITSU", "fi-8170", "scanner"
   };


/* the window can go anywhere on the page, which bounds it, so it has to
   follow the page size */
static void update_ranges (Scanner *s)
   {
   s->x_range.max = UNITS_TO_FIXED (s->page_width);
   s->y_range.max = UNITS_TO_FIXED (s->page_height);
   }


static bool jpeg_on (const Scanner *s)
   {
   return s->compress == COMPRESS_JPEG && s->mode >= MODE_GRAY;
   }


static void set_active (SANE_Option_Descriptor *opt, bool active)
   {
   if (active)
      opt->cap &= ~SANE_CAP_INACTIVE;
   else
      opt->cap |= SANE_CAP_INACTIVE;
   }


/* which options apply, as the fujitsu back end decides: there is no JPEG
   of a black-and-white page, and no JPEG level without JPEG */
static void update_caps (Scanner *s)
   {
   set_active (&s->opt [OPT_COMPRESS], s->mode >= MODE_GRAY);
   set_active (&s->opt [OPT_THRESHOLD], s->mode == MODE_LINEART);
   set_active (&s->opt [OPT_COMPRESS_ARG], jpeg_on (s));
   }


static void init_options (Scanner *s)
   {
   SANE_Option_Descriptor *opt;
   SANE_Fixed quant = SANE_FIX (25.4 / UNITS_PER_INCH);

   memset (s->opt, '\0', sizeof (s->opt));
   for (int i = 0; i < NUM_OPTIONS; i++)
      {
      s->opt [i].size = sizeof (SANE_Word);
      s->opt [i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

   s->res_range = { MIN_RES, MAX_RES, 1 };
   s->x_range = { 0, 0, quant };
   s->y_range = { 0, 0, quant };
   s->width_range = { UNITS_TO_FIXED (MIN_X), UNITS_TO_FIXED (MAX_X), quant };
   s->height_range = { UNITS_TO_FIXED (MIN_Y), UNITS_TO_FIXED (MAX_Y), quant };
   s->compress_arg_range = { 0, 7, 1 };
   s->enhance_range = { -127, 127, 1 };
   s->threshold_range = { 0, 255, 1 };
   update_ranges (s);

   opt = &s->opt [OPT_NUM_OPTS];
   opt->name = SANE_NAME_NUM_OPTIONS;
   opt->title = SANE_TITLE_NUM_OPTIONS;
   opt->desc = SANE_DESC_NUM_OPTIONS;
   opt->type = SANE_TYPE_INT;
   opt->cap = SANE_CAP_SOFT_DETECT;

   opt = &s->opt [OPT_STANDARD_GROUP];
   opt->name = SANE_NAME_STANDARD;
   opt->title = SANE_TITLE_STANDARD;
   opt->desc = SANE_DESC_STANDARD;
   opt->type = SANE_TYPE_GROUP;
   opt->size = 0;
   opt->cap = 0;

   opt = &s->opt [OPT_SOURCE];
   opt->name = SANE_NAME_SCAN_SOURCE;
   opt->title = SANE_TITLE_SCAN_SOURCE;
   opt->desc = SANE_DESC_SCAN_SOURCE;
   opt->type = SANE_TYPE_STRING;
   opt->size = sizeof ("ADF Duplex");
   opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
   opt->constraint.string_list = source_list;

   opt = &s->opt [OPT_MODE];
   opt->name = SANE_NAME_SCAN_MODE;
   opt->title = SANE_TITLE_SCAN_MODE;
   opt->desc = SANE_DESC_SCAN_MODE;
   opt->type = SANE_TYPE_STRING;
   opt->size = sizeof (SANE_VALUE_SCAN_MODE_HALFTONE);
   opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
   opt->constraint.string_list = mode_list;

   opt = &s->opt [OPT_RES];
   opt->name = SANE_NAME_SCAN_RESOLUTION;
   opt->title = SANE_TITLE_SCAN_RESOLUTION;
   opt->desc = SANE_DESC_SCAN_RESOLUTION;
   opt->type = SANE_TYPE_INT;
   opt->unit = SANE_UNIT_DPI;
   opt->constraint_type = SANE_CONSTRAINT_RANGE;
   opt->constraint.range = &s->res_range;

   opt = &s->opt [OPT_GEOMETRY_GROUP];
   opt->name = SANE_NAME_GEOMETRY;
   opt->title = SANE_TITLE_GEOMETRY;
   opt->desc = SANE_DESC_GEOMETRY;
   opt->type = SANE_TYPE_GROUP;
   opt->size = 0;
   opt->cap = 0;

   struct { int num; const char *name, *title, *desc; SANE_Range *range; }
      geom [] =
      {
      { OPT_TL_X, SANE_NAME_SCAN_TL_X, SANE_TITLE_SCAN_TL_X,
        SANE_DESC_SCAN_TL_X, &s->x_range },
      { OPT_TL_Y, SANE_NAME_SCAN_TL_Y, SANE_TITLE_SCAN_TL_Y,
        SANE_DESC_SCAN_TL_Y, &s->y_range },
      { OPT_BR_X, SANE_NAME_SCAN_BR_X, SANE_TITLE_SCAN_BR_X,
        SANE_DESC_SCAN_BR_X, &s->x_range },
      { OPT_BR_Y, SANE_NAME_SCAN_BR_Y, SANE_TITLE_SCAN_BR_Y,
        SANE_DESC_SCAN_BR_Y, &s->y_range },
      { OPT_PAGE_WIDTH, SANE_NAME_PAGE_WIDTH, SANE_TITLE_PAGE_WIDTH,
        SANE_DESC_PAGE_WIDTH, &s->width_range },
      { OPT_PAGE_HEIGHT, SANE_NAME_PAGE_HEIGHT, SANE_TITLE_PAGE_HEIGHT,
        SANE_DESC_PAGE_HEIGHT, &s->height_range },
      };

   for (const auto &g : geom)
      {
      opt = &s->opt [g.num];
      opt->name = g.name;
      opt->title = g.title;
      opt->desc = g.desc;
      opt->type = SANE_TYPE_FIXED;
      opt->unit = SANE_UNIT_MM;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = g.range;
      }

   opt = &s->opt [OPT_ENHANCEMENT_GROUP];
   opt->name = SANE_NAME_ENHANCEMENT;
   opt->title = SANE_TITLE_ENHANCEMENT;
   opt->desc = SANE_DESC_ENHANCEMENT;
   opt->type = SANE_TYPE_GROUP;
   opt->size = 0;
   opt->cap = 0;

   struct { int num; const char *name, *title, *desc; SANE_Range *range; }
      enhance [] =
      {
      { OPT_BRIGHTNESS, SANE_NAME_BRIGHTNESS, SANE_TITLE_BRIGHTNESS,
        SANE_DESC_BRIGHTNESS, &s->enhance_range },
      { OPT_CONTRAST, SANE_NAME_CONTRAST, SANE_TITLE_CONTRAST,
        SANE_DESC_CONTRAST, &s->enhance_range },
      { OPT_THRESHOLD, SANE_NAME_THRESHOLD, SANE_TITLE_THRESHOLD,
        SANE_DESC_THRESHOLD, &s->threshold_range },
      };

   for (const auto &e : enhance)
      {
      opt = &s->opt [e.num];
      opt->name = e.name;
      opt->title = e.title;
      opt->desc = e.desc;
      opt->type = SANE_TYPE_INT;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = e.range;
      }

   // the names, titles and descriptions the fujitsu back end gives
   opt = &s->opt [OPT_ADVANCED_GROUP];
   opt->name = SANE_NAME_ADVANCED;
   opt->title = SANE_TITLE_ADVANCED;
   opt->desc = SANE_DESC_ADVANCED;
   opt->type = SANE_TYPE_GROUP;
   opt->size = 0;
   opt->cap = 0;

   opt = &s->opt [OPT_ALD];
   opt->name = "ald";
   opt->title = "Auto length detection";
   opt->desc = "Scanner detects paper lower edge. May confuse some frontends.";
   opt->type = SANE_TYPE_BOOL;
   opt->cap |= SANE_CAP_ADVANCED;

   opt = &s->opt [OPT_COMPRESS];
   opt->name = "compression";
   opt->title = "Compression";
   opt->desc = "Enable compressed data. Needs a frontend which understands "
               "JPEG frames";
   opt->type = SANE_TYPE_STRING;
   opt->size = sizeof ("None");
   opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
   opt->constraint.string_list = compress_list;

   opt = &s->opt [OPT_COMPRESS_ARG];
   opt->name = "compression-arg";
   opt->title = "Compression argument";
   opt->desc = "Level of JPEG compression. 1 is small file, 7 is large file. "
               "0 (default) is same as 4";
   opt->type = SANE_TYPE_INT;
   opt->constraint_type = SANE_CONSTRAINT_RANGE;
   opt->constraint.range = &s->compress_arg_range;

   opt = &s->opt [OPT_HWDESKEWCROP];
   opt->name = "hwdeskewcrop";
   opt->title = "Hardware deskew and crop";
   opt->desc = "Request scanner to rotate and crop pages digitally.";
   opt->type = SANE_TYPE_BOOL;
   opt->cap |= SANE_CAP_ADVANCED;

   opt = &s->opt [OPT_BUFFERMODE];
   opt->name = "buffermode";
   opt->title = "Buffer mode";
   opt->desc = "Request scanner to read pages quickly from ADF into internal "
               "memory";
   opt->type = SANE_TYPE_STRING;
   opt->size = sizeof ("Default");
   opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
   opt->constraint.string_list = buffermode_list;
   opt->cap |= SANE_CAP_ADVANCED;

   // from the fujitsu back end in the libsane which paperman is used with
   opt = &s->opt [OPT_STOP_FEED];
   opt->name = "stop-feed";
   opt->title = "Stop feed";
   opt->desc = "Halt the paper feed during a batch but keep the sheets the "
               "scanner has already taken: the batch ends once they have "
               "been read.";
   opt->type = SANE_TYPE_BUTTON;
   opt->size = 0;
   opt->cap |= SANE_CAP_ADVANCED;

   /* what the scanner's buttons and sensors say, which the front end can
      read but not set */
   opt = &s->opt [OPT_SENSOR_GROUP];
   opt->name = SANE_NAME_SENSORS;
   opt->title = SANE_TITLE_SENSORS;
   opt->desc = SANE_DESC_SENSORS;
   opt->type = SANE_TYPE_GROUP;
   opt->size = 0;
   opt->cap = 0;

   struct { int num; const char *name, *title, *desc; } sensor [] =
      {
      { OPT_SCAN_SW, SANE_NAME_SCAN, SANE_TITLE_SCAN, SANE_DESC_SCAN },
      { OPT_EMAIL_SW, SANE_NAME_EMAIL, SANE_TITLE_EMAIL, SANE_DESC_EMAIL },
      { OPT_DOUBLE_FEED, "double-feed", "Double feed",
        "Double feed detected" },
      };

   for (const auto &sw : sensor)
      {
      opt = &s->opt [sw.num];
      opt->name = sw.name;
      opt->title = sw.title;
      opt->desc = sw.desc;
      opt->type = SANE_TYPE_BOOL;
      opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT
         | SANE_CAP_ADVANCED;
      }
   }


/* the settings a newly opened fujitsu scanner has: one side of a sheet
   of US letter in black and white at 300dpi */
static void init_values (Scanner *s)
   {
   s->source = SOURCE_ADF_FRONT;
   s->mode = MODE_LINEART;
   s->resolution = 300;
   s->page_width = UNITS_PER_INCH * 17 / 2;
   s->page_height = UNITS_PER_INCH * 11;
   s->tl_x = s->tl_y = 0;
   s->br_x = s->page_width;
   s->br_y = s->page_height;
   s->brightness = s->contrast = s->threshold = 0;
   s->ald = false;
   s->compress = COMPRESS_NONE;
   s->compress_arg = 0;
   s->hwdeskewcrop = false;
   s->buffermode = BUFFERMODE_DEFAULT;
   s->state = Scanner::IDLE;
   s->sent = 0;
   s->back_pending = false;
   update_ranges (s);
   update_caps (s);
   }


/* Bring a value within what the option allows, as sanei_constrain_value()
   does for a real back end */
static SANE_Status constrain (const SANE_Option_Descriptor *opt, void *val,
                              SANE_Int *info)
   {
   switch (opt->constraint_type)
      {
      case SANE_CONSTRAINT_RANGE:
         {
         const SANE_Range *range = opt->constraint.range;
         SANE_Word *word = (SANE_Word *)val;
         long long v = *word;

         if (v < range->min)
            v = range->min;
         if (v > range->max)
            v = range->max;
         if (range->quant)
            v = range->min + (v - range->min + range->quant / 2)
               / range->quant * range->quant;
         if (v != *word)
            {
            *word = (SANE_Word)v;
            *info |= SANE_INFO_INEXACT;
            }
         return SANE_STATUS_GOOD;
         }

      case SANE_CONSTRAINT_STRING_LIST:
         for (int i = 0; opt->constraint.string_list [i]; i++)
            if (!strcmp ((const char *)val, opt->constraint.string_list [i]))
               return SANE_STATUS_GOOD;
         return SANE_STATUS_INVAL;

      default:
         return SANE_STATUS_GOOD;
      }
   }


static int list_index (SANE_String_Const *list, const char *val)
   {
   for (int i = 0; list [i]; i++)
      if (!strcmp (list [i], val))
         return i;
   return -1;
   }


/* how a frame is sent, from how wide it is */
static void set_width (const Scanner *s, SANE_Parameters *p, int ppl)
   {
   switch (s->mode)
      {
      case MODE_COLOR:
         p->format = SANE_FRAME_RGB;
         p->depth = 8;
         p->pixels_per_line = ppl;
         p->bytes_per_line = ppl * 3;
         break;
      case MODE_GRAY:
         p->format = SANE_FRAME_GRAY;
         p->depth = 8;
         p->pixels_per_line = ppl;
         p->bytes_per_line = ppl;
         break;
      default:
         // a whole number of bytes to a line
         ppl -= ppl % 8;
         p->format = SANE_FRAME_GRAY;
         p->depth = 1;
         p->pixels_per_line = ppl;
         p->bytes_per_line = ppl / 8;
         break;
      }
   if (jpeg_on (s))
      p->format = SANE_FRAME_JPEG;
   }


/* the size of frame the window and mode make, as the fujitsu back end
   works it out */
static void calc_window (const Scanner *s, SANE_Parameters *p)
   {
   long long width = s->br_x - s->tl_x;
   long long height = s->br_y - s->tl_y;

   p->last_frame = SANE_TRUE;
   p->lines = (int)(height * s->resolution / UNITS_PER_INCH);
   set_width (s, p, (int)(width * s->resolution / UNITS_PER_INCH));
   }


/* where the sheet is, on the scanner: its centre, in scanner units from
   the left of the feeder and the leading edge. The feeder's guides
   centre it, and a sheet which goes through askew turns about there */
static void sheet_centre (const Sheet &sheet, double *x, double *y)
   {
   *x = MAX_X / 2.0;
   *y = sheet.height / 2.0;
   }


/* where the window starts, on the scanner. The fujitsu back end centres
   the page width on the feeder and measures the window from its left
   edge, so a sheet as wide as the page width fills the window from side
   to side */
static void window_origin (const Scanner *s, double *x, double *y)
   {
   *x = s->tl_x + (MAX_X - s->page_width) / 2.0;
   *y = s->tl_y;
   }


/* draw a side of a sheet, upright, into the given rectangle */
static void draw_side (QPainter &painter, const QImage &image,
                       const QRectF &where)
   {
   if (image.isNull ())
      painter.fillRect (where, Qt::white);
   else
      {
      painter.setRenderHint (QPainter::SmoothPixmapTransform);
      painter.drawImage (where, image);
      }
   }


/* What the scanner sees through the window: the side of the sheet, askew
   if it went through that way, on the backing. The window runs on past
   the foot of the sheet if it is longer */
static QImage scan_window (const Scanner *s, int side, int width, int height,
                           QRgb backing)
   {
   const Sheet &sheet = s->sheet;
   double scale = (double)s->resolution / UNITS_PER_INCH;
   double cx, cy, wx, wy;
   QImage out (width, height, QImage::Format_RGB32);

   sheet_centre (sheet, &cx, &cy);
   window_origin (s, &wx, &wy);
   out.fill (backing);

   QPainter painter (&out);

   painter.translate ((cx - wx) * scale, (cy - wy) * scale);
   painter.rotate (sheet.skew);
   draw_side (painter, sheet.side [side],
              QRectF (-sheet.width * scale / 2, -sheet.height * scale / 2,
                      sheet.width * scale, sheet.height * scale));

   return out;
   }


/* How many lines of the window the sheet reaches down, which is where a
   scanner finding the foot of the sheet ends the page. A sheet askew
   reaches down as far as its lowest corner */
static int sheet_foot (const Scanner *s, int lines)
   {
   const Sheet &sheet = s->sheet;
   double scale = (double)s->resolution / UNITS_PER_INCH;
   double angle = sheet.skew * M_PI / 180;
   double half = (sheet.width * fabs (sin (angle))
                  + sheet.height * fabs (cos (angle))) / 2;
   double cx, cy, wx, wy;

   sheet_centre (sheet, &cx, &cy);
   window_origin (s, &wx, &wy);

   int foot = (int)ceil ((cy + half - wy) * scale);

   return qBound (1, foot, lines);
   }


/* The sheet as a scanner which straightens and crops it sends it: turned
   upright and cut down to itself, or to the window if it is bigger.

   The size it reports is that of the sheet, but a JPEG it sends is the
   size of the box which held the sheet as it went through, cornerwise
   if it was askew, with the sheet upright in the middle of it on a
   black ground. That is what an fi-8170 does, and a front end which
   believes the size it was told runs off the end of the page

   \param size    set to the size the scanner reports
   \param box     true to send the box, false for just the sheet */
static QImage straighten (const Scanner *s, int side, const QSize &window,
                          QSize *size, bool box)
   {
   const Sheet &sheet = s->sheet;
   double scale = (double)s->resolution / UNITS_PER_INCH;
   double width = sheet.width * scale;
   double height = sheet.height * scale;

   *size = QSize (qMin ((int)(width + 0.5), window.width ()),
                  qMin ((int)(height + 0.5), window.height ()));
   if (box && sheet.skew)
      {
      double angle = sheet.skew * M_PI / 180;
      double c = fabs (cos (angle)), sn = fabs (sin (angle));
      QImage out ((int)ceil (width * c + height * sn),
                  (int)ceil (width * sn + height * c), QImage::Format_RGB32);
      QPainter painter (&out);

      out.fill (Qt::black);
      draw_side (painter, sheet.side [side],
                 QRectF ((out.width () - width) / 2,
                         (out.height () - height) / 2, width, height));
      return out;
      }

   QImage out (*size, QImage::Format_RGB32);
   QPainter painter (&out);

   out.fill (Qt::black);
   draw_side (painter, sheet.side [side], QRectF (0, 0, width, height));

   return out;
   }


/* turn a picture into lines as a frame sends them */
static QByteArray pack (const Scanner *s, const QImage &picture,
                        const SANE_Parameters &p)
   {
   int lines = picture.height ();
   QByteArray data (p.bytes_per_line * lines, '\0');

   for (int y = 0; y < lines; y++)
      {
      const QRgb *in = (const QRgb *)picture.constScanLine (y);
      uchar *line = (uchar *)data.data () + (qsizetype)y * p.bytes_per_line;

      for (int x = 0; x < p.pixels_per_line; x++)
         switch (s->mode)
            {
            case MODE_COLOR:
               line [x * 3] = qRed (in [x]);
               line [x * 3 + 1] = qGreen (in [x]);
               line [x * 3 + 2] = qBlue (in [x]);
               break;
            case MODE_GRAY:
               line [x] = qGray (in [x]);
               break;
            default:
               /* a set bit is black. Halftone is sent as line art for
                  now, since nothing yet looks at the difference */
               if (qGray (in [x]) < (s->threshold ? s->threshold : 128))
                  line [x / 8] |= 0x80 >> (x % 8);
               break;
            }
      }

   return data;
   }


/* Compress a picture as the scanner does, with a restart marker at the
   end of each row of blocks

   \param level   compression-arg: 1 for a small file up to 7 for a large
                  one, or 0 for the same as 4 */
static QByteArray encode_jpeg (const QImage &picture, bool gray, int level)
   {
   struct jpeg_compress_struct cinfo;
   struct jpeg_error_mgr jerr;
   unsigned char *buf = nullptr;
   unsigned long size = 0;
   QByteArray row (picture.width () * 3, '\0');

   cinfo.err = jpeg_std_error (&jerr);
   jpeg_create_compress (&cinfo);
   jpeg_mem_dest (&cinfo, &buf, &size);
   cinfo.image_width = picture.width ();
   cinfo.image_height = picture.height ();
   cinfo.input_components = gray ? 1 : 3;
   cinfo.in_color_space = gray ? JCS_GRAYSCALE : JCS_RGB;
   jpeg_set_defaults (&cinfo);
   jpeg_set_quality (&cinfo, 20 + (level ? level : 4) * 10, TRUE);

   /* a Fujitsu scanner keeps the colour at full resolution, where the
      library would halve it each way, which blurs a thin line of colour
      such as a pen stroke into the paper round it. Each row of blocks is
      then 8 lines */
   for (int i = 0; i < cinfo.num_components; i++)
      cinfo.comp_info [i].h_samp_factor = cinfo.comp_info [i].v_samp_factor = 1;
   cinfo.restart_in_rows = 1;
   jpeg_start_compress (&cinfo, TRUE);

   while (cinfo.next_scanline < cinfo.image_height)
      {
      const QRgb *in = (const QRgb *)picture.constScanLine
         (cinfo.next_scanline);
      JSAMPROW out = (JSAMPROW)row.data ();

      for (int x = 0; x < picture.width (); x++)
         if (gray)
            out [x] = qGray (in [x]);
         else
            {
            out [x * 3] = qRed (in [x]);
            out [x * 3 + 1] = qGreen (in [x]);
            out [x * 3 + 2] = qBlue (in [x]);
            }
      jpeg_write_scanlines (&cinfo, &out, 1);
      }
   jpeg_finish_compress (&cinfo);

   QByteArray data ((const char *)buf, (int)size);

   free (buf);
   jpeg_destroy_compress (&cinfo);

   return data;
   }


/* Change the height a JPEG promises, which a scanner ending the page at
   the foot of the sheet leaves as the height of the whole window: it has
   written that before it finds the foot, and then ends the picture there
   and finishes the file off properly */
static void promise_height (QByteArray &jpeg, int lines)
   {
   uchar *data = (uchar *)jpeg.data ();
   int pos = 2;   // past the start of image

   while (pos + 4 <= jpeg.size () && data [pos] == 0xff)
      {
      int marker = data [pos + 1];
      int len = data [pos + 2] << 8 | data [pos + 3];

      // any start of frame, which gives the precision and then the height
      if (marker >= 0xc0 && marker <= 0xc3 && pos + 7 <= jpeg.size ())
         {
         data [pos + 5] = lines >> 8;
         data [pos + 6] = lines & 0xff;
         return;
         }
      pos += 2 + len;
      }
   }


static const char *status_name (SANE_Status status)
   {
   static const char *const name [] =
      {
      "Good", "Unsupported", "Cancelled", "Device busy", "Invalid",
      "EOF", "Jammed", "No docs", "Cover open", "I/O error", "No memory",
      "Access denied"
      };

   return (unsigned)status < sizeof (name) / sizeof (name [0])
      ? name [status] : "?";
   }


/* add a line to what fakescan_log() gives */
static void note (const QString &what)
   {
   QMutexLocker locker (&machine.lock);

   machine.log << what;
   }


static SANE_Status noted (const char *call, SANE_Status status)
   {
   note (QString ("%1: %2").arg (call).arg (status_name (status)));
   return status;
   }


/* A call on a handle, which says if it finds another already in progress
   on the same one: the real back end keeps state in the handle which
   nothing guards, so two at once is a fault in the front end */
namespace {
class Call
   {
public:
   Call (Scanner *s, const char *name) : _s (s)
      {
      if (_s->calls.fetchAndAddOrdered (1))
         note (QString ("concurrent %1").arg (name));
      }

   ~Call ()
      {
      _s->calls.fetchAndAddOrdered (-1);
      }

private:
   Scanner *_s;
   };
}


/* Take the fault arranged for a side of a sheet, if there is one. With
   machine.lock held

   \param at_start   true for one at sane_start(), false for one part-way
                     through the side
   \param kind       the kind wanted, or -1 for any */
static bool take_fault (int sheet, int side, bool at_start,
                        fakescan_fault *out, int kind = -1)
   {
   for (int i = 0; i < machine.faults.size (); i++)
      {
      const fakescan_fault &f = machine.faults [i];

      if (f.sheet == sheet && f.side == side
          && (f.line < 0) == at_start && (kind == -1 || f.kind == kind))
         {
         *out = machine.faults.takeAt (i);
         return true;
         }
      }

   return false;
   }


/* Is there a sheet to read? With buffermode on, the feeder first takes
   sheets from the hopper, the one to be read and up to READ_AHEAD more,
   so they are in the scanner before the front end asks for them. Once
   the feed is stopped it takes no more, and only those it has already
   taken are left. With machine.lock held */
static bool sheet_ready (const Scanner *s)
   {
   if (s->buffermode == BUFFERMODE_ON)
      while (!machine.feed_stopped && machine.taken.size () <= READ_AHEAD
             && !machine.sheets.isEmpty ())
         machine.taken.append (machine.sheets.takeFirst ());

   return !machine.taken.isEmpty ()
          || (!machine.feed_stopped && !machine.sheets.isEmpty ());
   }


/* feed the next sheet, which sheet_ready() said there is */
static Sheet take_sheet (void)
   {
   machine.fed++;
   return !machine.taken.isEmpty () ? machine.taken.takeFirst ()
                                    : machine.sheets.takeFirst ();
   }


/* what the scanner says while it is stuck, with machine.lock held */
static SANE_Status stuck (void)
   {
   if (machine.wedged)
      return SANE_STATUS_IO_ERROR;
   if (machine.cover_open)
      return SANE_STATUS_COVER_OPEN;
   if (machine.jammed)
      return SANE_STATUS_JAMMED;

   return SANE_STATUS_GOOD;
   }


/* Make a fault happen: leave the scanner as it says, and say what the
   front end is told */
static SANE_Status happen (int kind)
   {
   switch (kind)
      {
      case FAKESCAN_JAM:
         machine.jammed = true;
         return SANE_STATUS_JAMMED;
      case FAKESCAN_DOUBLE_FEED:
         machine.jammed = true;
         machine.double_feed = true;
         return SANE_STATUS_JAMMED;
      case FAKESCAN_COVER_OPEN:
         machine.cover_open = true;
         return SANE_STATUS_COVER_OPEN;
      case FAKESCAN_IO_ERROR:
      default:
         machine.wedged = true;
         return SANE_STATUS_IO_ERROR;
      }
   }


/* Spoil some of a JPEG's data, in the middle of it, without making a
   marker of it */
static void spoil (QByteArray &jpeg)
   {
   for (int i = jpeg.size () / 2; i < jpeg.size () / 2 + 64
        && i < jpeg.size () - 2; i++)
      jpeg [i] = (char)0x5a;
   }


/* Make a sheet from its description, see fakescan_load_sheet()

   \returns 0 if OK, -1 if an image cannot be read, -2 if there is no
            size to be had */
static int make_sheet (const struct fakescan_sheet *in, Sheet *sheet)
   {
   const char *path [2] = { in->front, in->back };
   int dpi = in->dpi > 0 ? in->dpi : 300;

   sheet->skew = in->skew;
   for (int i = 0; i < 2; i++)
      if (path [i])
         {
         if (!sheet->side [i].load (QString::fromUtf8 (path [i])))
            return -1;
         sheet->side [i] = sheet->side [i].convertToFormat
            (QImage::Format_RGB32);
         }

   // with no size given, the paper is the size of its picture
   const QImage &image = sheet->side [0].isNull () ? sheet->side [1]
                                                   : sheet->side [0];

   sheet->width = in->width_mm > 0 ? MM_TO_UNITS (in->width_mm)
                                   : image.width () * UNITS_PER_INCH / dpi;
   sheet->height = in->height_mm > 0 ? MM_TO_UNITS (in->height_mm)
                                     : image.height () * UNITS_PER_INCH / dpi;
   if (sheet->width <= 0 || sheet->height <= 0)
      return -2;

   return 0;
   }


/* The resolution a picture of a sheet is at: what the image says, if it
   says something a scan could be at, else 300dpi. Most formats say 72dpi
   when nobody has told them anything */
static int image_dpi (const QString &path)
   {
   QImage image (path);
   int dpi = qRound (image.dotsPerMeterX () * 0.0254);

   return dpi >= 100 ? dpi : 300;
   }


/* Take up whatever a person has done through the directory named by
   FAKESCAN_DIR, see fakescan.h: put the sheets in its hopper into the
   scanner's hopper, in order of name, and press any buttons asked for.
   With machine.lock held */
static void look_at_dir (void)
   {
   QString control_dir = QString::fromLocal8Bit (qgetenv ("FAKESCAN_DIR"));

   if (control_dir.isEmpty ())
      return;

   QDir hopper (control_dir + "/hopper");
   QDir fed (control_dir + "/fed");
   QStringList files = hopper.entryList (QDir::Files, QDir::Name);

   if (!files.isEmpty ())
      fed.mkpath (".");
   for (const QString &name : files)
      {
      QFileInfo info (hopper.filePath (name));

      // a back goes with its front
      if (info.completeBaseName ().endsWith (".back"))
         continue;

      QString front = info.filePath ();
      QString back;

      for (const QString &other : files)
         if (QFileInfo (other).completeBaseName ()
             == info.completeBaseName () + ".back")
            back = hopper.filePath (other);

      QByteArray front8 = front.toUtf8 (), back8 = back.toUtf8 ();
      struct fakescan_sheet desc = {};
      Sheet sheet;

      desc.front = front8.constData ();
      desc.back = back.isEmpty () ? nullptr : back8.constData ();
      desc.dpi = image_dpi (front);
      if (!make_sheet (&desc, &sheet))
         machine.sheets.append (sheet);
      for (const QString &path : { front, back })
         if (!path.isEmpty ())
            QFile::rename (path, fed.filePath (QFileInfo (path).fileName ()));
      }

   for (const char *button : { SANE_NAME_SCAN, SANE_NAME_EMAIL })
      if (QFile::remove (control_dir + "/press-" + button))
         machine.pressed.insert (button);
   }


EXPORT SANE_Status sane_fakefujitsu_init (SANE_Int *version_code,
                                          SANE_Auth_Callback)
   {
   if (version_code)
      *version_code = SANE_VERSION_CODE (1, 0, 0);

   return SANE_STATUS_GOOD;
   }


EXPORT void sane_fakefujitsu_exit (void)
   {
   }


EXPORT SANE_Status sane_fakefujitsu_get_devices (const SANE_Device ***list,
                                                 SANE_Bool)
   {
   static const SANE_Device *devices [] = { &device, nullptr };

   *list = devices;
   return SANE_STATUS_GOOD;
   }


EXPORT SANE_Status sane_fakefujitsu_open (SANE_String_Const name,
                                          SANE_Handle *handle)
   {
   Scanner *s;

   // an empty name means the first scanner, which is the only one
   if (name && *name && strcmp (name, device.name))
      return SANE_STATUS_INVAL;

   s = new Scanner;
   init_options (s);
   init_values (s);
   *handle = s;
   note ("open");

   return SANE_STATUS_GOOD;
   }


EXPORT void sane_fakefujitsu_close (SANE_Handle handle)
   {
   note ("close");
   delete (Scanner *)handle;
   }


EXPORT const SANE_Option_Descriptor *
sane_fakefujitsu_get_option_descriptor (SANE_Handle handle, SANE_Int option)
   {
   Scanner *s = (Scanner *)handle;

   if (option < 0 || option >= NUM_OPTIONS)
      return nullptr;
   return &s->opt [option];
   }


static SANE_Status get_option (Scanner *s, SANE_Int option, void *val)
   {
   SANE_Word *word = (SANE_Word *)val;

   switch (option)
      {
      case OPT_NUM_OPTS:
         *word = NUM_OPTIONS;
         break;
      case OPT_SOURCE:
         strcpy ((char *)val, source_list [s->source]);
         break;
      case OPT_MODE:
         strcpy ((char *)val, mode_list [s->mode]);
         break;
      case OPT_RES:
         *word = s->resolution;
         break;
      case OPT_TL_X:
         *word = UNITS_TO_FIXED (s->tl_x);
         break;
      case OPT_TL_Y:
         *word = UNITS_TO_FIXED (s->tl_y);
         break;
      case OPT_BR_X:
         *word = UNITS_TO_FIXED (s->br_x);
         break;
      case OPT_BR_Y:
         *word = UNITS_TO_FIXED (s->br_y);
         break;
      case OPT_PAGE_WIDTH:
         *word = UNITS_TO_FIXED (s->page_width);
         break;
      case OPT_PAGE_HEIGHT:
         *word = UNITS_TO_FIXED (s->page_height);
         break;
      case OPT_BRIGHTNESS:
         *word = s->brightness;
         break;
      case OPT_CONTRAST:
         *word = s->contrast;
         break;
      case OPT_THRESHOLD:
         *word = s->threshold;
         break;
      case OPT_ALD:
         *word = s->ald;
         break;
      case OPT_COMPRESS:
         strcpy ((char *)val, compress_list [s->compress]);
         break;
      case OPT_COMPRESS_ARG:
         *word = s->compress_arg;
         break;
      case OPT_HWDESKEWCROP:
         *word = s->hwdeskewcrop;
         break;
      case OPT_BUFFERMODE:
         strcpy ((char *)val, buffermode_list [s->buffermode]);
         break;

      // a press stays until it has been seen
      case OPT_SCAN_SW:
      case OPT_EMAIL_SW:
         {
         QMutexLocker locker (&machine.lock);

         look_at_dir ();
         *word = machine.pressed.remove (s->opt [option].name);
         break;
         }
      case OPT_DOUBLE_FEED:
         {
         QMutexLocker locker (&machine.lock);

         *word = machine.double_feed;
         break;
         }
      default:
         return SANE_STATUS_INVAL;
      }

   return SANE_STATUS_GOOD;
   }


/* Change a setting, with the same effect on the others as the fujitsu
   back end has */
static SANE_Status set_option (Scanner *s, SANE_Int option, void *val,
                               SANE_Int *info)
   {
   SANE_Word word = *(SANE_Word *)val;
   SANE_Int reload = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
   int *where = nullptr;
   int units;

   switch (option)
      {
      case OPT_SOURCE:
         s->source = list_index (source_list, (const char *)val);
         *info |= reload;
         return SANE_STATUS_GOOD;

      case OPT_MODE:
         s->mode = list_index (mode_list, (const char *)val);
         update_caps (s);
         *info |= reload;
         return SANE_STATUS_GOOD;

      /* these say which options apply, or how big a page will be, but
         the fujitsu back end only says to reload for ald */
      case OPT_BRIGHTNESS:
         s->brightness = word;
         return SANE_STATUS_GOOD;

      case OPT_CONTRAST:
         s->contrast = word;
         return SANE_STATUS_GOOD;

      case OPT_THRESHOLD:
         s->threshold = word;
         return SANE_STATUS_GOOD;

      case OPT_ALD:
         s->ald = word;
         *info |= SANE_INFO_RELOAD_OPTIONS;
         return SANE_STATUS_GOOD;

      case OPT_COMPRESS:
         s->compress = list_index (compress_list, (const char *)val);
         update_caps (s);
         return SANE_STATUS_GOOD;

      case OPT_COMPRESS_ARG:
         s->compress_arg = word;
         return SANE_STATUS_GOOD;

      case OPT_HWDESKEWCROP:
         s->hwdeskewcrop = word;
         return SANE_STATUS_GOOD;

      case OPT_BUFFERMODE:
         s->buffermode = list_index (buffermode_list, (const char *)val);
         return SANE_STATUS_GOOD;

      case OPT_RES:
         if (s->resolution != word)
            {
            s->resolution = word;
            *info |= reload;
            }
         return SANE_STATUS_GOOD;

      case OPT_TL_X:
         where = &s->tl_x;
         break;
      case OPT_TL_Y:
         where = &s->tl_y;
         break;
      case OPT_BR_X:
         where = &s->br_x;
         break;
      case OPT_BR_Y:
         where = &s->br_y;
         break;

      /* a window which covers the whole page follows the page when its
         size changes; any other is left where it was put */
      case OPT_PAGE_WIDTH:
         units = FIXED_TO_UNITS (word);
         if (units != s->page_width)
            {
            if (s->tl_x == 0 && s->br_x == s->page_width)
               {
               s->br_x = units;
               *info |= SANE_INFO_RELOAD_PARAMS;
               }
            s->page_width = units;
            update_ranges (s);
            *info |= SANE_INFO_RELOAD_OPTIONS;
            }
         return SANE_STATUS_GOOD;

      case OPT_PAGE_HEIGHT:
         units = FIXED_TO_UNITS (word);
         if (units != s->page_height)
            {
            if (s->tl_y == 0 && s->br_y == s->page_height)
               {
               s->br_y = units;
               *info |= SANE_INFO_RELOAD_PARAMS;
               }
            s->page_height = units;
            update_ranges (s);
            *info |= SANE_INFO_RELOAD_OPTIONS;
            }
         return SANE_STATUS_GOOD;

      default:
         return SANE_STATUS_INVAL;
      }

   units = FIXED_TO_UNITS (word);
   if (*where != units)
      {
      *where = units;
      *info |= reload;
      }

   return SANE_STATUS_GOOD;
   }


EXPORT SANE_Status sane_fakefujitsu_control_option (SANE_Handle handle,
      SANE_Int option, SANE_Action action, void *val, SANE_Int *info)
   {
   Scanner *s = (Scanner *)handle;
   Call call (s, "control_option");
   SANE_Int dummy;
   SANE_Status status;

   if (!info)
      info = &dummy;
   *info = 0;
   if (option < 0 || option >= NUM_OPTIONS)
      return SANE_STATUS_INVAL;

   const SANE_Option_Descriptor *opt = &s->opt [option];

   switch (action)
      {
      case SANE_ACTION_GET_VALUE:
         if (!SANE_OPTION_IS_ACTIVE (opt->cap))
            return SANE_STATUS_INVAL;
         return get_option (s, option, val);

      case SANE_ACTION_SET_VALUE:
         /* the one option which makes sense only part-way through a
            batch: the feeder stops, keeping the sheets it has taken */
         if (option == OPT_STOP_FEED)
            {
            if (s->state == Scanner::IDLE)
               return SANE_STATUS_INVAL;

            QMutexLocker locker (&machine.lock);

            machine.feed_stopped = true;
            machine.log << "set stop-feed";
            return SANE_STATUS_GOOD;
            }

         // nothing can be changed part-way through a batch
         if (s->state != Scanner::IDLE)
            return SANE_STATUS_DEVICE_BUSY;
         if (!SANE_OPTION_IS_SETTABLE (opt->cap)
             || !SANE_OPTION_IS_ACTIVE (opt->cap))
            return SANE_STATUS_INVAL;
         if (opt->type == SANE_TYPE_BOOL
             && *(SANE_Word *)val != SANE_FALSE
             && *(SANE_Word *)val != SANE_TRUE)
            return SANE_STATUS_INVAL;
         status = constrain (opt, val, info);
         if (status == SANE_STATUS_GOOD)
            status = set_option (s, option, val, info);
         if (status == SANE_STATUS_GOOD)
            note (QString ("set %1").arg (opt->name));
         return status;

      default:
         return SANE_STATUS_INVAL;
      }
   }


EXPORT SANE_Status sane_fakefujitsu_get_parameters (SANE_Handle handle,
                                                    SANE_Parameters *params)
   {
   Scanner *s = (Scanner *)handle;
   Call call (s, "get_parameters");

   // once a frame has started it is its own size, not the settings'
   if (s->state == Scanner::IDLE)
      {
      calc_window (s, params);

      /* the page ends at the foot of the sheet, which is not known until
         the scanner gets there, unless it reads the whole page before
         sending any of it */
      if (s->ald && !s->hwdeskewcrop)
         params->lines = -1;
      }
   else
      *params = s->params;

   return SANE_STATUS_GOOD;
   }


/* Start the next frame: the back of the sheet already going through, in
   a duplex scan whose front has been read, or else a side of the next
   sheet in the hopper */
EXPORT SANE_Status sane_fakefujitsu_start (SANE_Handle handle)
   {
   Scanner *s = (Scanner *)handle;
   Call call (s, "start");
   SANE_Parameters window;
   QImage picture;
   QRgb backing;
   fakescan_fault fault;
   bool shorten = false, mid = false;
   fakescan_fault mid_fault;
   int busy_ms;

   /* the front end must read a frame to the end, or cancel it, before
      asking for another */
   if (s->state == Scanner::SCANNING)
      return noted ("start", SANE_STATUS_INVAL);

   calc_window (s, &window);
   if (window.pixels_per_line < 1 || window.lines < 1)
      return noted ("start", SANE_STATUS_INVAL);

   {
      QMutexLocker locker (&machine.lock);

      // a new batch sets the feeder going again
      if (s->state == Scanner::IDLE)
         machine.feed_stopped = false;
      look_at_dir ();

      SANE_Status status = stuck ();
      bool back = s->source == SOURCE_ADF_DUPLEX && s->back_pending;
      int sheet_num = back ? s->sheet_num : machine.fed + 1;
      int side = back ? 1 : s->source == SOURCE_ADF_BACK ? 1 : 0;

      busy_ms = machine.busy_ms;
      if (status == SANE_STATUS_GOOD && busy_ms < 0)
         {
         if (!back && !sheet_ready (s))
            status = SANE_STATUS_NO_DOCS;
         else if (take_fault (sheet_num, side, true, &fault))
            {
            if (fault.kind == FAKESCAN_BUSY)
               busy_ms = machine.busy_ms = fault.arg;
            else if (fault.kind == FAKESCAN_JAM
                     || fault.kind == FAKESCAN_DOUBLE_FEED)
               {
               // the sheet is fed, and sticks in the paper path
               if (!back)
                  take_sheet ();
               status = happen (fault.kind);
               }
            else if (fault.kind == FAKESCAN_COVER_OPEN
                     || fault.kind == FAKESCAN_IO_ERROR)
               status = happen (fault.kind);
            else
               // it happens once the side is under way
               machine.faults.prepend (fault);
            }
         }
      if (status != SANE_STATUS_GOOD)
         {
         s->state = Scanner::IDLE;
         s->back_pending = false;
         locker.unlock ();
         return noted ("start", status);
         }

      if (busy_ms < 0)
         {
         if (!back)
            {
            s->sheet = take_sheet ();
            s->sheet_num = machine.fed;
            }
         s->side = side;
         backing = machine.backing;
         shorten = take_fault (sheet_num, side, true, &fault,
                               FAKESCAN_SHORT)
                   || take_fault (sheet_num, side, false, &fault,
                                  FAKESCAN_SHORT);
         mid = take_fault (sheet_num, side, false, &mid_fault);
         if (!mid)
            mid = take_fault (sheet_num, side, true, &mid_fault);
         }
   }

   /* a scanner which is busy takes its time saying so, without the lock
      held, as a real one does not stop anyone else asking */
   if (busy_ms >= 0)
      {
      QThread::msleep (busy_ms);
      return noted ("start", SANE_STATUS_DEVICE_BUSY);
      }

   // and so does scanning a side
   int side_ms;

   {
      QMutexLocker locker (&machine.lock);

      side_ms = machine.side_ms;
   }
   if (side_ms)
      QThread::msleep (side_ms);

   s->params = window;
   if (s->hwdeskewcrop)
      {
      /* the whole sheet is read before any of it is sent, so its size
         is known by the time the front end asks */
      QSize size;

      picture = straighten (s, s->side, QSize (window.pixels_per_line,
                                               window.lines),
                            &size, jpeg_on (s));
      set_width (s, &s->params, size.width ());
      s->params.lines = size.height ();
      if (!jpeg_on (s))
         picture = picture.copy (0, 0, s->params.pixels_per_line,
                                 s->params.lines);
      }
   else
      {
      picture = scan_window (s, s->side, window.pixels_per_line,
                             window.lines, backing);
      if (s->ald)
         {
         picture = picture.copy (0, 0, picture.width (),
                                 sheet_foot (s, window.lines));
         s->params.lines = -1;
         }
      }

   /* what the scanner has promised by now: a page which stops short of
      it still ends properly, so a JPEG says it holds more than it does */
   int promised = s->ald && !s->hwdeskewcrop ? window.lines
                                             : picture.height ();

   if (shorten)
      picture = picture.copy (0, 0, picture.width (),
                              qBound (1, fault.arg, picture.height ()));
   if (jpeg_on (s))
      {
      s->frame = encode_jpeg (picture, s->mode == MODE_GRAY,
                              s->compress_arg);
      if (picture.height () < promised)
         promise_height (s->frame, promised);
      }
   else
      s->frame = pack (s, picture, s->params);

   /* something which goes wrong part-way through the side stops the
      frame there */
   s->stop_at = -1;
   if (mid)
      switch (mid_fault.kind)
         {
         case FAKESCAN_EOF_EMPTY:
            s->frame.clear ();
            break;
         case FAKESCAN_CORRUPT_JPEG:
            if (jpeg_on (s))
               spoil (s->frame);
            break;
         case FAKESCAN_JAM:
         case FAKESCAN_DOUBLE_FEED:
         case FAKESCAN_COVER_OPEN:
         case FAKESCAN_IO_ERROR:
            {
            int line = qBound (0, mid_fault.line, picture.height ());

            s->stop_at = (int)((long long)s->frame.size () * line
                               / picture.height ());
            s->stop_kind = mid_fault.kind;
            break;
            }
         default:
            break;
         }

   s->back_pending = s->source == SOURCE_ADF_DUPLEX && s->side == 0;
   s->sent = 0;
   s->state = Scanner::SCANNING;

   return noted ("start", SANE_STATUS_GOOD);
   }


EXPORT SANE_Status sane_fakefujitsu_read (SANE_Handle handle, SANE_Byte *buf,
                                          SANE_Int max_len, SANE_Int *len)
   {
   Scanner *s = (Scanner *)handle;
   Call call (s, "read");
   int end, left;

   *len = 0;
   {
      QMutexLocker locker (&machine.lock);

      if (machine.wedged)
         {
         locker.unlock ();
         return noted ("read", SANE_STATUS_IO_ERROR);
         }
   }
   if (s->state == Scanner::IDLE)
      return SANE_STATUS_CANCELLED;

   end = s->stop_at >= 0 ? s->stop_at : s->frame.size ();
   left = end - s->sent;
   if (!left && s->stop_at >= 0)
      {
      QMutexLocker locker (&machine.lock);
      SANE_Status status = happen (s->stop_kind);

      locker.unlock ();
      s->state = Scanner::IDLE;
      s->back_pending = false;
      return noted ("read", status);
      }
   if (!left)
      {
      if (s->state == Scanner::DONE)
         return SANE_STATUS_EOF;
      s->state = Scanner::DONE;
      return noted ("read", SANE_STATUS_EOF);
      }

   *len = qMin (max_len, left);
   memcpy (buf, s->frame.constData () + s->sent, *len);
   s->sent += *len;

   return SANE_STATUS_GOOD;
   }


/* End the batch. A duplex sheet whose back has not been read goes out
   of the scanner with it, as on a real one */
EXPORT void sane_fakefujitsu_cancel (SANE_Handle handle)
   {
   Scanner *s = (Scanner *)handle;
   Call call (s, "cancel");

   s->state = Scanner::IDLE;
   s->back_pending = false;
   s->frame.clear ();
   s->sent = 0;

   /* the sheets the feeder had already taken go through to the output
      tray unread, which is what stop-feed is for */
   QMutexLocker locker (&machine.lock);

   machine.log << "cancel";
   machine.taken.clear ();
   machine.feed_stopped = false;
   }


// the fujitsu back end says neither of these is supported
EXPORT SANE_Status sane_fakefujitsu_set_io_mode (SANE_Handle, SANE_Bool)
   {
   return SANE_STATUS_UNSUPPORTED;
   }


EXPORT SANE_Status sane_fakefujitsu_get_select_fd (SANE_Handle, SANE_Int *)
   {
   return SANE_STATUS_UNSUPPORTED;
   }


/* The back door, see fakescan.h */

EXPORT void fakescan_reset (void)
   {
   QMutexLocker locker (&machine.lock);

   machine.sheets.clear ();
   machine.backing = BACKING_DEFAULT;
   machine.faults.clear ();
   machine.taken.clear ();
   machine.feed_stopped = false;
   machine.side_ms = 0;
   machine.fed = 0;
   machine.jammed = machine.double_feed = false;
   machine.cover_open = machine.wedged = false;
   machine.busy_ms = -1;
   machine.pressed.clear ();
   machine.log.clear ();
   }


EXPORT int fakescan_load_sheet (const struct fakescan_sheet *in)
   {
   Sheet sheet;
   int ret = make_sheet (in, &sheet);

   if (ret)
      return ret;

   QMutexLocker locker (&machine.lock);

   machine.sheets.append (sheet);

   return 0;
   }


EXPORT int fakescan_sheets_left (void)
   {
   QMutexLocker locker (&machine.lock);

   return machine.sheets.size ();
   }


EXPORT void fakescan_set_backing (unsigned int rgb)
   {
   QMutexLocker locker (&machine.lock);

   machine.backing = qRgb (rgb >> 16 & 0xff, rgb >> 8 & 0xff, rgb & 0xff);
   }


EXPORT int fakescan_add_fault (const struct fakescan_fault *fault)
   {
   if (fault->sheet < 1 || fault->side < 0 || fault->side > 1
       || fault->kind < FAKESCAN_JAM || fault->kind > FAKESCAN_IO_ERROR)
      return -1;

   QMutexLocker locker (&machine.lock);

   machine.faults.append (*fault);

   return 0;
   }


EXPORT void fakescan_clear (void)
   {
   QMutexLocker locker (&machine.lock);

   machine.jammed = machine.double_feed = false;
   machine.cover_open = machine.wedged = false;
   machine.busy_ms = -1;
   }


EXPORT int fakescan_press (const char *name)
   {
   if (strcmp (name, SANE_NAME_SCAN) && strcmp (name, SANE_NAME_EMAIL))
      return -1;

   QMutexLocker locker (&machine.lock);

   machine.pressed.insert (name);

   return 0;
   }


EXPORT const char *fakescan_log (void)
   {
   QMutexLocker locker (&machine.lock);

   machine.log_out = machine.log.join ("\n").toUtf8 ();

   return machine.log_out.constData ();
   }


EXPORT void fakescan_set_side_time (int ms)
   {
   QMutexLocker locker (&machine.lock);

   machine.side_ms = ms;
   }
