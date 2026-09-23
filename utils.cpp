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

#include <cerrno>
#include <cstring>
#ifndef _WIN32
#include <grp.h>
#endif
#ifdef __GLIBC__
#include <cxxabi.h>
#include <execinfo.h>
#endif

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QEvent>
#include <QDir>
#ifndef QT_NO_WIDGETS
#include <QDropEvent>
#endif
#include <QFile>
#ifndef QT_NO_WIDGETS
#include <QAbstractButton>
#include <QAction>
#endif
#include <QGuiApplication>
#include <QPalette>
#include <QFileInfo>
#include <QImage>
#ifndef QT_NO_WIDGETS
#include <QMessageBox>
#include <QStyle>
#endif
#include <QMimeData>
#include <QMutex>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>

#include <sys/stat.h>
#include <sys/types.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#else
#include <pwd.h>
#endif

#include <QDir>
#include <QTemporaryFile>
#include <QThread>

#include "epeglite.h"


#include "config.h"
#include "err.h"
#include "mem.h"
#ifndef QT_NO_WIDGETS
#include "op.h"
#endif
#include "utils.h"
#include "zip.h"

#ifndef Q_OS_WIN
static int public_gid = -1;
#endif

bool getSettingsSizes (QString base, QList<int> &size)
   {
   QSettings qs;

//    qDebug () << "getSettingsSizes" << base;
   int count = qs.value (base + "splitter/size").toInt ();

//    if (qs.contains (base + "splitter/1/size"))
   size.clear ();
   qs.beginReadArray (base + "splitter");
   for (int i = 0; i < count; i++)
      {
      qs.setArrayIndex (i);
      size.append (qs.value ("size").toInt ());
      }
   qs.endArray ();
   return size.count () && size [0] > 0;
   }


void setSettingsSizes (QString base, QList<int> &size)
   {
   QSettings qs;

//    qDebug () << "setSettingsSizes" << base;
   qs.beginWriteArray (base + "splitter");
   for (int i = 0; i < size.count (); i++)
      {
      qs.setArrayIndex (i);
      qs.setValue ("size", size [i]);
      }
   qs.endArray ();
   }


#ifndef DELIVER

int jpeg_thumbnail (byte *data, int insize, byte **destp, int *dest_sizep, cpoint *sizep)
   {
   Epeg_Image *im;
   int stride, valid;

   im = epeg_memory_open(data, insize);
   if (!im)
      return 0;

   sizep->x = im->in.w / CONFIG_preview_scale;
   sizep->y = im->in.h / CONFIG_preview_scale;
   epeg_decode_size_set           (im, sizep->x, sizep->y);
   epeg_quality_set               (im, 75, false);
//   epeg_thumbnail_comments_enable (im, 1);
   epeg_memory_output_set (im, destp, dest_sizep);
//   epeg_encode                    (im);
   stride = (sizep->x * im->in.jinfo.num_components + 3) & ~3;

   if (epeg_raw (im, stride))
      {
      /* the data was too corrupt to decode at all; there is no
         thumbnail and, crucially, no output buffer was allocated, so
         copying would write through a wild pointer */
      *dest_sizep = 0;
      epeg_close (im);
      return 0;
      }
   valid = im->last_valid_row;
   if (valid != -1 && valid / CONFIG_preview_scale < sizep->y)
      {
      printf ("data short - shrink preview from %d to %d\n", sizep->y, valid / CONFIG_preview_scale);
      sizep->y = valid / CONFIG_preview_scale;    // shrink preview
      *dest_sizep = stride * sizep->y;
      }
   epeg_copy (im, sizep->x, sizep->y, stride);
   epeg_close                     (im);
   return valid;
   }


typedef struct jpeg_source_info
   {
   struct jpeg_source_mgr pub;  /* public fields */
//   max_info *max;
//   chunk_info *chunk;
   byte *data;
   int pos;
   int size;
   bool done;   //!< We have read all the data
   } jpeg_source_info;


typedef struct jpeg_dest_info
   {
   struct jpeg_destination_mgr pub;  /* public fields */
//   max_info *max;
//   chunk_info *chunk;
   byte *data;
   int pos;
   int size;
   bool overflow;   //!< the output did not fit in the buffer
   } jpeg_dest_info;


static void init_source(j_decompress_ptr cinfo)
   {
   cinfo = cinfo;
   }


static boolean fill_input_buffer(j_decompress_ptr cinfo)
   {
   jpeg_source_info *src = (jpeg_source_info *)cinfo->src;

   if (!src->done)
      {
       src->pub.next_input_byte = src->data;
       src->pub.bytes_in_buffer = src->size;
       src->done = true;
       return true;
       }

   return false;
   }


static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
   {
   jpeg_source_info *src = (jpeg_source_info *)cinfo->src;

   src->pub.next_input_byte += (size_t) num_bytes;
   src->pub.bytes_in_buffer -= (size_t) num_bytes;
   }


static void term_source(j_decompress_ptr cinfo)
   {
   cinfo = cinfo;
   }


/* The same thing tends to go wrong with a whole scan rather than with
   one page of it: a stack whose pages were all cut short reports the
   same fault for every page, and again for every thumbnail, which
   buries whatever else the log has to say. Say it the first time and
   then count, and give the count when something else goes wrong or
   when paperman stops */

static QMutex report_lock;
static QString report_last;   //!< what was last reported
static int report_more;       //!< how many times the same has happened since


static void utilReportSummary (void)
   {
   if (report_more)
      qWarning ().noquote ()
         << QString ("%1 (and %2 more like it)").arg (report_last)
               .arg (report_more);
   report_last.clear ();
   report_more = 0;
   }


void utilReportFlush (void)
   {
   QMutexLocker lock (&report_lock);

   utilReportSummary ();
   }


void utilReportOnce (const QString &what)
   {
   QMutexLocker lock (&report_lock);

   if (what == report_last)
      {
      report_more++;
      return;
      }

   utilReportSummary ();

   /* the count of whatever comes next is only given if something asks
      for it, so make sure that something does */
   static bool hooked;

   if (!hooked && QCoreApplication::instance ())
      {
      hooked = true;
      qAddPostRoutine (utilReportFlush);
      }

   report_last = what;
   qWarning ().noquote () << what;
   }


struct my_error_mgr
   {
   struct jpeg_error_mgr mgr;
   jmp_buf setjmp_buffer;
   int err;
   char msg [JMSG_LENGTH_MAX];   //!< what went wrong, for the caller to report
   };


/* Keep what the library says rather than printing it here. The caller
   knows what it was doing at the time, which is the part worth
   reporting, and printing from here loses that and says it once per
   page besides */

