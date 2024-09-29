/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/
/* program to decode paperport .max files */
/* Simon Glass, 19nov01 */

/* picked up again xmas 2004 */

/* copyright 2005 Bluewater Systems Ltd, www.bluewatersys.com */



#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <QDebug>
#include <QFileInfo>
#include <QHash>

#include "errno.h"
#include "jpeglib.h"

#include "tiffio.h"
#define G3CODES
#include "myt4.h"
#include "mytif_fax3.h"

#include <unistd.h>

#include "sys/stat.h"

#include "desk.h"
#include "filemax.h"
#include "utils.h"


static FILE *debugf = 0;
static int debug_level = 0;

#define warning(x) do {if (debug_level >= 0) dprintf x; } while (0)
#define debug1(x) do {if (debug_level >= 1) dprintf x; } while (0)
#define debug2(x) do {if (debug_level >= 2) dprintf x; } while (0)
#define debug3(x) do {if (debug_level >= 3) dprintf x; } while (0)


//#define FAX3_DEBUG

// this must be a multiple of 8 at the moment due to assumptions in scale_2bpp
// previews are scaled 1:24
#define PREVIEW_SCALE 24


// JPEG quality hardwired for now
#define JPEG_QUALITY 75




/** current version of max file - note that is 'our' version; legacy max files
don't have this value */
#define MAX_VERSION 1


/*
picked up again 8/12/04

Notes removed
*/

/*
   max format:

      0   w   signature 0x46476956
      ...
     5c   w   page_count
     ...
     6c   w   chunk_count
     ...
     88   w   chunk0_start
     ..
     a4   w   bermuda_pos
     a8   w   tunguska_pos
     ac   w   annot_pos
     b0   w   ?
     b4   w   ?
     b8   w   trail_pos


   bermuda chunk format:
      0   w   ?
     20   h   page_count
     22   h   ?
     24   w   ?
     28   h   ?
     2a   h   ?
     2c   w   ?
     30   h   ?
     32   h   ?
     34   h   ?
     36   c   page_info records, 1 per page

   page_info record format:
      0   h   chunkid
      2   h   ?
      4   w   roswell_pos
      8   w   ?

   roswell chunk format:
      0   h   signature 0x5a56
      2   h   size 0x1a0
      ..
     42   w   image_pos
     ..
     88   w   noti1_pos
     8c   w   title pos
     ..
     ce   w   noti2_pos
     d2   w   text_pos

   chunk format:

      0   h   'VZ' 0x5a56
      2   w   size
      6   h   chunkid
      8   h   used
      a   h   textflag (0=name, 3=not)
      c   h   type (0=image, 1=not image)
      e   h   ? 0
      10  h   ? 0
      12  h   ? 0
      14  h   ? 0
      16  h   ? 0
      18  h   ? 0
      1a  h   flags (CHUNKF_...)
      1c  h   flags (titletype) 0x80 = page title, 0x00 = stack title
      1e  h   ? 0
      20  h   ? 0x88
      22  h   ? 0x88
      24  h   ? 0
      26  h   ? 1
      28  h   ? 0

annot format:
      22  w   author position
      26  w   author length (inc \0)
      2a  w   title position
      2e  w   title length
      32  w   notes position?
      36  w   notes length
      3a  w   other position
      3e  w   other length

for images:
      2a  h   xsize
      2c  h   ysize
      2e  h   dpi_x
      30  h   dpi_y
      32  h   ? 0
      34  h   bpp
      36  h   image_channels
      38  h   preview_xsize
      3a  h   preview_ysize
      3c  h   ? 0x21
      3e  h   ? 2
      40  h   ? 0x53d
      42  h   part count
      44  -   part0 starts here

image tileinfo part:
       0  h   code
       4  h   tile_size.x in bytes?
       6  h   tile_size.y
       8  h   tile_extent.x
       a  h   tile_extent.y
       c  h   tile_count
       e  a   tile info records, 10 bytes each

tileinfo record:
       0  h   ? 2
       2  w   size of tile data + 4
       6  h   ? 0x8000
       8  h   ? 0

image tiledata part:
       0  h   tile number (0..tile_count-1)
       2  h   datatype (normally 0x43)
       4  ?   the data (always a multiple of 4 bytes?)
       ?      repeats for each tile


**************************************************************************
older format:

      0   w   signature 0x41476956
      ...
     24   w   page_count
     ...
     32   w   chunk_count
     ...
     ..
     58   w   bermuda_pos
     -    w   tunguska_pos
     -    w   annot_pos
     -    w   ?
     -    w   ?
     7c   w   trail_pos


   bermuda chunk format:
      0   w   ?
     20   h   ? - not page_count
     22   h   ?
     24   w   ?
     28   h   ?
     2a   h   ?
     2c   h
     2e   h   xsize
     30   h   ysize
     32   h   dpi
     34   ?   page_info records, 1 per page


for images:
      26  h   xsize
      28  h   ysize
      2a  h   dpi_x
      2c  h   dpi_y
      2e  h   bpp
      30  h   image_channels
      3e  h   preview_xsize
      40  h   preview_ysize
      42  h   part0 starts here (preview?)

   page_info record format:
      0   h   chunkid
      2   w   image_pos

   */


enum
   {
   POS_chunk_header_size   = 0x20,  // size of chunk header
   POS_chunk_step        = 0x20,  // chunk size is a multiple of this

#define ALIGN_CHUNK(x)  (((x) + POS_chunk_step) & ~(POS_chunk_step - 1))

   POSn_version          = 0x20, // file version number
   POS_chunk0_start      = 0x88,
   POS_page_count        = 0x5c,
   POS_chunk_count       = 0x6c,
   POS_bermuda           = 0xa4,
   POS_tunguska          = 0xa8,
   POS_annot             = 0xac,
   POS_trail             = 0xb8,

   /** this is the envelope information, including sender and recipient */
   POS_envelope          = 0xbc,

   // version A positions
   POSva_page_count      = 0x24,
   POSva_chunk_count     = 0x32,
   POSva_chunk0_start    = 0x58,
   POSva_bermuda         = 0x58,
   POSva_trail           = 0x7c,

//   POS_start_pagenum     = 0xfe,

   /* within chunks */
   POS_image_size_x      = 0x2a,
   POS_image_size_y      = 0x2c,
   POS_image_dpi_x       = 0x2e,
   POS_image_dpi_y       = 0x30,
   POS_image_bits        = 0x34,
   POS_image_channels    = 0x36,
   POS_image_preview_x   = 0x38,
   POS_image_preview_y   = 0x3a,

   POSva_image_size_x      = 0x26,
   POSva_image_size_y      = 0x28,
   POSva_image_dpi_x       = 0x2a,
   POSva_image_dpi_y       = 0x2c,
   POSva_image_bits        = 0x2e,
   POSva_image_channels    = 0x30,
   POSva_image_preview_x   = 0x3e,
   POSva_image_preview_y   = 0x40,

   // within envelope chunk
   POS_env_start         = 0x20,  // start of envelope records within chunk
   POS_env_count         = 0x24,
   // zeroes
   POS_env_string0       = 0x30,
   // envelope is simply a n integer count then a list of \0 terminated strings

   // within annot chunk
   POS_annot_start       = 0x22,  // start of annot records within chunk
   POS_annot_pos         = 0,
   POS_annot_size        = 4,
   POS_annot_offset      = 8,

   POS_text_start        = 0x20,  // start of text string within chunk

   POS_part_count        = 0x42,
   POS_part0             = 0x44, // start of (pos, size) info for each part

   // within bermuda block
   POS_bermuda_pagecount = 0x20,
   POS_bermuda_pageinfo  = 0x36,

   POSva_bermuda_pageinfo  = 0x34,

   // within roswell block
   POS_roswell_image     = 0x42,
   POS_roswell_noti1     = 0x88,
   POS_roswell_title     = 0x8c,
   POS_roswell_noti2     = 0xce,
   POS_roswell_text      = 0xd2,

   POSn_roswell_timestamp = 0x20,   // page timestamp

   /* chunk types */
   CT_image         = 0,                                    /* image chunk */
   CT_text,                                            /* OCRed text chunk */
   CT_notimage,        // not an image?
   CT_unknown,         // completely unknown
   CT_title,           // page title
   CT_roswell,         // roswell (page info)
   CT_bermuda,         // bermuda chunk
   CT_annot,           // annotation chunk
   CT_env,             // envelope chunk

   /* part types */
   PT_colourmap = 0,
   PT_preview,
   PT_notes,
   PT_tileinfo,
   PT_tiledata,

   /* flags to determine which parts of a page are decoded:
         bit 0 = decode preview, bit 1 = decode full picture */
   DECODE_none,
   DECODE_preview,
   DECODE_full,
   DECODE_all,

   TITLE_max_size = 128,

   // allocate space for new chunks in lots of this value
   CHUNK_alloc_step   = 32,
   PAGE_alloc_step    = 8
   };


typedef unsigned short ushort;

extern "C" {
#include "ztab.h"
#include "btab.h"
};

#include "epeglite.h"
#include "err.h"
#include "mem.h"


enum
   {
   CHUNKF_image   = 0x1000,    // chunk contains an image
   CHUNKF_text    = 0x1001,    // chunk contains text
   CHUNKF_roswell = 0x4000,    // chunk contains roswell data
   CHUNKF_bermuda = 0x8000,    // chunk contains bermuda data
   CHUNKF_annot   = 0x8002,    // chunk contains annot data
   CHUNKF_env     = 0x8004     // chunk contains envelope data
   };


#define GET_DATA(pos) max_check_scache (pos)

typedef struct decode_info
   {
   int version_a;  // true if an old version A file
   int used;      // number of bits used in 'word'
   unsigned word;  // current data word being processed
   int line_bytes;  // number of bytes in a line
   int stride;    // distance in bytes between pixels on consecutive lines
   int y;         // current ypos
   int width;     // image width in pixels
   byte *outptr;  // output data pointer
   byte *inptr;   // current input pointer
   cpoint image_size; // image size
   short *table_data;      // table data
   short *table;      // current table line
   short *table_prev;  // old table line
   int table_size;     // table size (in ushorts)

   ushort *ztab;       // a table
   byte *atab;         // another table
   unsigned *tab7;     // 7 bit lookup tables
   unsigned *tab_first, *tab_second; // 13 bit huffman lookup tables

   unsigned *btab;   // b table

   debug_info *debug; // debug info
   } decode_info;


// info used when encoding tiles
typedef struct encode_info
   {
   byte *buff;    // output data buffer
   int size;      // current size of data buffer
   int tile_line_bytes;  //!< byte width of this tile
   } encode_info;



static debug_info *no_debug (void)
   {
   static debug_info sdebug, *debug = &sdebug;

   debug->level = 0;
   debug->max_steps = INT_MAX;
   debug->start_tile = 0;
   debug->num_tiles = INT_MAX;
   debug->logf = stdout;
   debug->compf = NULL;
   return debug;
   }


Filemax::Filemax (const QString &dir, const QString &filename, Desk *desk)
   : File (dir, filename, desk, Type_max)
   {
   _version = -1;
   _chunk0_start = 0;
   _size = 0;
   _signature = -1;
   _debug = no_debug ();

   _fin = NULL;
   max_clear_cache (_cache);
   max_clear_cache (_scache);
   _bermuda = _tunguska = _annot = _trail = _b0 = _b4 = 0;
   _envelope = 0;
   _chunkid_next = 0;
   _hdr_updated = false;
   _version_a = false;
   _all_chunks_loaded = false;
   }


Filemax::~Filemax ()
   {
   max_free ();
   }


/** ensure that the page has a title loaded, if there is one */
err_info *Filemax::ensure_titlestr (int, page_info &page)
   {
   chunk_info *chunk;
   bool has_chars = false;
   char title [TITLE_max_size];
   int ch, j;
   bool temp = false;
   err_info *e = NULL;

   if (page.title_loaded)
      return NULL;
   page.title_loaded = page.title_saved = true;
   *title = '\0';
   if (page.title)
      {
      // find the chunk, supporting temporary loading
      CALL (chunk_find (page.title, chunk, &temp));
      if (page.title && chunk && chunk->type == CT_title)
         {
         for (j = 0; j < TITLE_max_size - 1; j++)
            {
            CALLB (getbytee (page.title + 0x20 + j, &ch));
            title [j] = ch;
            if (!title [j] || title [j] < ' ')
               break;
            if (title [j] > ' ')
               has_chars = true;
            }
         if (!has_chars)   // ignore a title with only spaces
            j = 0;
         title [j] = '\0';
         }
      }

   // if temporarily loaded, free it now
   if (temp)
      {
      chunk_free (*chunk);
      delete chunk;
      }

   page.titlestr = title;

   return e;
   }


err_info *Filemax::find_page (int pagenum, page_info* &page)
   {
   if (pagenum < 0 || pagenum >= _pages.size ())
      return err_make (ERRFN, ERR_page_number_out_of_range2, pagenum, _pages.size () - 1);
   page = &_pages [pagenum];
   return NULL;
   }


static void dprintf (const char *fmt, ...)
   {
   va_list ptr;
   char str [250];

   va_start (ptr, fmt);
   vsprintf (str, fmt, ptr);
   va_end (ptr);

   if (debugf)
      fprintf (debugf, "%s", str);
   }


static const char *chunk_namestr (int type)
   {
   static char str [8];

   switch (type)
      {
      case CT_image : return "Image";
      case CT_text  : return "Text ";
      case CT_notimage : return "NotI ";
      case CT_title    : return "Title";
      case CT_roswell  : return "Roswell";
      case CT_bermuda  : return "Bermuda";
      case CT_annot    : return "Annot";
      case CT_env      : return "Envelope";
      default :
         sprintf (str, "t%-4x:", type);
         return str;
         break;
      }
   }


static const char *parttype_str [] =
   {
   "colourmap",
   "preview",
   "notes",
   "tileinfo",
   "tiledata"
   };


static int make_err (const char *func, int err)
   {
   printf ("err: %s %d\n", func, err);
   return err;
   }


#define ERR(err) make_err (__PRETTY_FUNCTION__, err)


void Filemax::debug_page (page_info *page)
   {
   printf ("id %05x roswell %05x image %05x title %05x\n",
      page->chunkid, page->roswell, page->image, page->title);
   }


void Filemax::debug_pages (void)
   {
   int i;

   for (i = 0; i < _pages.size (); i++)
      {
      page_info &page = _pages [i];
      printf ("Page %d: ", i + 1);
      debug_page (&page);
      }
   }


err_info *Filemax::max_checkerr (void)
   {
   err_info *err;

   err = _err;
   _err = NULL;
   return err;
   }


err_info *Filemax::getworde (int pos, int *wordp)
   {
   int word;
   byte *p;

   if (pos < 0 || pos > _size - 4)
      return err_make (ERRFN, ERR_file_position_out_of_range3, pos, 0,
               _size);
   p = GET_DATA (pos);
   if (p)
      word = *p | (p [1] << 8) | (p [2] << 16) | ((unsigned)p [3] << 24);
   else
      {
      printf ("   - getworde %x\n", pos);
      fseek (_fin, pos, SEEK_SET);
      word = fgetc (_fin);
      if (word == EOF)
         printf ("eof error\n");
      word |= fgetc (_fin) << 8;
      word |= fgetc (_fin) << 16;
      word |= (unsigned)fgetc (_fin) << 24;
      }
   *wordp = word;
   return NULL;
   }


int Filemax::getword (int pos)
   {
   int value;

   if (!_err)
      _err = getworde (pos, &value);
   return _err ? -1 : value;
   }


err_info *Filemax::gethwe (int pos, int *valuep)
   {
   int word;
   byte *p;

   if (pos < 0 || pos > _size - 2)
      return err_make (ERRFN, ERR_file_position_out_of_range3, pos, 0,
               _size);
   p = GET_DATA (pos);
   if (p)
      word = *p | (p [1] << 8);
   else
      {
      fseek (_fin, pos, SEEK_SET);
      word = fgetc (_fin);
      word |= fgetc (_fin) << 8;
      }
   *valuep = word;
   return NULL;
   }


int Filemax::gethw (int pos)
   {
   int value;

   if (!_err)
      _err = gethwe (pos, &value);
   return _err ? -1 : value;
   }


err_info *Filemax::getbytee (int pos, int *bytep)
   {
   byte *data;

   if (pos < 0 || pos > _size - 1)
      return err_make (ERRFN, ERR_file_position_out_of_range3, pos, 0,
               _size);
   data = GET_DATA (pos);
   if (data)
      *bytep = *data;
   else
      {
      fseek (_fin, pos, SEEK_SET);
      *bytep = fgetc (_fin);
      }
   return NULL;
   }


err_info *Filemax::merr_make (const char *func_name, int errnum, ...)
   {
   va_list ptr;
   err_info *e;
   char str [260];

   va_start (ptr, errnum);
   e = err_vmake (func_name, errnum, ptr);
   va_end (ptr);
   sprintf (str, "%s: %s", _filename.toLatin1 ().constData(), e->errstr);
   strcpy (e->errstr, str);
   return e;
   }


err_info *Filemax::max_cache_data (cache_info &cache, int pos,
      int size, int min, byte **datap)
   {
   int n;

   cache.buff.resize (size);

   // read the data
   fseek (_fin, pos, SEEK_SET);
   n = fread (cache.buff.data (), 1, size, _fin);
   if (n != size)
      {
      if (n >= min)
         size = n;
      else
         return err_make (ERRFN, ERR_file_position_out_of_range3, pos, 0,
               _size);
      }

   if (datap)
      *datap = (byte *)cache.buff.data ();
   cache.pos = pos;
   return NULL;
   }


byte *Filemax::max_check_scache (int pos)
   {
   byte *ptr;
   err_info *err = NULL;

#define CACHE_4K_SIZE  4096
   // if the cache has the wrong data, load it
   if (pos < _scache.pos || (pos + 4 > _scache.pos + _scache.buff.size ()))
      err = max_cache_data (_scache, pos, CACHE_4K_SIZE, 4, NULL);

   // return the data
   if (err)
      {
      _err = err;
      return NULL;
      }
   ptr = (byte *)_scache.buff.data () + (pos - _scache.pos);
   return ptr;
   }


err_info *Filemax::max_read_data (int pos, byte *buf, int size)
   {
   int count;

   // read the data
   fseek (_fin, pos, SEEK_SET);
   count = fread (buf, 1, size, _fin);
   return count == size ? NULL : merr_make (ERRFN, ERR_failed_to_read_bytes1, size);
   }


err_info *Filemax::max_write_data (int pos, byte *data, int size)
   {
   int count;

   // write the data
   fseek (_fin, pos, SEEK_SET);
   count = fwrite (data, 1, size, _fin);
   max_clear_cache (_scache, pos, size);
   max_clear_cache (_cache, pos, size);

   return count == size ? NULL : merr_make (ERRFN, ERR_failed_to_write_bytes1, size);
   }


void Filemax::max_clear_cache (cache_info &cache, int pos, int size)
   {
   if (size == -1
      || (pos >= cache.pos && pos < cache.pos + cache.buff.size ())
      || (pos + size >= cache.pos && pos + size < cache.pos + cache.buff.size ()))
      {
      cache.buff.clear ();
      cache.pos = 0;
      }
   }


err_info *Filemax::read_part (part_info &part, int pos)
   {
   CALL (getworde (pos, &part.start));
   CALL (getworde (pos + 4, &part.size));
   part.raw_size = part.size;
   return NULL;
   }


unsigned Filemax::getbits (decode_info &decode, int count)
   {
   unsigned data, mask;

   assert (decode.used + count < 0x20);
   mask = 0xffffffff >> decode.used;
   data = decode.word & mask;
   data = data >> (0x20 - decode.used - count);
   debug3 (("word=%x, used=%d, mask=%08x, count=%d, data=%x, len=%d\n", decode.word, decode.used,
           mask, count, data, count));
   decode.used += count;
   return data;
   }


int Filemax::update_table (decode_info &decode, byte *inptr, int width,
                         short *table)
   {
   short *orig_table = table;
   int x, count;
   int colour;
   int byte;

   colour = 0;
   x = 0;
//   debug3 (("update_table:\n"));
   for (count = (width + 7) / 8; count != 0; count--)
      {
      byte = (*inptr++ ^ colour) & 0xff;
//      debug3 (("byte=%x: ", byte));
      while (byte)
         {
         *table++ = x + decode.ztab [byte * 2];
         colour = ~colour;
         byte = decode.atab [byte * 4];
//         debug3 (("%x ", byte));
         assert (table - orig_table <= width);
         }
//      debug3 (("\n"));
      x += 8;
      }

   table [0] = width;
   table [1] = width;
   table [2] = width;
   return table - orig_table + 3;
   }


