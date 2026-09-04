/*
 * SANE front end for TWAIN scanners on Windows
 *
 * Paperman drives every scanner through the SANE API, so on Windows this
 * file provides the SANE entry points on top of TWAIN. It talks to the
 * TWAIN Data Source Manager (TWAINDSM.dll for TWAIN 2, falling back to the
 * legacy TWAIN_32.DLL) and presents each data source as a SANE device with
 * an option set modelled on SANE's fujitsu backend, which is what paperman
 * expects: source, mode, resolution, the scan window, page size, brightness,
 * contrast and threshold.
 *
 * Images are transferred in memory strips (TWSX_MEMORY) and converted to
 * SANE's packed rows: on Windows the strips arrive as DIB data, so colour
 * pixels are reordered from BGR and 1-bit data is inverted to SANE's
 * 1 = black. A duplex sheet arrives as two images, which the ADF loop in
 * paperman sees as two pages, exactly as with the fujitsu backend.
 *
 * Set PAPERMAN_TWAIN_DEBUG=1 to log every TWAIN operation and the
 * capabilities of the data source to stderr; with a new scanner this is
 * the first thing to look at.
 *
 * Paperman only ever has one scanner open, so the state is kept in a few
 * static structures rather than behind the SANE_Handle.
 */

#include <windows.h>
#include <objbase.h>

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sane/sane.h>
#include <sane/saneopts.h>

#include "twain.h"

namespace
{

/* ---------------------------------------------------------------------- */
/* Logging */

bool debug_enabled (void)
   {
   static int enabled = -1;

   if (enabled < 0)
      {
      const char *env = getenv ("PAPERMAN_TWAIN_DEBUG");
      enabled = env && *env && *env != '0';
      }
   return enabled != 0;
   }

void logf (const char *fmt, ...)
   {
   static DWORD start;

   if (!debug_enabled ())
      return;
   if (!start)
      start = GetTickCount ();
   va_list ap;
   va_start (ap, fmt);
   fprintf (stderr, "twain %7.3f: ", (GetTickCount () - start) / 1000.0);
   vfprintf (stderr, fmt, ap);
   fputc ('\n', stderr);
   fflush (stderr);
   va_end (ap);
   }

/* ---------------------------------------------------------------------- */
/* The data source manager */

struct Dsm
   {
   HMODULE lib;            // TWAINDSM.dll or TWAIN_32.DLL
   DSMENTRYPROC entry;     // its DSM_Entry
   TW_IDENTITY app;        // our identity
   TW_ENTRYPOINT ep;       // memory functions from a TWAIN 2 DSM
   bool dsm2;              // true if the DSM speaks TWAIN 2
   bool open;              // DSM is open (state 3)
   bool com;               // we initialised COM on this thread
   HWND hwnd;              // hidden parent window for the data source
   std::vector<TW_IDENTITY> sources;   // from enumerating the DSM
   std::vector<SANE_Device> devices;   // SANE view of the sources
   std::vector<const SANE_Device *> device_list;
   std::vector<std::string> strings;   // backing store for device names
   };

Dsm dsm;

/* Call the DSM. 'dest' is the data source, or NULL for the DSM itself */
TW_UINT16 dsm_call (pTW_IDENTITY dest, TW_UINT32 dg, TW_UINT16 dat,
                    TW_UINT16 msg, TW_MEMREF data)
   {
   if (!dsm.entry)
      return TWRC_FAILURE;
   TW_UINT16 rc = dsm.entry (&dsm.app, dest, dg, dat, msg, data);
   if (rc != TWRC_SUCCESS && rc != TWRC_XFERDONE && rc != TWRC_DSEVENT
       && rc != TWRC_NOTDSEVENT && rc != TWRC_ENDOFLIST)
      logf ("dg %lx dat %x msg %x -> rc %u", (unsigned long)dg, dat, msg, rc);
   return rc;
   }

/* Condition code from the last failure */
TW_UINT16 dsm_status (pTW_IDENTITY dest)
   {
   TW_STATUS status;

   memset (&status, 0, sizeof (status));
   dsm.entry (&dsm.app, dest, DG_CONTROL, DAT_STATUS, MSG_GET, &status);
   return status.ConditionCode;
   }

/* Memory for capability containers: a TWAIN 2 DSM provides its own
   functions, otherwise the global heap is used */
TW_HANDLE mem_alloc (TW_UINT32 size)
   {
   if (dsm.dsm2 && dsm.ep.DSM_MemAllocate)
      return dsm.ep.DSM_MemAllocate (size);
   return (TW_HANDLE)GlobalAlloc (GHND, size);
   }

void mem_free (TW_HANDLE h)
   {
   if (!h)
      return;
   if (dsm.dsm2 && dsm.ep.DSM_MemFree)
      dsm.ep.DSM_MemFree (h);
   else
      GlobalFree ((HGLOBAL)h);
   }

void *mem_lock (TW_HANDLE h)
   {
   if (dsm.dsm2 && dsm.ep.DSM_MemLock)
      return dsm.ep.DSM_MemLock (h);
   return GlobalLock ((HGLOBAL)h);
   }

void mem_unlock (TW_HANDLE h)
   {
   if (dsm.dsm2 && dsm.ep.DSM_MemUnlock)
      dsm.ep.DSM_MemUnlock (h);
   else
      GlobalUnlock ((HGLOBAL)h);
   }

/* ---------------------------------------------------------------------- */
/* Capability values */

/* A capability read from the source: the current value and, for
   enumerations and ranges, the allowed values */
struct CapValue
   {
   bool valid;
   TW_UINT16 type;            // TWTY_*
   double current;            // numbers and booleans; FIX32 as a double
   std::string str;           // strings
   std::vector<double> allowed;   // enumeration items (empty for a range)
   bool is_range;
   double min, max, step;