static void my_error_exit (j_common_ptr cinfo)
   {
   struct my_error_mgr* myerr = (struct my_error_mgr*) cinfo->err;

   (*cinfo->err->format_message) (cinfo, myerr->msg);
   myerr->err = 1;
   longjmp(myerr->setjmp_buffer, 1);
}


/* Cut the sides off a JPEG by moving its coefficients about, so that the
   pixels which are kept are the ones the scanner sent, bit for bit.

   A JPEG is stored as blocks of 8 pixels, grouped into units as wide as
   the colour sampling needs, so a cut can only fall where one of those
   units ends: the left edge is moved out to the unit below it, which
   keeps a little more of the page than was asked for and never less.
   The right edge needs no such care, since the last unit of a row may
   run past the width the file records and the decoder drops what lies
   beyond it. */
static int jpeg_crop_blocks (const byte *data, int size, int lo, int hi,
                             QByteArray &out)
   {
   struct jpeg_decompress_struct srcinfo;
   struct jpeg_compress_struct dstinfo;
   struct my_error_mgr jerr;
   jvirt_barray_ptr *src_coef;
   jvirt_barray_ptr *dst_coef;
   unsigned char *buf = NULL;
   unsigned long buf_size = 0;
   int ci, unit, x_off, width, result = -1;

   /* the error handler must be in place before either object is made,
      since making one already reports through it */
   memset (&srcinfo, '\0', sizeof (srcinfo));
   memset (&dstinfo, '\0', sizeof (dstinfo));
   srcinfo.err = dstinfo.err = jpeg_std_error (&jerr.mgr);
   jerr.err = 0;
   jerr.mgr.error_exit = my_error_exit;
   jpeg_create_decompress (&srcinfo);
   jpeg_create_compress (&dstinfo);

   if (!setjmp (jerr.setjmp_buffer))
      {
      jpeg_mem_src (&srcinfo, data, size);
      jpeg_read_header (&srcinfo, true);
      src_coef = jpeg_read_coefficients (&srcinfo);
      if (!src_coef)
         goto done;

      /* where a cut can fall: the width of one unit of blocks */
      unit = DCTSIZE * srcinfo.max_h_samp_factor;
      x_off = qMax (0, lo) / unit * unit;
      width = qMin ((int)srcinfo.image_width, hi + 1) - x_off;
      if (width <= 0 || x_off + width > (int)srcinfo.image_width)
         goto done;
      if (!x_off && width == (int)srcinfo.image_width)
         goto done;   // nothing to cut

      jpeg_mem_dest (&dstinfo, &buf, &buf_size);
      jpeg_copy_critical_parameters (&srcinfo, &dstinfo);
      dstinfo.image_width = width;
      dstinfo.image_height = srcinfo.image_height;
#if JPEG_LIB_VERSION >= 70
      /* the size the blocks are laid out for, which is what the
         library works from when it is handed coefficients rather than
         pixels: copying the parameters over took it from the source,
         and leaving it there lays out the whole of the old width */
      dstinfo.jpeg_width = dstinfo.image_width;
      dstinfo.jpeg_height = dstinfo.image_height;
#endif

      /* room for the blocks which are kept, a whole number of units
         each way as the format requires */
      dst_coef = (jvirt_barray_ptr *) (*dstinfo.mem->alloc_small)
         ((j_common_ptr) &dstinfo, JPOOL_IMAGE,
          sizeof (jvirt_barray_ptr) * dstinfo.num_components);
      for (ci = 0; ci < dstinfo.num_components; ci++)
         {
         jpeg_component_info *comp = dstinfo.comp_info + ci;
         int h = comp->h_samp_factor, v = comp->v_samp_factor;
         long wb = ((long)width * h
                    + (long)srcinfo.max_h_samp_factor * DCTSIZE - 1)
               / ((long)srcinfo.max_h_samp_factor * DCTSIZE);
         long hb = ((long)dstinfo.image_height * v
                    + (long)srcinfo.max_v_samp_factor * DCTSIZE - 1)
               / ((long)srcinfo.max_v_samp_factor * DCTSIZE);

         dst_coef [ci] = (*dstinfo.mem->request_virt_barray)
            ((j_common_ptr) &dstinfo, JPOOL_IMAGE, FALSE,
             (JDIMENSION) ((wb + h - 1) / h * h),
             (JDIMENSION) ((hb + v - 1) / v * v),
             (JDIMENSION) v);
         }
      jpeg_write_coefficients (&dstinfo, dst_coef);

      // copy the blocks which are kept, a row of units at a time
      for (ci = 0; ci < dstinfo.num_components; ci++)
         {
         jpeg_component_info *comp = dstinfo.comp_info + ci;
         JDIMENSION skip = x_off / unit * comp->h_samp_factor;
         JDIMENSION y;

         for (y = 0; y < comp->height_in_blocks;
              y += comp->v_samp_factor)
            {
            JBLOCKARRAY dst_rows = (*dstinfo.mem->access_virt_barray)
               ((j_common_ptr) &dstinfo, dst_coef [ci], y,
                (JDIMENSION) comp->v_samp_factor, TRUE);
            JBLOCKARRAY src_rows = (*srcinfo.mem->access_virt_barray)
               ((j_common_ptr) &srcinfo, src_coef [ci], y,
                (JDIMENSION) comp->v_samp_factor, FALSE);

            for (int row = 0; row < comp->v_samp_factor; row++)
               memcpy (dst_rows [row], src_rows [row] + skip,
                       comp->width_in_blocks * sizeof (JBLOCK));
            }
         }
      jpeg_finish_compress (&dstinfo);
      jpeg_finish_decompress (&srcinfo);
      if (buf && buf_size)
         {
         out = QByteArray ((const char *)buf, (int)buf_size);
         result = x_off;
         }
      }

done:
   jpeg_destroy_compress (&dstinfo);
   jpeg_destroy_decompress (&srcinfo);
   if (buf)
      free (buf);
   if (jerr.err)
      utilReportOnce (QString ("Cannot cut the page image to %1..%2: %3")
                      .arg (lo).arg (hi).arg (jerr.msg));

   return jerr.err ? -1 : result;
   }


int jpegCrop (const byte *data, int size, int lo, int hi, QByteArray &out)
   {
   return jpeg_crop_blocks (data, size, lo, hi, out);
   }