void Filemax::do_uncomp (decode_info &decode, int bits_used)
   {
   // move over byte used
   decode.inptr += (bits_used + 7) / 8;

   // copy these bytes
//   debug3 (("memcpy %d bytes\n", decode.line_bytes));
   memcpy (decode.outptr, decode.inptr, decode.line_bytes);

   // no bits now
   decode.used = 0;

   // update the table
   update_table (decode, decode.inptr, decode.width,
                 decode.table + 1);

   // advance input pointer
   decode.inptr += decode.line_bytes;
   }


err_info *Filemax::decomp (decode_info &decode, byte *inptr, int bits_used, int width,
                   short *table_prev, short *table, int *bits_usedp)
   {
   int colour;  // 0 == white, 1 == black
   int x;
   unsigned bits, *tab;
   int bits_left;
   byte *orig_inptr = inptr;
   int offset;
   int entry, code, count;
   int pass;
   int len;

   table_prev++;
   colour = 0;
   x = -1;
   *table++ = -1;
   bits = (*inptr << 8) | inptr [1];
   inptr += 2;
   bits_left = 0x10 - bits_used;
   debug3 (("decomp: starting with word=0x%x, bits_left=0x%x, width=0x%x\n", bits, bits_left, width));

   while (x < width)
      {
      while (*table_prev <= x)
         table_prev += 2;

      debug3 (("1: %d, bits_left=0x%x, ", x, bits_left));

      // get more bits if required
      if (bits_left <= 0x10)
         {
         bits = (bits << 8) | *inptr++;
         bits = (bits << 8) | *inptr++;
         bits_left += 0x10;
         }

      // decode next 7 bits and lookup
      entry = decode.tab7 [(bits >> (bits_left - 7)) & 0x7f];
      len = entry & 0xffff;
      debug3 (("word=%08x, 7bits=%x, data=%x, entry=%x, now %x",
               bits, (bits >> (bits_left - 7)) & 0x7f,
               (bits >> (bits_left - len)) & ((1 << len) - 1), entry, bits_left));
      bits_left -= len;
      if (!len)
         return err_make (ERRFN, ERR_decompression_invalid_data);
      code = (entry >> 16) - 3;
      switch (code)
         {
         case 4 :
            debug3 (("->h\n"));
            for (pass = 2; pass != 0; pass--)
               {
               do
                  {
                  // get 16 more bits if needed
                  if (bits_left < 0x10)
                     {
                     bits = (bits << 8) | *inptr++;
                     bits = (bits << 8) | *inptr++;
                     bits_left += 0x10;
                     }

                  // decode next 13 bits and lookup in either white or black table
                  tab = colour == 0 ? decode.tab_first : decode.tab_second;
                  entry = tab [(bits >> (bits_left - 13)) & 0x1fff];
                  len = entry & 0xffff;

                  debug3 (("  2%c: %d, word=%08x, 13bits=%x, data=%x, entry=0x%x, left=0x%x, ->%d\n",
                           colour ? 'b' : 'w',
                           x, bits, (bits >> (bits_left - 13)) & 0x1fff,
                           (bits >> (bits_left - len)) & ((1 << len) - 1), entry, bits_left,
                           x + (entry >> 16)));
                  bits_left -= len;
                  count = entry >> 16;
                  x += count;
                  } while (count > 0x3f);

               *table++ = x;
               colour = !colour;
               }
            break;

         case 5 :
            x = table_prev [1];
            debug3 (("->p %d\n", x));
            break;

         default :
            x = *table_prev + code;
            *table++ = x;
            debug3 (("->v %d\n", x));
            if (x < width)
               {
               table_prev--;
               colour = !colour;
               }
            break;
         }
      }

   table [-1] = table [0] = table [1] = width;

   offset = inptr - orig_inptr;
   debug3 (("inptr advanced 0x%x, bits_used=0x%x, bits_left=0x%x", offset, bits_used, bits_left));
   offset = offset * 8 - bits_used;
   offset -= bits_left;
   debug3 ((", ret=0x%x\n", offset));
   *bits_usedp = offset;
   return NULL;
   }


void Filemax::output (short *table, byte *destptr, int width)
   {
   short *tab = table;
   int x;
   int entry;
   byte *out;

   if (*tab == -1)
      return;
   assert (*tab != -1);

   // get starting position
   for (x = *tab++; x < width; x = *tab++)
      {
      // get position
      out = (byte *)(destptr + x / 8);

      // set bits from x...
      *out |= 0xff >> (x & 7);
      out++;
      x += 8 - (x & 7);

      // ...through to the table entry
      entry = *tab++;
      assert (entry < 4000);  // reasonable limit?
      while (x < entry)
         {
         *out++ = 0xff;
         x += 8;
         }

      // fix up last byte if necessary
      if (x != entry)
         out [-1] &= 0xff << (8 - (entry & 7));
      }
   }


err_info *Filemax::do_compressed (decode_info &decode)
   {
   int bits_used;

   CALL (decomp (decode, decode.inptr, decode.used,
                    decode.width, decode.table_prev, decode.table, &bits_used));
   bits_used += decode.used;
   debug3 (("bits_used = 0x%x\n", bits_used));
   decode.inptr += (bits_used / 8) & 0xfe;
   decode.used = bits_used & 0xf;
   output (decode.table + 1, decode.outptr, decode.width);
   return NULL;
   }


void Filemax::free_tables (decode_info &decode)
   {
   if (decode.table_data)
      free (decode.table_data);
   if (decode.tab7)
      free (decode.tab7);
   if (decode.tab_first)
      free (decode.tab_first);
   if (decode.tab_second)
      free (decode.tab_second);
   }


void Filemax::setup_huffman_13 (unsigned *src, unsigned *src2, unsigned *tab)
   {
   int count;
   int low, pos;

   for (count = 0; count < 0x40; count++, src++)
      {
      low = *src & 0xffff;
      pos = *src >> 16;
      pos <<= 13 - low;
      tab [pos] = (count << 16) | low;
      }

   for (count = 1; count < 0x28; count++, src2++)
      {
      low = *src2 & 0xffff;
      pos = *src2 >> 16;
      pos <<= (13 - low) & 0xff;
      tab [pos] = low | (count << 22);
      }

   for (count = 0x1fff; count != 0; count--)
      {
      tab++;
      if (!(*tab & 0xffff))
         *tab = tab [-1];
      }
   }


void Filemax::setup_huffman_7 (unsigned *in, unsigned *out)
   {
   int count;
   int low, pos;

   for (count = 0; count < 9; count++, in++)
      {
      low = *in & 0xffff;
      pos = *in >> 16;
      pos <<= 7 - low;
      out [pos] = low | (count << 16);
      }

   for (count = 127; count >= 0; count--)
      {
      out++;
      if (!(*out & 0xffff))
         *out = out [-1];
      }
   }


err_info *Filemax::setup_tables (decode_info &decode)
   {
   // allow for the fact that tables may already be set up
   if (!decode.tab_first)
      {
      CALL (mem_allocz (CV &decode.tab_first, 0x8000, "setup_tables1"));
      CALL (mem_allocz (CV &decode.tab_second, 0x8000, "setup_tables2"));

      setup_huffman_13 (decode.btab, decode.btab + 0x80,
                        decode.tab_first);
      setup_huffman_13 (decode.btab + 0x40,
                 decode.btab + 0xa8, decode.tab_second);
      setup_huffman_7 (decode.btab + 0x80 + 0x28 + 0x28, decode.tab7);
      }

   return NULL;
   }


/* decode some data consisting of single pixel positions in the line */

void Filemax::do_single (decode_info &decode)
   {
   short *tab = decode.table;
   int ch;
   byte *ptr;

   *tab++ = -1;
   for (ptr = decode.inptr + (decode.used + 7) / 8, ch = 0;
        ch < decode.width;
        ptr += 2)
      {
      ch = (ptr [0] << 8) | ptr [1];
      debug3 (("ch=%d (0x%x), width=%x\n", ch, ch, decode.width));
      *tab++ = ch;
      }

   tab [0] = tab [1] = decode.width;
   output (decode.table + 1, decode.outptr, decode.width);
   decode.inptr = ptr;
   decode.used = 0;
   }


err_info *Filemax::decode_init (decode_info &decode, chunk_info &chunk,
                        byte *data, byte *image, int stride, cpoint &tile_size)
   {
   int size;
   err_info *err;

   decode.debug = _debug;
   debug_level = decode.debug->level;
   debugf = decode.debug->logf;

   decode.version_a = _version_a;
   decode.inptr = data;
   decode.stride = stride;
   decode.outptr = image;

   if (!_version_a)
      assert (chunk.parts.size () == 5);

   decode.image_size = tile_size;
   decode.width = tile_size.x;
   while (decode.width & 3)
      decode.width++;
   decode.line_bytes = (tile_size.x + 7) / 8;

   decode.atab = atab;
   decode.ztab = (ushort *)(atab + 2);

   // alloc memory for tables
   if (decode.table_data)
      free (decode.table_data);
   decode.table_size = 4 + decode.width;
   size = 2 * sizeof (ushort) * decode.table_size;
   CALL (mem_allocz (CV &decode.table_data, size, "decode_init1"));
   decode.table_prev = decode.table_data;
   decode.table = decode.table_data + decode.table_size;

   decode.table [0] = -1;
   decode.table_prev [0] = -1;
   decode.table_prev [1] = decode.table_prev [2] = decode.table_prev [3]
      = decode.width;

   if (!decode.tab7)
      {
      decode.tab7 = (unsigned *)malloc (0x304);
      assert (decode.tab7);
      memset ((void *)decode.tab7, '\0', 0x304);
      }

   decode.btab = (unsigned *)btab;

   // setup tables
   err = setup_tables (decode);
   if (err)
      {
      free_tables (decode);
      return err;
      }

/*
   printf ("tile is %d x %d\n", chunk->tile_size.x,
       chunk->tile_size.y);
*/
   // now decode */
   decode.used = 0;
   decode.y = 0;

   // all ok
   return NULL;
   }


err_info *Filemax::decode_compressed_tile (decode_info &decode, int size)
   {
   unsigned data;
   short *temp;
   int used, diff, type;
   byte *inptr = decode.inptr;
   debug_info *debug = decode.debug;

   debug->step = 0;
   do
      {
      debug3 (("%d (", decode.y));

      if (decode.used >= 0x10)
         {
         decode.inptr += 2;
         decode.used -= 0x10;
         debug3 (("+2"));
         }

      // get some bits
      decode.word = (decode.inptr [0] << 24)
         | (decode.inptr [1] << 16)
         | (decode.inptr [2] << 8)
         | decode.inptr [3];
      debug3 (("word=%08x): ", decode.word));

      // get 2 bits
      if (decode.version_a)
         type = 2;
      else
         type = getbits (decode, 2);

      switch (type)
         {
         case 0 : // uncompressed
            debug3 (("uncomp\n"));
            do_uncomp (decode, decode.used);
            break;

         case 1 : // single pixels
            debug3 (("single\n"));
            do_single (decode);
            debug3 (("single done\n"));
            break;

         case 2 : // compressed
            debug3 (("comp\n"));
            CALL (do_compressed (decode));
            break;

         case 3 : // blank lines
            data = getbits (decode, 6) - 1;
            debug3 (("blank %d\n", data + 1));
            if (decode.used >= 0x10)
               {
               decode.used -= 0x10;
               decode.inptr += 2;
               }
            decode.table [1] = decode.table [2] = decode.table [3]
                     = decode.width;
            decode.y += data;
            decode.outptr += decode.stride * data;
            break;

         default :
            printf ("*** "); // error
            decode.y = decode.image_size.y;
            break;
         }

      // decode.table has a list of level changes (white->black or b->w)
      if (debug->compf)
         {
         int i;

         fprintf (debug->compf, "line %d %d\n", decode.y, type);
         for (i = 1; decode.table [i] != decode.width; i++)
            fprintf (debug->compf, "%d ", decode.table [i]);
         fprintf (debug->compf, "\n");
         }

      decode.y++;
      decode.outptr += decode.stride;

      // swap table & old_table
      temp = decode.table;
      decode.table = decode.table_prev;
      decode.table_prev = temp;

      // for debugging, we can stop after a certain number of steps
      debug->step++;
      } while (decode.y < decode.image_size.y
               && debug->step < debug->max_steps);
   used = decode.inptr - inptr;
   diff = size - used;
   debug3 (("used 0x%lx\n", decode.inptr - inptr));
   if (debug->step != debug->max_steps)
      if (diff < -4 || diff > 1)
         warning (("   ** decode diff %d: used %d bytes, should be %d\n", diff, used, size));
   return NULL;
   }


err_info *Filemax::decode_tileinfo (chunk_info &chunk)
   {
   int pos, i, total;
   tile_info *tile;

   if (_version_a)
      {
      // doesn't have chunks?
      pos = chunk.start;
      chunk.code = 0;
      chunk.tile_size = chunk.image_size;
      chunk.tile_extent.x = 1;
      chunk.tile_extent.y = 1;
      chunk.tile_count = 1;
      chunk.tile = (tile_info *)malloc (sizeof (tile_info) * chunk.tile_count);
      chunk.tile [0].size = getword (pos + 0x20);
      return NULL;
      }
   part_info &part = chunk.parts [PT_tileinfo];
   pos = chunk.start + 0x20 + part.start;
   chunk.code = gethw (pos);
   chunk.tile_size.x = gethw (pos + 4);
   if (chunk.bits == 1)
      chunk.tile_size.x *= 8;
   chunk.tile_size.y = gethw (pos + 6);
   chunk.tile_extent.x = gethw (pos + 8);
   chunk.tile_extent.y = gethw (pos + 10);
   chunk.tile_count = gethw (pos + 12);

   // see if any of the above yielded an error
   CALL (max_checkerr ());

   CALL (mem_allocz (CV &chunk.tile, sizeof (tile_info) * chunk.tile_count,
               "decode_tileinfo"));
   pos += 14;

   debug1 (("Image is %d x %d\n", chunk.image_size.x, chunk.image_size.y));
   debug1 (("Image has %d x %d = %d tiles\n",
            chunk.tile_extent.x, chunk.tile_extent.y, chunk.tile_count));

   debug1 (("Chunkid %d: tile %d x %d\n", chunk.chunkid,
         chunk.tile_size.x, chunk.tile_size.y));

   for (i = total = 0, tile = chunk.tile; i < chunk.tile_count;
         i++, pos += 10, tile++)
      {
      CALL (gethwe (pos, &tile->code));
      CALL (getworde (pos + 2, &tile->size));
      total += tile->size;
      CALL (getworde (pos + 6, &tile->other));
      debug3 (("tile %d: code 0x%x, other %d (0x%x), %d bytes\n", i,
         tile->code, tile->other, tile->other, tile->size));
      }
   debug3 (("total tile data size 0x%x\n", total));
   return NULL;
   }

err_info *Filemax::decode_tile (chunk_info &chunk,
         decode_info &decode, int code, int pos, int size, byte *ptr,
         cpoint &tile_size)
   {
   byte *data;
   err_info *e;

   CALL (max_cache_data (_cache, pos, size, size, &data));
   debug3 (("decode_tile: source data extends from %p to %p\n", data,
         data + size));
/*
   fprintf (debugf, "bytes = 0x%x\n", size);
   dump (debugf, max, pos, size);
*/
   switch (code)
      {
      case 0x0043 :  // compressed format
         switch (chunk.bits)
            {
            case 1 :
               CALL (decode_init (decode, chunk, data, ptr,
                                    chunk.line_bytes, tile_size));
               e = decode_compressed_tile (decode, size - 4);
               if (e)
                  printf ("warning: %s\n", e->errstr);
               break;

            case 8 :
            case 24 :
               // decode raw JPEG file here
               // need to restrict output to width tile_size->x
               jpeg_decode (data, size, ptr, chunk.line_bytes,
                            chunk.bits == 8 ? 8 : 32, tile_size.x,
                            tile_size.y);
               break;

            default :
               printf ("bpp = %d\b", chunk.bits);
            }
         break;

      case 0x0044 :  // uncompress data
         {
         byte *out, *in;
         int y, x, pixel;

         /* it seems that for 24bpp images in fact only 8bpp are stored.
            Not sure about the encoding though */
         for (y = 0, out = ptr, in = data; y < chunk.tile_size.y; y++)
            {
            if (in + chunk.tile_line_bytes >= data + size)
               {
               debug1 (("Attempt to read outside data in uncompressed memcpy\n"));
               break;
               }
            if (chunk.bits == 24)
               {
               for (x = 0; x < tile_size.x; x++)
                  {
                  pixel = in [x];
                  out [x * 3 + 0] = pixel & 0xc0;
                  out [x * 3 + 1] = pixel << 2;
                  out [x * 3 + 2] = pixel << 5;
                  out [x * 4 + 0] = pixel;
                  out [x * 4 + 1] = pixel;
                  out [x * 4 + 2] = pixel;
                  }
               in += tile_size.x;
               }
            else
               {
               memcpy (out, in, chunk.tile_line_bytes);
               in += chunk.tile_line_bytes;
               }
            out += chunk.line_bytes;
            }

         break;
         }

      default:
         printf ("unknown tile code %x\n", code);
         break;
      }
   return NULL;
   }


static void calc_tile_bytes (int tile_width, int image_width, int bpp,
      int *tile_line_bytes, int *line_bytes, int force_32bpp)
   {
   if (force_32bpp && bpp == 24)
      bpp = 32;  // adjust 3 bytes per pixel to 4 which is what we use
   *tile_line_bytes = (tile_width * bpp + 7) / 8;
   *line_bytes = (image_width * bpp + 7) / 8;
   }


/* calculate the tile size and expected number, and also work out the byte
position in the image of the top left of the tile

   \param x      tile x coord (0..tile_extent.x-1)
   \param y      tile y coord (0..tile_extent.x-1)
   \param chunk  chunk we are working with
   \param tile_size  returns the calculated tile size
   \param tilenum    returns the tile number
   \param stride     image stride (line_bytes) value, pass as -1 to use
                            chunk->line_bytes */
static byte *get_tile_size (chunk_info &chunk, int x, int y, cpoint *tile_size,
                  int *tilenum, int stride, int tile_stride)
   {
   byte *ptr;

   if (stride == -1)
      stride = chunk.line_bytes;
   if (tile_stride == -1)
      tile_stride = chunk.tile_line_bytes;
   ptr = chunk.image + (tile_stride /*chunk.tile_line_bytes*/ * x)
      + (stride * y * chunk.tile_size.y);

   /* work out the size of the tile to encode/decode. In most cases this is
      just chunk.tilesize, but for the rightmost and bottom tiles, it
      may be less */
   tile_size->y = chunk.image_size.y - chunk.tile_size.y * y;
   if (tile_size->y > chunk.tile_size.y)
      tile_size->y = chunk.tile_size.y;
   tile_size->x = chunk.image_size.x - chunk.tile_size.x * x;
   if (tile_size->x > chunk.tile_size.x)
      tile_size->x = chunk.tile_size.x;
   tile_size->x = (tile_size->x + 7) & ~7;
   *tilenum = chunk.tile_extent.x * y + x;  // tile sequence number
   return ptr;
   }


/* given a raw image size, calculate its true size in terms of pixels, taking account of the
word-alignment requirements for each line - i.e. the width will expand such that the stride
is a multiple of 4 bytes */
static void calc_true_size (chunk_info *chunk, QSize &true_size)
   {
   int x = chunk->image_size.x;
   if (chunk->bits == 1) // ensure image width is multiple of 32
      x = (x + 31) & ~31;
   true_size = QSize (x, chunk->image_size.y);
   }


