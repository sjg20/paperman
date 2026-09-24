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
   in the mode asked for.

   It is a model of what a front end sees, not of the scanner's firmware
   or of what passes over USB */

#include <string.h>

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>

#include <sane/sane.h>
#include <sane/saneopts.h>

#include "fakescan.h"

#define EXPORT extern "C" __attribute__ ((visibility ("default")))

/* The scanner works in 1/1200ths of an inch, as the fujitsu back end
   does, and gives sizes to the front end in mm */
#define UNITS_PER_INCH    1200
#define MM_TO_UNITS(mm)   ((int) ((mm) / 25.4 * UNITS_PER_INCH + 0.5))
#define UNITS_TO_MM(u)    ((u) * 25.4 / UNITS_PER_INCH)
#define UNITS_TO_FIXED(u) SANE_FIX (UNITS_TO_MM (u))
#define FIXED_TO_UNITS(f) MM_TO_UNITS (SANE_UNFIX (f))

/* What an fi-8170 takes: paper from 50.8 x 54mm up to 8.5in wide and,
   in its long-page mode, 5588mm long. The fujitsu back end reads the
   longest page from the scanner for each resolution and allows less at
   the higher ones; this allows the longest at all of them */
#define MIN_X             MM_TO_UNITS (50.8)
#define MIN_Y             MM_TO_UNITS (54)
#define MAX_X             (UNITS_PER_INCH * 17 / 2)
#define MAX_Y             MM_TO_UNITS (5588)
#define MIN_RES           50
#define MAX_RES           600

/* a light grey, darker than paper. This is not yet measured from a real
   fi-8170 */
#define BACKING_DEFAULT   qRgb (0xc8, 0xc8, 0xc8)

enum { SOURCE_ADF_FRONT, SOURCE_ADF_BACK, SOURCE_ADF_DUPLEX };
enum { MODE_LINEART, MODE_HALFTONE, MODE_GRAY, MODE_COLOR };

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
   NUM_OPTIONS
   };

namespace {

/** a sheet of paper, in the hopper or going through the scanner */
struct Sheet
   {
   QImage side [2];   //!< front and back, or null for plain paper
   int width;         //!< size of the paper, in scanner units
   int height;
   };

/** what is at the scanner rather than behind a SANE handle: it outlasts
    sane_close() and sane_exit(), as paper in a real hopper does */
struct Hopper
   {
   QMutex lock;
   QList<Sheet> sheets;
   QRgb backing = BACKING_DEFAULT;
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

   int source;
   int mode;
   int resolution;
   int tl_x, tl_y, br_x, br_y;      //!< the window, in scanner units
   int page_width, page_height;     //!< the paper, in scanner units

   /** where a scan is up to: IDLE before sane_start() and after
       sane_cancel(), SCANNING while a frame is being read and DONE
       once all of it has been */
   enum { IDLE, SCANNING, DONE } state;
   SANE_Parameters params;    //!< of the frame being read
   QByteArray frame;          //!< all of it, as it is sent
   int sent;                  //!< how much of it has been
   bool back_pending;         //!< a duplex sheet whose back is still to go
   Sheet sheet;               //!< the sheet going through
   };

}

static Hopper hopper;

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
   s->state = Scanner::IDLE;
   s->sent = 0;
   s->back_pending = false;
   update_ranges (s);
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


/* the size of frame the window and mode make, as the fujitsu back end
   works it out */
static void calc_params (const Scanner *s, SANE_Parameters *p)
   {
   long long width = s->br_x - s->tl_x;
   long long height = s->br_y - s->tl_y;
   int ppl = (int)(width * s->resolution / UNITS_PER_INCH);

   p->last_frame = SANE_TRUE;
   p->lines = (int)(height * s->resolution / UNITS_PER_INCH);
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
   }


/* Scan one side of the sheet going through: lay it on the backing inside
   the window and send it in the mode asked for.

   The feeder's guides centre the sheet, and the fujitsu back end centres
   the page width on the feeder, measuring the window from its left
   edge. A sheet as wide as the page width therefore fills the window
   from side to side, and one which is narrower has backing either side
   of it. The window starts at the leading edge of the sheet and runs on
   past its foot, over the backing, if it is longer */
static QByteArray scan_side (const Scanner *s, int side, QRgb backing)
   {
   const SANE_Parameters &p = s->params;
   const Sheet &sheet = s->sheet;
   const QImage &image = sheet.side [side];
   double scale = (double)s->resolution / UNITS_PER_INCH;
   double sheet_x = (MAX_X - sheet.width) / 2.0;
   double window_x = s->tl_x + (MAX_X - s->page_width) / 2.0;
   QRectF paper ((sheet_x - window_x) * scale, -s->tl_y * scale,
                 sheet.width * scale, sheet.height * scale);
   QImage out (p.pixels_per_line, p.lines, QImage::Format_RGB32);

   out.fill (backing);
   {
      QPainter painter (&out);

      if (image.isNull ())
         painter.fillRect (paper, Qt::white);
      else
         {
         painter.setRenderHint (QPainter::SmoothPixmapTransform);
         painter.drawImage (paper, image);
         }
   }

   QByteArray data (p.bytes_per_line * p.lines, '\0');

   for (int y = 0; y < p.lines; y++)
      {
      const QRgb *in = (const QRgb *)out.constScanLine (y);
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
               if (qGray (in [x]) < 128)
                  line [x / 8] |= 0x80 >> (x % 8);
               break;
            }
      }

   return data;
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

   return SANE_STATUS_GOOD;
   }