void jpeg_decode (byte *data, int size, byte * volatile dest, int line_bytes,
                  int bpp, int max_width, int max_height)
   {
   struct jpeg_decompress_struct cinfo;
   JSAMPARRAY buffer;/* Output row buffer */
   jpeg_source_info source;
   int tile_bytes;
   struct my_error_mgr jerr;
   bool short_data = false;   //!< the data ended before the image did

   /* how far the image got, for the report if it goes wrong; these
      survive the jump out of the decoder */
   volatile int got = 0, want = 0;

   jpeg_create_decompress(&cinfo);
   cinfo.err = jpeg_std_error(&jerr.mgr);

   cinfo.src = &source.pub;
//   source.max = max;
//   source.chunk = chunk;
   source.data = data;
   source.size = size;
   source.done = false;
//   source.pos = pos;
   source.pub.init_source = init_source;
   source.pub.fill_input_buffer = fill_input_buffer;
   source.pub.skip_input_data = skip_input_data;
   source.pub.resync_to_restart = jpeg_resync_to_restart;/* use default method */
   source.pub.term_source = term_source;
   source.pub.bytes_in_buffer = 0;   /* forces fill_input_buffer on first read */
   source.pub.next_input_byte = NULL;/* until buffer loaded */

   cinfo.err = jpeg_std_error(&jerr.mgr);
   jerr.err = 0;
   jerr.mgr.error_exit = my_error_exit;
   if (!setjmp(jerr.setjmp_buffer))
      {
      jpeg_read_header(&cinfo, true);
      jpeg_start_decompress(&cinfo);

      /* Make a one-row-high sample array that will go away when done with image */
      tile_bytes = cinfo.output_width * cinfo.output_components;
      buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, tile_bytes, 1);

//      printf ("width = %d, tile_bytes = %d, line_bytes = %d\n",
//         cinfo.output_width, tile_bytes, line_bytes);

      while (cinfo.output_scanline < cinfo.output_height &&
             cinfo.output_scanline < (unsigned)max_height)
         {
         /* The data can end before the image does: a scanner which cut
            the page short leaves a JPEG whose header promises more
            lines than it sent, and a file still being written has only
            the part that reached the disk. The source then has nothing
            to give, jpeg_read_scanlines() decodes no line and leaves
            output_scanline where it was, and going round again would
            write a line further past the end of the image every time,
            until the write lands off the end of memory */
         if (!jpeg_read_scanlines (&cinfo, buffer, 1))
            {
            short_data = true;
            break;
            }
         if (cinfo.output_components == 3 && bpp == 32)
            {
            byte *in;
            uint32_t *out;
            int i;

            in = (byte *)buffer [0];
            i = cinfo.output_width;
            if (max_width != -1 && i > max_width)
                i = max_width;
            for (out = (uint32_t *)dest; i != 0;
                 i--, in += 3)
               *out++ = in [2] | (in [1] << 8) | (in [0] << 16);
            }
         else
            {
            int bytes = tile_bytes;

            /* the tile may be padded beyond the image width, so avoid
               writing past the end of each destination row */
            if (max_width != -1 &&
                bytes > max_width * (int)cinfo.output_components)
               bytes = max_width * cinfo.output_components;
            memcpy (dest, buffer[0], bytes);
            }
         dest += line_bytes;
         }
//       debug1 (("width = %d, tile_bytes = %d, line_bytes = %d, decoded %d lines\n",
//          cinfo.output_width, tile_bytes, line_bytes, cinfo.output_height));

      /* Finishing asks the library to make sure the whole image came
         out, which it did not whenever the page holds fewer lines than
         its header promises (a page cut short by a jam) or the caller
         wanted only the top of it. Neither is a fault, so in both
         cases stop instead of asking */
      got = cinfo.output_scanline;
      want = cinfo.output_height;
      if (short_data || cinfo.output_scanline < cinfo.output_height)
         jpeg_abort_decompress (&cinfo);
      else
         jpeg_finish_decompress(&cinfo);
      }
   jpeg_destroy_decompress(&cinfo);
   if (jerr.err)
      utilReportOnce (QString ("Cannot decode the page image"
                               " (%1 lines of %2): %3")
                      .arg (got).arg (want).arg (jerr.msg));
   }


static void init_destination (j_compress_ptr cinfo)
   {
   jpeg_dest_info *dest = (jpeg_dest_info *)cinfo->dest;

   dest->pub.next_output_byte = dest->data;
   dest->pub.free_in_buffer = dest->size;
   }


/* called by libjpeg when the output buffer is full. We cannot grow the
   buffer, so record the overflow and let libjpeg continue writing from
   the start of the buffer. The output is useless but stays in bounds;
   jpeg_encode() reports the failure through its size */
static boolean empty_output_buffer (j_compress_ptr cinfo)
   {
   jpeg_dest_info *dest = (jpeg_dest_info *)cinfo->dest;

   dest->overflow = true;
   dest->pub.next_output_byte = dest->data;
   dest->pub.free_in_buffer = dest->size;
   return true;
   }


static void term_destination (j_compress_ptr cinfo)
   {
   cinfo = cinfo;
   }