err_info *Filemax::decode_tiledata (chunk_info &chunk,
                   decode_info &decode, byte *&imagep, QSize *image_size)
   {
   part_info *part;
   int x, y, pos, size;
   int code;
   cpoint tile_size;
   byte *ptr, *image;
   debug_info *debug = _debug;

   // image is always in chunk 4
   part = _version_a ? NULL : &chunk.parts [PT_tiledata];

   // calculate number of bytes across in a tile and a whole line
   calc_tile_bytes (chunk.tile_size.x, chunk.image_size.x, chunk.bits,
                    &chunk.tile_line_bytes, &chunk.line_bytes, true);

   // ensure image width is a multiple of 32 bits
   chunk.line_bytes = (chunk.line_bytes + 3) & ~3;

   debug2 (("image size %d x %d in %d x %d tiles of %d x %d each\n",
            chunk.image_size.x, chunk.image_size.y,
            chunk.tile_extent.x, chunk.tile_extent.y,
            chunk.tile_size.x, chunk.tile_size.y));

   size = chunk.line_bytes * chunk.image_size.y;

   debug2 (("tile_line_bytes = %d, line_bytes = %d, total bytes = %d\n",
           chunk.tile_line_bytes, chunk.line_bytes, size));
   if (!imagep)
      CALL (mem_alloc (CV &imagep, size, "decode_tiledata"));
   image = imagep;
   memset (image, '\0', size);
   chunk.image = image;
   chunk.image_bytes = size;
   debug3 (("image buffer extends from %p to %p\n", image, image + size));

   pos = _version_a ? chunk.start + 0x42 : chunk.start + 0x20 + part->start;
   for (y = 0; y < chunk.tile_extent.y; y++)
      for (x = 0; x < chunk.tile_extent.x; x++)
         {
         int tilenum, my_tilenum;

         // older files didn't have a tilenum and code
         if (_version_a)
            {
            tilenum = y * chunk.tile_extent.x + x;  // calculate
            code = 0x0043;
            }
         else
            {
            CALL (gethwe (pos, &tilenum));

            CALL (gethwe (pos + 2, &code));
            pos += 4;
            }
         debug2 (("code = %x, tilenum=%x\n", code, tilenum));

         ptr = get_tile_size (chunk, x, y, &tile_size, &my_tilenum, -1, -1);

         if (tilenum != my_tilenum)
            ;
         else if (tilenum >= debug->start_tile
            && (debug->num_tiles == INT_MAX
                || tilenum < debug->start_tile + debug->num_tiles))
            {
            int startx = chunk.tile_size.x * x;

            /* reduce the tile width if it extends past the right size of the image.
               This should only happen for JPEG images where the image size is always
               encode as a multiple of 8 pixels */
            if (image_size && startx + tile_size.x > image_size->width ())
               {
               debug3 (("truncating tile from %d to %d to fit in image width %d\n",
                        tile_size.x, image_size->width () - startx,
                        image_size->width ()));
               tile_size.x = image_size->width () - startx;
               }

            debug2 (("decoding tile %d at %x: code %x (%d, %d), "
                     "size %d x %d (0x%x x 0x%x)\n", tilenum, pos, code, x, y,
                  tile_size.x, tile_size.y, tile_size.x, tile_size.y));
            if (debug->compf)
               fprintf (debug->compf, "decode_tile %d\n", tilenum);

            if (chunk.tile [tilenum].size <= 0)
               debug2 (("   - size %d (0x%x) <= 0 so skipping tile\n",
                     chunk.tile [tilenum].size, chunk.tile [tilenum].size));
            else
               CALL (decode_tile (chunk, decode, code, pos,
                      chunk.tile [tilenum].size - 4, ptr, tile_size));
            }
         pos += chunk.tile [my_tilenum].size - 4;
         }
   imagep = image;
   return NULL;
   }


#define DEBUG_MAX_COUNT 0


static int decode_8bpp_preview (byte *ptr, byte *end, byte *out, byte *out_end)
   {
   byte *out_start = out;
   int count, value;
   int ch;

   while (ptr < end)
      {
      ch = *ptr++;
      count = (ch >> 4) * 2;
      if (count == 0)
         count++;
      value = ch & 0xf;
      value |= value << 4;
      while (count > 0)
         {
         if (out < out_end)
            *out = 255 - value;
         out++;
         count--;
         }
      }
   return out - out_start;
   }


/* decode a buffer of run-length-encoded data

   \param buf   buffer holding rle data
   \param size  size of input data
   \param preview_size  size of preview image x, y
   \param bpp   bits per pixels of input data
   \param sizep returns size of output data
   \param flip  flip the image after decode (normally required)
   \returns     output preview, either 8bpp greyscale or 32bpp colour */

static err_info *rle_decode (QString &fname, byte *buf, int size,
         cpoint *preview_size, int bpp, int *sizep, int flip, byte **previewp)
   {
   int width, srcwidth, bytes;
   byte *rev;
   byte *preview, *end, *ptr, *out;
   int i, ch, type, count, j, wrote, diff;
   int debug_count = 0;

   debug2 (("decode %d bytes\n", size));
   ptr = buf;
   end = buf + size;
   switch (bpp)
      {
      case 24 :
         width = preview_size->x;
         srcwidth = width * 4;
         width *= 4;
         break;

      case 1 :
      case 8 :
         // generate an 8bpp preview
         width = (preview_size->x + 3) & ~3;
         srcwidth = width;
         break;

      default :
         return err_make (ERRFN, ERR_unknown_pixel_depth1, bpp);
      }

   bytes = width * preview_size->y;
   CALL (mem_allocz (CV &preview, bytes + preview_size->x * 8, "rle_decode1"));
   CALL (mem_allocz (CV &rev, bytes + preview_size->x * 8, "rle_decode2"));

   debug2 (("Preview %d x %d, size is %d, width %d, bpp %d\n",
       preview_size->x, preview_size->y, size, width, bpp));

//   dumpb (ptr, size);

   // now read data and generate preview
   out = rev;
   switch (bpp)
      {
      case 24 :
         while (ptr < end)
            {
            for (i = 0; i < preview_size->x; i++)
               {
               *out++ = *ptr++;
               *out++ = *ptr++;
               *out++ = *ptr++;
               *out++ = 0;
               }
            while ((ptr - buf) & 3)
               ptr++;  // word align at the end of each line
            }
   //      memcpy (out, _data + pos, bytes);
         break;

      case 8 :
         {
         wrote = decode_8bpp_preview (ptr, end, out, out + bytes);
         out += wrote;
         break;
         }

      case 1 :
         while (ptr < end)
            {
            ch = *ptr++;
            type = ch >> 6;
            count = ch & 0x3f;
            debug_count++;
            if (debug_count < DEBUG_MAX_COUNT)
               printf ("%d: %d\n", type, count);
            switch (type)
               {
               case 0 :
                  if (out + count * 4 - rev <= bytes)
                     {
                     memset (out, '\x0', count * 4);
                     out += count * 4;
                     }
                  break;

               case 1 :
                  if (out + count * 4 - rev <= bytes)
                     {
                     memset (out, '\xff', count * 4);
                     out += count * 4;
                     }
                  break;

               case 2 :
                  if (debug_count < DEBUG_MAX_COUNT)
                     printf ("   :");
                  if (out + count - rev <= bytes) while (count > 0)
                     {
                     ch = *ptr++;
                     if (debug_count < DEBUG_MAX_COUNT)
                        printf ("%x ", ch);
                     for (j = 6; j >= 0; j -= 2)
                        *out++ = ((ch >> j) & 3) * 85;
                     count--;
                     }
                  if (debug_count < DEBUG_MAX_COUNT)
                     printf ("\n");
                  break;

               default :
                  printf ("preview: unknown type 3\n");
                  ptr = end;
                  break;
               }
            }
         break;
      }

//   printf ("bytes = %d, wrote %d\n", bytes, out - rev);
//   assert (out - rev == bytes);  // should be, but we don't understand type 3
   wrote = out - rev;
   *sizep = wrote;
   diff = bytes - wrote;
   if (diff < 0)
      diff = -diff;
   if (diff > preview_size->x + 3)
      warning (("%s: %d x %d, bytes = %d, wrote %d, diff %d, size %d\n",
          fname.toLatin1 ().constData(), preview_size->x, preview_size->y, bytes, wrote,
                bytes - wrote, size));

   // preview is always upside down, so fix it
   if (flip)
      {
      for (i = 0; i < preview_size->y; i++)
         memcpy (preview + i * srcwidth,
                 rev + (preview_size->y - 1 - i) * width,
                 srcwidth);
      free (rev);
      *previewp = preview;
      }
   else
      {
      free (preview);
      *previewp = rev;
      }
   return NULL;
   }


err_info *Filemax::decode_preview (chunk_info &chunk, int flip,
          byte **previewp)
   {
   byte *preview;
   int pos, wrote;
   byte *buf;

   if (_version_a)
      {
      CALL (max_cache_data (_cache, chunk.start, chunk.size, chunk.size, &buf));

      CALL (rle_decode (_filename, buf + 0x42, chunk.size, &chunk.preview_size,
                 chunk.bits, &wrote, flip, &preview));
      }
   else if (chunk.parts.size () > PT_preview)
      {
      CALL (chunk_ensure (chunk));
      part_info &part = chunk.parts [PT_preview];
      pos = chunk.start + 0x20 + part.start;

      CALL (max_cache_data (_cache, pos, part.size, part.size, &buf));

      CALL (rle_decode (_filename, buf, part.size, &chunk.preview_size,
                 chunk.bits, &wrote, flip, &preview));
      }
   else
      return err_make (ERRFN, ERR_unable_to_read_preview);
   *previewp = preview;
   return NULL;
   }


err_info *Filemax::decode_image (chunk_info &chunk, byte *&imagep, QSize *image_size)
   {
   decode_info decode;

   debug_level = _debug->level;
   debugf = _debug->logf;

   // this is just for the benefit of free_tables()
   memset (&decode, '\0', sizeof (decode));
   CALL (chunk_ensure (chunk));
   CALL (decode_tiledata (chunk, decode, imagep, image_size));

   free_tables (decode);
   return NULL;
   }


const char *Filemax::check_chunk (int start)
   {
   if (start == _bermuda)
      return "bermuda";
   if (start == _tunguska)
      return "tunguska";
   if (start == _annot)
      return "annot";
   if (start == _b0)
      return "b0";
   if (start == _b4)
      return "b4";
   if (start == _trail)
      return "trail";
   foreach (const page_info &page, _pages)
      {
      if (start == page.roswell)
         return "roswell";
      if (start == page.noti1)
         return "noti1";
      if (start == page.noti2)
         return "noti2";
      if (start == page.text)
         return "text";
      if (start == page.title)
         return "title";
      if (start == page.image)
         return "image";
      }
   return "";
   }


/* read the chunk at the given file position into chunk->buf */

err_info *Filemax::read_chunk_buf (chunk_info &chunk)
   {
   if (chunk.buf)
      free (chunk.buf);
   CALL (mem_alloc (CV &chunk.buf, chunk.size, "read_chunk_buf"));
   return max_read_data (chunk.start, chunk.buf, chunk.size);
   }


err_info *Filemax::read_chunk (chunk_info &chunk, int pos)
   {
   int ppos, i;
   err_info *err;

   chunk.start = pos;
   CALL (getworde (pos + 2, &chunk.size));

   int test;

   if (_version_a)
      {
      chunk.chunkid = 0;
      chunk.textflag = 0;
      chunk.flags = gethw (pos + 6);
      chunk.titletype = 0;
      test = 0;
      }
   else
      {
      chunk.chunkid = gethw (pos + 6);
      chunk.textflag = gethw (pos + 10);
      chunk.flags = gethw (pos + 26);
      chunk.titletype = gethw (pos + 28);
      test = gethw (pos + 12);
      }
   chunk.used = gethw (pos + 8);
   CALL (max_checkerr ());
   switch (chunk.flags)
      {
      case CHUNKF_image :
         chunk.type = test ? CT_notimage : CT_image;
         break;

      case CHUNKF_text :
         chunk.type = chunk.textflag ? CT_text : CT_title;
         break;

      case CHUNKF_bermuda :
         chunk.type = CT_bermuda;
         break;

      case CHUNKF_roswell :
         chunk.type = _version_a ? CT_image : CT_roswell;
         break;

      case CHUNKF_annot :
         chunk.type = CT_annot;
         break;

      case CHUNKF_env :
         chunk.type = CT_env;
         break;

      default :
         chunk.type = CT_unknown;
         break;
      }

   debug2 (("id=%d: start=0x%x, size=0x%04x, used=%d, flags=0x%04x, test=0x%x: %s %s\n", chunk.chunkid, chunk.start, chunk.size,
              chunk.used, chunk.flags, test, chunk_namestr (chunk.type), check_chunk (chunk.start)));
   err = NULL;
   if (chunk.type == CT_image)
      {
      if (_version_a)
         {
         chunk.image_size.x = gethw (pos + POSva_image_size_x);
         chunk.image_size.y = gethw (pos + POSva_image_size_y);

         debug2 (("image is %dx%d\n", chunk.image_size.x, chunk.image_size.y));
         chunk.dpi.x = gethw (pos + POSva_image_dpi_x);
         chunk.dpi.y = gethw (pos + POSva_image_dpi_y);
         chunk.bits = gethw (pos + POSva_image_bits);
         chunk.channels = gethw (pos + POSva_image_channels);
         chunk.preview_size.x = gethw (pos + POSva_image_preview_x);
         chunk.preview_size.y = gethw (pos + POSva_image_preview_y);

         chunk.parts.resize (0);
         chunk.after_part = 0;
         }
      else
         {
         chunk.image_size.x = gethw (pos + POS_image_size_x);
         chunk.image_size.y = gethw (pos + POS_image_size_y);
         debug2 (("image is %dx%d\n", chunk.image_size.x, chunk.image_size.y));
         chunk.dpi.x = gethw (pos + POS_image_dpi_x);
         chunk.dpi.y = gethw (pos + POS_image_dpi_y);
         chunk.bits = gethw (pos + POS_image_bits);
         chunk.channels = gethw (pos + POS_image_channels);
         chunk.preview_size.x = gethw (pos + POS_image_preview_x);
         chunk.preview_size.y = gethw (pos + POS_image_preview_y);

         chunk.parts.resize (gethw (pos + POS_part_count));
         Q_ASSERT (chunk.parts.size () > 0);
         debug2 (("ppos=0x%x, %d parts\n", pos + POS_part0 - chunk.start,
                  chunk.parts.size ()));
         for (i = 0, ppos = pos + POS_part0; i < chunk.parts.size (); i++)
            {
            err = read_part (chunk.parts [i], ppos);
            if (err)
               break;
            ppos += 8;
            debug2 (("part %d: pos %x size 0x%x", i, chunk.parts [i].start,
               chunk.parts [i].size));
            if (i == chunk.parts.size () -1)
               debug2 ((" (calced 0x%x)", chunk.size - chunk.parts [i].start));
            debug2 (("\n"));
            }
         chunk.after_part = ppos;

         // calculate size of final part, which seems to sometimes differ
         if (chunk.parts.size ())
            {
            part_info &part = chunk.parts [chunk.parts.size () - 1];
            part.size = chunk.size - part.start - 0x20;
            }
         }
      if (err)
         {
         char str [256];

         chunk.parts.resize (0);
         strcpy (str, err->errstr);
         err = err_make (ERRFN,
            ERR_unable_to_read_image_header_information1, str);
         }
      else
         decode_tileinfo (chunk);
/*
      write_image (chunk);
*/
      }
   else
      chunk.parts.resize (0);
   chunk.loaded = chunk.saved = true;
   return err;
   }


err_info *Filemax::chunk_ensure (chunk_info &chunk)
   {
   if (!chunk.loaded)
      CALL (read_chunk (chunk, chunk.start));
   return NULL;
   }


err_info *Filemax::chunk_find (int pos, chunk_info *&chunkp, bool *tempp)
   {
   if (tempp)
      *tempp = false;
   chunkp = NULL;
   if (_all_chunks_loaded) for (int i = 0; i < _chunks.size (); i++)
      {
      chunk_info &chunk = _chunks [i];

      if (chunk.start == pos)
         {
         chunkp = &chunk;
         return NULL;
         }
      }

   // if temporary loading, try to load it
   if (tempp && pos)
      {
//      printf ("temporary load chunk 0x%x\n", pos);
      chunkp = new chunk_info;
      chunk_init (*chunkp);
      *tempp = true;
      return read_chunk (*chunkp, pos);
      }

   return err_make (ERRFN, ERR_chunk_at_pos_not_found1, pos);
   }


debug_info *max_new_debug (FILE *logf, int level, int max_steps,
                           int start_tile, int num_tiles)
   {
   debug_info *debug;

   debug = new debug_info;
   assert (debug);
   memset (debug, '\0', sizeof (*debug));
   debug->logf = logf;
   debug->level = level;
   debug->max_steps = max_steps;
   debug->start_tile = start_tile;
   debug->num_tiles = num_tiles;
   return debug;
   }


void Filemax::max_set_debug (debug_info *debug)
   {
   _debug = debug;
   }


void Filemax::max_set_debug_compf (debug_info *debug, FILE *compf)
   {
   debug->compf = compf;
   }


/** update the page title */

err_info *Filemax::update_titlestr (page_info *page, const QString &title)
   {
   chunk_info *chunk;
   const char *str = title.toLatin1 ();
   bool temp;
   int j;

   if (!page->title)
      return NULL;

   // find the chunk, supporting temporary loading
   CALL (chunk_find (page->title, chunk, &temp));
   CALL (chunk_ensure (*chunk));
   CALL (read_chunk_buf (*chunk));

   page->titlestr = title;
   if (chunk->type == CT_title)
      {
      char *dest = (char *)chunk->buf + 0x20;

      for (j = 0; str [j] && j < TITLE_max_size - 1; j++)
         dest [j] = str [j];
      dest [j] = '\0';
      CALL (max_write_data (chunk->start, chunk->buf, chunk->size));
      chunk->saved = true;
      }

   return NULL;
   }


err_info *Filemax::ensure_all_chunks (void)
   {
   int i, pos;
   chunk_info *chunk;

   if (_all_chunks_loaded)
      return NULL;
   for (i = 0, pos = _chunk0_start; i < _chunks.size ();
        i++)
      {
      chunk = &_chunks [i];
      if (pos == _size)
         {
         debug1 (("short file - only %d out of %d chunks read\n", i, _chunks.size ()));
         _chunks.resize (i);
         break;
         }
      debug3 (("\nChunk %d: ", i));
      if (chunk->loaded)
         continue;
      CALL (read_chunk (*chunk, pos));
      pos += chunk->size;
//      debug3 (("chunk %d: start=%d, size=%d\n", i, chunk->start, chunk->size));
      }

   // do this here otherwise ensure_titlestr might use temporary loading
   _all_chunks_loaded = true;

   // get titles
   for (int i = 0; i < _pages.size (); i++)
      ensure_titlestr (i, _pages [i]);

   return NULL;
   }


err_info *Filemax::page_read_roswell (page_info &page)
   {
   page.image = getword (page.roswell + POS_roswell_image);
   page.noti1 = getword (page.roswell + POS_roswell_noti1);
   page.noti2 = getword (page.roswell + POS_roswell_noti2);
   page.text = getword (page.roswell + POS_roswell_text);
   page.title = getword (page.roswell + POS_roswell_title);
   page.timestamp = _timestamp;
   if (_version >= 1)
      page.timestamp.setTime_t (getword (page.roswell + POSn_roswell_timestamp));
//    printf ("page_read_roswell: page = %d, timestamp = %d\n", page.title, page.timestamp);
   page.have_roswell = true;
   return NULL;
   }