   CapValue () : valid (false), type (0), current (0), is_range (false),
                 min (0), max (0), step (0) {}
   };

double fix32_to_double (const TW_FIX32 &fix)
   {
   return fix.Whole + fix.Frac / 65536.0;
   }

TW_FIX32 double_to_fix32 (double val)
   {
   TW_FIX32 fix;
   TW_INT32 whole = (TW_INT32)floor (val);

   fix.Whole = (TW_INT16)whole;
   fix.Frac = (TW_UINT16)((val - whole) * 65536.0 + 0.5);
   return fix;
   }

size_t type_size (TW_UINT16 type)
   {
   switch (type)
      {
      case TWTY_INT8: case TWTY_UINT8: case TWTY_BOOL: return 1;
      case TWTY_INT16: case TWTY_UINT16: return 2;
      case TWTY_INT32: case TWTY_UINT32: case TWTY_FIX32: return 4;
      case TWTY_STR32: return sizeof (TW_STR32);
      case TWTY_STR64: return sizeof (TW_STR64);
      case TWTY_STR128: return sizeof (TW_STR128);
      case TWTY_STR255: return sizeof (TW_STR255);
      }
   return 0;
   }

/* Read one item of a container as a number */
double item_value (TW_UINT16 type, const void *item)
   {
   switch (type)
      {
      case TWTY_INT8: return *(const TW_INT8 *)item;
      case TWTY_UINT8: return *(const TW_UINT8 *)item;
      case TWTY_INT16: return *(const TW_INT16 *)item;
      case TWTY_UINT16: return *(const TW_UINT16 *)item;
      case TWTY_INT32: return *(const TW_INT32 *)item;
      case TWTY_UINT32: return *(const TW_UINT32 *)item;
      case TWTY_BOOL: return *(const TW_BOOL *)item;
      case TWTY_FIX32: return fix32_to_double (*(const TW_FIX32 *)item);
      }
   return 0;
   }

void set_item_value (TW_UINT16 type, void *item, double val)
   {
   switch (type)
      {
      case TWTY_INT8: *(TW_INT8 *)item = (TW_INT8)val; break;
      case TWTY_UINT8: *(TW_UINT8 *)item = (TW_UINT8)val; break;
      case TWTY_INT16: *(TW_INT16 *)item = (TW_INT16)val; break;
      case TWTY_UINT16: *(TW_UINT16 *)item = (TW_UINT16)val; break;
      case TWTY_INT32: *(TW_INT32 *)item = (TW_INT32)val; break;
      case TWTY_UINT32: *(TW_UINT32 *)item = (TW_UINT32)val; break;
      case TWTY_BOOL: *(TW_BOOL *)item = (TW_BOOL)val; break;
      case TWTY_FIX32: *(TW_FIX32 *)item = double_to_fix32 (val); break;
      }
   }

bool is_string_type (TW_UINT16 type)
   {
   return type == TWTY_STR32 || type == TWTY_STR64 || type == TWTY_STR128
         || type == TWTY_STR255;
   }

/* Decode a capability container into a CapValue */
void decode_container (const TW_CAPABILITY &cap, CapValue &out)
   {
   out = CapValue ();
   if (!cap.hContainer)
      return;
   void *p = mem_lock (cap.hContainer);
   if (!p)
      return;

   switch (cap.ConType)
      {
      case TWON_ONEVALUE:
         {
         const TW_ONEVALUE *ov = (const TW_ONEVALUE *)p;
         out.type = (TW_UINT16)ov->ItemType;
         if (is_string_type (out.type))
            out.str = std::string ((const char *)&ov->Item,
                                   strnlen ((const char *)&ov->Item,
                                            type_size (out.type)));
         else
            out.current = item_value (out.type, &ov->Item);
         out.valid = true;
         break;
         }

      case TWON_ENUMERATION:
         {
         const TW_ENUMERATION *en = (const TW_ENUMERATION *)p;
         out.type = (TW_UINT16)en->ItemType;
         size_t size = type_size (out.type);
         const char *items = (const char *)en->ItemList;
         for (TW_UINT32 i = 0; i < en->NumItems; i++)
            out.allowed.push_back (item_value (out.type, items + i * size));
         if (en->CurrentIndex < en->NumItems)
            out.current = out.allowed [en->CurrentIndex];
         out.valid = !out.allowed.empty ();
         break;
         }

      case TWON_RANGE:
         {
         const TW_RANGE *r = (const TW_RANGE *)p;
         out.type = (TW_UINT16)r->ItemType;
         if (out.type == TWTY_FIX32)
            {
            out.min = fix32_to_double (*(const TW_FIX32 *)&r->MinValue);
            out.max = fix32_to_double (*(const TW_FIX32 *)&r->MaxValue);
            out.step = fix32_to_double (*(const TW_FIX32 *)&r->StepSize);
            out.current = fix32_to_double (*(const TW_FIX32 *)&r->CurrentValue);
            }
         else
            {
            out.min = (TW_INT32)r->MinValue;
            out.max = (TW_INT32)r->MaxValue;
            out.step = (TW_INT32)r->StepSize;
            out.current = (TW_INT32)r->CurrentValue;
            }
         out.is_range = true;
         out.valid = true;
         break;
         }

      case TWON_ARRAY:
         {
         const TW_ARRAY *ar = (const TW_ARRAY *)p;
         out.type = (TW_UINT16)ar->ItemType;
         size_t size = type_size (out.type);
         const char *items = (const char *)ar->ItemList;
         for (TW_UINT32 i = 0; i < ar->NumItems; i++)
            out.allowed.push_back (item_value (out.type, items + i * size));
         if (!out.allowed.empty ())
            out.current = out.allowed [0];
         out.valid = true;
         break;
         }
      }
   mem_unlock (cap.hContainer);
   }

const char *cap_name (TW_UINT16 cap)
   {
   switch (cap)
      {
      case CAP_FEEDERENABLED: return "CAP_FEEDERENABLED";
      case CAP_FEEDERLOADED: return "CAP_FEEDERLOADED";
      case CAP_AUTOFEED: return "CAP_AUTOFEED";
      case CAP_DUPLEX: return "CAP_DUPLEX";
      case CAP_DUPLEXENABLED: return "CAP_DUPLEXENABLED";
      case CAP_XFERCOUNT: return "CAP_XFERCOUNT";
      case CAP_PAPERDETECTABLE: return "CAP_PAPERDETECTABLE";
      case ICAP_XFERMECH: return "ICAP_XFERMECH";
      case ICAP_PIXELTYPE: return "ICAP_PIXELTYPE";
      case ICAP_BITDEPTH: return "ICAP_BITDEPTH";
      case ICAP_PIXELFLAVOR: return "ICAP_PIXELFLAVOR";
      case ICAP_XRESOLUTION: return "ICAP_XRESOLUTION";
      case ICAP_YRESOLUTION: return "ICAP_YRESOLUTION";
      case ICAP_UNITS: return "ICAP_UNITS";
      case ICAP_PHYSICALWIDTH: return "ICAP_PHYSICALWIDTH";
      case ICAP_PHYSICALHEIGHT: return "ICAP_PHYSICALHEIGHT";
      case ICAP_SUPPORTEDSIZES: return "ICAP_SUPPORTEDSIZES";
      case ICAP_BRIGHTNESS: return "ICAP_BRIGHTNESS";
      case ICAP_CONTRAST: return "ICAP_CONTRAST";
      case ICAP_THRESHOLD: return "ICAP_THRESHOLD";
      case ICAP_COMPRESSION: return "ICAP_COMPRESSION";
      case ICAP_UNDEFINEDIMAGESIZE: return "ICAP_UNDEFINEDIMAGESIZE";
      case ICAP_AUTOMATICBORDERDETECTION: return "ICAP_AUTOMATICBORDERDETECTION";
      case ICAP_AUTOMATICDESKEW: return "ICAP_AUTOMATICDESKEW";
      case ICAP_AUTOSIZE: return "ICAP_AUTOSIZE";
      case ICAP_AUTOMATICROTATE: return "ICAP_AUTOMATICROTATE";
      case ICAP_AUTOMATICLENGTHDETECTION: return "ICAP_AUTOMATICLENGTHDETECTION";
      case ICAP_ORIENTATION: return "ICAP_ORIENTATION";
      case ICAP_ROTATION: return "ICAP_ROTATION";
      case ICAP_OVERSCAN: return "ICAP_OVERSCAN";
      case ICAP_AUTOMATICCOLORENABLED: return "ICAP_AUTOMATICCOLORENABLED";
      }
   return "?";
   }

void log_cap (const char *what, TW_UINT16 cap, const CapValue &v)
   {
   if (!debug_enabled ())
      return;
   std::string items;
   char buf [64];
   if (v.is_range)
      {
      snprintf (buf, sizeof (buf), " range %g..%g step %g", v.min, v.max,
                v.step);
      items = buf;
      }
   else
      {
      for (size_t i = 0; i < v.allowed.size () && i < 32; i++)
         {
         snprintf (buf, sizeof (buf), " %g", v.allowed [i]);
         items += buf;
         }
      if (v.allowed.size () > 32)
         items += " ...";
      }
   if (is_string_type (v.type))
      logf ("%s %s (%x): '%s'", what, cap_name (cap), cap, v.str.c_str ());
   else
      logf ("%s %s (%x): current %g type %u%s%s", what, cap_name (cap), cap,
            v.current, v.type, v.is_range ? "" : " allowed:", items.c_str ());
   }

/* Query a capability: msg is MSG_GET (all values) or MSG_GETCURRENT */
bool cap_get (pTW_IDENTITY ds, TW_UINT16 id, TW_UINT16 msg, CapValue &out)
   {
   TW_CAPABILITY cap;

   memset (&cap, 0, sizeof (cap));
   cap.Cap = id;
   cap.ConType = TWON_DONTCARE16;
   TW_UINT16 rc = dsm_call (ds, DG_CONTROL, DAT_CAPABILITY, msg, &cap);
   if (rc != TWRC_SUCCESS)
      {
      out = CapValue ();
      logf ("get %s (%x) failed: rc %u cc %u", cap_name (id), id, rc,
            dsm_status (ds));
      return false;
      }
   decode_container (cap, out);
   mem_free (cap.hContainer);
   log_cap (msg == MSG_GET ? "get" : "getcurrent", id, out);
   return out.valid;
   }

/* Set a capability to a single value */
bool cap_set (pTW_IDENTITY ds, TW_UINT16 id, TW_UINT16 type, double val)
   {
   TW_CAPABILITY cap;

   memset (&cap, 0, sizeof (cap));
   cap.Cap = id;
   cap.ConType = TWON_ONEVALUE;
   cap.hContainer = mem_alloc (sizeof (TW_ONEVALUE));
   if (!cap.hContainer)
      return false;
   TW_ONEVALUE *ov = (TW_ONEVALUE *)mem_lock (cap.hContainer);
   ov->ItemType = type;
   ov->Item = 0;
   set_item_value (type, &ov->Item, val);
   mem_unlock (cap.hContainer);

   TW_UINT16 rc = dsm_call (ds, DG_CONTROL, DAT_CAPABILITY, MSG_SET, &cap);
   mem_free (cap.hContainer);
   logf ("set %s (%x) = %g: rc %u", cap_name (id), id, val, rc);
   /* TWRC_CHECKSTATUS means the source picked the nearest value it
      supports, which is fine */
   return rc == TWRC_SUCCESS || rc == TWRC_CHECKSTATUS;
   }

/* ---------------------------------------------------------------------- */
/* The open scanner */

enum
   {
   OPT_NUM_OPTS,
   OPT_STANDARD_GROUP,
   OPT_SOURCE,
   OPT_MODE,
   OPT_RESOLUTION,
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
   OPT_COMPRESSION,
   NUM_OPTIONS
   };

const char *string_Flatbed = "Flatbed";
const char *string_ADFFront = "ADF Front";
const char *string_ADFDuplex = "ADF Duplex";
const char *string_Lineart = "Lineart";
const char *string_Gray = "Gray";
const char *string_Color = "Color";
const char *string_None = "None";

const double MM_PER_INCH = 25.4;

/* Everything about the one open data source */
struct Scanner
   {
   TW_IDENTITY ds;
   bool open;                 // data source is open (state 4)
   bool enabled;              // data source is enabled (state 5+)
   bool transferring;         // an image is being transferred (state 7)
   bool batch_done;           // the feeder emptied: next start says so
   bool per_sheet;            // the source feeds one sheet per enable
   TW_UINT32 pending;         // images the source still has for us