void jpeg_encode (byte *image, cpoint *tile_size, byte *outbuff, int *size,
            int bpp, int line_bytes, int quality, int valid_width)
   {
  struct jpeg_compress_struct cinfo;
  jpeg_dest_info dest;
  struct my_error_mgr jerr;
  JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
  int row_stride = line_bytes;      /* physical row width in image buffer */
  byte *buff, *ptr;
  int bytes;

  cinfo.err = jpeg_std_error(&jerr.mgr);
  jpeg_create_compress(&cinfo);
   cinfo.dest = &dest.pub;

  dest.data = outbuff;
  dest.size = *size;
  dest.overflow = false;
//  dest.pub.next_output_byte = NULL;
//  dest.pub.free_in_buffer = 0;
  dest.pub.next_output_byte = outbuff;
  dest.pub.free_in_buffer = *size;
  dest.pub.init_destination = init_destination;
  dest.pub.empty_output_buffer = empty_output_buffer;
  dest.pub.term_destination = term_destination;

//  printf ("jpeg_encode: bpp=%d\n", bpp);

  /* the tile may be wider than the source image (it is padded to a
     multiple of 8 pixels), in which case only valid_width pixels exist
     in each source row and the rest must be synthesised */
  int out_bpp = bpp == 8 ? 1 : 3;
  int valid = valid_width;
  if (valid < 1 || valid > tile_size->x)
     valid = tile_size->x;

  *size = 0;
  bytes = tile_size->x * out_bpp;
  buff = (byte *)malloc (bytes > row_stride ? bytes : row_stride);
  if (!buff)
     return;   // should report error, but I suppose *size is 0

   jerr.err = 0;
   jerr.mgr.error_exit = my_error_exit;
   if (!setjmp(jerr.setjmp_buffer))
      {

     /* Step 3: set parameters for compression */

     /* First we supply a description of the input image.
      * Four fields of the cinfo struct must be filled in:
      */
     cinfo.image_width = tile_size->x;    /* image width and height, in pixels */
     cinfo.image_height = tile_size->y;
     cinfo.input_components = bpp == 8 ? 1 : 3;    /* # of color components per pixel */
     cinfo.in_color_space = bpp == 8 ? JCS_GRAYSCALE : JCS_RGB;   /* colorspace of input image */
     jpeg_set_defaults(&cinfo);
     jpeg_set_quality(&cinfo, quality, true /* limit to baseline-JPEG values */);

     jpeg_start_compress(&cinfo, true);

     while (cinfo.next_scanline < cinfo.image_height) {
       /* jpeg_write_scanlines expects an array of pointers to scanlines.
        * Here the array is only one element long, but you could pass
        * more than one scanline at a time if that's more convenient.
        */
       row_pointer[0] = buff;  //&image [cinfo.next_scanline * row_stride];
       ptr = &image [cinfo.next_scanline * row_stride];

       if (bpp == 32)
          {
          int i;
          uint32_t *in;
          byte *out;

          for (i = valid, in = (unsigned *)ptr, out = buff; i != 0;
               i--, in++, out += 3)
             {
             out [2] = *in;
             out [1] = *in >> 8;
             out [0] = *in >> 16;
             }
          }
       else
          memcpy (buff, ptr, valid * out_bpp);

       // fill the padding columns by repeating the last valid pixel
       for (int x = valid; x < (int)cinfo.image_width; x++)
          memcpy (buff + x * out_bpp, buff + (valid - 1) * out_bpp,
                  out_bpp);
       (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
     }

     /* Step 6: Finish compression */
     jpeg_finish_compress(&cinfo);

      // report an overflowed buffer rather than returning rubbish
      *size = dest.overflow ? -1 : dest.pub.next_output_byte - outbuff;
      }
   jpeg_destroy_compress(&cinfo);
   free (buff);
   if (jerr.err)
      utilReportOnce (QString ("Cannot compress the page image"
                               " (%1 x %2, %3 bits): %4")
                      .arg (tile_size->x).arg (tile_size->y).arg (bpp)
                      .arg (jerr.msg));
   }

#endif


QString removeExtension (const QString &fname, QString &ext)
   {
   int pos;

   pos = fname.indexOf ('.');
   if (pos != -1)
      {
      ext = fname.mid (pos);   // include the .
      return fname.left (pos);
      }
   ext = "";
   return fname;
   }


void memtest (const char *name)
   {
   int *test = new int [5];

   fprintf (stderr, "test %s: %p\n", name, test);
   Q_ASSERT ((uintptr_t)test < 0xb0000000);
   }


QImage utilConvertImageToGrey (QImage &image)
   {
   QImage grey = image.convertToFormat (QImage::Format_Indexed8, Qt::MonoOnly);
   QVector<QRgb> table = grey.colorTable ();

   // now set up the pixel values
   for (int i = 0; i < grey.height (); i++)
      {
      uchar *line = grey.scanLine (i);

      for (int x = 0; x < grey.width (); x++, line++)
         *line = qRed (table [*line]);
      }
   table.clear ();
   for (int i = 0; i < 256; i++)
      table << qRgb (i, i, i);
   grey.setColorTable (table);
   return grey;
   }


QImage util_smooth_scale_image (QImage &image, QSize size)
   {
   QImage img = image;
   QSize oldsize = image.size ();

   // fast scale it first if it is far too big
   if (size.width () * 4 < oldsize.width () * 4
      && size.height () * 4 < oldsize.height () * 4)
      {
      oldsize = size * 4;
      // qDebug () << "util_smooth_scale_image1" << image.width () << image.height () << image.bits () << image.size ();
      // qDebug () << "util_smooth_scale_image2" << img.width () << img.height () << img.bits () << img.size ();
      img = img.scaled (oldsize, Qt::KeepAspectRatio);
      }
   img = img.scaled (size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
   return img;
   }


err_info *util_get_tmp (char *tmp)
   {
   QTemporaryFile file (QDir::tempPath () + "/maxviewXXXXXX");

   if (!file.open ())
      return err_make (ERRFN, ERR_could_not_make_temporary_file);
   QByteArray name = QFile::encodeName (file.fileName ());
   if (name.size () >= PATH_MAX)
      return err_make (ERRFN, ERR_could_not_make_temporary_file);
   strcpy (tmp, name.constData ());
   return NULL;
   }


QString util_getUnique (QString fname, QString dir, QString ext)
{
   QFileInfo fi (QDir (dir), fname);
//    QString ext = fi.suffix ();
   QString leaf = fi.baseName ();

   dir += "/";
   leaf = util_findNextFilename (leaf, dir, ext);
   fname = QString ("%1%2%3").arg (dir).arg (leaf).arg (ext);
   return fname;
}


err_info *util_buildZip (QString &zip, const QStringList &fnamelist)
   {
//    char tmp [PATH_MAX + 1];
   Zip::ErrorCode ec;
   Zip uz;

   zip = util_getUnique (zip, QDir::tempPath (), ".zip");
//    zip = util_findNextFilename (zip, P_tmpdir, ".zip");
//    zip = QString ("%1/%2.zip").arg (P_tmpdir).arg (zip);
   ec = uz.createArchive (zip);
   if (ec != Zip::Ok)
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (zip));
   foreach (QString fname, fnamelist)
      {
      ec = uz.addFile (fname);
      if (ec != Zip::Ok)
         return err_make (ERRFN, ERR_cannot_add_file_to_zip1, qPrintable (fname));
      }
      
   uz.setArchiveComment("This archive has been created by Maxview using OSDaB Zip (http://osdab.sourceforge.net/).");
   if (uz.closeArchive () != Zip::Ok)
      return err_make (ERRFN, ERR_cannot_close_file1, qPrintable (zip));
   return NULL;
   }


QString util_findNextFilename (QString fname, QString dir, QString ext)
   {
   QString orig = fname;
   QString str;
   QFile f;
   int i;

   for (i = 0; i < 10000; i++)
      {
      int pos;

      str = fname;
      if (str.right (1) == "_")
         str.truncate (str.length () - 1);
      pos = str.lastIndexOf (UTIL_PAGE_PREFIX);

      QStringList sl;
      if (pos == -1)
         sl << str + "*";
      else
         sl << str.left (pos) + "*";
      QDir qdir(dir);
      qdir.setNameFilters (sl);
      if (!qdir.entryInfoList().size ())
         return fname;

   // increment filename, ignoring any numbers present
      util_incrementFilename (fname, fname != orig);
      }

// if that failed we are in trouble - try random numbers
   for (i = 0; i < 1000; i++)
   {
      str.setNum (rand ());
      f.setFileName (dir + str + ext);
      if (!f.exists ())
         return fname;
   }

   // things are probably broken, just return null for now - caller will get an error when using the name
   return QString();
   }

   
void util_incrementFilename (QString &name, bool useNum)
   {
      int p, digits;
      int num = 0;

//   printf ("was %s\n", name.latin1 ());

   // find the number at the end of the string
      for (p = name.length (); p > 0; p--)
         if (name [p - 1] < '0' || name [p - 1] > '9')
            break;

//   printf ("%s: p=%d, left=%s\n", name.latin1 (), p, name.left (p).latin1 ());
      digits = name.length () - p;

   // if two digits or more, append _1 (it might be a year)
   // only do this if there isn't an _ before the number already
      if ((!useNum && p != name.length ())
            || (digits >= 2 && (p == 0 || name [p - 1] != '_')))
      {
         name.append ("_");
         num = 0;
      }

   // otherwise get the number
      else if (p != (int)name.length ())
      {
         num = name.mid (p).toInt ();
         name.truncate (p);
      }

      QString fred;

      fred.setNum (num + 1);
      name.append (fred);
//   printf ("now %s\n", name.latin1 ());
   }


err_info *util_getUsername (QString &userName)
   {
   userName = "whoami";
#ifdef Q_OS_WIN
   char winUserName[UNLEN + 1];
   DWORD winUserNameSize = sizeof(winUserName);

   if (!GetUserNameA (winUserName, &winUserNameSize))
      return err_make (ERRFN, ERR_failed_to_read_username1,
                       "GetUserName() failed");
   userName = QString::fromLocal8Bit (winUserName);
#else
   char name [200];
   struct passwd pwd, *user;

   int err = getpwuid_r (getuid(), &pwd, name, sizeof (name), &user);
   if (err)
      return err_make (ERRFN, ERR_failed_to_read_username1, "getpwuid_r() failed");
   if (!user)
      return err_make (ERRFN, ERR_failed_to_read_username1, getuid ());
   userName = user->pw_name;
#endif
   return NULL;
   }

/* How long something took, in a form to read at a glance: 45s, 1m4s or
   1h2m3s, with the parts that are zero left off the front

   \param ms   how many milliseconds it took */

#ifndef QT_NO_WIDGETS

/* Watches for message boxes and writes what each one says to the log.
   The boxes are raised from four dozen places, and any of them may be
   the one that appears while something else is being chased, so they
   are caught as they are shown rather than at each of those places */

class Dialoglogger : public QObject
   {
public:
   Dialoglogger (QObject *parent) : QObject (parent) {}

protected:
   bool eventFilter (QObject *obj, QEvent *event) override
      {
      QMessageBox *box;

      if (event->type () == QEvent::Show
          && (box = qobject_cast<QMessageBox *> (obj)))
         {
         QString str = box->text () + " " + box->informativeText ()
               + " " + box->detailedText ();

         str = str.simplified ();
         qWarning ().noquote ()
            << QString ("dialog: [%1] %2").arg (box->windowTitle (), str);
         }
      return QObject::eventFilter (obj, event);
      }
   };


void utilLogDialogs (QCoreApplication *app)
   {
   app->installEventFilter (new Dialoglogger (app));
   }

#else

void utilLogDialogs (QCoreApplication *)
   {
   }

#endif


/* Say what this paperman was built from, and which scanner libraries
   it has loaded, with the date each was built. A log from a build a few
   days old, or from a backend which was not the one that was just
   installed, is worse than no log at all: this puts the answer at the
   top of it */

Q_LOGGING_CATEGORY (logView, "paperman.view", QtWarningMsg)
Q_LOGGING_CATEGORY (logErr, "paperman.err", QtWarningMsg)
Q_LOGGING_CATEGORY (logBuild, "paperman.build", QtWarningMsg)


void utilLogEnable (void)
   {
   QLoggingCategory::setFilterRules ("paperman.*.debug=true");
   }


#ifdef __GLIBC__

/* Turn one line of what backtrace_symbols() gives us into something
   readable. It comes as

      ./paperman(_ZN8Filejpeg4loadEv+0x1f) [0x5654321]

   where the part before the '+' is the mangled name of the function,
   which is the only part worth reading */

static QString utilTidyFrame (const char *sym)
   {
   const char *open = strchr (sym, '(');
   const char *plus = open ? strchr (open, '+') : NULL;

   if (!open || !plus || plus == open + 1)
      return QString::fromLatin1 (sym);

   QByteArray name (open + 1, plus - open - 1);
   int status = 0;
   char *plain = abi::__cxa_demangle (name.constData (), NULL, NULL, &status);
   QString out = status == 0 && plain ? QString::fromLatin1 (plain)
                                      : QString::fromLatin1 (name);

   free (plain);

   /* keep the object the function came from, since a frame in a
      library is worth telling apart from one of ours */
   QByteArray obj (sym, open - sym);
   int slash = obj.lastIndexOf ('/');

   if (slash >= 0)
      obj = obj.mid (slash + 1);

   return out + " [" + QString::fromLatin1 (obj) + "]";
   }

#endif


QStringList utilBacktrace (int skip)
   {
   QStringList out;

#ifdef __GLIBC__
   void *frame [MAX_TRACE_DEPTH];
   int count = backtrace (frame, MAX_TRACE_DEPTH);
   char **sym = backtrace_symbols (frame, count);

   if (!sym)
      return out;

   /* drop this function as well as however many the caller asks for */
   for (int i = skip + 1; i < count; i++)
      out << utilTidyFrame (sym [i]);
   free (sym);
#else
   (void)skip;
#endif

   return out;
   }


void utilLogBuild (const char *when)
   {
   static bool said;
   QFile maps ("/proc/self/maps");
   QStringList seen;

   /* nothing to say when no one is listening; this also means that
      what was built is reported the first time the log is on, rather
      than being used up by a silent call before that */
   if (!logBuild ().isDebugEnabled ())
      return;

   if (!said)
      {
      said = true;
      qCDebug (logBuild).noquote ()
         << QString ("paperman built %1 %2").arg (__DATE__).arg (__TIME__);
      }

   /* the SANE libraries are found by looking at what is mapped, since
      the back ends are loaded as they are needed and where they came
      from is exactly what is in doubt */
   if (!maps.open (QIODevice::ReadOnly | QIODevice::Text))
      return;
   while (!maps.atEnd ())
      {
      QString line = QString::fromLatin1 (maps.readLine ());
      int pos = line.indexOf ("/");

      if (pos < 0 || !line.contains ("libsane"))
         continue;

      QString path = line.mid (pos).trimmed ();
      QFileInfo info (path);

      if (seen.contains (path))
         continue;
      seen << path;
      qCDebug (logBuild).noquote ()
         << QString ("%1 %2 built %3").arg (when).arg (path)
            .arg (info.lastModified ().toString ("yyyy-MM-dd hh:mm:ss"));
      }
   }


QString utilTimeStr (qint64 ms)
   {
   int secs = (ms + 500) / 1000;
   int mins = secs / 60;
   int hours = mins / 60;
   QString str;

   secs -= mins * 60;
   mins -= hours * 60;
   if (hours)
      str = QString ("%1h").arg (hours);
   if (hours || mins)
      str += QString ("%1m").arg (mins);
   return str + QString ("%1s").arg (secs);
   }


QString utilRemoveQuotes (QString str)
   {
   // Remove quotes
   if (str.length () >= 2 && str [0] == '\'')
      {
      str.chop (1);
      str.remove (0, 1);
      }
   return str;
   }


int utilDetectYear(const QString& fname, int& foundPos)
{
   int len = fname.length();

   // search for year from 1900 to 2099
   QRegularExpression rx("(\\d{4})");
   QRegularExpressionMatchIterator it = rx.globalMatch(fname);

   while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      int pos = m.capturedStart();

      // make sure here is no digit either side
      if ((pos && fname[pos - 1].isDigit()) ||
          (pos + 4 < len && fname[pos + 4].isDigit()))
         continue;
      int year = m.captured(0).toInt();

      if (year >= 1900 && year < 2100) {
         foundPos = pos;
         return year;
      }
   }

   return 0;
}