err_info *Filemax::setup_max (void)
   {
   int i;
   int pos, old_pages_count, chunk_count, page_count;

   // initially the version is 0, unless it is one of our files
   _version = 0;

   // check signature to see that it is a paperport file
   CALL (getworde (0, &_signature));
   if (_signature != 0x46476956       // ViGF
       && _signature != 0x46426956    // ViBF
       && _signature != 0x43476956
// CGBF found on file emailed by Thorsten Lemke, lemkesoft@t-online.de,01/03/06

       && _signature != 0x41476956
       // another one, from 1996, doesn't work yet

       && _signature != 0x12345678    // our own marker signature
       && _signature != 0x45476956)   // ViGE
      return merr_make (ERRFN, ERR_signature_failure1, _signature);

   if (_signature == 0x12345678)
      // get our version number
      _version = getword (POSn_version);

   _version_a = 0;
   if (_signature == 0x41476956)
      {
      _version_a = 1;
      debug1 (("Note: version_a file not supported yet- this will crash or "
         "give incorrect results\n"));
      }
   if (_version_a)
      {
      _chunk0_start = getword (POSva_chunk0_start);
      old_pages_count = getword (POSva_page_count);
      chunk_count = getword (POSva_chunk_count);
      }
   else
      {
      _chunk0_start = getword (POS_chunk0_start);
      old_pages_count = getword (POS_page_count);
      chunk_count = getword (POS_chunk_count);
      }
   CALL (max_checkerr ());

   // read header
   _hdr.resize (_chunk0_start);
   CALL (max_read_data (0, (byte *)_hdr.data (), _hdr.size ()));

   //FIXME: this is not correct - should read correct value from file
   _chunkid_next = chunk_count;

   if (_version_a)
      {
      _bermuda = getword (POSva_bermuda);
      _trail = getword (POSva_trail);
      }
   else
      {
      _bermuda = getword (POS_bermuda);
      _tunguska = getword (POS_tunguska);

      _annot = getword (POS_annot);
      _b0 = getword (0xb0);
      _b4 = getword (0xb4);
      _trail = getword (POS_trail);

      // do we have an envelope?
      if (_version >= 1)
         _envelope = getword (POS_envelope);
      }
   CALL (max_checkerr ());

   chunk_resize (_chunks, chunk_count);

   // get bermuda info
   pos = _bermuda;
   if (pos < 0 || pos >= _size)
      return merr_make (ERRFN, ERR_bermuda_chunk_corrupt1, pos);

   if (_version_a)
      page_count = old_pages_count;
   else if (pos)
      CALL (gethwe (pos + POS_bermuda_pagecount, &page_count));
   else
      {
      // need to deal with this, but don't know how at present
      return merr_make (ERRFN, ERR_bermuda_chunk_corrupt1, pos);

      printf ("no bermuda chunk - using old page count %d\n", old_pages_count);
      page_count = old_pages_count;
      }

   // printf ("old page count=%d, bermuda=%d\n", old_pages.size (), _pages.size ());
   page_resize (_pages, page_count);

   if (!pos)
      ; // hmmm, we are missing the bermuda chunk

   else if (_version_a)
      {
      if (pos) for (i = 0, pos += POSva_bermuda_pageinfo; i < page_count;
           i++, pos += 12)
         {
         page_info &page = _pages [i];

         page.chunkid = gethw (pos);
         page.image = getword (pos + 2);
         page.titlestr = "";
         page.roswell = 0;
         page.noti1 = page.noti2 = page.text = page.title = 0;
         printf ("   page %d, chunkid %d, image %x\n", i, page.chunkid, page.image);
         }
      }
   else
      {
      // first read the page info, then the roswell info (more cache-efficient)
      for (i = 0, pos += POS_bermuda_pageinfo; i < page_count;
           i++, pos += 12)
         {
         page_info &page = _pages [i];

         page.chunkid = gethw (pos);
         page.roswell = getword (pos + 4);
         page.titlestr = "";

         // get roswell info
         page.have_roswell = false;
         //printf ("   page %d, chunkid %d, image %d\n", i, page.chunkid,
         //        page.image);
         }

      for (i = 0; i < page_count; i++)
         CALL (page_read_roswell (_pages [i]));
      }
   CALL (max_checkerr ());

   // read chunks only as needed
   //CALL (ensure_all_chunks (max));

   _hdr_updated = false;
   return NULL;
   }


char *Filemax::max_get_shell_filename (void)
   {
   static char out [256];

   err_fix_filename (_filename.toLatin1(), out);
   return out;
   }


err_info *Filemax::max_openf()
{
   struct stat stat;

   debugf = _debug->logf;
   debug_level = _debug->level;

   fstat(fileno(_fin), &stat);
   _size = stat.st_size;
   _timestamp.setTime_t (stat.st_ctime);

   if (!_size)
      return err_make (ERRFN, ERR_signature_failure1, -1);

   CALL (setup_max ());
   return NULL;
}


static void tile_free (tile_info *tile)
   {
   if (tile->buf)
      {
      debug2 (("tile_free: %p", tile->buf));
      free (tile->buf);
      tile->buf = NULL;
      }
   }


void Filemax::chunk_free (chunk_info &chunk)
   {
   int i;

   for (i = 0; i < chunk.tile_count; i++)
      tile_free (&chunk.tile [i]);

   if (chunk.tile)
      {
      free (chunk.tile);
      chunk.tile = 0;
      }
   if (chunk.preview)
      {
      free (chunk.preview);
      chunk.preview = 0;
      }
   if (chunk.image)
      {
      free (chunk.image);
      chunk.image = 0;
      }
   chunk.parts.resize (0);
   }


void Filemax::max_free (void)
   {
   int i;

   for (i = 0; i < _chunks.size (); i++)
      chunk_free (_chunks [i]);
   _chunks.clear ();
   max_clear_cache (_cache);
   max_clear_cache (_scache);
   if (_fin)
      fclose (_fin);
   }


err_info *Filemax::find_page_chunk (int pagenum,
            chunk_info *&chunkp, bool *tempp, page_info **pagep)
   {
   page_info *page;
   int i, id;

   Q_ASSERT (_valid);

   // support temporary loading if required
   if (tempp)
      *tempp = false;
   else
      CALL (ensure_all_chunks ());

   CALL (find_page (pagenum, page));
   if (pagep)
      *pagep = page;

//   printf ("find_page_chunk %d, count=%d\n", pagenum, _pages.size ());
   id = page->chunkid;
//   printf ("find_page_chunk: pagenum=%d, id=%d\n", pagenum, id);
   if (_all_chunks_loaded) for (i = 0; i < _chunks.size (); i++)
      {
      chunk_info &chunk = _chunks [i];

//      printf ("- %d: %d, type=%s, %d (need %d)\n", i, chunk.chunkid,
//          chunk_namestr (chunk.type), chunk.type, CT_image);
      if (chunk.loaded && chunk.chunkid == id && chunk.type == CT_image)
         {
         chunkp = &chunk;
         if (chunk.start != page->image)
            printf ("find_page_chunk: chunk.start = 0x%x, page->image = 0x%x, id = %d\n",
               chunk.start, page->image, id);
         return NULL;
         }
      }

   // if temporary loading, try to load it
   if (tempp && page->image)
      {
//      printf ("temporary load page %d at 0x%x\n", pagenum, page->image);
      *tempp = true;
      chunkp = new chunk_info;
      chunk_init (*chunkp);
      return read_chunk (*chunkp, page->image);
      }

   return merr_make (ERRFN, ERR_could_not_find_image_chunk_for_page1, pagenum);
   }

err_info *Filemax::load_envelope (void)
   {
   chunk_info *chunk = 0;
   bool temp;
   QByteArray ba;
   int pos;

   // clear out the annotation info
   _env_loaded = true;

   // find the chunk, supporting temporary loading (note there may be no annotations)
   if (_envelope)
      CALL (chunk_find (_envelope, chunk, &temp));
   pos = _envelope + POS_env_start;
   _env_data.clear ();

   int i;
   if (_envelope)
      {
      CALL (read_chunk_buf (*chunk));

      // how many strings?
      int count = *(int *)(chunk->buf + POS_env_count);

      // read the \0-terminated strings until we get
      for (pos = POS_env_string0, i = 0; i < count; count++)
         {
         _env_data << QString ((const char *)chunk->buf + pos);
         int len = strlen ((const char *)chunk->buf + pos);
         pos += len + 1;
         }
      }
   else
      i = 0;

   // add blank data
   for (; i < Env_count; i++)
      _env_data << "";

   return NULL;
   }


err_info *Filemax::load_annot (void)
   {
   chunk_info *chunk = 0;
   bool temp;
   QByteArray ba;
   int pos, apos, size, type;

   // clear out the annotation info
   _annot_loaded = true;

   // find the chunk, supporting temporary loading (note there may be no annotations)
   if (_annot)
      CALL (chunk_find (_annot, chunk, &temp));
   pos = _annot + POS_annot_start;
   _annot_data.clear ();
   for (type = 0; type < Annot_count; type++, pos += POS_annot_offset)
      if (chunk)
         {
         CALL (getworde (pos + POS_annot_size, &size));
         CALL (getworde (pos + POS_annot_pos, &apos));
         if (apos < POS_annot_start || apos + size > chunk->size)
            return err_make (ERRFN, ERR_invalid_annot_block_data);
         ba.resize (size);
         CALL (max_read_data (chunk->start + 0x20 + apos, (unsigned char *)ba.data (), size));
         _annot_data << ba.data ();
         }
      else
         _annot_data << "";
   return NULL;
   }


err_info *Filemax::putEnvelope (QStringList &env)
   {
   int type;

   if (!_env_loaded)
      CALL (load_envelope ());

   for (type = 0; type < Env_count; type++)
      _env_data [type] = env [type];
   CALL (create_envelope ());
   return NULL;
   }


err_info *Filemax::putAnnot (QHash<int, QString> &updates)
   {
   int type;

   if (!_annot_loaded)
      CALL (load_annot ());

   for (type = 0; type < Annot_count; type++)
      if (updates.contains (type))
         _annot_data [type] = updates [type];
   create_annot ();
   return NULL;
   }


err_info *Filemax::getImageInfo (int pagenum, QSize &size,
      QSize &true_size, int &bpp, int &image_size, int &compressed_size,
      QDateTime &timestamp)
   {
   chunk_info *chunk;
   page_info *page;

   /* if we haven't loaded the file yet, just return an error. We are not
      allowed to silently load the file. The caller should have done this
      already. This does actually happen when Desktopview::currentChanged()
      is called on an item not currently visible. For example, if you open
      a long viewer and the first item is not visible, QT selects it as \
      the current item even though it isn't valid yet */
   if (!_valid)
       return err_make (ERRFN, ERR_file_not_loaded_yet1, qPrintable (_filename));

   CALL(ensure_open());
   CALL (find_page_chunk (pagenum, chunk, NULL, &page));
   ensure_closed();
   size = QSize (chunk->image_size.x, chunk->image_size.y);
   compressed_size = chunk->size;
   calc_true_size (chunk, true_size);
   bpp = chunk->bits;
   int bits = chunk->bits == 24 ? 32 : chunk->bits;
   int line_bytes = (chunk->image_size.x * bits + 7) / 8;

   line_bytes = (line_bytes + 3) & ~3;
   image_size = line_bytes * chunk->image_size.y;
   timestamp = page->timestamp;
   return NULL;
   }


err_info *Filemax::max_free_image (int pagenum)
   {
   chunk_info *chunk;

   CALL (find_page_chunk (pagenum, chunk, NULL, NULL));
   if (chunk->image)
      {
      free (chunk->image);
      chunk->image = NULL;
      }
   return NULL;
   }

int Filemax::pagecount (void)
   {
   return _pages.size ();
   }

err_info *Filemax::ensure_open()
{
   if (!_fin) {
      _fin = fopen (_pathname.toLatin1(), "r+b");
      if (!_fin) {
         // try read-only
         _fin = fopen (_pathname.toLatin1(), "rb");
         if (!_fin)
            return err_make (ERRFN, ERR_cannot_open_file1, _pathname.toLatin1().constData());
      }
   }

   return nullptr;
}

err_info *Filemax::max_open_file()
   {
   CALL(ensure_open());
   setvbuf(_fin, NULL, _IONBF, 0);
   CALL (max_openf());
   ensure_closed();

   return NULL;
   }

void Filemax::ensure_closed()
{
   if (_fin) {
      fclose(_fin);
      _fin = nullptr;
   }
}

err_info *Filemax::dodump (FILE *f, byte *ptr, int start, int count)
   {
   char str [33];
   int i, col, ch;

   assert (this || ptr);
   memset (str, ' ', 32);
   str [32] = '\0';
   for (i = 0; i < count; i++)
      {
      col = i & 31;
      if (col == 16)
         fprintf (f, "  ");
      if (!col)
         {
         fprintf (f, "  %1.16s  %1.16s\n        ", str, str + 16);
         memset (str, ' ', 32);
         }
      if (ptr)
         ch = ptr [start + 1];
      else
         CALL (getbytee (start + i, &ch));
      fprintf (f, "%02x ", ch);
      str [col] = ch >= ' ' && ch < 127 ? ch : '.';
      }
   col = i & 31;
   if (col) while (col < 32)
      {
      fprintf (f, "   ");
      if (col == 16)
         fprintf (f, "  ");
      col++;
      }
   fprintf (f, "  %1.16s  %1.16s\n        ", str, str + 16);
   fputc ('\n', f);
   return NULL;
   }


err_info *Filemax::dump (FILE *f, int start, int count)
   {
   return dodump (f, NULL, start, count);
   }

err_info *Filemax::dumpc (FILE *f, int start)
   {
   int i, ch;

   for (i = 0; ; i++)
      {
      if (!(i & 127))
         fprintf (f, "\n        ");
      CALL (getbytee (start + i, &ch));
//      ch = _data [start + i];
      if (!ch)
         break;
      fputc (ch >= ' ' && ch != 127 ? ch : '.', f);
      }
   if (i & 127)
      fputc ('\n', f);
   return NULL;
   }

err_info *Filemax::dump_block (FILE *f, const char *name, int pos)
   {
   int size;

   fprintf (f, "%s: %x\n", name, pos);
   if (pos)
      {
      CALL (getworde (pos + 2, &size));
      CALL (dump (f, pos, size));
      fprintf (f, "\n");
      }
   return NULL;
   }


void Filemax::chunk_dump (int num, chunk_info &chunk)
   {
   const char *name = check_chunk (chunk.start);

   printf ("%2d%s %-5x %-5x: page %-2d, type %s (%d), flags 0x%x, textflag=%x (%x) %s\n", num, chunk.used ? " " : "u", chunk.start, chunk.size,
            chunk.chunkid, chunk_namestr (chunk.type), chunk.type, chunk.flags, chunk.textflag, chunk.titletype,
            name);
   }


void Filemax::chunks_dump (void)
   {
   for (int i = 0; i < _chunks.size (); i++)
      chunk_dump (i, _chunks [i]);
   }


err_info *Filemax::show_file (FILE *f)
   {
   int i, j, pos;
   const char *name;

   fprintf (f, "\nfile: %s\n", _filename.toLatin1 ().constData());
   fprintf (f, "chunk0 start %x\n", _chunk0_start);
   fprintf (f, "chunk count %d\n", _chunks.size ());
   fprintf (f, "page count %d\n", _pages.size ());
   fprintf (f, "signature %x\n", _signature);

   fprintf (f, "\nPage Summary:\n");
   fprintf (f, "   Page   Chunkid   Roswell    Image    noti1     noti2     Text    Title\n");
   for (i = 0; i < _pages.size (); i++)
      {
      page_info &page = _pages [i];

      fprintf (f, "   %4d   %7d   %7x   %6x   %6x    %6x   %6x   %6x\n",
         i, page.chunkid, page.roswell, page.image, page.noti1,
      page.noti2, page.text, page.title);
      }
   fprintf (f, "\n");

   CALL (dump_block (f, "bermuda", _bermuda));
   CALL (dump_block (f, "tunguska", _tunguska));
   CALL (dump_block (f, "annot", _annot));
   CALL (dump_block (f, "b0", _b0));
   CALL (dump_block (f, "b4", _b4));
   CALL (dump_block (f, "trail", _trail));

   CALL (dump (f, 0, 0x100));
   for (i = 0; i < _chunks.size (); i++)
      {
      chunk_info &chunk = _chunks [i];

      name = check_chunk (chunk.start);
      fprintf (f, "%2d%s %-5x %-5x: page %-2d, type %s (%d), flags 0x%x, textflag=%x (%x) %s", i, chunk.used ? "" : "u", chunk.start, chunk.size,
              chunk.chunkid, chunk_namestr (chunk.type), chunk.type, chunk.flags, chunk.textflag, chunk.titletype,
              name);
      if (chunk.type == CT_image)
         fprintf (f, "(%dx%d %d bpp, %d channels, tile %dx%d of %dx%d, preview %dx%d) ",
             chunk.image_size.x, chunk.image_size.y, chunk.bits,
             chunk.channels,
             chunk.tile_extent.x, chunk.tile_extent.y,
             chunk.tile_size.x, chunk.tile_size.y,
             chunk.preview_size.x, chunk.preview_size.y);
      if (chunk.type == CT_text || chunk.type == CT_title)
         {
         CALL (dumpc (f, chunk.start + 32));
         CALL (dump (f, chunk.start, 64));
         }
      if (chunk.type == CT_unknown || chunk.type == CT_notimage)
         {
         CALL (dump (f, chunk.start, chunk.size));
         fprintf (f, "\n");
         }
      else if (chunk.parts.size ())
         {
         int len, old;
         byte *image;

         CALL (dump (f, chunk.start, 0x80));
         fprintf (f, "\n");
         for (j = 0; j < chunk.parts.size (); j++)
            {
            part_info &part = chunk.parts [j];
            pos = chunk.start + 0x20 + part.start;
            fprintf (f, "      %d (%s): %5x  %5x:  %5x - %5x  ", j, parttype_str [j], part.start, part.size,
                    pos, pos + part.size - 1);
            len = 0x40;
            if (j != PT_tiledata) // && j != PT_preview)
               len = part.size;
            if (part.size < len)
               len = part.size;
            CALL (dump (f, pos, len));
            fprintf (f, "\n");
            }
         old = debug_level;
//         debug_level = 3;
         debugf = f;
         CALL (decode_image (chunk, image, NULL));
         debugf = stdout;
         debug_level = old;

         len = chunk.start + 0x20 + chunk.parts [0].start - chunk.after_part;
/*          printf ("        after_part=%x, %x, %x\n", chunk.after_part, chunk.part [0].start, len); */
/*          dump (chunk.after_part, len); */
         fprintf (f, "\n");
         }
      else
         fprintf (f, "\n");
      }
   fprintf (f, "\n(ends)\n");
   fflush (f);
   return NULL;
   }


/* given a dimension, calculate a suitable tile size to break it into,
and a count. So 2480 may become 504 x 5. The algorithm uses multiples of 8
and tries to minimise wastage  */

static void calc_tile_dimension (int target, int *tilep, int *countp)
   {
   int waste, count;
   int best, best_waste, size;

   best = -1; best_waste = target;
   for (size = 440; size < 550; size += 8)
      {
      count = (target + size - 1) / size;
      waste = count * size - target;
      if (waste < best_waste)
         {
         best = size;
         best_waste = waste;
         }
      }
   *tilep = best;
   *countp = (target + best - 1) / best;
   }


/* given an image size, calculates a suitable tile size and number of tiles */

static void calc_tiles (cpoint *image_size, cpoint *tile_size, cpoint *tile_extent)
   {
   calc_tile_dimension (image_size->x, &tile_size->x, &tile_extent->x);
   calc_tile_dimension (image_size->y, &tile_size->y, &tile_extent->y);
   }


static int scale_2bpp (byte *image, cpoint *image_size, byte *preview,
             cpoint *preview_size, int stride)
   {
   int *line;    // pixels sums for current preview line
   int sum;    // current pixel sum being calculated
   int image_line_bytes;
   int line_bytes;
   int x, y;     // work through the preview
   int xsub, ysub;  // which image pixel we are up to
   int mask;     // source image bit mask
   int pshift;   // 2bpp preview shift
   byte *in;     // image data in
   byte *out;    // preview data out
   int ysubcount;  // number of image lines to scan for this preview lines
   int result;

   pshift = 6;
   out = preview;

   // add together all the PREVIEW_SCALE pixels into one int in
   // each direction
   line_bytes = preview_size->x * sizeof (int);
   line = (int *)malloc (line_bytes);

   // use stride because SANE may not word align
   image_line_bytes = stride; //(image_size->x + 31) / 32 * 4;
   for (y = 0; y < preview_size->y; y++)
      {
      memset (line, '\0', line_bytes);

      // work out how many lines we need to scan
      ysubcount = image_size->y - 1 - y * PREVIEW_SCALE;
      if (ysubcount > PREVIEW_SCALE)
         ysubcount = PREVIEW_SCALE;

      // now scan the lines, building up a pixel value sum in line[]
      for (ysub = 0; ysub < ysubcount; ysub++)
         {
         in = image + image_line_bytes *
                    ((preview_size->y - y - 1) * PREVIEW_SCALE + (ysubcount - ysub - 1));
         assert (in >= image && in <= image + image_line_bytes * (image_size->y - 1));
         mask = 1;

         // calculate the value for each preview pixel
         for (x = 0; x < preview_size->x; x++)
            {
            sum = 0;
            for (xsub = 0; xsub < PREVIEW_SCALE; xsub++)
               {
               sum += *in & mask ? 255 : 0;
               mask <<= 1;
               if (mask == 0x100)
                  {
                  mask = 1;
                  in++;
                  }
               }
            line [x] += sum;
            }
         }

      // we now have the line values - each represents
      // PREVIEW_SCALE x PREVIEW_SCALE pixels
      // convert to 2bpp preview
      for (x = 0; x < preview_size->x; x++)
         {
         // scale result up by 5/2 to get a darker image
         result = (line [x] * 5 / PREVIEW_SCALE / 2 / ysubcount) >> 6;
         if (result > 3)
            result = 3;
         *out |= result << pshift;
/*
         pshift += 2;
         if (pshift == 8)
            {
            pshift = 0;
            out++;
            }
         }
*/
         pshift -= 2;
         if (pshift < 0)
            {
            pshift = 6;
            out++;
            }
         }
      }

   // done
   return out - preview;
   }