   // events from the source, set by the callback or the message loop
   volatile bool xfer_ready;
   volatile bool close_request;
   volatile bool device_event;

   // the option table and its values
   SANE_Option_Descriptor opt [NUM_OPTIONS];
   const SANE_String_Const *source_list;
   SANE_String_Const source_items [4];
   SANE_String_Const mode_items [4];
   SANE_String_Const compress_items [2];
   SANE_Range res_range;
   SANE_Range x_range, y_range, page_x_range, page_y_range;
   SANE_Range brightness_range, contrast_range, threshold_range;
   char source [32], mode [32], compression [32];
   SANE_Int resolution;
   SANE_Fixed tl_x, tl_y, br_x, br_y, page_width, page_height;
   SANE_Int brightness, contrast, threshold;
   bool has_brightness, has_contrast, has_threshold;
   bool has_flatbed, has_adf, has_duplex;

   // what the source told us about the image being transferred
   TW_IMAGEINFO info;
   TW_SETUPMEMXFER setup;
   SANE_Parameters params;
   bool params_valid;
   int bytes_per_line;        // as SANE wants them
   bool invert_bits;          // 1-bit data has 0 = black
   bool swap_rgb;             // colour data is BGR
   TW_HANDLE strip;           // buffer for a strip
   size_t queue_pos;
   bool xfer_done;            // the last strip of the image has arrived
   int lines_done;
   };

Scanner scanner;

/* kept outside Scanner so that it stays a plain struct which can be reset
   with memset() */
std::vector<SANE_Word> res_list;    // resolution word list: count, values
std::vector<SANE_Byte> queue;       // converted rows waiting for sane_read

/* ---------------------------------------------------------------------- */
/* Events from the data source */

TW_UINT16 TW_CALLINGSTYLE ds_callback (pTW_IDENTITY, pTW_IDENTITY,
                                       TW_UINT32, TW_UINT16, TW_UINT16 msg,
                                       TW_MEMREF)
   {
   logf ("callback msg %x", msg);
   switch (msg)
      {
      case MSG_XFERREADY:
         scanner.xfer_ready = true;
         break;
      case MSG_CLOSEDSREQ:
      case MSG_CLOSEDSOK:
         scanner.close_request = true;
         break;
      case MSG_DEVICEEVENT:
         scanner.device_event = true;
         break;
      }
   return TWRC_SUCCESS;
   }

/* Run the message loop briefly, passing messages to the source. A TWAIN 1
   source signals events this way rather than through the callback */
void pump_events (void)
   {
   MSG msg;

   while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
      {
      bool handled = false;

      if (scanner.enabled)
         {
         TW_EVENT event;
         event.pEvent = &msg;
         event.TWMessage = MSG_NULL;
         TW_UINT16 rc = dsm_call (&scanner.ds, DG_CONTROL, DAT_EVENT,
                                  MSG_PROCESSEVENT, &event);
         if (rc == TWRC_DSEVENT)
            {
            handled = true;
            if (event.TWMessage != MSG_NULL)
               ds_callback (NULL, NULL, 0, 0, event.TWMessage, NULL);
            }
         }
      if (!handled)
         {
         TranslateMessage (&msg);
         DispatchMessage (&msg);
         }
      }
   }

/* Wait for the source to have an image ready, or give up */
bool wait_for_transfer (int timeout_ms)
   {
   DWORD start = GetTickCount ();

   while (!scanner.xfer_ready && !scanner.close_request)
      {
      pump_events ();
      if (scanner.xfer_ready || scanner.close_request)
         break;
      if (GetTickCount () - start > (DWORD)timeout_ms)
         {
         logf ("timed out waiting for the source");
         return false;
         }
      Sleep (5);
      }
   return scanner.xfer_ready;
   }

/* ---------------------------------------------------------------------- */
/* Opening the DSM */

bool load_dsm (void)
   {
   if (dsm.lib)
      return true;

   dsm.lib = LoadLibraryA ("TWAINDSM.dll");

   /* Windows still ships the legacy TWAIN 1 manager, but every current
      driver installs the TWAIN 2 one, so only fall back to it on request */
   const char *legacy = getenv ("PAPERMAN_TWAIN_LEGACY");
   if (!dsm.lib && legacy && *legacy && *legacy != '0')
      {
      char path [MAX_PATH];
      if (GetWindowsDirectoryA (path, sizeof (path)))
         {
         strncat (path, "\\twain_32.dll", sizeof (path) - strlen (path) - 1);
         dsm.lib = LoadLibraryA (path);
         }
      }
   if (!dsm.lib)
      {
      logf ("no TWAIN data source manager found");
      return false;
      }
   dsm.entry = (DSMENTRYPROC)(void *)GetProcAddress (dsm.lib, "DSM_Entry");
   if (!dsm.entry)
      {
      logf ("DSM_Entry not found in the data source manager");
      FreeLibrary (dsm.lib);
      dsm.lib = NULL;
      return false;
      }
   return true;
   }

bool open_dsm (void)
   {
   if (dsm.open)
      return true;
   if (!load_dsm ())
      return false;

   /* data sources tend to use COM internally and expect the caller to
      have set it up */
   if (!dsm.com)
      {
      HRESULT hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
      dsm.com = SUCCEEDED (hr);
      logf ("CoInitializeEx: %lx", (unsigned long)hr);
      }

   // the source needs a parent window; keep it hidden
   if (!dsm.hwnd)
      dsm.hwnd = CreateWindowExA (0, "STATIC", "paperman TWAIN", WS_POPUP,
                                  0, 0, 1, 1, NULL, NULL,
                                  GetModuleHandle (NULL), NULL);

   memset (&dsm.app, 0, sizeof (dsm.app));
   dsm.app.Version.MajorNum = 1;
   dsm.app.Version.MinorNum = 0;
   dsm.app.Version.Language = TWLG_ENGLISH_USA;
   dsm.app.Version.Country = TWCY_USA;
   strncpy (dsm.app.Version.Info, "paperman", sizeof (dsm.app.Version.Info) - 1);
   dsm.app.ProtocolMajor = TWON_PROTOCOLMAJOR;
   dsm.app.ProtocolMinor = TWON_PROTOCOLMINOR;
   dsm.app.SupportedGroups = DG_IMAGE | DG_CONTROL | DF_APP2;
   strncpy (dsm.app.Manufacturer, "paperman", sizeof (dsm.app.Manufacturer) - 1);
   strncpy (dsm.app.ProductFamily, "paperman", sizeof (dsm.app.ProductFamily) - 1);
   strncpy (dsm.app.ProductName, "paperman", sizeof (dsm.app.ProductName) - 1);

   TW_UINT16 rc = dsm_call (NULL, DG_CONTROL, DAT_PARENT, MSG_OPENDSM,
                            &dsm.hwnd);
   if (rc != TWRC_SUCCESS)
      {
      logf ("cannot open the data source manager: rc %u", rc);
      return false;
      }
   dsm.dsm2 = (dsm.app.SupportedGroups & DF_DSM2) != 0;
   memset (&dsm.ep, 0, sizeof (dsm.ep));
   if (dsm.dsm2)
      {
      dsm.ep.Size = sizeof (dsm.ep);
      if (dsm_call (NULL, DG_CONTROL, DAT_ENTRYPOINT, MSG_GET, &dsm.ep)
          != TWRC_SUCCESS)
         {
         memset (&dsm.ep, 0, sizeof (dsm.ep));
         dsm.dsm2 = false;
         }
      }
   logf ("data source manager open (TWAIN %s)", dsm.dsm2 ? "2" : "1");
   dsm.open = true;
   return true;
   }

void close_dsm (void)
   {
   if (dsm.open)
      {
      dsm_call (NULL, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, &dsm.hwnd);
      dsm.open = false;
      }
   if (dsm.hwnd)
      {
      DestroyWindow (dsm.hwnd);
      dsm.hwnd = NULL;
      }
   if (dsm.lib)
      {
      FreeLibrary (dsm.lib);
      dsm.lib = NULL;
      dsm.entry = NULL;
      }
   if (dsm.com)
      {
      CoUninitialize ();
      dsm.com = false;
      }
   }

/* ---------------------------------------------------------------------- */
/* Option table */

SANE_Fixed mm_from_inches (double inches)
   {
   return SANE_FIX (inches * MM_PER_INCH);
   }

double inches_from_mm (SANE_Fixed mm)
   {
   return SANE_UNFIX (mm) / MM_PER_INCH;
   }

size_t max_string_size (const SANE_String_Const *list)
   {
   size_t size = 0;

   for (; *list; list++)
      if (strlen (*list) + 1 > size)
         size = strlen (*list) + 1;
   return size;
   }

void init_range (SANE_Range &range, SANE_Word min, SANE_Word max,
                 SANE_Word quant)
   {
   range.min = min;
   range.max = max;
   range.quant = quant;
   }

void set_group (SANE_Option_Descriptor &opt, const char *name,
                const char *title, const char *desc)
   {
   opt.name = name;
   opt.title = title;
   opt.desc = desc;
   opt.type = SANE_TYPE_GROUP;
   opt.constraint_type = SANE_CONSTRAINT_NONE;
   opt.cap = 0;
   opt.size = 0;
   }

void set_mm_option (SANE_Option_Descriptor &opt, const char *name,
                    const char *title, const char *desc,
                    const SANE_Range *range)
   {
   opt.name = name;
   opt.title = title;
   opt.desc = desc;
   opt.type = SANE_TYPE_FIXED;
   opt.unit = SANE_UNIT_MM;
   opt.size = sizeof (SANE_Word);
   opt.constraint_type = SANE_CONSTRAINT_RANGE;
   opt.constraint.range = range;
   opt.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   }

void set_int_option (SANE_Option_Descriptor &opt, const char *name,
                     const char *title, const char *desc,
                     const SANE_Range *range, bool present)
   {
   opt.name = name;
   opt.title = title;
   opt.desc = desc;
   opt.type = SANE_TYPE_INT;
   opt.unit = SANE_UNIT_NONE;
   opt.size = sizeof (SANE_Word);
   opt.constraint_type = SANE_CONSTRAINT_RANGE;
   opt.constraint.range = range;
   opt.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   if (!present)
      opt.cap |= SANE_CAP_INACTIVE;
   }

void set_string_option (SANE_Option_Descriptor &opt, const char *name,
                        const char *title, const char *desc,
                        const SANE_String_Const *list)
   {
   opt.name = name;
   opt.title = title;
   opt.desc = desc;
   opt.type = SANE_TYPE_STRING;
   opt.unit = SANE_UNIT_NONE;
   opt.constraint_type = SANE_CONSTRAINT_STRING_LIST;
   opt.constraint.string_list = list;
   opt.size = max_string_size (list);
   opt.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   }

/* Ask the source what it can do and build the option table from that */
void build_options (void)
   {
   Scanner &s = scanner;
   pTW_IDENTITY ds = &s.ds;
   CapValue v;

   memset (s.opt, 0, sizeof (s.opt));

   s.opt [OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
   s.opt [OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
   s.opt [OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
   s.opt [OPT_NUM_OPTS].type = SANE_TYPE_INT;
   s.opt [OPT_NUM_OPTS].size = sizeof (SANE_Word);
   s.opt [OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

   set_group (s.opt [OPT_STANDARD_GROUP], "standard", "Standard",
              "Source, mode and resolution options");

   /* source: a feeder which can be turned off implies a flatbed */
   s.has_adf = false;
   s.has_flatbed = false;
   s.has_duplex = false;
   if (cap_get (ds, CAP_FEEDERENABLED, MSG_GET, v))
      {
      for (size_t i = 0; i < v.allowed.size (); i++)
         {
         if (v.allowed [i])
            s.has_adf = true;
         else
            s.has_flatbed = true;
         }
      if (v.allowed.empty ())
         {
         s.has_adf = true;
         s.has_flatbed = true;
         }
      }
   else
      s.has_flatbed = true;
   if (cap_get (ds, CAP_DUPLEX, MSG_GET, v) && v.current != TWDX_NONE)
      s.has_duplex = true;

   int n = 0;
   if (s.has_flatbed)
      s.source_items [n++] = string_Flatbed;
   if (s.has_adf)
      s.source_items [n++] = string_ADFFront;
   if (s.has_adf && s.has_duplex)
      s.source_items [n++] = string_ADFDuplex;
   s.source_items [n] = NULL;
   set_string_option (s.opt [OPT_SOURCE], SANE_NAME_SCAN_SOURCE,
                      SANE_TITLE_SCAN_SOURCE, SANE_DESC_SCAN_SOURCE,
                      s.source_items);
   strcpy (s.source, s.has_adf ? string_ADFFront : string_Flatbed);

   /* mode */
   n = 0;
   if (cap_get (ds, ICAP_PIXELTYPE, MSG_GET, v))
      {
      for (size_t i = 0; i < v.allowed.size (); i++)
         switch ((int)v.allowed [i])
            {
            case TWPT_BW: s.mode_items [n++] = string_Lineart; break;
            case TWPT_GRAY: s.mode_items [n++] = string_Gray; break;
            case TWPT_RGB: s.mode_items [n++] = string_Color; break;
            }
      }
   if (!n)
      {
      s.mode_items [n++] = string_Lineart;
      s.mode_items [n++] = string_Gray;
      s.mode_items [n++] = string_Color;
      }
   s.mode_items [n] = NULL;
   set_string_option (s.opt [OPT_MODE], SANE_NAME_SCAN_MODE,
                      SANE_TITLE_SCAN_MODE, SANE_DESC_SCAN_MODE, s.mode_items);
   strcpy (s.mode, s.mode_items [0]);

   /* resolution: either a list or a range, as the source reports it */
   s.opt [OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
   s.opt [OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
   s.opt [OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
   s.opt [OPT_RESOLUTION].type = SANE_TYPE_INT;
   s.opt [OPT_RESOLUTION].unit = SANE_UNIT_DPI;
   s.opt [OPT_RESOLUTION].size = sizeof (SANE_Word);
   s.opt [OPT_RESOLUTION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   s.resolution = 300;
   if (cap_get (ds, ICAP_XRESOLUTION, MSG_GET, v) && v.is_range)
      {
      init_range (s.res_range, (SANE_Word)v.min, (SANE_Word)v.max,
                  (SANE_Word)(v.step > 0 ? v.step : 1));
      s.opt [OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
      s.opt [OPT_RESOLUTION].constraint.range = &s.res_range;
      s.resolution = (SANE_Int)v.current;
      }
   else if (v.valid && !v.allowed.empty ())
      {
      res_list.clear ();
      res_list.push_back (0);
      for (size_t i = 0; i < v.allowed.size (); i++)
         if (v.allowed [i] >= 1)
            res_list.push_back ((SANE_Word)v.allowed [i]);
      res_list [0] = (SANE_Word)(res_list.size () - 1);
      s.opt [OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      s.opt [OPT_RESOLUTION].constraint.word_list = &res_list [0];
      s.resolution = (SANE_Int)v.current;
      }
   else
      {
      init_range (s.res_range, 50, 600, 1);
      s.opt [OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
      s.opt [OPT_RESOLUTION].constraint.range = &s.res_range;
      }
   if (s.resolution <= 0)
      s.resolution = 300;

   /* geometry, from the physical size of the scan area */
   set_group (s.opt [OPT_GEOMETRY_GROUP], "geometry", "Geometry",
              "Scan area and media size options");
   double width_in = 8.5, height_in = 14;
   if (cap_get (ds, ICAP_PHYSICALWIDTH, MSG_GETCURRENT, v) && v.current > 0)
      width_in = v.current;
   if (cap_get (ds, ICAP_PHYSICALHEIGHT, MSG_GETCURRENT, v) && v.current > 0)
      height_in = v.current;
   SANE_Fixed max_x = mm_from_inches (width_in);
   SANE_Fixed max_y = mm_from_inches (height_in);
   SANE_Fixed quant = SANE_FIX (0.1);

   init_range (s.x_range, 0, max_x, quant);
   init_range (s.y_range, 0, max_y, quant);
   init_range (s.page_x_range, 0, max_x, quant);
   init_range (s.page_y_range, 0, max_y, quant);
   set_mm_option (s.opt [OPT_TL_X], SANE_NAME_SCAN_TL_X, SANE_TITLE_SCAN_TL_X,
                  SANE_DESC_SCAN_TL_X, &s.x_range);
   set_mm_option (s.opt [OPT_TL_Y], SANE_NAME_SCAN_TL_Y, SANE_TITLE_SCAN_TL_Y,
                  SANE_DESC_SCAN_TL_Y, &s.y_range);
   set_mm_option (s.opt [OPT_BR_X], SANE_NAME_SCAN_BR_X, SANE_TITLE_SCAN_BR_X,
                  SANE_DESC_SCAN_BR_X, &s.x_range);
   set_mm_option (s.opt [OPT_BR_Y], SANE_NAME_SCAN_BR_Y, SANE_TITLE_SCAN_BR_Y,
                  SANE_DESC_SCAN_BR_Y, &s.y_range);
   set_mm_option (s.opt [OPT_PAGE_WIDTH], "page-width", "Page width",
                  "Specifies the width of the media.", &s.page_x_range);
   set_mm_option (s.opt [OPT_PAGE_HEIGHT], "page-height", "Page height",
                  "Specifies the height of the media.", &s.page_y_range);

   // default to A4 within the scanner's limits
   s.page_width = SANE_FIX (210.0) < max_x ? SANE_FIX (210.0) : max_x;
   s.page_height = SANE_FIX (297.0) < max_y ? SANE_FIX (297.0) : max_y;
   s.tl_x = 0;
   s.tl_y = 0;
   s.br_x = s.page_width;
   s.br_y = s.page_height;

   /* enhancement */
   set_group (s.opt [OPT_ENHANCEMENT_GROUP], "enhancement", "Enhancement",
              "Image modification options");
   s.has_brightness = cap_get (ds, ICAP_BRIGHTNESS, MSG_GET, v) && v.is_range;
   init_range (s.brightness_range, s.has_brightness ? (SANE_Word)v.min : -1000,
               s.has_brightness ? (SANE_Word)v.max : 1000, 1);
   s.brightness = s.has_brightness ? (SANE_Int)v.current : 0;
   set_int_option (s.opt [OPT_BRIGHTNESS], SANE_NAME_BRIGHTNESS,
                   SANE_TITLE_BRIGHTNESS, SANE_DESC_BRIGHTNESS,
                   &s.brightness_range, s.has_brightness);

   s.has_contrast = cap_get (ds, ICAP_CONTRAST, MSG_GET, v) && v.is_range;
   init_range (s.contrast_range, s.has_contrast ? (SANE_Word)v.min : -1000,
               s.has_contrast ? (SANE_Word)v.max : 1000, 1);
   s.contrast = s.has_contrast ? (SANE_Int)v.current : 0;
   set_int_option (s.opt [OPT_CONTRAST], SANE_NAME_CONTRAST,
                   SANE_TITLE_CONTRAST, SANE_DESC_CONTRAST,
                   &s.contrast_range, s.has_contrast);

   s.has_threshold = cap_get (ds, ICAP_THRESHOLD, MSG_GET, v) && v.is_range;
   init_range (s.threshold_range, s.has_threshold ? (SANE_Word)v.min : 0,
               s.has_threshold ? (SANE_Word)v.max : 255, 1);
   s.threshold = s.has_threshold ? (SANE_Int)v.current : 128;
   set_int_option (s.opt [OPT_THRESHOLD], SANE_NAME_THRESHOLD,
                   SANE_TITLE_THRESHOLD, SANE_DESC_THRESHOLD,
                   &s.threshold_range, s.has_threshold);

   /* advanced: paperman looks for a compression option; only raw data
      is supported here */
   set_group (s.opt [OPT_ADVANCED_GROUP], "advanced", "Advanced",
              "Hardware specific options");
   s.compress_items [0] = string_None;
   s.compress_items [1] = NULL;
   set_string_option (s.opt [OPT_COMPRESSION], "compression", "Compression",
                      "Enable compressed transfer of image data",
                      s.compress_items);
   strcpy (s.compression, string_None);

   /* log the rest of what the source offers, for bringing up new models */
   if (debug_enabled ())
      {
      static const TW_UINT16 extra [] = {
         CAP_XFERCOUNT, CAP_AUTOFEED, CAP_FEEDERLOADED, CAP_PAPERDETECTABLE,
         ICAP_XFERMECH, ICAP_BITDEPTH, ICAP_PIXELFLAVOR, ICAP_YRESOLUTION,
         ICAP_UNITS, ICAP_SUPPORTEDSIZES, ICAP_COMPRESSION,
         ICAP_UNDEFINEDIMAGESIZE, ICAP_AUTOMATICBORDERDETECTION,
         ICAP_AUTOMATICDESKEW, ICAP_AUTOSIZE, ICAP_AUTOMATICROTATE,
         ICAP_AUTOMATICLENGTHDETECTION, ICAP_ORIENTATION, ICAP_ROTATION,
         ICAP_OVERSCAN, ICAP_AUTOMATICCOLORENABLED };
      for (size_t i = 0; i < sizeof (extra) / sizeof (extra [0]); i++)
         cap_get (ds, extra [i], MSG_GET, v);
      }
   }

/* Estimate the scan parameters from the options, for before a scan */
void estimate_params (void)
   {
   Scanner &s = scanner;
   double width_in = inches_from_mm (s.br_x - s.tl_x);
   double height_in = inches_from_mm (s.br_y - s.tl_y);

   s.params.pixels_per_line = (int)(width_in * s.resolution + 0.5);
   s.params.lines = (int)(height_in * s.resolution + 0.5);
   if (!strcmp (s.mode, string_Color))
      {
      s.params.format = SANE_FRAME_RGB;
      s.params.depth = 8;
      s.params.bytes_per_line = s.params.pixels_per_line * 3;
      }
   else if (!strcmp (s.mode, string_Gray))
      {
      s.params.format = SANE_FRAME_GRAY;
      s.params.depth = 8;
      s.params.bytes_per_line = s.params.pixels_per_line;
      }
   else
      {
      s.params.format = SANE_FRAME_GRAY;
      s.params.depth = 1;
      s.params.bytes_per_line = (s.params.pixels_per_line + 7) / 8;
      }
   s.params.last_frame = SANE_TRUE;
   }

/* ---------------------------------------------------------------------- */
/* Scanning */

/* Push the options to the source, ready to enable it */
bool apply_options (void)
   {
   Scanner &s = scanner;
   pTW_IDENTITY ds = &s.ds;

   cap_set (ds, ICAP_XFERMECH, TWTY_UINT16, TWSX_MEMORY);
   cap_set (ds, ICAP_UNITS, TWTY_UINT16, TWUN_INCHES);

   bool adf = strcmp (s.source, string_Flatbed) != 0;
   bool duplex = !strcmp (s.source, string_ADFDuplex);

   /* Left to itself a source scans the whole feeder ahead into its own
      buffer, as fast as the scanner goes, so stopping after a few pages
      wastes the rest of the stack. Paperman works a sheet at a time, so
      ask for one sheet's images per enable and re-enable for the next.
      That needs the source to say whether paper is loaded, otherwise the
      end of the stack could not be told from a slow feeder */
   CapValue v;
   s.per_sheet = adf
         && cap_get (ds, CAP_PAPERDETECTABLE, MSG_GETCURRENT, v) && v.current;
   cap_set (ds, CAP_XFERCOUNT, TWTY_INT16,
            s.per_sheet ? (duplex ? 2 : 1) : -1);
   if (s.has_adf)
      {
      cap_set (ds, CAP_FEEDERENABLED, TWTY_BOOL, adf);
      if (adf)
         cap_set (ds, CAP_AUTOFEED, TWTY_BOOL, 1);
      }
   if (s.has_duplex)
      cap_set (ds, CAP_DUPLEXENABLED, TWTY_BOOL, duplex);

   int pixel = TWPT_BW;
   if (!strcmp (s.mode, string_Color))
      pixel = TWPT_RGB;
   else if (!strcmp (s.mode, string_Gray))
      pixel = TWPT_GRAY;
   cap_set (ds, ICAP_PIXELTYPE, TWTY_UINT16, pixel);
   cap_set (ds, ICAP_BITDEPTH, TWTY_UINT16, pixel == TWPT_BW ? 1 : 8);

   cap_set (ds, ICAP_XRESOLUTION, TWTY_FIX32, s.resolution);
   cap_set (ds, ICAP_YRESOLUTION, TWTY_FIX32, s.resolution);

   if (s.has_brightness)
      cap_set (ds, ICAP_BRIGHTNESS, TWTY_FIX32, s.brightness);
   if (s.has_contrast)
      cap_set (ds, ICAP_CONTRAST, TWTY_FIX32, s.contrast);
   if (s.has_threshold && pixel == TWPT_BW)
      cap_set (ds, ICAP_THRESHOLD, TWTY_FIX32, s.threshold);

   /* the scan window: tell the source we are giving our own frame, and
      turn off the source's own cropping and deskew so the image is
      exactly that frame, as a SANE backend delivers it. Paperman does its
      own blank-page and skew handling */
   cap_set (ds, ICAP_SUPPORTEDSIZES, TWTY_UINT16, TWSS_NONE);
   cap_set (ds, ICAP_AUTOMATICBORDERDETECTION, TWTY_BOOL, 0);
   cap_set (ds, ICAP_UNDEFINEDIMAGESIZE, TWTY_BOOL, 0);
   cap_set (ds, ICAP_AUTOMATICDESKEW, TWTY_BOOL, 0);
   cap_set (ds, ICAP_AUTOMATICROTATE, TWTY_BOOL, 0);
   cap_set (ds, ICAP_AUTOMATICLENGTHDETECTION, TWTY_BOOL, 0);
   cap_set (ds, ICAP_ORIENTATION, TWTY_UINT16, TWOR_PORTRAIT);
   cap_set (ds, ICAP_ROTATION, TWTY_FIX32, 0);
   TW_IMAGELAYOUT layout;
   memset (&layout, 0, sizeof (layout));
   layout.Frame.Left = double_to_fix32 (inches_from_mm (s.tl_x));
   layout.Frame.Top = double_to_fix32 (inches_from_mm (s.tl_y));
   layout.Frame.Right = double_to_fix32 (inches_from_mm (s.br_x));
   layout.Frame.Bottom = double_to_fix32 (inches_from_mm (s.br_y));
   layout.DocumentNumber = 1;
   layout.PageNumber = 1;
   layout.FrameNumber = 1;
   TW_UINT16 rc = dsm_call (ds, DG_IMAGE, DAT_IMAGELAYOUT, MSG_SET, &layout);
   logf ("image layout %g,%g-%g,%g in: rc %u", inches_from_mm (s.tl_x),
         inches_from_mm (s.tl_y), inches_from_mm (s.br_x),
         inches_from_mm (s.br_y), rc);

   /* how the source will hand us the pixels */
   s.invert_bits = true;
   if (cap_get (ds, ICAP_PIXELFLAVOR, MSG_GETCURRENT, v))
      s.invert_bits = v.current == TWPF_CHOCOLATE;
   const char *rgb = getenv ("PAPERMAN_TWAIN_RGB");
   s.swap_rgb = !(rgb && *rgb && *rgb != '0');
   return true;
   }

void disable_source (void)
   {
   Scanner &s = scanner;

   if (s.enabled)
      {
      TW_USERINTERFACE ui;
      memset (&ui, 0, sizeof (ui));
      ui.hParent = dsm.hwnd;
      dsm_call (&s.ds, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &ui);
      s.enabled = false;
      }
   s.transferring = false;
   s.xfer_ready = false;
   s.pending = 0;
   }

void free_strip (void)
   {
   if (scanner.strip)
      {
      mem_free (scanner.strip);
      scanner.strip = NULL;
      }
   }

/* The source is ready with an image: find out about it */
SANE_Status begin_image (void)
   {
   Scanner &s = scanner;
   pTW_IDENTITY ds = &s.ds;

   memset (&s.info, 0, sizeof (s.info));
   if (dsm_call (ds, DG_IMAGE, DAT_IMAGEINFO, MSG_GET, &s.info) != TWRC_SUCCESS)
      {
      logf ("cannot get image info: cc %u", dsm_status (ds));
      return SANE_STATUS_IO_ERROR;
      }
   logf ("image %ldx%ld, %d bits, pixel type %d, %g x %g dpi",
         (long)s.info.ImageWidth, (long)s.info.ImageLength,
         s.info.BitsPerPixel, s.info.PixelType,
         fix32_to_double (s.info.XResolution),
         fix32_to_double (s.info.YResolution));

   s.params.pixels_per_line = s.info.ImageWidth;
   s.params.lines = s.info.ImageLength;   // -1 if the source does not know
   s.params.last_frame = SANE_TRUE;
   switch (s.info.PixelType)
      {
      case TWPT_RGB:
         s.params.format = SANE_FRAME_RGB;
         s.params.depth = 8;
         s.params.bytes_per_line = s.info.ImageWidth * 3;
         break;
      case TWPT_GRAY:
         s.params.format = SANE_FRAME_GRAY;
         s.params.depth = 8;
         s.params.bytes_per_line = s.info.ImageWidth;
         break;
      default:
         s.params.format = SANE_FRAME_GRAY;
         s.params.depth = 1;
         s.params.bytes_per_line = (s.info.ImageWidth + 7) / 8;
         break;
      }
   if (s.info.BitsPerPixel != 1 && s.info.BitsPerPixel != 8
       && s.info.BitsPerPixel != 24)
      {
      logf ("unsupported %d bits per pixel", s.info.BitsPerPixel);
      return SANE_STATUS_UNSUPPORTED;
      }
   s.bytes_per_line = s.params.bytes_per_line;
   s.params_valid = true;

   memset (&s.setup, 0, sizeof (s.setup));
   if (dsm_call (ds, DG_CONTROL, DAT_SETUPMEMXFER, MSG_GET, &s.setup)
       != TWRC_SUCCESS)
      return SANE_STATUS_IO_ERROR;
   free_strip ();
   TW_UINT32 size = s.setup.Preferred;
   if (!size)
      size = 256 * 1024;
   s.strip = mem_alloc (size);
   if (!s.strip)
      return SANE_STATUS_NO_MEM;
   s.setup.Preferred = size;
   logf ("strip buffer %lu bytes (min %lu max %lu)", (unsigned long)size,
         (unsigned long)s.setup.MinBufSize, (unsigned long)s.setup.MaxBufSize);

   queue.clear ();
   s.queue_pos = 0;
   s.xfer_done = false;
   s.lines_done = 0;
   s.transferring = true;
   return SANE_STATUS_GOOD;
   }

/* With PAPERMAN_TWAIN_DUMP=<file> the converted rows of the first image
   are also written there as a PBM/PGM/PPM, to check the pixel format */
static void dump_rows (const SANE_Byte *rows, size_t size)
   {
   static FILE *f;
   static int lines;
   Scanner &s = scanner;

   if (!f)
      {
      const char *path = getenv ("PAPERMAN_TWAIN_DUMP");
      if (!path || !*path || lines)
         return;
      f = fopen (path, "wb");
      if (!f)
         return;
      fprintf (f, "P%d\n%d %d\n", s.params.format == SANE_FRAME_RGB ? 6
               : s.params.depth == 1 ? 4 : 5, s.params.pixels_per_line,
               s.params.lines > 0 ? s.params.lines : 0);
      if (s.params.depth != 1)
         fprintf (f, "255\n");
      }
   fwrite (rows, 1, size, f);
   lines += (int)(size / s.bytes_per_line);
   if (s.params.lines > 0 && lines >= s.params.lines)
      {
      fclose (f);
      f = NULL;
      }
   }

/* Convert the rows of a strip to SANE's layout and queue them */
void queue_strip (const TW_IMAGEMEMXFER &xfer, const SANE_Byte *data)
   {
   Scanner &s = scanner;
   int rows = xfer.Rows;
   int in_stride = xfer.BytesPerRow;
   int out_stride = s.bytes_per_line;
   size_t start = queue.size ();

   queue.resize (start + (size_t)rows * out_stride);
   for (int y = 0; y < rows; y++)
      {
      const SANE_Byte *in = data + (size_t)y * in_stride;
      SANE_Byte *out = &queue [start + (size_t)y * out_stride];
      int copy = in_stride < out_stride ? in_stride : out_stride;

      if (s.params.format == SANE_FRAME_RGB && s.swap_rgb)
         {
         int pixels = copy / 3;
         for (int x = 0; x < pixels; x++)
            {
            out [x * 3] = in [x * 3 + 2];
            out [x * 3 + 1] = in [x * 3 + 1];
            out [x * 3 + 2] = in [x * 3];
            }
         }
      else if (s.params.depth == 1 && s.invert_bits)
         {
         for (int x = 0; x < copy; x++)
            out [x] = ~in [x];
         }
      else
         memcpy (out, in, copy);
      if (copy < out_stride)
         memset (out + copy, s.params.depth == 1 ? 0 : 0xff,
                 out_stride - copy);
      }
   s.lines_done += rows;
   dump_rows (&queue [start], (size_t)rows * out_stride);
   }

/* Fetch the next strip from the source */
SANE_Status read_strip (void)
   {
   Scanner &s = scanner;
   TW_IMAGEMEMXFER xfer;

   memset (&xfer, 0, sizeof (xfer));
   xfer.Compression = TWON_DONTCARE16;
   xfer.BytesPerRow = TWON_DONTCARE32;
   xfer.Columns = TWON_DONTCARE32;
   xfer.Rows = TWON_DONTCARE32;
   xfer.XOffset = TWON_DONTCARE32;
   xfer.YOffset = TWON_DONTCARE32;
   xfer.BytesWritten = TWON_DONTCARE32;
   xfer.Memory.Flags = TWMF_APPOWNS | TWMF_HANDLE;
   xfer.Memory.Length = s.setup.Preferred;
   xfer.Memory.TheMem = s.strip;

   TW_UINT16 rc = dsm_call (&s.ds, DG_IMAGE, DAT_IMAGEMEMXFER, MSG_GET, &xfer);
   if (rc == TWRC_SUCCESS || rc == TWRC_XFERDONE)
      {
      if (xfer.Rows && xfer.Rows != TWON_DONTCARE32)
         {
         const SANE_Byte *data = (const SANE_Byte *)mem_lock (s.strip);
         if (data)
            {
            queue_strip (xfer, data);
            mem_unlock (s.strip);
            }
         }
      if (rc == TWRC_XFERDONE)
         s.xfer_done = true;
      return SANE_STATUS_GOOD;
      }
   if (rc == TWRC_CANCEL)
      return SANE_STATUS_CANCELLED;
   logf ("memory transfer failed: rc %u cc %u", rc, dsm_status (&s.ds));
   return SANE_STATUS_IO_ERROR;
   }

/* The image is complete: see whether the source has more */
void end_image (void)
   {
   Scanner &s = scanner;
   TW_PENDINGXFERS pending;

   memset (&pending, 0, sizeof (pending));
   dsm_call (&s.ds, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, &pending);
   s.pending = pending.Count;
   s.transferring = false;
   s.xfer_ready = false;
   free_strip ();
   logf ("image done after %d lines, %lu pending", s.lines_done,
         (unsigned long)s.pending);
   if (!s.pending)
      {
      disable_source ();
      // with per-sheet feeding the next start checks the feeder instead
      if (!s.per_sheet)
         s.batch_done = true;
      }
   else
      {
      /* the next image is ready as soon as the source says so; some
         sources do not send another XFERREADY, so do not insist on it */
      s.xfer_ready = true;
      }
   }

void reset_scanner (void)
   {
   memset (&scanner, 0, sizeof (scanner));
   res_list.clear ();
   queue.clear ();
   }

} // namespace

/* ---------------------------------------------------------------------- */
/* The SANE API */

extern "C"
{

SANE_Status sane_init (SANE_Int *version_code, SANE_Auth_Callback)
   {
   if (version_code)
      *version_code = SANE_VERSION_CODE (1, 0, 0);
   reset_scanner ();
   return SANE_STATUS_GOOD;
   }

void sane_exit (void)
   {
   sane_close (NULL);
   close_dsm ();
   dsm.devices.clear ();
   dsm.device_list.clear ();
   dsm.sources.clear ();
   dsm.strings.clear ();
   }

SANE_Status sane_get_devices (const SANE_Device ***device_list, SANE_Bool)
   {
   dsm.sources.clear ();
   dsm.devices.clear ();
   dsm.device_list.clear ();
   dsm.strings.clear ();

   if (open_dsm ())
      {
      TW_IDENTITY id;
      memset (&id, 0, sizeof (id));
      TW_UINT16 rc = dsm_call (NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST,
                               &id);
      while (rc == TWRC_SUCCESS)
         {
         dsm.sources.push_back (id);
         memset (&id, 0, sizeof (id));
         rc = dsm_call (NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, &id);
         }
      }

   /* the strings must stay put once the list is built */
   dsm.strings.reserve (dsm.sources.size () * 3);
   dsm.devices.reserve (dsm.sources.size ());
   for (size_t i = 0; i < dsm.sources.size (); i++)
      {
      const TW_IDENTITY &id = dsm.sources [i];
      SANE_Device dev;

      dsm.strings.push_back (std::string ("twain:") + id.ProductName);
      dev.name = dsm.strings.back ().c_str ();
      dsm.strings.push_back (id.Manufacturer);
      dev.vendor = dsm.strings.back ().c_str ();
      dsm.strings.push_back (id.ProductName);
      dev.model = dsm.strings.back ().c_str ();
      dev.type = "TWAIN scanner";
      dsm.devices.push_back (dev);
      logf ("source %u: '%s' by '%s' (%s), TWAIN %u.%u", (unsigned)i,
            id.ProductName, id.Manufacturer, id.ProductFamily,
            id.ProtocolMajor, id.ProtocolMinor);
      }
   for (size_t i = 0; i < dsm.devices.size (); i++)
      dsm.device_list.push_back (&dsm.devices [i]);
   dsm.device_list.push_back (NULL);
   *device_list = &dsm.device_list [0];
   return SANE_STATUS_GOOD;
   }

SANE_Status sane_open (SANE_String_Const name, SANE_Handle *handle)
   {
   *handle = NULL;
   if (scanner.open)
      sane_close (NULL);
   if (!open_dsm ())
      return SANE_STATUS_UNSUPPORTED;

   // enumerate if that has not happened yet
   if (dsm.sources.empty ())
      {
      const SANE_Device **list;
      sane_get_devices (&list, SANE_TRUE);
      }

   const TW_IDENTITY *found = NULL;
   for (size_t i = 0; i < dsm.sources.size (); i++)
      if (dsm.devices [i].name == std::string (name)
          || !strcmp (dsm.sources [i].ProductName, name))
         found = &dsm.sources [i];
   if (!found)
      {
      logf ("no source called '%s'", name);
      return SANE_STATUS_INVAL;
      }

   reset_scanner ();
   scanner.ds = *found;
   TW_UINT16 rc = dsm_call (NULL, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS,
                            &scanner.ds);
   if (rc != TWRC_SUCCESS)
      {
      logf ("cannot open '%s': cc %u", name, dsm_status (NULL));
      return SANE_STATUS_DEVICE_BUSY;
      }
   scanner.open = true;
   logf ("opened '%s'", name);

   /* ask for the source's events through the callback (TWAIN 2); older
      sources are served by the message loop instead */
   if (dsm.dsm2)
      {
      TW_CALLBACK2 cb2;
      memset (&cb2, 0, sizeof (cb2));
      cb2.CallBackProc = (TW_MEMREF)ds_callback;
      if (dsm_call (&scanner.ds, DG_CONTROL, DAT_CALLBACK2, MSG_REGISTER_CALLBACK,
                    &cb2) != TWRC_SUCCESS)
         {
         TW_CALLBACK cb;
         memset (&cb, 0, sizeof (cb));
         cb.CallBackProc = (TW_MEMREF)ds_callback;
         dsm_call (&scanner.ds, DG_CONTROL, DAT_CALLBACK, MSG_REGISTER_CALLBACK,
                   &cb);
         }
      }

   build_options ();
   estimate_params ();
   *handle = &scanner;
   return SANE_STATUS_GOOD;
   }

void sane_close (SANE_Handle)
   {
   if (!scanner.open)
      return;
   if (scanner.transferring)
      {
      TW_PENDINGXFERS pending;
      memset (&pending, 0, sizeof (pending));
      dsm_call (&scanner.ds, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &pending);
      }
   disable_source ();
   free_strip ();
   dsm_call (NULL, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, &scanner.ds);
   logf ("closed '%s'", scanner.ds.ProductName);
   scanner.open = false;
   }

const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle,
                                                          SANE_Int option)
   {
   if (!scanner.open || option < 0 || option >= NUM_OPTIONS)
      return NULL;
   return &scanner.opt [option];
   }

SANE_Status sane_control_option (SANE_Handle, SANE_Int option,
                                 SANE_Action action, void *value,
                                 SANE_Int *info)
   {
   Scanner &s = scanner;

   if (info)
      *info = 0;
   if (!s.open || option < 0 || option >= NUM_OPTIONS)
      return SANE_STATUS_INVAL;
   if (s.opt [option].cap & SANE_CAP_INACTIVE)
      return SANE_STATUS_INVAL;

   if (action == SANE_ACTION_GET_VALUE)
      {
      switch (option)
         {
         case OPT_NUM_OPTS: *(SANE_Word *)value = NUM_OPTIONS; break;
         case OPT_SOURCE: strcpy ((char *)value, s.source); break;
         case OPT_MODE: strcpy ((char *)value, s.mode); break;
         case OPT_RESOLUTION: *(SANE_Word *)value = s.resolution; break;
         case OPT_TL_X: *(SANE_Word *)value = s.tl_x; break;
         case OPT_TL_Y: *(SANE_Word *)value = s.tl_y; break;
         case OPT_BR_X: *(SANE_Word *)value = s.br_x; break;
         case OPT_BR_Y: *(SANE_Word *)value = s.br_y; break;
         case OPT_PAGE_WIDTH: *(SANE_Word *)value = s.page_width; break;
         case OPT_PAGE_HEIGHT: *(SANE_Word *)value = s.page_height; break;
         case OPT_BRIGHTNESS: *(SANE_Word *)value = s.brightness; break;
         case OPT_CONTRAST: *(SANE_Word *)value = s.contrast; break;
         case OPT_THRESHOLD: *(SANE_Word *)value = s.threshold; break;
         case OPT_COMPRESSION: strcpy ((char *)value, s.compression); break;
         default: return SANE_STATUS_INVAL;
         }
      return SANE_STATUS_GOOD;
      }

   if (action != SANE_ACTION_SET_VALUE)
      return SANE_STATUS_UNSUPPORTED;
   if (s.transferring)
      return SANE_STATUS_DEVICE_BUSY;

   /* a string must be one of the choices */
   if (s.opt [option].type == SANE_TYPE_STRING)
      {
      const SANE_String_Const *list = s.opt [option].constraint.string_list;
      const char *str = (const char *)value;
      bool ok = false;
      for (; *list; list++)
         if (!strcmp (*list, str))
            ok = true;
      if (!ok)
         return SANE_STATUS_INVAL;
      }

   /* clamp numbers to their range */
   SANE_Word word = 0;
   if (s.opt [option].type == SANE_TYPE_INT
       || s.opt [option].type == SANE_TYPE_FIXED)
      {
      word = *(SANE_Word *)value;
      if (s.opt [option].constraint_type == SANE_CONSTRAINT_RANGE)
         {
         const SANE_Range *r = s.opt [option].constraint.range;
         if (word < r->min)
            word = r->min;
         if (word > r->max)
            word = r->max;
         }
      else if (s.opt [option].constraint_type == SANE_CONSTRAINT_WORD_LIST)
         {
         const SANE_Word *list = s.opt [option].constraint.word_list;
         SANE_Word best = list [1];
         for (SANE_Word i = 1; i <= list [0]; i++)
            if (abs (list [i] - word) < abs (best - word))
               best = list [i];
         word = best;
         }
      if (word != *(SANE_Word *)value && info)
         *info |= SANE_INFO_INEXACT;
      }

   switch (option)
      {
      case OPT_SOURCE:
         strcpy (s.source, (const char *)value);
         break;
      case OPT_MODE:
         strcpy (s.mode, (const char *)value);
         if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
         break;
      case OPT_RESOLUTION:
         s.resolution = word;
         if (info)
            *info |= SANE_INFO_RELOAD_PARAMS;
         break;
      case OPT_TL_X: s.tl_x = word; break;
      case OPT_TL_Y: s.tl_y = word; break;
      case OPT_BR_X: s.br_x = word; break;
      case OPT_BR_Y: s.br_y = word; break;
      case OPT_PAGE_WIDTH:
         /* the scan window follows the page, as the fujitsu backend does */
         s.page_width = word;
         if (s.br_x > word || s.br_x == s.page_x_range.max)
            s.br_x = word;
         if (info)
            *info |= SANE_INFO_RELOAD_OPTIONS;
         break;
      case OPT_PAGE_HEIGHT:
         s.page_height = word;
         if (s.br_y > word || s.br_y == s.page_y_range.max)
            s.br_y = word;
         if (info)
            *info |= SANE_INFO_RELOAD_OPTIONS;
         break;
      case OPT_BRIGHTNESS: s.brightness = word; break;
      case OPT_CONTRAST: s.contrast = word; break;
      case OPT_THRESHOLD: s.threshold = word; break;
      case OPT_COMPRESSION:
         strcpy (s.compression, (const char *)value);
         break;
      default:
         return SANE_STATUS_INVAL;
      }
   if (option == OPT_TL_X || option == OPT_TL_Y || option == OPT_BR_X
       || option == OPT_BR_Y || option == OPT_PAGE_WIDTH
       || option == OPT_PAGE_HEIGHT)
      if (info)
         *info |= SANE_INFO_RELOAD_PARAMS;
   s.params_valid = false;
   estimate_params ();
   return SANE_STATUS_GOOD;
   }

SANE_Status sane_get_parameters (SANE_Handle, SANE_Parameters *params)
   {
   if (!scanner.open)
      return SANE_STATUS_INVAL;
   if (!scanner.params_valid)
      estimate_params ();
   *params = scanner.params;
   return SANE_STATUS_GOOD;
   }

SANE_Status sane_start (SANE_Handle)
   {
   Scanner &s = scanner;

   if (!s.open)
      return SANE_STATUS_INVAL;
   if (s.transferring)
      return SANE_STATUS_DEVICE_BUSY;

   /* the feeder ran out on the previous image: report that once, so the
      ADF loop in paperman ends, and start afresh next time */
   if (s.batch_done)
      {
      s.batch_done = false;
      return SANE_STATUS_NO_DOCS;
      }

   if (!s.enabled)
      {
      bool adf = strcmp (s.source, string_Flatbed) != 0;

      apply_options ();

      /* do not enable the source for an empty feeder: that shows a
         'load paper' dialog on some sources */
      CapValue v;
      if (adf && cap_get (&s.ds, CAP_PAPERDETECTABLE, MSG_GETCURRENT, v)
          && v.current && cap_get (&s.ds, CAP_FEEDERLOADED, MSG_GETCURRENT, v)
          && !v.current)
         {
         logf ("feeder is empty");
         return SANE_STATUS_NO_DOCS;
         }

      TW_USERINTERFACE ui;
      memset (&ui, 0, sizeof (ui));
      ui.ShowUI = FALSE;
      ui.ModalUI = FALSE;
      ui.hParent = dsm.hwnd;
      s.xfer_ready = false;
      s.close_request = false;
      TW_UINT16 rc = dsm_call (&s.ds, DG_CONTROL, DAT_USERINTERFACE,
                               MSG_ENABLEDS, &ui);
      if (rc != TWRC_SUCCESS)
         {
         TW_UINT16 cc = dsm_status (&s.ds);
         logf ("cannot enable the source: rc %u cc %u", rc, cc);
         if (cc == TWCC_NOMEDIA)
            return SANE_STATUS_NO_DOCS;
         if (cc == TWCC_PAPERJAM)
            return SANE_STATUS_JAMMED;
         if (cc == TWCC_PAPERDOUBLEFEED)
            return SANE_STATUS_JAMMED;
         return SANE_STATUS_IO_ERROR;
         }
      s.enabled = true;
      logf ("source enabled");
      }

   /* wait for the source to have the image: a big feeder takes a while
      to get going, and a flatbed a while to scan */
   if (!wait_for_transfer (120 * 1000))
      {
      bool closing = s.close_request;
      disable_source ();
      if (closing)
         {
         logf ("source asked to close: no more documents");
         return SANE_STATUS_NO_DOCS;
         }
      return SANE_STATUS_IO_ERROR;
      }
   s.xfer_ready = false;
   return begin_image ();
   }

SANE_Status sane_read (SANE_Handle, SANE_Byte *data, SANE_Int max_length,
                       SANE_Int *length)
   {
   Scanner &s = scanner;

   *length = 0;
   if (!s.open)
      return SANE_STATUS_INVAL;
   if (!s.transferring)
      return SANE_STATUS_CANCELLED;

   while (s.queue_pos >= queue.size ())
      {
      queue.clear ();
      s.queue_pos = 0;
      if (s.xfer_done)
         {
         end_image ();
         return SANE_STATUS_EOF;
         }
      SANE_Status status = read_strip ();
      if (status != SANE_STATUS_GOOD)
         {
         TW_PENDINGXFERS pending;
         memset (&pending, 0, sizeof (pending));
         dsm_call (&s.ds, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &pending);
         disable_source ();
         free_strip ();
         return status;
         }
      }

   size_t avail = queue.size () - s.queue_pos;
   size_t count = avail < (size_t)max_length ? avail : (size_t)max_length;
   memcpy (data, &queue [s.queue_pos], count);
   s.queue_pos += count;
   *length = (SANE_Int)count;
   return SANE_STATUS_GOOD;
   }

void sane_cancel (SANE_Handle)
   {
   Scanner &s = scanner;

   if (!s.open)
      return;
   /* paperman cancels at the end of a scan, so this ends the batch: a
      cancel between two images of a batch would otherwise be harmless */
   if (s.enabled)
      {
      TW_PENDINGXFERS pending;
      memset (&pending, 0, sizeof (pending));
      dsm_call (&s.ds, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &pending);
      logf ("cancelled");
      }
   disable_source ();
   free_strip ();
   s.batch_done = false;
   }

SANE_Status sane_set_io_mode (SANE_Handle, SANE_Bool non_blocking)
   {
   return non_blocking ? SANE_STATUS_UNSUPPORTED : SANE_STATUS_GOOD;
   }

SANE_Status sane_get_select_fd (SANE_Handle, SANE_Int *fd)
   {
   if (fd)
      *fd = -1;
   return SANE_STATUS_UNSUPPORTED;
   }

SANE_String_Const sane_strstatus (SANE_Status status)
   {
   switch (status)
      {
      case SANE_STATUS_GOOD: return "Success";
      case SANE_STATUS_UNSUPPORTED: return "Operation not supported";
      case SANE_STATUS_CANCELLED: return "Operation was cancelled";
      case SANE_STATUS_DEVICE_BUSY: return "Device busy";
      case SANE_STATUS_INVAL: return "Invalid argument";
      case SANE_STATUS_EOF: return "End of file reached";
      case SANE_STATUS_JAMMED: return "Document feeder jammed";
      case SANE_STATUS_NO_DOCS: return "Document feeder out of documents";
      case SANE_STATUS_COVER_OPEN: return "Scanner cover is open";
      case SANE_STATUS_IO_ERROR: return "Error during device I/O";
      case SANE_STATUS_NO_MEM: return "Out of memory";
      case SANE_STATUS_ACCESS_DENIED: return "Access to resource denied";
      }
   return "Unknown SANE status";
   }

}