int utilDetectMonth(const QString& fname, int& foundPos)
{
   QString months = "(01jan|02feb|03mar|04apr|05may|06jun|07jul|08aug|09sep|10oct|11nov|12dec)";
   QRegularExpression rx(months,
                         QRegularExpression::CaseInsensitiveOption);
   QRegularExpressionMatchIterator it = rx.globalMatch(fname);

   while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      int pos = m.capturedStart();
      int len = pos + 5;

      // make sure here is no letter either side
      if ((pos && fname[pos - 1].isLetter()) ||
          (fname.size() > len && fname[len].isLetter()))
         continue;

      int month = 1 + months.indexOf(m.captured(0).toLower()) / 6;
      foundPos = pos;

      return month;
   }

   return 0;
}

QStringList utilDetectMatches(const QDate& date, QStringList& matches,
                              QStringList& missing,
                              bool keep_other_dates)
{
   QStringList to_sort, suggests;

   missing.clear();
   foreach (const QString& item, matches) {
      int ypos, mpos;
      int year = utilDetectYear(item, ypos);
      int month = utilDetectMonth(item, mpos);
      bool skip = false;
      QString suggest;

      if (year && year != date.year())
         skip = true;

      // Much assumption about English here
      if (year && month && month != date.month()) {
         if ((year == date.year() - 1 && month == 12 && date.month() == 1) ||
             (year == date.year() && month == date.month() - 1))
            suggest = item.left(ypos) + QString("%1").arg(date.year(), 4) +
                       item.mid(ypos + 4, mpos - ypos - 4) +
                       date.toString("MM") +
                       date.toString("MMM").toLower().left(3) +
                       item.mid(mpos + 5);
         skip = true;
      } else if (year && year == date.year() - 1 && !month) {
         if (suggest.isEmpty())
            suggest = item.left(ypos) + QString("%1").arg(date.year(), 4) +
                  item.mid(ypos + 4);
         skip = true;
      }
      if (!suggest.isEmpty())
         suggests << suggest;

      if (skip)
         {
         /* the directory is dated with another year or month: keep it,
            after everything else, if the caller asked for that */
         if (keep_other_dates)
            to_sort << "4" + item;
         }
      else if (year && month)
         to_sort << "1" + item;
      else if (year && !month)
         to_sort << "2" + item;
      else if (!year && !month)
         to_sort << "3" + item;
   }
   to_sort.sort(Qt::CaseInsensitive);
   QStringList final;

   // Drop the number prefix and the / so for "1/paper/match" we get "match"
   foreach (const QString& item, to_sort)
      final << item.mid(1);

   // ensure that no string in missing is contained within another
   for (int i = 0; i < suggests.size(); i++) {
      const QString& tocheck = suggests[i];
      bool add = true;

      for (int j = 0; j < suggests.size(); j++) {
         if (i != j && suggests[j].startsWith(tocheck))
             add = false;
      }

      if (add && !final.contains(tocheck))
            missing << tocheck;
   }

   return final;
}