/** scale down a 24bpp or 32bpp image by a factor of PREVIEW_SCALE

   If 32bpp then it is assumed to have red and blue in the right order, if 24bpp they are swapped

   \param image       pointer to image
   \param image_size  size of image (x, y)
   \param preview     buffer to use for preview
   \param preview_size required size for preview
   \param stride      line stride for image
   \param bpp         image bits per pixel (24 or 32)
   \returns number of bytes in preview image */

static int scale_24bpp (byte *image, cpoint *image_size, byte *preview,
                        cpoint *preview_size, int stride, int bpp)
   {
   int *line;    // RGB pixels sums for current preview line
   int sum [3];    // current pixel RGB sum being calculated
   int image_line_bytes;
   int line_bytes;
   int x, y;     // work through the preview
   int xsub, ysub;  // which image pixel we are up to
   byte *in;     // image data in
   byte *out;    // preview data out
   int ysubcount;  // number of image lines to scan for this preview lines
   int result;

   out = preview;

   // add together all the PREVIEW_SCALE pixels into 3 ints (RGB) in
   // each direction
   line_bytes = preview_size->x * 3 * sizeof (int);
   line = (int *)malloc (line_bytes);

   // use stride because SANE may not word align
   image_line_bytes = stride; //(image_size->x + 31) / 32 * 4;
   for (y = 0; y < preview_size->y; y++)
      {
      memset (line, '\0', line_bytes);

      // work out how many lines we need to scan
      ysubcount = image_size->y - 1 - y * PREVIEW_SCALE;
      if (ysubcount > PREVIEW_SCALE)
         ysubcount = PREVIEW_SCALE;

      // now scan the lines, building up a pixel value sum in line[]
      for (ysub = 0; ysub < ysubcount; ysub++)
         {
         in = image + image_line_bytes *
                    ((preview_size->y - y - 1) * PREVIEW_SCALE + (ysubcount - ysub - 1));
         assert (in >= image && in <= image + image_line_bytes * (image_size->y - 1));
         // calculate the value for each preview pixel
         for (x = 0; x < preview_size->x; x++)
            {
            sum [0] = sum [1] = sum [2] = 0;
            for (xsub = 0; xsub < PREVIEW_SCALE; xsub++, in += bpp / 8)
               {
               sum [0] += in [0];
               sum [1] += in [1];
               sum [2] += in [2];
               }
            if (bpp == 24) // swap red and blue: comes from a scan
               {
               line [x * 3 + 0] += sum [2];
               line [x * 3 + 1] += sum [1];
               line [x * 3 + 2] += sum [0];
               }
            else  // don't swap red and blue: comes from a QImage
               {
               line [x * 3 + 0] += sum [0];
               line [x * 3 + 1] += sum [1];
               line [x * 3 + 2] += sum [2];
               }
            }
         }

      // we now have the line values - each represents
      // PREVIEW_SCALE x PREVIEW_SCALE pixels
      // convert to 24bpp preview
      for (x = 0; x < preview_size->x * 3; x++)
         {
         result = (line [x] / PREVIEW_SCALE / ysubcount);
         if (result > 0xff)
            result = 0xff;
         *out++ = result;
         }

      // word align
      for (; x & 3; x++)
         *out++ = 0;
      }

   // done
   debug2 (("24bit preview: %d bytes\n", out - preview));
   return out - preview;
   }


static int build_preview (chunk_info &chunk, int stride, int bpp)
   {
   int size, width;
   int len;

   switch (bpp)
      {
      case 1 :  // build a 2bpp preview
         {
         cpoint psize;

         psize.x = (chunk.preview_size.x + 3) & ~3;
         psize.y = chunk.preview_size.y;
         width = psize.x / 4;
         size = width * chunk.preview_size.y;
         chunk.preview_bytes = size;
         chunk.preview = (byte *)malloc (size);
         if (!chunk.preview)
            return ERR (-ENOMEM);
         memset (chunk.preview, '\0', size);
         len = scale_2bpp (chunk.image, &chunk.image_size,
                     chunk.preview, &psize, stride);
         debug2 (("1bpp preview size %d, alloced %d\n", len, size));
         break;
         }

      case 8 : // not sure what to do
         printf ("8bpp preview not supported\n");
         break;

      case 24 :  // build a 24bpp colour preview padded to words at EOL
      case 32 :
         width = (chunk.preview_size.x * 3 + 3) & ~3;
         size = width * chunk.preview_size.y;
         chunk.preview_bytes = size;
         chunk.preview = (byte *)malloc (size);
         if (!chunk.preview)
            return ERR (-ENOMEM);
         memset (chunk.preview, '\0', size);
         len = scale_24bpp (chunk.image, &chunk.image_size,
              chunk.preview, &chunk.preview_size, stride, bpp);
         debug2 (("24bpp preview size %d, alloced %d\n", len, size));
         break;
      }
   return 0;
   }


typedef byte *tidata_t;

/*
 * Compression+decompression state blocks are
 * derived from this ``base state'' block.
 */
typedef struct {
   int   mode;       /* operating mode */
   uint32_t   rowbytes;      /* bytes in a decoded scanline */
   uint32_t   rowpixels;     /* pixels in a scanline */
   uint32_t   stride;

   uint16_t   cleanfaxdata;     /* CleanFaxData tag */
   uint32_t   badfaxrun;     /* BadFaxRun tag */
   uint32_t   badfaxlines;      /* BadFaxLines tag */
   uint32_t   groupoptions;     /* Group 3/4 options tag */
   uint32_t   recvparams;    /* encoded Class 2 session params */
   char* subaddress;    /* subaddress string */
   uint32_t   recvtime;      /* time spent receiving (secs) */
   TIFFVGetMethod vgetparent; /* super-class method */
   TIFFVSetMethod vsetparent; /* super-class method */
} Fax3BaseState;
#define  Fax3State(tif)    ((Fax3BaseState*) (tif)->tif_data)

enum enc_state { G3_1D, G3_2D };

typedef struct {
   Fax3BaseState b;
   int   data;       /* current i/o byte */
   int   bit;        /* current i/o bit in byte */
   enum enc_state tag; /* encoding state */
   u_char*  refline;    /* reference line for 2d decoding */
   int   k;       /* #rows left that can be 2d encoded */
   int   maxk;       /* max #rows that can be 2d encoded */
} Fax3EncodeState;
#define  EncoderState(tif) ((Fax3EncodeState*) Fax3State(tif))

typedef struct {
   Fax3BaseState b;
   const u_char* bitmap;      /* bit reversal table */
   uint32_t   data;       /* current i/o byte/word */
   int   bit;        /* current i/o bit in byte */
   int   EOLcnt;        /* count of EOL codes recognized */
   TIFFFaxFillFunc fill;      /* fill routine */
   uint16_t*  runs;       /* b&w runs for current/previous row */
   uint16_t*  refruns;    /* runs for reference line */
   uint16_t*  curruns;    /* runs for current line */
} Fax3DecodeState;
#define  DecoderState(tif) ((Fax3DecodeState*) Fax3State(tif))


//typedef struct TIFF
//   {
//   Fax3EncodeState *sp;
//   } TIFF;


struct tiff {
   tidata_t tif_data;   /* compression scheme private data */
   unsigned tif_row; /* current scanline */
   tsize_t     tif_rawdatasize;/* # of bytes in raw data buffer */
   tidata_t tif_rawcp;  /* current spot in raw buffer */
   tsize_t     tif_rawcc;  /* bytes unread from raw buffer */
   int bit_count;
   const char *tif_name;
};


// things we need to save the output state of the G4 compressor

typedef struct state_info
   {
   int   data;       /* current i/o byte */
   int   bit;        /* current i/o bit in byte */
   tidata_t tif_rawcp;  /* current spot in raw buffer */
   tsize_t     tif_rawcc;  /* bytes unread from raw buffer */
   } state_info;


#define INLINE

static void TIFFFlushData1 (TIFF *tiff)
   {
   (void)tiff;
   fprintf (stderr, "fatal error - TIFFFlushData1() called\n");
   exit (1);
   }

#define  is2DEncoding(sp) \
   (sp->b.groupoptions & GROUP3OPT_2DENCODING)
#define  isAligned(p,t) ((((u_long)(p)) & (sizeof (t)-1)) == 0)
#define  TIFFroundup(x, y) (TIFFhowmany(x,y)*((uint32_t)(y)))
#define  TIFFhowmany(x, y) ((((uint32_t)(x))+(((uint32_t)(y))-1))/((uint32_t)(y)))


/*
 * CCITT Group 3 FAX Encoding.
 */

#define  Fax3FlushBits(tif, sp) {            \
   if ((tif)->tif_rawcc >= (tif)->tif_rawdatasize)    \
      (void) TIFFFlushData1(tif);         \
   debug3 ((" [f %02x] ", (sp)->data));      \
   *(tif)->tif_rawcp++ = (sp)->data;         \
   (tif)->tif_rawcc++;              \
   (sp)->data = 0, (sp)->bit = 8;            \
}
#define  _FlushBits(tif) {             \
   if ((tif)->tif_rawcc >= (tif)->tif_rawdatasize)    \
      (void) TIFFFlushData1(tif);         \
   debug3 ((" [_ %02x] ", data));      \
   *(tif)->tif_rawcp++ = data;            \
   (tif)->tif_rawcc++;              \
   data = 0, bit = 8;               \
}
static const int _msbmask[9] =
    { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
#define  _PutBits(tif, bits, length) {          \
   debug3 (("data=%x, length=%x  ", bits, length));   \
   tif->bit_count += length;    \
   while ((int)length > (int)bit) {             \
      data |= bits >> (length - bit);        \
      length -= bit;             \
      _FlushBits(tif);           \
   }                    \
   data |= (bits & _msbmask[length]) << (bit - length);  \
   bit -= length;                \
   if (bit == 0)                 \
      _FlushBits(tif);           \
   debug3 (("  bit=%d\n", bit)); \
}

/*
 * Write a variable-length bit-value to
 * the output stream.  Values are
 * assumed to be at most 16 bits.
 */
static void
Fax3PutBits(TIFF* tif, u_int bits, u_int length)
{
   Fax3EncodeState* sp = EncoderState(tif);
   int bit = sp->bit;
   int data = sp->data;

   _PutBits(tif, bits, length);

   sp->data = data;
   sp->bit = bit;
}

/*
 * Write a code to the output stream.
 */
#define putcode(tif, te)   Fax3PutBits(tif, (te)->code, (te)->length)

#ifdef FAX3_DEBUG
#define  DEBUG_COLOR(w) (tab == myTIFFFaxWhiteCodes ? w "W" : w "B")
#define  DEBUG_PRINT(what,len) {                \
    int t;                       \
    printf("%08X/%-2d: %s%5d\t", data, bit, DEBUG_COLOR(what), len); \
    for (t = length-1; t >= 0; t--)             \
   putchar(code & (1<<t) ? '1' : '0');          \
    putchar('\n');                     \
}
#endif

/*
 * Write the sequence of codes that describes
 * the specified span of zero's or one's.  The
 * appropriate table that holds the make-up and
 * terminating codes is supplied.
 */
static void
putspan(TIFF* tif, uint32_t span, const tableentry* tab)
{
   Fax3EncodeState* sp = EncoderState(tif);
   int bit = sp->bit;
   int data = sp->data;
   u_int code, length;

   while (span >= 2624) {
      const tableentry* te = &tab[63 + (2560>>6)];
      code = te->code, length = te->length;
#ifdef FAX3_DEBUG
      DEBUG_PRINT("MakeUp", te->runlen);
#endif
      _PutBits(tif, code, length);
      span -= te->runlen;
   }
   if (span >= 64) {
      const tableentry* te = &tab[63 + (span>>6)];
      assert((unsigned int)te->runlen == 64*(span>>6));
      code = te->code, length = te->length;
#ifdef FAX3_DEBUG
      DEBUG_PRINT("MakeUp", te->runlen);
#endif
      _PutBits(tif, code, length);
      span -= te->runlen;
   }
   code = tab[span].code, length = tab[span].length;
#ifdef FAX3_DEBUG
   DEBUG_PRINT("  Term", tab[span].runlen);
#endif
   _PutBits(tif, code, length);

   sp->data = data;
   sp->bit = bit;
}


/*
 * Reset encoding state at the start of a strip.
 */
static int
Fax3PreEncode(TIFF* tif, tsample_t s)
{
   Fax3EncodeState* sp = EncoderState(tif);

   (void) s;
   assert(sp != NULL);
   sp->bit = 8;
   sp->data = 0;
   sp->tag = G3_1D;
   tif->bit_count = 0;
   /*
    * This is necessary for Group 4; otherwise it isn't
    * needed because the first scanline of each strip ends
    * up being copied into the refline.
    */
   if (sp->refline)
      _TIFFmemset(sp->refline, 0x00, sp->b.rowbytes + 4); //!
   if (is2DEncoding(sp)) {
#if 0
      float res = tif->tif_dir.td_yresolution;
      /*
       * The CCITT spec says that when doing 2d encoding, you
       * should only do it on K consecutive scanlines, where K
       * depends on the resolution of the image being encoded
       * (2 for <= 200 lpi, 4 for > 200 lpi).  Since the directory
       * code initializes td_yresolution to 0, this code will
       * select a K of 2 unless the YResolution tag is set
       * appropriately.  (Note also that we fudge a little here
       * and use 150 lpi to avoid problems with units conversion.)
       */
      if (tif->tif_dir.td_resolutionunit == RESUNIT_CENTIMETER)
         res = (res * .3937f) / 2.54f; /* convert to inches */
      sp->maxk = (res > 150 ? 4 : 2);
#else
      sp->maxk = 4;
#endif
      sp->k = sp->maxk-1;
   } else
      sp->k = sp->maxk = 0;
   return (1);
}

static const u_char zeroruns[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,   /* 0x00 - 0x0f */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,   /* 0x10 - 0x1f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   /* 0x20 - 0x2f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   /* 0x30 - 0x3f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0x40 - 0x4f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0x50 - 0x5f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0x60 - 0x6f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0x70 - 0x7f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x80 - 0x8f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x90 - 0x9f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0xa0 - 0xaf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0xb0 - 0xbf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0xc0 - 0xcf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0xd0 - 0xdf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0xe0 - 0xef */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0xf0 - 0xff */
};
static const u_char oneruns[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x00 - 0x0f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x10 - 0x1f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x20 - 0x2f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x30 - 0x3f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x40 - 0x4f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x50 - 0x5f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x60 - 0x6f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x70 - 0x7f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0x80 - 0x8f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0x90 - 0x9f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0xa0 - 0xaf */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   /* 0xb0 - 0xbf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   /* 0xc0 - 0xcf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   /* 0xd0 - 0xdf */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,   /* 0xe0 - 0xef */
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8,   /* 0xf0 - 0xff */
};

/*
 * Find a span of ones or zeros using the supplied
 * table.  The ``base'' of the bit string is supplied
 * along with the start+end bit indices.
 */
INLINE static uint32_t
find0span(u_char* bp, uint32_t bs, uint32_t be)
{
   uint32_t bits = be - bs;
   uint32_t n, span;

   bp += bs>>3;
   /*
    * Check partial byte on lhs.
    */
   if (bits > 0 && (n = (bs & 7))) {
      span = zeroruns[(*bp << n) & 0xff];
      if (span > 8-n)      /* table value too generous */
         span = 8-n;
      if (span > bits)  /* constrain span to bit range */
         span = bits;
      if (n+span < 8)      /* doesn't extend to edge of byte */
         return (span);
      bits -= span;
      bp++;
   } else
      span = 0;
   if (bits >= 2*8*(int)sizeof (long)) {
      long* lp;
      /*
       * Align to longword boundary and check longwords.
       */
      while (!isAligned(bp, long)) {
         if (*bp != 0x00)
            return (span + zeroruns[*bp]);
         span += 8, bits -= 8;
         bp++;
      }
      lp = (long*) bp;
      while (bits >= 8*(int)sizeof (long) && *lp == 0) {
         span += 8*sizeof (long), bits -= 8*sizeof (long);
         lp++;
      }
      bp = (u_char*) lp;
   }
   /*
    * Scan full bytes for all 0's.
    */
   while (bits >= 8) {
      if (*bp != 0x00)  /* end of run */
         return (span + zeroruns[*bp]);
      span += 8, bits -= 8;
      bp++;
   }
   /*
    * Check partial byte on rhs.
    */
   if (bits > 0) {
      n = zeroruns[*bp];
      span += (n > bits ? bits : n);
   }
   return (span);
}

INLINE static uint32_t
find1span(u_char* bp, uint32_t bs, uint32_t be)
{
   uint32_t bits = be - bs;
   uint32_t n, span;

   bp += bs>>3;
   /*
    * Check partial byte on lhs.
    */
   if (bits > 0 && (n = (bs & 7))) {
      span = oneruns[(*bp << n) & 0xff];
      if (span > 8-n)      /* table value too generous */
         span = 8-n;
      if (span > bits)  /* constrain span to bit range */
         span = bits;
      if (n+span < 8)      /* doesn't extend to edge of byte */
         return (span);
      bits -= span;
      bp++;
   } else
      span = 0;
   if (bits >= 2*8*(int)sizeof (long)) {
      long* lp;
      /*
       * Align to longword boundary and check longwords.
       */
      while (!isAligned(bp, long)) {
         if (*bp != 0xff)
            return (span + oneruns[*bp]);
         span += 8, bits -= 8;
         bp++;
      }
      lp = (long*) bp;
      while (bits >= 8*(int)sizeof (long) && *lp == ~0) {
         span += 8*sizeof (long), bits -= 8*sizeof (long);
         lp++;
      }
      bp = (u_char*) lp;
   }
   /*
    * Scan full bytes for all 1's.
    */
   while (bits >= 8) {
      if (*bp != 0xff)  /* end of run */
         return (span + oneruns[*bp]);
      span += 8, bits -= 8;
      bp++;
   }
   /*
    * Check partial byte on rhs.
    */
   if (bits > 0) {
      n = oneruns[*bp];
      span += (n > bits ? bits : n);
   }
   return (span);
}

/*
 * Return the offset of the next bit in the range
 * [bs..be] that is different from the specified
 * color.  The end, be, is returned if no such bit
 * exists.
 */
#define  finddiff(_cp, _bs, _be, _color)  \
   (_bs + (_color ? find1span(_cp,_bs,_be) : find0span(_cp,_bs,_be)))
/*
 * Like finddiff, but also check the starting bit
 * against the end in case start > end.
 */
#define  finddiff2(_cp, _bs, _be, _color) \
   (_bs < _be ? finddiff(_cp,_bs,_be,_color) : _be)

static const tableentry horizcode =
    { 3, 0x1, 0 };      /* 001 */
static const tableentry passcode =
    { 4, 0x1, 0 };      /* 0001 */
static const tableentry vcodes[7] = {
    { 7, 0x03, 0 },  /* 0000 011 */
    { 6, 0x03, 0 },  /* 0000 11 */
    { 3, 0x03, 0 },  /* 011 */
    { 1, 0x1, 0 },      /* 1 */
    { 3, 0x2, 0 },      /* 010 */
    { 6, 0x02, 0 },  /* 0000 10 */
    { 7, 0x02, 0 }      /* 0000 010 */
};

/*
 * 2d-encode a row of pixels.  Consult the CCITT
 * documentation for the algorithm.
 */