EXPORT void sane_fakefujitsu_close (SANE_Handle handle)
   {
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
         *info |= reload;
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
         // nothing can be changed part-way through a batch
         if (s->state != Scanner::IDLE)
            return SANE_STATUS_DEVICE_BUSY;
         if (!SANE_OPTION_IS_SETTABLE (opt->cap))
            return SANE_STATUS_INVAL;
         status = constrain (opt, val, info);
         if (status != SANE_STATUS_GOOD)
            return status;
         return set_option (s, option, val, info);

      default:
         return SANE_STATUS_INVAL;
      }
   }


EXPORT SANE_Status sane_fakefujitsu_get_parameters (SANE_Handle handle,
                                                    SANE_Parameters *params)
   {
   Scanner *s = (Scanner *)handle;

   // once a frame has started it is its own size, not the settings'
   if (s->state == Scanner::IDLE)
      calc_params (s, params);
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
   QRgb backing;
   int side;

   /* the front end must read a frame to the end, or cancel it, before
      asking for another */
   if (s->state == Scanner::SCANNING)
      return SANE_STATUS_INVAL;

   calc_params (s, &s->params);
   if (s->params.pixels_per_line < 1 || s->params.lines < 1)
      return SANE_STATUS_INVAL;

   {
      QMutexLocker locker (&hopper.lock);

      backing = hopper.backing;
      if (s->source == SOURCE_ADF_DUPLEX && s->back_pending)
         side = 1;
      else
         {
         if (hopper.sheets.isEmpty ())
            {
            s->state = Scanner::IDLE;
            s->back_pending = false;
            return SANE_STATUS_NO_DOCS;
            }
         s->sheet = hopper.sheets.takeFirst ();
         side = s->source == SOURCE_ADF_BACK ? 1 : 0;
         }
   }

   s->back_pending = s->source == SOURCE_ADF_DUPLEX && side == 0;
   s->frame = scan_side (s, side, backing);
   s->sent = 0;
   s->state = Scanner::SCANNING;

   return SANE_STATUS_GOOD;
   }


EXPORT SANE_Status sane_fakefujitsu_read (SANE_Handle handle, SANE_Byte *buf,
                                          SANE_Int max_len, SANE_Int *len)
   {
   Scanner *s = (Scanner *)handle;
   int left;

   *len = 0;
   if (s->state == Scanner::IDLE)
      return SANE_STATUS_CANCELLED;

   left = s->frame.size () - s->sent;
   if (!left)
      {
      s->state = Scanner::DONE;
      return SANE_STATUS_EOF;
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

   s->state = Scanner::IDLE;
   s->back_pending = false;
   s->frame.clear ();
   s->sent = 0;
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
   QMutexLocker locker (&hopper.lock);

   hopper.sheets.clear ();
   hopper.backing = BACKING_DEFAULT;
   }


EXPORT int fakescan_load_sheet (const struct fakescan_sheet *in)
   {
   const char *path [2] = { in->front, in->back };
   int dpi = in->dpi > 0 ? in->dpi : 300;
   Sheet sheet;

   for (int i = 0; i < 2; i++)
      if (path [i])
         {
         if (!sheet.side [i].load (QString::fromUtf8 (path [i])))
            return -1;
         sheet.side [i] = sheet.side [i].convertToFormat
            (QImage::Format_RGB32);
         }

   // with no size given, the paper is the size of its picture
   const QImage &image = sheet.side [0].isNull () ? sheet.side [1]
                                                  : sheet.side [0];

   sheet.width = in->width_mm > 0 ? MM_TO_UNITS (in->width_mm)
                                  : image.width () * UNITS_PER_INCH / dpi;
   sheet.height = in->height_mm > 0 ? MM_TO_UNITS (in->height_mm)
                                    : image.height () * UNITS_PER_INCH / dpi;
   if (sheet.width <= 0 || sheet.height <= 0)
      return -2;

   QMutexLocker locker (&hopper.lock);

   hopper.sheets.append (sheet);

   return 0;
   }


EXPORT int fakescan_sheets_left (void)
   {
   QMutexLocker locker (&hopper.lock);

   return hopper.sheets.size ();
   }


EXPORT void fakescan_set_backing (unsigned int rgb)
   {
   QMutexLocker locker (&hopper.lock);

   hopper.backing = qRgb (rgb >> 16 & 0xff, rgb >> 8 & 0xff, rgb & 0xff);
   }