#ifndef QT_NO_WIDGETS
bool utilDropSupported(QDropEvent *event, const QStringList& allowedTypes)
{
   foreach (const QString& item, event->mimeData()->formats()) {
      if (!allowedTypes.contains(item)) {
         event->ignore();
         QMessageBox::warning (0, "Paperman", "That type is not supported");
         return false;
      }
   }

   return true;
}
#endif

TreeItem::TreeItem(const QVector<QVariant> &data, TreeItem *parent)
    : m_itemData(data), m_parentItem(parent), m_isDir(false)
{}

TreeItem::~TreeItem()
{
    qDeleteAll(m_childItems);
}

void TreeItem::appendChild(TreeItem *item)
{
    m_childItems.append(item);
}

TreeItem *TreeItem::child(int row)
{
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

void TreeItem::freeChildren()
{
   while (!m_childItems.empty())
      delete m_childItems.takeFirst();
}

void TreeItem::freeTree(TreeItem *tree)
{
   tree->freeChildren();
   delete tree;
}

TreeItem *TreeItem::child(const QString& name) const
{
   foreach (TreeItem *child, m_childItems) {
      if (child->dirName() == name)
         return child;
   }

   return nullptr;
}

const TreeItem *TreeItem::childConst(int row) const
{
   return m_childItems.at(row);
}

int TreeItem::childCount() const
{
    return m_childItems.count();
}

bool TreeItem::isDir() const
{
    return m_isDir;
}

void TreeItem::setDir(bool isdir)
{
   m_isDir = isdir;
}

int TreeItem::columnCount() const
{
    return m_itemData.count();
}

QVariant TreeItem::data(int column) const
{
    if (column < 0 || column >= m_itemData.size())
        return QVariant();
    return m_itemData.at(column);
}

QString TreeItem::dirName() const
{
   return m_itemData.at(0).toString();
}

TreeItem *TreeItem::parentItem()
{
    return m_parentItem;
}

int TreeItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<TreeItem*>(this));

    return 0;
}