static int
Fax3Encode2DRow(TIFF* tif, u_char* bp, u_char* rp, uint32_t bits, int *badp)
{
#define  PIXEL(buf,ix)  ((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)
// uint32_t a0 = 0;
  int a0 = -1, a0p = 0;
  int a1 = (PIXEL(bp, 0) != 0 ? 0 : finddiff(bp, 0, bits, 0));
  int b1 = (PIXEL(rp, 0) != 0 ? 0 : finddiff(rp, 0, bits, 0));
  int a2, b2;

  *badp = 0;
  for (;;) {
    b2 = finddiff2(rp, b1, (int)bits, PIXEL(rp,b1));
    debug3 (("1: b1=%d, b2=%d: ", b1, b2));
    if (b2 >= a1) {
      uint32_t d = b1 - a1;
      if (!(-3 <= (signed int)d && (signed int)d <= 3)) {   /* horizontal mode */
   a2 = finddiff2(bp, a1, (int)bits, PIXEL(bp,a1));
   putcode(tif, &horizcode);
   // if (!a0 && !a1)
   //   *badp = 1;
   // if (a0+a1 == 0 || PIXEL(bp, a0) == 0) {
//   if (a0p+a1 == 0 || PIXEL(bp, a0p) == 0) {   // simon attempt works for p1
   if (a0 == -1 || PIXEL(bp, a0) == 0) {   // simon attempt
     debug3 (("  2ha: a0=%d, a1=%d: ", a0, a1));
     putspan(tif, a1-a0, myTIFFFaxWhiteCodes);
     debug3 (("  2ha: a1=%d, a2=%d: ", a1, a2));
     putspan(tif, a2-a1, myTIFFFaxBlackCodes);
   } else {
     debug3 (("  2hb: a0=%d, a1=%d: ", a0, a1));
     putspan(tif, a1-a0, myTIFFFaxBlackCodes);
     debug3 (("  2hb: a1=%d, a2=%d: ", a1, a2));
     putspan(tif, a2-a1, myTIFFFaxWhiteCodes);
   }
   a0 = a0p = a2;
   //          if (a0 == bits)
   //             putcode(tif, &vcodes[3]);
      } else {       /* vertical mode */
   //          if (b2 == bits && type == 2)
   //             break;
   //            if (!a1)
   //               *badp = 1;
   debug3 (("  2v: a1=%d, b1=%d: ", a1, b1));
   putcode(tif, &vcodes[d+3]);
   debug3 (("a0=%d, a1=%d, a2=%d, b1=%d, b2=%d\n", a0, a1, a2, b1, b2));
   a0 = a0p = a1;
      }
    } else {            /* pass mode */
      debug3 (("  2p: pass b2=%d: ", b2));
      putcode(tif, &passcode);
      a0 = a0p = b2;
    }
    if (a0 >= (int)bits)
      break;
    a1 = finddiff(bp, a0, bits, PIXEL(bp,a0));
    b1 = finddiff(rp, a0, bits, !PIXEL(bp,a0));
    b1 = finddiff(rp, b1, bits, PIXEL(bp,a0));
  }
  return (1);
#undef PIXEL
}


static int try_single (TIFF *tif, u_char *ptr, int rowpixels, int write)
   {
//   Fax3EncodeState* sp = EncoderState(tif);
   int state = 0;   // current state (starts as white)
   int mask = 0x80;
   int bit;
   u_char *end = ptr + rowpixels;
   int used, value;

   debug3 (("Single: "));

   // we must move to the next whole byte
//   used = sp->bit;
   used = 0;

   // search for colour changes - each adds a short
   for (bit = 0; ptr < end; bit++)
      {
      value = *ptr & mask ? 1 : 0;
      if (value != state)
         {
         state = value;
         debug3 (("%d ", bit));
         if (write)
            {
            *tif->tif_rawcp++ = bit >> 8;
            *tif->tif_rawcp++ = bit;
            tif->tif_rawcc += 2;
            }
         used += 16;
         }
      mask >>= 1;
      if (!mask)
         {
         mask = 0x80;
         ptr++;
         }
      }

   // also one for the last value
   debug3 (("%d\n", bit));
   used += 16;
   if (write)
      {
      *tif->tif_rawcp++ = bit >> 8;
      *tif->tif_rawcp++ = bit;
      tif->tif_rawcc += 2;
      }
   return used;
   }


static void save_state (state_info *state, TIFF *tiff, Fax3EncodeState *sp)
   {
   state->tif_rawcp = tiff->tif_rawcp;
   state->tif_rawcc = tiff->tif_rawcc;
   state->data = sp->data;
   state->bit = sp->bit;
   }


static void restore_state (state_info *state, TIFF *tiff, Fax3EncodeState *sp)
   {
   tiff->tif_rawcp = state->tif_rawcp;
   tiff->tif_rawcc = state->tif_rawcc;
   sp->data = state->data;
   sp->bit = state->bit;
   }


// encode type

enum
  {
  ENCODE_uncomp,   // uncompressed line
  ENCODE_single,   // single value line (list of shorts)
  ENCODE_comp,     // G4 compressed line
  ENCODE_blank     // blank line
  };


/*
 * Encode the requested amount of data.
 */
static int
Fax4Encode(TIFF* tif, tidata_t bp, tsize_t cc, tsample_t s, int debug_max_steps)
   {
   Fax3EncodeState *sp = EncoderState(tif);
   state_info state;
   int bad;
   int blank;
   int blank_count = 0;
   byte *ptr, *end;
   int step = 0, nbits;
   int type;

   (void) s;
   debug2 (("Fax4Encode: rowbytes=%d\n", sp->b.rowbytes));
   while ((long)cc > 0 && step < debug_max_steps)
      {
      debug3 (("\n^%d: ", tif->tif_row));

      blank = 1;
      end = bp + sp->b.rowbytes;
      for (ptr = bp; ptr < end; ptr++)
         if (*ptr)
            {
            debug3 (("   data[0x%x]=0x%x - not blank\n", ptr - bp, *ptr));
            blank = 0;
            break;
            }
      if (blank)
         {
         if (blank_count == 63)
            {
            step++;
            Fax3PutBits (tif, 0xc0 | blank_count, 8);  // write 8 bits
            blank_count = 0;
            }
         memset (sp->refline, '\0', sp->b.rowbytes); //!
         blank_count++;
         }

      if (!blank || cc == 1)  // not blank, or last line
         {
         // write out blank lines
         if (blank_count)
            {
            step++;
            Fax3PutBits (tif, 0xc0 | blank_count, 8);  // write 8 bits
            blank_count = 0;
            }
         }

      save_state (&state, tif, sp);
      if (!blank)
         {
         step++;
         type = ENCODE_comp;

         // write the 2-bit code
         Fax3PutBits (tif, ENCODE_comp, 2);  // 2 means compressed data

         tif->bit_count = 0;
         if (!Fax3Encode2DRow(tif, bp, sp->refline, sp->b.rowpixels, &bad)) //!
            return (0);

         debug3 (("bits_sent = 0x%x\n", tif->bit_count));

         // work out how many bits would be sent in single mode
         nbits = try_single (tif, bp, sp->b.rowbytes, false);
         debug3 (("single mode would use 0x%x\n", nbits));

         if (nbits - 18 < tif->bit_count)
            type = ENCODE_single;
         else
            nbits = tif->bit_count;

         if (nbits > (int)sp->b.rowpixels)
            type = ENCODE_uncomp;

         switch (type)
            {
            case ENCODE_comp :
               // already done
               break;

            case ENCODE_uncomp :
               debug2 (("**************** to big: writing uncompressed\n"));
               // write uncompressed
               restore_state (&state, tif, sp);
               step++;
               Fax3PutBits (tif, type, 2);  // 0 means uncompressed data
               if (sp->bit != 8)
                  Fax3FlushBits(tif, sp);
               memcpy (tif->tif_rawcp, bp, sp->b.rowbytes);
               tif->tif_rawcp += sp->b.rowbytes;
               tif->tif_rawcc += sp->b.rowbytes;
               //       sp->refline [sp->b.rowbytes] = 0;   //!
               break;

            case ENCODE_single :
               restore_state (&state, tif, sp);
               step++;
               Fax3PutBits (tif, type, 2);  // 1 means single data
               if (sp->bit != 8)
                  Fax3FlushBits(tif, sp);
               try_single (tif, bp, sp->b.rowbytes, true);
               break;
            }
         //            _TIFFmemcpy(sp->refline, bp, sp->b.rowbytes + 1); //!
         _TIFFmemcpy(sp->refline, bp, sp->b.rowbytes); //!
         }
//              bp += sp->b.rowbytes;
      bp += sp->b.stride;
//              cc -= sp->b.rowbytes;
      cc--;
      if (cc != 0)
         tif->tif_row++;
//         debug3 (("\n"));
      }
   debug2 (("Fax4Encode: done\n"));
   return (1);
   }


static int
Fax3PostEncode(TIFF* tif)
   {
   Fax3EncodeState* sp = EncoderState(tif);

   if (sp->bit != 8)
       Fax3FlushBits(tif, sp);
   return (1);
   }


/*
 * Group 3 and Group 4 Decoding.
 */

/*
 * These macros glue the TIFF library state to
 * the state expected by Frank's decoder.
 */
#define  DECLARE_STATE(tif, sp, mod)               \
    static const char module[] = mod;              \
    Fax3DecodeState* sp = DecoderState(tif);          \
    int a0;          /* reference element */    \
    int lastx = sp->b.rowpixels; /* last element in row */  \
    unsigned BitAcc;       /* bit accumulator */      \
    int BitsAvail;         /* # valid bits in BitAcc */  \
    int RunLength;         /* length of current run */   \
    u_char* cp;            /* next byte of input data */ \
    u_char* ep;            /* end of input data */    \
    uint16_t* pa;            /* place to stuff next run */ \
    uint16_t* thisrun;       /* current row's run array */ \
    int EOLcnt;            /* # EOL codes recognized */  \
    const u_char* bitmap = sp->bitmap; /* input data bit reverser */ \
    const TIFFFaxTabEnt* TabEnt
#define  DECLARE_STATE_2D(tif, sp, mod)               \
    DECLARE_STATE(tif, sp, mod);             \
    int b1;          /* next change on prev line */   \
    uint16_t* pb          /* next run in reference line */\
/*
 * Load any state that may be changed during decoding.
 */
#define  CACHE_STATE(tif, sp) do {              \
    BitAcc = sp->data;                    \
    BitsAvail = sp->bit;                  \
    EOLcnt = sp->EOLcnt;                  \
    cp = (unsigned char*) tif->tif_rawcp;          \
    ep = cp + tif->tif_rawcc;                \
} while (0)
/*
 * Save state possibly changed during decoding.
 */
#define  UNCACHE_STATE(tif, sp) do {               \
    sp->bit = BitsAvail;                  \
    sp->data = BitAcc;                    \
    sp->EOLcnt = EOLcnt;                  \
    tif->tif_rawcc -= (tidata_t) cp - tif->tif_rawcp;       \
    tif->tif_rawcp = (tidata_t) cp;             \
} while (0)

static err_info *encode_g4 (byte *out, int tile_line_bytes, byte *end,
                   cpoint *tile_size, byte *ptr, int stride, int *sizep,
                   int debug_max_steps)
   {
   TIFF stif, *tif = &stif;
   Fax3EncodeState sp;
   int ok;

   // set up tiff environment
   tif->tif_data = (tidata_t)&sp;
   tif->tif_row = 0;
   tif->tif_rawcc = 0;
   tif->tif_rawcp = out;
   tif->tif_rawdatasize = end - out;

   CALL (mem_allocz (CV &sp.refline, tile_line_bytes + 4, "encode_g4")); //!

   sp.b.rowpixels = tile_size->x;
   sp.b.rowbytes = tile_line_bytes;
   sp.b.groupoptions = GROUP3OPT_2DENCODING;
   sp.b.stride = stride;

   Fax3PreEncode(tif, 0);

   // do it
   ok = Fax4Encode (tif, ptr, tile_size->y, 0, debug_max_steps);
   if (!ok)
      return err_make ("encode_g4", ERR_g4_compression_failed);

   Fax3PostEncode(tif);

   debug2 (("tif: encoded to %ld bytes\n", tif->tif_rawcc));
   *sizep = tif->tif_rawcc;

   mem_free (CV &sp.refline);

   // all ok
   return NULL;
   }


err_info *encode_tile (chunk_info &chunk, encode_info &encode, int *sizep,
              byte *ptr, cpoint *tile_size, int stride, int bpp,
              int tile_line_bytes, int debug_max_steps)
   {
   byte *out = encode.buff, *end = encode.buff + encode.size;
   int size;
//   int y;

   end = end; ptr = ptr;

   debug2 (("tile_line_bytes=%d, chunk->line_bytes=%d, stride=%d\n",
          tile_line_bytes, chunk.line_bytes, stride));

   // encode a tile
   switch (bpp)
      {
      case 1 :
         CALL (encode_g4 (out, tile_line_bytes, end, tile_size, ptr, stride,
                   &size, debug_max_steps));
         break;

      case 8 :
      case 24 :
      case 32 :
         size = encode.size;
         jpeg_encode (ptr, tile_size, out, &size, bpp, stride,
              JPEG_QUALITY);
         break;
      }

   // get size, rounding up to word boundary
//   *sizep = out - encode->buff;
   while (size & 3)
      size++;
   *sizep = size;
   return NULL;
   }


static int build_tiledata (chunk_info &chunk, int stride, int bpp,
                           debug_info &debug)
   {
   int x, y, size;
   int ret, temp;
   cpoint tile_size;
   byte *ptr;
   encode_info encode;
   tile_info *tile;
   err_info *e;

   // ensure image width is a multiple of 32 bits
   chunk.line_bytes = (chunk.line_bytes + 3) & ~3;

   debug2 (("image size %d x %d\n", chunk.image_size.x, chunk.image_size.y));

   // allocate enough memory for encoding each tile
   encode.size = chunk.tile_size.x * chunk.tile_size.y;  // should be enough
   encode.buff = (byte *)malloc (encode.size);
   if (!encode.buff)
      return ERR (-ENOMEM);

   calc_tile_bytes (chunk.tile_size.x, chunk.image_size.x, bpp,
                    &encode.tile_line_bytes, &temp, false);

   tile = chunk.tile;
   for (y = 0, ret = 0; !ret && y < chunk.tile_extent.y; y++)
      for (x = 0; x < chunk.tile_extent.x; x++, tile++)
         {
         int tilenum;
         int tile_line_bytes;

         // recalculate tile_line_bytes each time
         ptr = get_tile_size (chunk, x, y, &tile_size, &tilenum, stride,
                  encode.tile_line_bytes);

         calc_tile_bytes (tile_size.x, chunk.image_size.x, bpp,
                    &tile_line_bytes, &temp, false);
//       printf ("tile_size.x=%d, encode.tile_line_bytes=%d, tlb=%d\n",
//               tile_size.x, encode.tile_line_bytes, tile_line_bytes);
         if (tilenum >= debug.start_tile
            && (debug.num_tiles == INT_MAX
                || tilenum < debug.start_tile + debug.num_tiles))
            {
            debug2 (("encoding tile %d (%d, %d), bpp %d, size %d x %d (0x%x x 0x%d)\n", tilenum, x, y,
                  bpp, tile_size.x, tile_size.y, tile_size.x, tile_size.y));
            e = encode_tile (chunk, encode, &size, ptr, &tile_size, stride,
                  bpp, tile_line_bytes, debug.max_steps);
            if (e)
               {
               printf ("encode_tile() failed, tile %d\n", tilenum);
               break;
               }
            if (size > encode.size)
               {
               printf("Encode size %d, buffer only %d\n", size, encode.size);
               return ERR (-ENOMEM);
               }
            tile->size = size + 4;
            tile->buf = (byte *)malloc (tile->size);
            if (!tile->buf)
               {
               ret = -ENOMEM;
               break;
               }

            // add header data
            *(short *)tile->buf = tilenum;
            *(short *)(tile->buf + 2) = 0x43;
            memcpy (tile->buf + 4, encode.buff, size);

            debug2 (("tile %d,%d: encoded to %d bytes: %p\n", x, y, tile->size,
                     tile->buf));
            }
         else
            {
            static unsigned zero;

            printf ("skip %d: %d-%d\n", tilenum, debug.start_tile,
                    debug.num_tiles);
            tile->buf = (byte *)&zero;
            tile->size = 4;
            }
         }
   free (encode.buff);
   return ret;

   }


// allocate memory for a part of given size, returning 1 if all ok

static int alloc_part_buf (part_info &part, int size)
   {
   part.buf = (byte *)malloc (size);
   if (!part.buf)
      return false;
   memset (part.buf, '\0', size);
   part.size = size;
   return true;
   }


static int add_colourmap (chunk_info &, part_info &part)
   {
   if (!alloc_part_buf (part, 0x41c))
      return ERR (-ENOMEM);
   *(unsigned *)part.buf = 0x41c;
   part.buf [8] = 2;
   *(unsigned *)(part.buf + 32) = 0xffffff;
   return 0;
   }


/* run-length-encode the input buffer of given size to out. Returns the
number of resulting bytes */

static int rle_encode (byte *in, int size, byte *out_buff)
   {
   int ch, run;
   int count;
   byte *p, *end = in + size, *start, *out = out_buff;
   int debug_count = 0;

   /* we need to search for runs of:
         0, encoded as 0
         255, encoded as 1
         anything that isn't 0 or 255, encoded as 2 */
   p = in;
   count = 0;
   for (start = p; p <= end; p++)
      {
      // get the next byte
      if (p < end)
         {
         ch = *p;
         if (ch == 255)
            ch = 1;
         else if (ch != 0)
            ch = 2;
         }

      // start a new run?
      if (!count)
         run = ch;

      // end of a run, or end of data?
      if (p == end || ch != run || count == 63) // end the run
         {
         debug_count++;
         if (debug_count < DEBUG_MAX_COUNT)
            printf ("count=%d, run=%d\n", count, run);
         *out++ = (run << 6) | count;

         // output data if required
         if (run == 2)
            {
            if (debug_count < DEBUG_MAX_COUNT)
               printf ("   :");
            for (; start < p; start++)
               {
               if (debug_count < DEBUG_MAX_COUNT)
                  printf ("%x ", *start);
               *out++ = *start;
               }
            if (debug_count < DEBUG_MAX_COUNT)
               printf ("\n");
            }
         count = 1;
         run = ch;
         start = p;  // start of next run
         }

      // otherwise inc the count
      else
         count++;
      }

   // return length
   return out - out_buff;
   }


static int add_preview (chunk_info &chunk, part_info &part)
   {
   byte *dest;
   int len;

   switch (chunk.bits)
      {
      case 1 : // RLE
         {
         // allocate plenty of space for worst case?
         dest = (byte *)malloc (chunk.preview_size.x * chunk.preview_size.y);
         len = rle_encode (chunk.preview, chunk.preview_bytes, dest);
         break;
         }

      case 8 :
         dest = (byte *)malloc (4);
         len = 4;
         *(int *)dest = 0;
         printf ("cannot add_preview for %dbpp\n", chunk.bits);
         break;

      case 24 :
         dest = (byte *)malloc (chunk.preview_bytes);
         memcpy (dest, chunk.preview, chunk.preview_bytes);
         len = chunk.preview_bytes;
         break;
      }
   if (!alloc_part_buf (part, len))
      return ERR (-ENOMEM);
   memcpy (part.buf, dest, len);
   debug1 (("preview size %d, %d->%d\n", chunk.preview_bytes,
            chunk.preview_size.x * 2
             * chunk.preview_size.y / 8, len));
   debug2 (("preview bytes: %x %x %x %x\n", dest [0], dest [1], dest [2], dest [3]));
   free (dest);
   return 0;
   }


static int add_notes (chunk_info &chunk, part_info &part)
   {
   chunk = chunk;
   if (!alloc_part_buf (part, 0x66))
      return ERR (-ENOMEM);
   *(int *)part.buf = 0x66;
   return 0;
   }


static int add_tileinfo (chunk_info &chunk, part_info &part)
   {
   unsigned short *info;
   int i, size;

   if (!alloc_part_buf (part, 0xe + 10 * chunk.tile_count))
      return ERR (-ENOMEM);
   info = (unsigned short *)part.buf;
   info [0] = 0xe;      //?
   info [1] = 0x9845;   //?
   info [2] = chunk.bits == 1 ? chunk.tile_size.x / 8 : chunk.tile_size.x;
   info [3] = chunk.tile_size.y;
   info [4] = chunk.tile_extent.x;
   info [5] = chunk.tile_extent.y;
   debug2 (("tile_size %d x %d\n", chunk.tile_size.x, chunk.tile_size.y));
   info [6] = chunk.tile_count;
   info += 7;
   for (i = 0; i < chunk.tile_count; i++, info += 5)
      {
      info [0] = 2;       //?
      // use hw access to avoid writing a word to a non-word-aligned location
      size = chunk.tile [i].size;
      info [1] = size & 0xffff;
      info [2] = size >> 16;
//      printf ("tile %d, size %d\n", i, size);
      info [3] = 0x8000;  //?
      info [4] = 0;       //?
      }
   return 0;
   }


static int add_tiledata (chunk_info &chunk, part_info &part)
   {
   byte *ptr;
   int size;
   int i;
   tile_info *tile;

   // work out the total tile data size
   for (i = size = 0, tile = chunk.tile; i < chunk.tile_count; i++, tile++)
      size += tile->size;

   // allocate
   if (!alloc_part_buf (part, size))
      return ERR (-ENOMEM);

   // stuff in the data
   for (i = 0, tile = chunk.tile, ptr = part.buf; i < chunk.tile_count; i++, tile++)
      {
      memcpy (ptr, tile->buf, tile->size);
      ptr += tile->size;
      }
   return 0;
   }


static void add_generic_chunk_header (chunk_info &chunk, byte *buf)
   {
   unsigned short *ptr = (unsigned short *)buf;

   ptr [0] = 0x5a56;
   ptr [1] = chunk.size;
   ptr [2] = chunk.size >> 16;
   ptr [3] = chunk.chunkid;
   ptr [4] = chunk.type == CT_bermuda ? 0 : 1;
   ptr [5] = chunk.textflag;
   ptr [6] = chunk.type == CT_image || chunk.type == CT_bermuda ? 0 : 1;
   ptr [7] = 0;
   ptr [8] = 0;
   ptr [9] = 0;
   ptr [10] = 0;
   ptr [11] = 0;
   ptr [12] = 0;
   ptr [13] = chunk.flags;
   ptr [14] = chunk.titletype;
   ptr [15] = 0;
   }


static int add_image_header (chunk_info &chunk, byte *buf)
   {
   unsigned short *ptr = (unsigned short *)buf;
   unsigned *iptr;
   int i;

   add_generic_chunk_header (chunk, buf);

   ptr [16] = 0x88;
   ptr [17] = 0x88;
   ptr [18] = 0;
   ptr [19] = 1;
   ptr [20] = 0;

   // now add image-specific stuff
   ptr = (unsigned short *)(buf + POS_image_size_x);
   ptr [0] = chunk.image_size.x;
   ptr [1] = chunk.image_size.y;
   ptr [2] = chunk.dpi.x;
   ptr [3] = chunk.dpi.x;
   ptr [4] = 0; //?
   ptr [5] = chunk.bits;
   ptr [6] = chunk.channels;
   ptr [7] = chunk.preview_size.x;
   ptr [8] = chunk.preview_size.y;
   ptr [9] = 0x21; //?
   ptr [10] = 2; //?
   ptr [11] = 0x53d; //?
   ptr [12] = chunk.parts.size ();

   iptr = (unsigned *)(buf + POS_part0);
   assert (!((long int)iptr & 3));
   for (i = 0; i < chunk.parts.size (); i++, iptr += 2)
      {
      part_info &part = chunk.parts [i];

      debug2 (("write to 0x%lx: start=%x, size=%x\n", (long int)((byte *)iptr - buf),
                  part.start, part.size));
      iptr [0] = part.start;
      iptr [1] = part.size;
      }
   return 0;
   }


err_info *Filemax::max_replace_page (page_info &page, Filemaxpage &mp)
   {
   page_info newpage;

   page_init (page);
   page.titlestr = mp.name ();
   page.chunkid = _chunkid_next;
   CALL (max_setup_page (newpage, mp));
   free_page (page);
   page = newpage;
   return flush ();
   }

err_info *Filemaxpage::compress (void)
   {
   int err = 0;

   byte *buf = (byte *)_data.constData ();
   int size = _size;

   debug_level = _debug.level;
   debugf = _debug.logf;

//   printf ("compress_page %dx%dx%d @%d\n", _width, _height, _depth,
//         _stride);
   // for JPEG, do the thumbnailing early in case we detect a shortfall in data
   if (_jpeg)
      jpeg_thumbnail (buf, size, &_chunk.preview, &_chunk.preview_bytes, &_chunk.preview_size);

   _chunk.textflag = CT_text ? 0 : 3;
   _chunk.titletype = 0;  //?
   _chunk.flags = CHUNKF_image;
   _chunk.type = CT_image;
   _chunk.used = true;
   _chunk.image_size.x = _width;
   _chunk.image_size.y = _height;
   if (!_jpeg)
      {
      _chunk.preview_size.x = _chunk.image_size.x / PREVIEW_SCALE;
      _chunk.preview_size.y = _chunk.image_size.y / PREVIEW_SCALE;
      }
   _chunk.dpi.x = _chunk.dpi.y = 300;           // dots per inch
   _chunk.bits = _depth;
   if (_chunk.bits == 32)
      _chunk.bits = 24;
   _chunk.channels = 1;

   if (_jpeg)
      {
      // JPEG compressed images have only 1 tile
      _chunk.tile_size = _chunk.image_size;
      _chunk.tile_extent.x = _chunk.tile_extent.y = 1;
      }
   else
      calc_tiles (&_chunk.image_size, &_chunk.tile_size, &_chunk.tile_extent);
   debug1 (("tiles %dx%d of %dx%d, 1 channels \n",
      _chunk.tile_extent.x, _chunk.tile_extent.y,
      _chunk.tile_size.x, _chunk.tile_size.y));
   _chunk.tile_count = _chunk.tile_extent.x * _chunk.tile_extent.y;

   CALL (mem_allocz (CV &_chunk.tile, sizeof (tile_info) * _chunk.tile_count,
             "max_compress_page1"));

   // calculate number of bytes across in a tile and a whole line
   calc_tile_bytes (_chunk.tile_size.x, _chunk.image_size.x, _chunk.bits,
                    &_chunk.tile_line_bytes, &_chunk.line_bytes, true);

   Filemax::part_resize (_chunk.parts, 5);

   // if it's a JPEG, just stuff it straight in
   if (_jpeg)
      {
      tile_info *tile;

      tile = _chunk.tile;
      tile->size = size + 4;
      tile->buf = (byte *)malloc (tile->size);
      if (!tile->buf)
         return err_make (ERRFN, ERR_out_of_memory_bytes1, tile->size);
      *(short *)tile->buf = 0; // tile number
      *(short *)(tile->buf + 2) = 0x43;  // data type
      memcpy (tile->buf + 4, buf, size);
      }

   // if not a JPEG, we need to compress it
   else
      {
      _chunk.image = buf;
      _chunk.image_bytes = size;

      // create the preview
      err |= build_preview (_chunk, _stride, _depth);

      // and the compressed tile data
      debug_level = _debug.level;
   //   printf ("_depth=%d\n", _depth);
      err |= build_tiledata (_chunk, _stride, _depth, _debug);
      }

   // set up the chunks
   err |= add_colourmap (_chunk, _chunk.parts [0]);
   err |= add_preview (_chunk, _chunk.parts [1]);
   err |= add_notes (_chunk, _chunk.parts [2]);
   err |= add_tileinfo (_chunk, _chunk.parts [3]);
   err |= add_tiledata (_chunk, _chunk.parts [4]);

   //! should check err each time above

   // work out the total data size
   _size = POS_part0 + 8 * _chunk.parts.size ();
   for (int i = 0; i < _chunk.parts.size (); i++)
      {
      part_info &part = _chunk.parts [i];

      part.start = _size - 0x20;
      debug2 (("part %d: pos %x size: %x\n", i, part.start, part.size));
      _size += part.size;
      }
   _size = ALIGN_CHUNK (_size);
   _chunk.size = _size;
   debug2 (("_size=%x\n", _size));
   _maxdata = NULL;

   /* the caller will free this supplied image, so remove it from our
      structure, otherwise we will free it too! */
   _chunk.image = NULL;
   return 0;
   }


static err_info *alloc_chunk_buf (chunk_info &chunk, byte **bufp)
   {
   CALL (mem_allocz (CV &chunk.buf, chunk.size, "alloc_chunk_buf"));
   *bufp = chunk.buf;
   return NULL;
   }


static void write_roswell (byte *buf, page_info &page)
   {
   unsigned short *data = (unsigned short *)buf;
   unsigned *idata = (unsigned *)buf;

//    printf ("write_roswell page %d, timestamp %d\n", page.title, page.timestamp);
   idata [POSn_roswell_timestamp / 4] = page.timestamp.toTime_t ();

   // curse of the unaligned positions
   data [0x42 / 2] = page.image;
   data [0x42 / 2 + 1] = page.image >> 16;
   idata [0x8c / 4] = page.title;
   }


void Filemax::write_bermuda (byte *buf)
   {
   unsigned short *data = (unsigned short *)buf;
   int i;

   data [0x20 / 2] = _pages.size ();
   data += 0x36 / 2;
   for (i = 0; i < _pages.size (); i++, data += 0xc / 2)
      {
      page_info &page = _pages [i];

      data [0] = page.chunkid;  // chunkid same as page number
      data [4 / 2] = page.roswell;
      data [4 / 2 + 1] = page.roswell >> 16;
      }
   }


static void write_annot (byte *buf, int size, QStringList &annot_data)
   {
   int type, upto, len;
   unsigned *data = (unsigned *)(buf + POS_annot_start); // unaligned

   for (type = 0, upto = 0x45; type < File::Annot_count;
         type++, upto += len, data += POS_annot_offset / 4)
      {
      QString &str = annot_data [type];
      len = str.length () + 1;
      Q_ASSERT (upto + POS_chunk_header_size + len < size);
      data [POS_annot_pos / 4] = upto;
      data [POS_annot_size / 4] = len;
      strcpy ((char *)buf + upto + POS_chunk_header_size, str.toLatin1 ());
      }
   }


static void write_env (byte *buf, int size, QStringList &env_data)
   {
   int type, upto, len;

   *(buf + POS_env_count) = env_data.size ();
   for (type = 0, upto = POS_env_string0; type < env_data.size ();
         type++, upto += len)
      {
      QString &str = env_data [type];
      len = str.length () + 1;
      Q_ASSERT (upto + POS_chunk_header_size + len < size);
      strcpy ((char *)buf + upto, qPrintable (str));
      }
   }


void Filemax::write_max_header (void)
   {
   unsigned *idata = (unsigned *)_hdr.data ();

   memset (_hdr.data (), '\0', _hdr.size ());

   // hack so we can distinguish our own .max files
   _signature = 0x12345678;

   idata [0] = _signature;
   idata [POSn_version / 4] = MAX_VERSION; //_version;
   idata [0x5c / 4] = _pages.size ();
   idata [0x6c / 4] = _chunks.size ();
   idata [0x88 / 4] = _chunk0_start;
   idata [0xa4 / 4] = _bermuda;
   idata [0xa8 / 4] = _tunguska;
   idata [0xac / 4] = _annot;
   idata [0xb8 / 4] = _trail;
   idata [POS_envelope / 4] = _trail;
   }

/* converts the information in the various chunk->... variables into
a buffer of chunk data in chunk->buf. What is actually written there
depends on the type of the chunk */
err_info *Filemax::build_chunk (chunk_info &chunk, bool force)
   {
   byte *buf;

   // if we already have data, just update the 'used' flag
   if (chunk.buf)
      chunk.buf [8] = chunk.used;

   if (force || !chunk.buf) switch (chunk.type)
      {
      case CT_bermuda :
         chunk.size = ALIGN_CHUNK (POS_chunk_header_size
                     + 0x16 + 0xc * _pages.size ());
         //TODO: need to free old buf here
         CALL (alloc_chunk_buf (chunk, &buf));
         add_generic_chunk_header (chunk, buf);
         write_bermuda (buf);
         break;

      case CT_annot :
         {
         int type, count = 0;

         Q_ASSERT (_annot_loaded);
         for (type = 0; type < Annot_count; type++)
            count += 1 + strlen (_annot_data [type].toLatin1());
         chunk.size = ALIGN_CHUNK (POS_chunk_header_size
                     + 0x45 + count);
         //TODO: need to free old buf here
         CALL (alloc_chunk_buf (chunk, &buf));
         add_generic_chunk_header (chunk, buf);
         write_annot (buf, chunk.size, _annot_data);
         break;
         }

      case CT_env :
         {
         int type, count = 0;

         Q_ASSERT (_env_loaded);
         for (type = 0; type < Env_count; type++)
            count += 1 + strlen (_env_data [type].toLatin1());
         chunk.size = ALIGN_CHUNK (POS_chunk_header_size
                     + POS_env_string0 + count);
         //TODO: need to free old buf here
         CALL (alloc_chunk_buf (chunk, &buf));
         add_generic_chunk_header (chunk, buf);
         write_env (buf, chunk.size, _env_data);
         break;
         }

      default :
         printf ("can't make chunk type %d\n", chunk.type);
         break;
      }
   return NULL;
   }


err_info *Filemax::remove_chunk (chunk_info &chunk)
   {
   // ensure we have loaded it
   CALL (read_chunk_buf (chunk));

   chunk.used = 0;   // get rid of this chunk, it's too small
   chunk.saved = false;  // will be saved back to disc
   return NULL;
   }


err_info *Filemax::remove_chunknum (int pos)
   {
   chunk_info *chunk;

   if (!pos)
      return NULL;
   CALL (chunk_find (pos, chunk, NULL));

   // ensure we have loaded it
   CALL (read_chunk_buf (*chunk));

   chunk->used = 0;   // get rid of this chunk, it's too small
   chunk->saved = false;  // will be saved back to disc
   return NULL;
   }


err_info *Filemax::restore_chunknum (int pos)
   {
   chunk_info *chunk;

   if (!pos)
      return NULL;
   CALL (chunk_find (pos, chunk, NULL));

   // ensure we have loaded it
   CALL (read_chunk_buf (*chunk));

   chunk->used = 1;   // restore this chunk
   chunk->saved = false;  // will be saved back to disc
   return NULL;
   }


void Filemax::page_init (page_info &page)
   {
   page.chunkid = page.roswell = 0;
   page.have_roswell = false;
   page.image = page.noti1 = page.noti2 = page.title = page.text = 0;
   page.title_loaded = page.title_saved = false;
   }


void Filemax::page_resize (QVector<page_info> &pages, int count)
   {
   // check we don't need to dealloc anything
   Q_ASSERT (pages.count () == 0);

   for (int i = 0; i < count; i++)
      {
      page_info page;

      page_init (page);
      pages << page;
      }
   }


void Filemax::chunk_init (chunk_info &chunk)
   {
   chunk.start = chunk.size = chunk.chunkid = chunk.flags = chunk.textflag = 0;
   chunk.titletype = chunk.type = 0;
   chunk.used = 0;
   chunk.image_size.x = chunk.image_size.y = 0;
   chunk.preview_size.x = chunk.image_size.y = 0;
   chunk.dpi.x = chunk.dpi.y = 0;
   chunk.bits = chunk.channels = 0;
   chunk.tile_size.x = chunk.tile_size.y = 0;
   chunk.tile_extent.x = chunk.tile_extent.y = 0;
   chunk.tile_count = chunk.tile_line_bytes = 0;
   chunk.line_bytes = 0;
   chunk.tile = 0;
   chunk.image_start = chunk.code = 0;
   chunk.after_part = 0;
   chunk.image = NULL;
   chunk.image_bytes = 0;
   chunk.preview = NULL;
   chunk.preview_bytes = 0;
   chunk.buf = NULL;
   chunk.loaded = chunk.saved = false;
   }


void Filemax::part_init (part_info &part)
   {
   part.start = part.size = part.raw_size = 0;
   part.buf = NULL;
   }


void Filemax::part_resize (QVector<part_info> &parts, int count)
   {
   // check we don't need to dealloc anything
   Q_ASSERT (parts.count () == 0);

   for (int i = 0; i < count; i++)
      {
      part_info part;

      part_init (part);
      parts << part;
      }
   }


void Filemax::chunk_resize (QVector<chunk_info> &chunks, int count)
   {
   // check we don't need to dealloc anything
   Q_ASSERT (chunks.count () == 0);

   for (int i = 0; i < count; i++)
      {
      chunk_info chunk;

      chunk_init (chunk);
      chunks << chunk;
      }
   }


err_info *Filemax::alloc_chunk (int chunk_size,
         chunk_info **chunkp)
   {
   chunk_info chunk;

   chunk_init (chunk);
   chunk.start = _size;
   chunk.size = chunk_size;
   _size += chunk.size;
//    printf ("new chunk at %d, size %d\n", chunk.start, chunk.size);
   _chunks << chunk;
   *chunkp = &_chunks.last (); // [_chunks.size () - 1];
   return NULL;
   }


err_info *Filemax::insert_chunk (chunk_info &new_chunk, int *posp)
   {
   chunk_info *chunk = NULL;
   int oldpos = posp ? *posp : 0;

   // if there is an existing chunk, check it
   if (oldpos)
      {
      CALL (chunk_find (oldpos, chunk, NULL));
//      printf ("chunk %d, old size = %d, ", chunk - _chunk, chunk->size);
      if (new_chunk.size > chunk->size)
         {
         // chunk too small, so throw it away
         remove_chunk (*chunk);
         oldpos = 0;
         chunk = NULL;
         }

      // otherwise expand our new chunk if necessary
      // we can't cope with any gaps!
      else
         {
         unsigned short *ptr = (unsigned short *)new_chunk.buf;

         new_chunk.size = chunk->size;
         ptr [1] = chunk->size;
         ptr [2] = chunk->size >> 16;
         }
      }

   // allocate a new chunk if required
   if (!chunk)
      {
      CALL (alloc_chunk (new_chunk.size, &chunk));
      oldpos = chunk->start;
      }

   if (chunk->buf)
      mem_free (CV &chunk->buf);

   // move in new chunk data
   *chunk = new_chunk;

   // place it
   chunk->start = oldpos;
   chunk->loaded = true;
   chunk->saved = false;

//      printf ("new size (%d) = %d\n", chunk - _chunk, chunk->size);
   // all ok
   *posp = oldpos;
   return NULL;
   }


/* creates or updates a bermuda chunk. If one already exists (_bermuda
non-zero), then updates it. But if it is too large to fit in the currently
allocated area, it will mark the old one as unused and create a new one */
err_info *Filemax::create_bermuda (void)
   {
   chunk_info chunk;

   chunk_init (chunk);
   chunk.type = CT_bermuda;
   chunk.flags = CHUNKF_bermuda;

   CALL (build_chunk (chunk));

   CALL (insert_chunk (chunk, &_bermuda));
   return 0;
   }


/* creates or updates an annot chunk. If one already exists (_annot
non-zero), then updates it. But if it is too large to fit in the currently
allocated area, it will mark the old one as unused and create a new one */
err_info *Filemax::create_annot (void)
   {
   chunk_info chunk;

   chunk_init (chunk);
   chunk.type = CT_annot;
   chunk.flags = CHUNKF_annot;

   CALL (build_chunk (chunk));

   CALL (insert_chunk (chunk, &_annot));
   return 0;
   }


/* creates or updates an envelope chunk. If one already exists (_env
non-zero), then updates it. But if it is too large to fit in the currently
allocated area, it will mark the old one as unused and create a new one */
err_info *Filemax::create_envelope (void)
   {
   chunk_info chunk;

   chunk_init (chunk);
   chunk.type = CT_env;
   chunk.flags = CHUNKF_env;

   CALL (build_chunk (chunk));

   CALL (insert_chunk (chunk, &_envelope));
   return 0;
   }


err_info *Filemax::create_roswell (page_info &page)
   {
   chunk_info chunk;
   byte *buf;

   chunk_init (chunk);
   chunk.chunkid = page.chunkid;
   chunk.type = CT_roswell;
   chunk.size = 0x1a0;  // always this size?
   chunk.textflag = 0;
   chunk.flags = CHUNKF_roswell;
   chunk.titletype = 0;
   CALL (alloc_chunk_buf (chunk, &buf));
   add_generic_chunk_header (chunk, buf);
   write_roswell (buf, page);

   CALL (insert_chunk (chunk, &page.roswell));

   return NULL;
   }


err_info *Filemax::create_title (page_info &page)
   {
   chunk_info chunk;
   byte *buf;

   if (!page.have_roswell && page.roswell)
      CALL (page_read_roswell (page));
   chunk_init (chunk);
   chunk.chunkid = page.chunkid;
   chunk.type = CT_title;
   chunk.size = ALIGN_CHUNK (POS_chunk_header_size
                     + strlen (page.titlestr.toLatin1()) + 1);
   chunk.textflag = 0;  // not text, is title
   chunk.flags = CHUNKF_text;
   chunk.titletype = 0x80;  // page title
   CALL (alloc_chunk_buf (chunk, &buf));
   add_generic_chunk_header (chunk, buf);
   strcpy ((char *)buf + POS_chunk_header_size, page.titlestr.toLatin1());
   CALL (insert_chunk (chunk, &page.title));
   return NULL;
   }

err_info *Filemax::page_add (int chunkid, const QString &titlestr,
          page_info *&pagep)
   {
   page_info page;

//   printf ("add page %d, chunkid %d\n", _pages.size (), pagenum);
   page_init (page);
   page.titlestr = titlestr;
   page.chunkid = chunkid;
   _pages << page;
   pagep = &_pages.last (); //[_pages.size () - 1];
   return NULL;
   }