void TreeItem::adopt(TreeItem *old_parent)
{
    while (!old_parent->m_childItems.empty())
        m_childItems.append(old_parent->m_childItems.takeFirst());
}

const TreeItem *TreeItem::findItem(QString path) const
{
    const TreeItem *parent = this;

    QDir dir(path);
    // An empty string has a component of "." so handle that specially
    if (path.size()) {
        QStringList components = dir.path().split("/");
        for (const QString &component : components) {
            const TreeItem *child = parent->child(component);
            if (!child)
                return nullptr;
            parent = child;
        }
    }

    return parent;
}

TreeItem *TreeItem::findItemW(QString path)
{
    return (TreeItem *)findItem(path);
}

void TreeItem::dump(int indent) const {
   foreach (TreeItem *item, m_childItems) {
      qInfo() << QString("%1").arg(' ', indent) << item->dirName();
      item->dump(indent + 3);
   }
}

void TreeItem::write(QTextStream& stream, int level) const
{
   if (level) {
      for (int i = 0; i < level; i++)
         stream << QString(" ");
      if (isDir())
         stream << QString("+ ");
      else
         stream << QString("- ");
      stream << dirName() << '\n';
   }
   foreach (TreeItem *item, m_childItems)
      item->write(stream, level + 1);
}

bool TreeItem::read(QTextStream& stream, TreeItem *parent, int cur_level)
{
   QString line;

   while (!stream.atEnd()) {
      line = stream.readLine();

      int level = 0;
      for (int i = 0; i < line.length(); i++) {
         if (line[i] == ' ')
            level++;
         else
            break;
      }
      if (level < 1) {
         qInfo() << "Invalid level < 1" << line;
         return false;
      }

      QString type, fname;
      if (line.mid(level + 1, 1) == " ") {
         type = line.mid(level, 1);
         fname = line.mid(level + 2);
      } else {
         fname = line.mid(level);
      }

      TreeItem *child = new TreeItem({fname}, nullptr);
      if (type == "+")
         child->setDir(true);

      if (level == cur_level) {
         child->m_parentItem = parent;
         parent->appendChild(child);
      } else if (level == cur_level + 1) {
         // The last-added child becomes a parent
         child->m_parentItem = parent->m_childItems.last();
         child->m_parentItem->appendChild(child);
         parent = child->m_parentItem;
         cur_level++;
      } else if (level < cur_level) {
         // Find the right parent of this child
         do {
            parent = parent->m_parentItem;
         } while (level < --cur_level);
         child->m_parentItem = parent;
         parent->appendChild(child);
      } else {
         qInfo() << "Invalid level transition from cur_level" << cur_level
                 << "to level" << level << line;
         delete child;
         return false;
      }
   }

   return true;
}

#ifndef QT_NO_WIDGETS
static void scanDir(const QString &dirPath, TreeItem *parent, Operation *op)
{
   QDir dir(dirPath);
   dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks);
   dir.setSorting(QDir::Name);
   const QFileInfoList list = dir.entryInfoList();

   if (op)
      op->setCount(list.size ());
   for (int i = 0; i < list.size (); i++) {
      QFileInfo fi = list.at(i);

      QVector<QVariant> columnData = {fi.fileName()};

      if (fi.fileName()[0] == ' ') {
         qInfo() << "starts with space" << fi.fileName();
         continue;
      }

      if (fi.fileName() != "." && fi.fileName() != "..") {
         TreeItem *child = new TreeItem(columnData, parent);

         parent->appendChild(child);
         if (fi.isDir ()) {
            child->setDir(true);
            scanDir(dirPath + fi.fileName() + "/", child, 0);
         }
      }
      if (op)
         op->setProgress(i);
   }
}

TreeItem *utilScanDir(QString dirPath, Operation *op)
{
   TreeItem *root;

   QDir dir(dirPath);
   if (!dir.exists()) {
      qInfo() << "dir not found" << dirPath;
      return nullptr;
   }

   root = new TreeItem({dirPath});
   scanDir(dirPath + "/", root, op);

   return root;
}
#endif

bool utilWriteTree(QString fname, TreeItem *tree)
{
   QFile file(fname);
   QTextStream stream(&file);

   if (!file.open(QIODevice::WriteOnly))
      return false;
   tree->write(stream, 0);
   utilSetGroup(fname);

   return true;
}

TreeItem *utilReadTree(QString fname, QString rootName)
{
   QFile file(fname);
   QTextStream stream(&file);

   if (!file.open(QIODevice::ReadOnly))
      return nullptr;
   TreeItem *root = new TreeItem({rootName});

   if (!TreeItem::read(stream, root, 1)) {
      delete root;
      return nullptr;
   }

   return root;
}

/* The chmod and chown only make sense when paperman is configured to
 * share its scratch directories with a public group (set via the
 * QSettings key "files/group", consumed by utilInit()).  When no group
 * is configured, paperman is running for a single user — there is no
 * one to share with — and the previous unconditional chmod 0666/0777
 * just produced noise on directories the user didn't own (e.g. files
 * on an NFS mount).  Skip silently when public_gid is unset.
 *
 * Even when a group IS configured, EPERM from chmod/chown is
 * expected on shared NFS paths owned by other UIDs; treat that as
 * not-our-problem and silently move on.  Other errors (ENOENT, EIO,
 * misconfigured group) still warn so genuinely broken setups get
 * surfaced. */

bool utilSetDirGroup(const QString& dirname)
{
#ifdef Q_OS_WIN
    Q_UNUSED(dirname);
    return true;
#else
    if (public_gid == -1)
        return true;
    if (chmod(qPrintable(dirname), 0777) == -1) {
        if (errno != EPERM)
            qInfo() << "Failed to change permissions" << dirname
                    << ":" << strerror(errno);
        return false;
    }
    if (chown(qPrintable(dirname), -1, public_gid) == -1) {
        if (errno != EPERM)
            qInfo() << "Failed to change group" << public_gid << dirname
                    << ":" << strerror(errno);
        return false;
    }
    return true;
#endif
}