err_info *Filemax::write_max (FILE *f)
   {
   int i;

   debug1 (("write_max, hdr_size=0x%x, chunks=%d\n", _hdr.size (),
          _chunks.size ()));

   // first write the header
   fseek (f, 0, SEEK_SET);

   if ((int)fwrite (_hdr.data (), 1, _hdr.size (), f) != _hdr.size ())
      return err_make (ERRFN, ERR_failed_to_write_bytes1, _hdr.size ());

   // and then the chunks one by one
   for (i = 0; i < _chunks.size (); i++)
      {
      chunk_info &chunk = _chunks [i];

      debug2 (("%d  %lx  %x  %x\n", i, ftell (f), chunk.start, chunk.size));
      if ((int)fwrite (chunk.buf, 1, chunk.size, f) != chunk.size)
         return err_make (ERRFN, ERR_failed_to_write_bytes1, chunk.size);
      }
   debug1 (("size %lx %x\n", ftell (f), _size));

   // done
   return NULL;
   }

/* read a chunk at 'chunk_pos' from 'src' and append it to 'dest' */

err_info *Filemax::merge_chunk (Filemax *src, int chunk_pos, int *new_pos)
   {
   chunk_info *srcchunk, *destchunk;
   byte *data;

   // do nothing if no chunk
   if (!chunk_pos)
      return NULL;

   // find source chunk
   CALL (src->chunk_find (chunk_pos, srcchunk, NULL));

   // read source chunk
   if (!srcchunk->loaded)
      CALL (src->read_chunk (*srcchunk, chunk_pos));

   // create new chunk in destchunk
   CALL (alloc_chunk (srcchunk->size, &destchunk));
   // destchunk->loaded, ->saved will be 0

   // copy data
   CALL (src->max_cache_data (src->_cache, chunk_pos, srcchunk->size, srcchunk->size, &data));

   // update chunkid
   *(unsigned short *)(data + 6) = _chunkid_next;
   CALL (max_write_data (destchunk->start, data, destchunk->size));

   // read the new chunk
   CALL (read_chunk (*destchunk, destchunk->start));
   chunk_dump (_chunks.size () - 1, *destchunk);
   *new_pos = destchunk->start;
   return NULL;
   }


err_info *Filemax::flush_chunks (void)
   {
   int i;

   // flush all the chunks
   for (i = 0; i < _chunks.size (); i++)
      {
      chunk_info &chunk = _chunks [i];

      if (chunk.loaded && !chunk.saved)
         {
         //printf ("   - flush chunk %d\n", i);
         CALL (build_chunk (chunk));

         // write the chunk to disc
         assert (chunk.buf);
         CALL(ensure_open());
         CALL (max_write_data (chunk.start, chunk.buf, chunk.size));
         ensure_closed();
         chunk.saved = true;
         }
      }
   return NULL;
   }


err_info *Filemax::flush_pages (void)
   {
   int i;

   // flush all the pages
   for (i = 0; i < _pages.size (); i++)
      {
      page_info &page = _pages [i];

      if (page.title_loaded && !page.title_saved)
         create_title (page);
      }
   return NULL;
   }


err_info *Filemax::flush (void)
   {
   CALL(ensure_open());
   ensure_all_chunks ();

   // write back any changed page titles
   CALL (flush_pages ());

   CALL (create_bermuda ());

   CALL (flush_chunks ());

   // now flush header
   write_max_header ();
   CALL(ensure_open());
   CALL (max_write_data (0, (byte *)_hdr.data (), _hdr.size ()));
   _hdr_updated = true;

   fflush (_fin);
   ensure_closed();

   // update the file size
   QFileInfo fi (_pathname);
   _size = fi.size ();
   return NULL;
   }


err_info *Filemax::merge_chunks (Filemax *src,
           page_info &dstpage, page_info &srcpage)
   {
   CALL (merge_chunk (src, srcpage.image, &dstpage.image));
   CALL (merge_chunk (src, srcpage.title, &dstpage.title));
   CALL (merge_chunk (src, srcpage.text, &dstpage.text));
   return NULL;
   }


err_info *Filemax::free_page (page_info &page)
   {
   page.titlestr.clear ();
   CALL (remove_chunknum (page.roswell));
   CALL (remove_chunknum (page.image));
   CALL (remove_chunknum (page.title));
   CALL (remove_chunknum (page.text));
   return NULL;
   }


err_info *Filemax::restore_page (page_info &page)
   {
   CALL (restore_chunknum (page.roswell));
   CALL (restore_chunknum (page.image));
   CALL (restore_chunknum (page.title));
   CALL (restore_chunknum (page.text));

   return NULL;
   }

err_info *Filemax::remove_pages (int pagenum, int count)
   {
   assert (pagenum >= 0 && pagenum + count <= _pages.size ());
   _pages.remove (pagenum, count);

   // now need to update header and bermuda on disc!
   return flush ();
   }


int remove_page_size (int count)
   {
   // we store info about each deleted page
   return count * sizeof (page_info);
   }


err_info *Filemax::max_setup_page (page_info &page, const Filemaxpage &maxpage)
   {
   byte *buf;
   int i;

   // first the image chunk
   chunk_info chunk;

   // This takes over _chunk.tile, _chunk.image and other allocate data
   chunk = maxpage._chunk;

   // Copy the tiles
   chunk.chunkid = _chunkid_next;

   // add the header, then all the chunk data
   CALL (alloc_chunk_buf (chunk, &buf));
   add_image_header (chunk, buf);
   for (i = 0; i < chunk.parts.size (); i++)
      {
      part_info &part = chunk.parts [i];

      memcpy (buf + 0x20 + part.start, part.buf, part.size);
      }
   CALL (insert_chunk (chunk, &page.image));

   debug2 (("image chunk->size=0x%x\n", chunk.size));
   chunk.buf = NULL;  // so we won't free it in maxpage->chunk

   // title
   if (!page.titlestr.isEmpty ())
      CALL (create_title (page));

   // timestamp
   page.timestamp = maxpage._timestamp;

   // roswell
   CALL (create_roswell (page));

   _chunkid_next++;

   CALL (flush_chunks ());

   return NULL;
   }


err_info *Filemax::chunk_unload (int pos)
   {
   chunk_info *chunk;
   tile_info *tile;
   int i;

   if (!pos)
      return NULL;
   CALL (chunk_find (pos, chunk, NULL));
   if (chunk->image)
      mem_free (CV &chunk->image);
   if (chunk->preview)
      mem_free (CV &chunk->preview);
   if (chunk->buf)
      mem_free (CV &chunk->buf);
   for (i = 0; i < chunk->tile_count; i++)
      {
      tile = &chunk->tile [i];
      tile_free (tile);
      }
   for (i = 0; i < chunk->parts.size (); i++)
      {
      part_info &part = chunk->parts [i];
      if (part.buf)
         mem_free (CV &part.buf);
      }
   chunk->loaded = false;
   return NULL;
   }


err_info *Filemax::max_unload (int pagenum)
   {
   page_info *page;

   CALL (find_page (pagenum, page));

   // unload all chunks for this page
   CALL (chunk_unload (page->roswell));
   CALL (chunk_unload (page->image));
   CALL (chunk_unload (page->title));
   CALL (chunk_unload (page->text));
   return NULL;
   }


void decpp_set_debug (int d)
   {
   debug_level = d;
   }


static void dump_short (FILE *f, short *tab, int count)
   {
   int i;

   assert (!(count & 1));
   for (i = 0; i < count; i += 2)
      fprintf (f, "   0x%x, 0x%x,\n", tab [i], tab [i + 1]);
   }


static void dump_unsigned (FILE *f, unsigned *tab, int count)
   {
   int i;

   for (i = 0; i < count; i++)
      fprintf (f, "   0x%x,\n", tab [i]);
   }


void dump_tables (void)
   {
   FILE *f;

   f = fopen ("tables.h", "w");

   assert (f);
   fprintf (f, "/* decoder tables */\n\n");

   fprintf (f, "static short atab[] = {\n");
   dump_short (f, (short *)atab, sizeof (atab) / 2);
   fprintf (f, "   };\n\n");

   fprintf (f, "static unsigned btab[] = {\n");
   dump_unsigned (f, (unsigned *)btab, sizeof (btab) / 4);
   fprintf (f, "   };\n\n");
   fclose (f);
   }


int Filemax::getSize (void)
   {
   if (!_chunks.size ())
      return 0;
   chunk_info &chunk = _chunks [_chunks.size () - 1];
   return chunk.start + chunk.size;
   }


err_info *Filemax::load ()  // was desk->ensureMax
   {
   err_info *err;

   if (!_valid)
      {
      _valid = true;
      struct stat st;

      if (!stat(_pathname.toLatin1 (), &st))
         {
         _size = st.st_size;
         _timestamp = QDateTime::fromTime_t (st.st_mtime);
         }

      err = max_open_file();
      if (err)
         {
         _serr = *err;
         _err = &_serr;
         return err;
         }
      if (!_debug)
         _debug = max_new_debug (stdout, debug_level, INT_MAX, 0, INT_MAX);
      }

   return NULL;
   }


err_info *Filemax::getAnnot (e_annot type, QString &text)
   {
   if (!_valid)
      return err_make (ERRFN, ERR_file_not_loaded_yet1,
                       qPrintable (_filename));
   if (!_annot_loaded) {
      CALL(ensure_open());
      CALL (load_annot ());
      ensure_closed();
   }
   Q_ASSERT (type >= 0 && type < Annot_count);
   if (type < _annot_data.size ())
      text = _annot_data [type];
   return NULL;
   }


err_info *Filemax::getPageText (int pagenum, QString &str)
   {
   page_info *page;
   chunk_info *chunk;
   bool temp;

   CALL (find_page (pagenum, page));
   if (!page->text)
      return NULL;
   CALL (chunk_find (page->text, chunk, &temp));
   QByteArray ba (chunk->size, '\0');
   CALL (max_read_data (chunk->start + POS_text_start, (byte *)ba.data (),
            chunk->size - POS_text_start));
   if (temp)
      {
      chunk_free (*chunk);
      mem_free (CV &chunk);
      }
   str = ba.constData ();
   return NULL;
   }


err_info *Filemax::renamePage (int pagenum, QString &name)
   {
   if (_valid)
      {
      page_info *page;

      CALL (find_page (pagenum, page));
      page->titlestr = name;
      page->title_loaded = true;
      page->title_saved = false;
      flush();
      }
   return NULL;
   }


err_info *Filemax::addPage (const Filepage *mp, bool do_flush)
   {
   Q_ASSERT (this);
   Q_ASSERT (mp);

   page_info *page;

   // set up page title
   CALL (page_add (_chunkid_next, mp->_name, page));

   CALL (max_setup_page (*page, *(Filemaxpage *)mp));

   if (do_flush)
      // now need to update header and bermuda on disc!
      CALL (flush ());

   // done
   return NULL;
   }


err_info *Filemax::getPageTitle (int pagenum, QString &title)
   {
   page_info *page;

   CALL (find_page (pagenum, page));
   CALL(ensure_open());
   CALL (ensure_titlestr (pagenum, *page));
   title = page->titlestr;
   ensure_closed();
   return NULL;
   }


err_info *Filemax::stackStack (File *fsrc)
   {
   Filemax *src = (Filemax *)fsrc;

   int destpage = _pagenum;
   int pagenum;
   QVector <page_info> pages;

   printf ("merging %d pages, destpage=%d, destchunks=%d\n", src->_pages.size (),
         destpage, _chunks.size ());

   CALL (ensure_all_chunks ());
   CALL (src->ensure_all_chunks ());

   // allocate a new page list
   pages.reserve (_pages.size () + src->_pages.size ());

   // take a guess at home many chunks we will need
   _chunks.reserve (_chunks.size () + src->_pages.size () * 5 + 1);

   // copy pages up to destpage
   for (pagenum = 0; pagenum < destpage; pagenum++)
      pages << _pages [pagenum];

   // copy pages from source
   for (pagenum = 0; pagenum < src->_pages.size (); pagenum++)
      {
      // get source page
      page_info &srcpage = src->_pages [pagenum];

//      printf ("merge %d from src %d\n", page_count, pagenum);

      // create destination page
      page_info newpage;

      page_init (newpage);
      pages << newpage;
      page_info &dstpage = pages.last ();
      dstpage.chunkid = _chunkid_next;
      dstpage.titlestr = srcpage.titlestr;

      // copy chunks
      CALL (merge_chunks (src, dstpage, srcpage));

      // create roswell chunk
      CALL (create_roswell (dstpage));

      // the next page will have a new page chunk ID
      _chunkid_next++;
      }

   // copy the rest of the pages
   for (pagenum = destpage; pagenum < _pages.size (); pagenum++)
      pages << _pages [pagenum];

   // use the new page set
   _pages = pages;

//    chunks_dump ();

   // now need to update header and bermuda on disc!
   CALL (flush ());

   return NULL;
   }


err_info *Filemax::unstackPages (int pagenum, int pagecount, bool remove,
         File *fdest)
   {
   Filemax *dest = (Filemax *)fdest;
   page_info *srcpage, *dstpage;
   int i;

   CALL (ensure_all_chunks ());

   for (i = 0; i < pagecount; i++)
      {
      // find the page
      CALL (find_page (pagenum + i, srcpage));

      // create destination page
      CALL (dest->page_add (dest->_chunkid_next, srcpage->titlestr, dstpage));

      // copy chunks
      CALL (dest->merge_chunks (this, *dstpage, *srcpage));

      // create roswell chunk
      CALL (dest->create_roswell (*dstpage));

      dest->_chunkid_next++;
      }

   // now need to update header and bermuda on disc!
   CALL (dest->flush ());

   // now remove from src file if required
   if (remove)
      remove_pages (pagenum, pagecount);

   if (_pagenum >= _pages.size ())
      _pagenum = _pages.size () - 1;
   return NULL;
   }


err_info *Filemax::removePages (QBitArray &pages,
      QByteArray &del_info, int &count)
   {
   QString fname, uniq;

   load ();

   Q_ASSERT (count > 0 && count < _pages.size ());

   // create a new list of pages that survive the cull
   QVector<page_info> dest_pages;
   dest_pages.reserve (_pages.size () - count);
   int i, upto;

   CALL (ensure_all_chunks ());
//    debug_pages (max);

   // work through the list adding either to dest_pages or del_info
   del_info.clear ();
   for (i = upto = 0; i < _pages.size (); i++)
      {
      page_info &src_page = _pages [i];

      if (pages.testBit (i))
         {
         CALL (free_page (src_page));
         del_info.append (QByteArray ((const char *)&src_page, sizeof (src_page)));
         upto++;
         }
      else
         dest_pages << src_page;
      }

   // we have a new list of pages for this stack
   Q_ASSERT (upto == count);
   _pages = dest_pages;

   // now need to update header and bermuda on disc!
   flush ();

   // we return a list of deleted pages in del_info

   return NULL;
   }


err_info *Filemax::restorePages (QBitArray &pages,
      QByteArray &del_info, int count)
   {
   QString fname, uniq;

   load ();

   int i, upto, newcount;

   CALL (ensure_all_chunks ());

   // create new page_info structure
   newcount = _pages.size () + count;
   QVector<page_info> dest_pages (newcount);

   // count how many pages are to be deleted
   int srcnum;
   for (i = upto = srcnum = 0; i < newcount; i++)
      if (pages.testBit (i))
         {
         page_info page;

         memcpy ((void *)&page, del_info.constData () + upto++ * sizeof (page_info),
            sizeof (page_info));
         assert (page.titlestr.isNull ());
         CALL (restore_page (page));
         printf ("page %d: titlestr = %p\n", i, page.titlestr.toLatin1 ().constData());
         dest_pages << page;
         }
      else
         dest_pages << _pages [srcnum++];

   // switch over the page list and free the old one
   _pages = dest_pages;

//    debug_pages (max);
   Q_ASSERT (upto == count);

   // now need to update header and bermuda on disc!
   CALL (flush ());
   return NULL;
   }


err_info *Filemax::duplicate (File *&, File::e_type , const QString &,
      int, Operation &, bool &supported)
   {
   // we don't add anything of value here, so just let File do it
   supported = false;
   return NULL;
   }


err_info *Filemax::remove ()
   {
   QFile file (_dir + _filename);

   if (!file.remove ())
      return err_make (ERRFN, ERR_could_not_remove_file2,
                file.fileName ().toLatin1 ().constData(),
                file.errorString ().toLatin1 ().constData());
   return NULL;
   }


err_info *Filemax::create (void)
   {
   _version = MAX_VERSION;
   _hdr.resize (0xe0);   //0xc8;  // allow room for file header
   _chunk0_start = _hdr.size ();
   _size = _chunk0_start;
   _signature = 0x46476956;
   _valid = true;

   const char *fname = _pathname.toLatin1().constData();
   _fin = fopen (fname, "w+b");
   if (!_fin)
      return err_make (ERRFN, ERR_cannot_open_file1,
                       _pathname.toLatin1 ().constData());

   utilSetGroup(fname);

   debug2 (("page count %d, chunk count %d\n", _pages.size (), _chunks.size ()));
   return NULL;
   }


Filemaxpage::Filemaxpage (void)
   : Filepage ()
   {
   _maxdata = 0;
   Filemax::chunk_init (_chunk);
   _debug = *no_debug ();
//    _debug = *max_new_debug (stdout, 2, INT_MAX, 0, INT_MAX);
   }


Filemaxpage::~Filemaxpage (void)
   {
   // We don't free chunk.tile here, since the memory has been 'taken over'
   }


err_info *Filemax::getImage (int pagenum, bool,
            QImage &image, QSize &Size, QSize &trueSize, int &bpp, bool blank)
   {
   int num_bytes;
   int compressed_size;
   QDateTime timestamp;

   load ();
   CALL (getImageInfo (pagenum, Size, trueSize, bpp, num_bytes, compressed_size, timestamp));
   Filepage::getImageFromLines (0, trueSize.width (), trueSize.height (),
         bpp == 24 ? 32 : bpp, 0, image, false, blank);

   // get the actual image data
   byte *imagep = image.bits ();

   chunk_info *chunk;

   CALL (find_page_chunk (pagenum, chunk, NULL, NULL));
   CALL(ensure_open());
   CALL (decode_image (*chunk, imagep, &trueSize));
   ensure_closed();

   // the QImage 'owns' the bitmap, so remove it from the chunk, otherwise we free twice
   chunk->image = NULL;
   return NULL;
   }


err_info *Filemax::getPreviewInfo (int pagenum, QSize &Size, int &bpp)
   {
   QString title;

   load ();
   chunk_info *chunk;
   bool temp;  //!< chunk is temporarily allocated

   //! really we should cache this information rather than reading from the file each time
   CALL(ensure_open());
   CALL (find_page_chunk (pagenum, chunk, &temp, NULL));
   Size = QSize (chunk->preview_size.x, chunk->preview_size.y);
   bpp = chunk->bits == 24 ? 24 : 8;
   if (temp)
      {
      chunk_free (*chunk);
      delete chunk;
      }
   ensure_closed();

   return NULL;
   }


err_info *Filemax::getPreviewPixmap (int pagenum, QPixmap &pixmap, bool blank)
   {
   byte *preview;
   QString path;

   load ();

   chunk_info *chunk;
   bool temp;  //!< chunk is temporarily allocated

   if (debug_level >= 3)
      show_file (stderr);
   CALL (find_page_chunk (pagenum, chunk, &temp, NULL));
   QSize Size = QSize (chunk->preview_size.x, chunk->preview_size.y);
   int bpp = chunk->bits == 24 ? 24 : 8;
   CALL(ensure_open());
   CALL (decode_preview (*chunk, true, &preview));
   ensure_closed();
   if (temp)
      {
      chunk_free (*chunk);
      delete chunk; //      mem_free (CV &chunk);
      }

   QVector<QRgb> table;
   table.reserve (256);
   QImage image;

   // create a greyscale palette
   switch (bpp)
      {
      case 8 :
         table.resize (256);
         for (int i = 0; i < 256; i++)
            table [255 - i] = (blank ? qRgb (i * CONFIG_preview_col_mult,
                  i * CONFIG_preview_col_mult, i) : qRgb (i, i, i));

         // create a QImage
         image = QImage (preview, Size.width (), Size.height (), QImage::Format_Indexed8);
         image.setColorTable (table);
         break;

      case 24 :
         image = QImage (preview, Size.width (), Size.height (), QImage::Format_RGB32);
         if (blank)
            colour_image_for_blank (image);
         break;
      }

   pixmap = QPixmap::fromImage(image);
   free (preview);
   return pixmap.isNull () ? err_make (ERRFN, ERR_failed_to_generate_preview_image) : NULL;
   }


QPixmap Filemax::pixmap (bool recalc)
{
   err_info *err = NULL;

   if (recalc) {
      err = ensure_open();
      if (!err) {
         err = getPreviewPixmap (_pagenum, _pixmap, false);
         _pixmap = _pixmap.copy ();
         ensure_closed();
      }
   }

   return err || _pixmap.isNull () ? unknownPixmap () : _pixmap;
}