bool utilSetGroup(const QString& fname)
{
#ifdef Q_OS_WIN
    Q_UNUSED(fname);
    return true;
#else
    if (public_gid == -1)
        return true;
    if (chmod(qPrintable(fname), 0666) == -1) {
        if (errno != EPERM)
            qInfo() << "Failed to change permissions" << fname
                    << ":" << strerror(errno);
        return false;
    }
    if (chown(qPrintable(fname), -1, public_gid) == -1) {
        if (errno != EPERM)
            qInfo() << "Failed to change group" << public_gid << fname
                    << ":" << strerror(errno);
        return false;
    }
    return true;
#endif
}

QString utilUserName()
{
#ifdef Q_OS_WIN
   QString name;

   if (!util_getUsername (name))
      return name;
   return "";
#else
   char username[80];

   /* identify the user by uid: getlogin_r() needs a controlling
      terminal and fails under cron, IDE launches and tests, which
      would silently drop the per-user suffix from the .papertree
      cache filename and read a stale, shared cache instead */
   struct passwd *pw = getpwuid (getuid ());
   if (pw && pw->pw_name && *pw->pw_name)
      return pw->pw_name;

   // fall back to the login name on the controlling terminal
   if (!getlogin_r (username, sizeof (username)))
      return username;

   return "";
#endif
}

static bool headless_mode;

// how long to keep retrying a rename in total, in 50ms steps
static const int RENAME_RETRIES = 20;

bool utilRenameFile(const QString &from, const QString &to, QString *error)
{
   QFile file(from);

   for (int i = 0; i < RENAME_RETRIES; i++) {
      if (file.rename(to))
         return true;
      if (i == RENAME_RETRIES - 1 || !file.exists() || QFile::exists(to))
         break;
      QThread::msleep(50);
   }
   if (error)
      *error = file.errorString();
   return false;
}

bool utilReplaceFile(const QString &from, const QString &to, QString *error)
{
   QFile old(to);

   for (int i = 0; i < RENAME_RETRIES; i++) {
      if (!old.exists() || old.remove())
         return utilRenameFile(from, to, error);
      QThread::msleep(50);
   }
   if (error)
      *error = QString("cannot remove %1: %2").arg(to, old.errorString());
   return false;
}

void utilSetHeadless(bool headless)
{
   headless_mode = headless;
}

bool utilHeadless()
{
   return headless_mode;
}

#ifndef QT_NO_WIDGETS
const QPalette &utilStylePalette(QStyle *style)
{
   static QPalette pal;
   static QStyle *pal_style = nullptr;
   static qint64 pal_key = -1;
   qint64 key = QGuiApplication::palette().cacheKey();

   if (style != pal_style || key != pal_key) {
      pal = style->standardPalette();
      pal_style = style;
      pal_key = key;
   }
   return pal;
}
#endif

void utilInit(const QString& group)
{
#ifdef Q_OS_WIN
   // there are no Unix groups to share scratch directories with
   if (!group.isEmpty())
      qDebug() << "Ignoring group" << group << "on Windows";
#else
   struct group *grp;

   if (!group.isEmpty()) {
      grp = getgrnam(qPrintable(group));
      if (!grp) {
         qDebug() << "Cannot find group" << group;
         return;
      }
      public_gid = grp->gr_gid;
   }
#endif
}


int utilImageDepth(const QImage &image)
{
   /* Indexed and greyscale formats already have the right depth */
   if (image.depth() <= 8)
      return image.depth();

   bool grey_seen = false;
   int w = image.width();
   int h = image.height();
   int colour_count = 0;
   long npix = (long)w * h;

   for (int y = 0; y < h; y++) {
      const QRgb *line = (const QRgb *)image.constScanLine(y);

      for (int x = 0; x < w; x++) {
         int r = qRed(line[x]);
         int g = qGreen(line[x]);
         int b = qBlue(line[x]);

         int maxc = qMax(r, qMax(g, b));
         int minc = qMin(r, qMin(g, b));

         if (maxc - minc > 30)
            colour_count++;

         if (!grey_seen && r >= 32 && r <= 224)
            grey_seen = true;
      }
   }

   if (colour_count > npix / 200)
      return 24;

   return grey_seen ? 8 : 1;
}


QImage utilReduceDepth(QImage &image, int target_depth)
{
   if (target_depth == 1)
      return image.convertToFormat(QImage::Format_Mono);
   if (target_depth == 8)
      return utilConvertImageToGrey(image);

   return image;
}


#ifndef QT_NO_WIDGETS

/* Icon filenames used in the UI files and code */
static const char *icon_names[] = {
   "print", "swap", "prev", "next", "pprev", "pnext", "options",
   "scan-go", "scan", "rleft", "rright", "hflip", "vflip",
   "pointer", "hand", "scanmode", "info", "document-save",
   "document-revert", "zoom-best-fit", "zoom-original", "zoom-out",
   "zoom-in", "locate", "unknown", "no_access", "left", "right",
   "pages", "pageblank", "pagekeep", "pageremove",
   NULL
};


static QIcon darkIcon(const QIcon &icon)
{
   QString dark = QStringLiteral(":/images/images/dark/");

   /* Compare the icon's pixmap against each known light icon to find
      which one it is, then load the dark version */
   QPixmap orig = icon.pixmap(48);

   if (orig.isNull())
      return icon;

   QImage origImg = orig.toImage();

   for (const char **p = icon_names; *p; p++) {
      QString lightPath = QStringLiteral(":/images/images/") + *p + ".xpm";
      QPixmap lightPix(lightPath);

      if (lightPix.isNull())
         continue;
      if (lightPix.toImage() == origImg) {
         QString darkPath = dark + *p + ".xpm";
         QPixmap darkPix(darkPath);

         if (!darkPix.isNull())
            return QIcon(darkPix);
      }
   }

   return icon;
}


void utilUpdateIcons(QWidget *widget)
{
   if (!utilIsDarkMode())
      return;

   /* Replace icons on all actions with their dark variants */
   for (QAction *act : widget->findChildren<QAction *>()) {
      if (act->icon().isNull() || act->isSeparator())
         continue;
      act->setIcon(darkIcon(act->icon()));
   }

   /* Handle tool buttons that have icons set directly */
   for (QAbstractButton *btn : widget->findChildren<QAbstractButton *>()) {
      if (btn->icon().isNull())
         continue;
      btn->setIcon(darkIcon(btn->icon()));
   }
}
#endif


bool utilIsDarkMode(void)
{
   QPalette pal = QGuiApplication::palette();
   int bg = pal.color(QPalette::Window).lightness();
   int fg = pal.color(QPalette::WindowText).lightness();

   return fg > bg;
}


QString utilIconPath(void)
{
   if (utilIsDarkMode())
      return QStringLiteral(":/images/images/dark/");

   return QStringLiteral(":/images/images/");
}
