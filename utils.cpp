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

#include <QDate>
#include <QDebug>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QStringList>

#include <sys/types.h>

#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

#include "epeglite.h"


#include "config.h"
#include "err.h"
#include "mem.h"
#include "utils.h"
#include "zip.h"



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

   epeg_raw                    (im, stride);
#if 1 // don't do this for now */
   valid = im->last_valid_row;
   if (valid != -1 && valid < im->in.h)
      {
      printf ("data short - shrink preview from %d to %d\n", sizep->y, valid / CONFIG_preview_scale);
      sizep->y = valid / CONFIG_preview_scale;    // shrink preview
      *dest_sizep = stride * sizep->y;
      }
#endif
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


struct my_error_mgr
   {
   struct jpeg_error_mgr mgr;
   jmp_buf setjmp_buffer;
   int err;
   };


static void my_error_exit (j_common_ptr cinfo)
   {
   struct my_error_mgr* myerr = (struct my_error_mgr*) cinfo->err;
   char buffer[JMSG_LENGTH_MAX];

   (*cinfo->err->format_message)(cinfo, buffer);
   printf ("%s", buffer);
   myerr->err = 1;
   longjmp(myerr->setjmp_buffer, 1);
}


void jpeg_decode (byte *data, int size, byte *dest, int line_bytes, int bpp,
                  int max_width, int max_height)
   {
   struct jpeg_decompress_struct cinfo;
   JSAMPARRAY buffer;/* Output row buffer */
   jpeg_source_info source;
   int tile_bytes;
   struct my_error_mgr jerr;

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
         jpeg_read_scanlines(&cinfo, buffer, 1);
         if (cinfo.output_components == 3 && bpp == 32)
            {
            byte *in;
            u_int32_t *out;
            int i;

            mem_check ();
            in = (byte *)buffer [0];
            i = cinfo.output_width;
            if (max_width != -1 && i > max_width)
                i = max_width;
            for (out = (u_int32_t *)dest; i != 0;
                 i--, in += 3)
               *out++ = in [2] | (in [1] << 8) | (in [0] << 16);
            mem_check ();
            }
         else
            memcpy (dest, buffer[0], tile_bytes);
         dest += line_bytes;
         }
//       debug1 (("width = %d, tile_bytes = %d, line_bytes = %d, decoded %d lines\n",
//          cinfo.output_width, tile_bytes, line_bytes, cinfo.output_height));

      jpeg_finish_decompress(&cinfo);
      }
   jpeg_destroy_decompress(&cinfo);
   if (jerr.err)
      printf ("error = %d\n", jerr.err);
   }


static void init_destination (j_compress_ptr cinfo)
   {
   jpeg_dest_info *dest = (jpeg_dest_info *)cinfo->dest;

   dest->pub.next_output_byte = dest->data;
   dest->pub.free_in_buffer = dest->size;
   }


static boolean empty_output_buffer (j_compress_ptr cinfo)
   {
   cinfo = cinfo;
   return true;
   }


static void term_destination (j_compress_ptr cinfo)
   {
   cinfo = cinfo;
   }


void jpeg_encode (byte *image, cpoint *tile_size, byte *outbuff, int *size,
            int bpp, int line_bytes, int quality)
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
//  dest.pub.next_output_byte = NULL;
//  dest.pub.free_in_buffer = 0;
  dest.pub.next_output_byte = outbuff;
  dest.pub.free_in_buffer = *size;
  dest.pub.init_destination = init_destination;
  dest.pub.empty_output_buffer = empty_output_buffer;
  dest.pub.term_destination = term_destination;

//  printf ("jpeg_encode: bpp=%d\n", bpp);

  *size = 0;
  buff = (byte *)malloc (row_stride);
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
     bytes = cinfo.image_width * bpp / 8;
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
          u_int32_t *in;
          byte *out;

          for (i = cinfo.image_width, in = (unsigned *)ptr, out = buff; i != 0;
               i--, in++, out += 3)
             {
             out [2] = *in;
             out [1] = *in >> 8;
             out [0] = *in >> 16;
             }
          }
       else
          memcpy (buff, ptr, bytes);
       (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
     }

     /* Step 6: Finish compression */
     jpeg_finish_compress(&cinfo);

      *size = dest.pub.next_output_byte - outbuff;
      }
   jpeg_destroy_compress(&cinfo);
   free (buff);
   if (jerr.err)
      printf ("error = %d\n", jerr.err);
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
   Q_ASSERT ((unsigned long)test < 0xb0000000);
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
   int fd;

   sprintf (tmp, "%s/maxviewXXXXXX", P_tmpdir);
   fd = mkstemp (tmp);
   if (fd < 0)
      return err_make (ERRFN, ERR_could_not_make_temporary_file);
   close (fd);
   unlink (tmp);
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

   zip = util_getUnique (zip, P_tmpdir, ".zip");
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
#if defined(Q_WS_WIN)
   // in Windows land...
#if defined(UNICODE)
   if ( qWinVersion() & Qt::WV_NT_based )
      {
      TCHAR winUserName[UNLEN + 1]; // UNLEN is defined in LMCONS.H
      DWORD winUserNameSize = sizeof(winUserName);
      GetUserName( winUserName, &winUserNameSize );
      userName = qt_winQString( winUserName );
      } 
   else
#endif
      {
      char winUserName[UNLEN + 1]; // UNLEN is defined in LMCONS.H
      DWORD winUserNameSize = sizeof(winUserName);
      GetUserNameA( winUserName, &winUserNameSize );
      userName = QString::fromLocal8Bit( winUserName );
      }
#else
   // in the real world
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
   QRegExp rx("(\\d{4})");

   for (int pos = 0; pos = rx.indexIn(fname, pos), pos != -1;
        pos += rx.matchedLength()) {

      // make sure here is no digit either side
      if ((pos && fname[pos - 1].isDigit()) ||
          (pos + 4 < len && fname[pos + 4].isDigit()))
         continue;
      int year = rx.cap(0).toInt();

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
   // search for year from 1900 to 2099
   QRegExp rx(months, Qt::CaseInsensitive);

   for (int pos = 0; pos = rx.indexIn(fname, pos), pos != -1;
        pos += rx.matchedLength()) {
      int len = rx.pos() + 5;

      // make sure here is no letter either side
      if ((pos && fname[pos - 1].isLetter()) ||
          (fname.size() > len && fname[len].isLetter()))
         continue;

      int month = 1 + months.indexOf(rx.cap(0)) / 6;
      foundPos = pos;

      return month;
   }

   return 0;
}

QStringList utilDetectMatches(const QDate& date, QStringList& matches,
                              QStringList& missing)
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
         ;
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

TreeItem::TreeItem(const QVector<QVariant> &data, TreeItem *parent)
    : m_itemData(data), m_parentItem(parent)
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

int TreeItem::childCount() const
{
    return m_childItems.count();
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

void setupModelData(const QStringList &lines, TreeItem *parent)
{
    QVector<TreeItem*> parents;
    QVector<int> indentations;
    parents << parent;
    indentations << 0;

    int number = 0;

    while (number < lines.count()) {
        int position = 0;
        while (position < lines[number].length()) {
            if (lines[number].at(position) != ' ')
                break;
            position++;
        }

        const QString lineData = lines[number].mid(position).trimmed();

        if (!lineData.isEmpty()) {
            // Read the column data from the rest of the line.
            const QStringList columnStrings =
                lineData.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
            QVector<QVariant> columnData;
            columnData.reserve(columnStrings.count());
            for (const QString &columnString : columnStrings)
                columnData << columnString;

            if (position > indentations.last()) {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.

                if (parents.last()->childCount() > 0) {
                    parents << parents.last()->child(parents.last()->childCount()-1);
                    indentations << position;
                }
            } else {
                while (position < indentations.last() && parents.count() > 0) {
                    parents.pop_back();
                    indentations.pop_back();
                }
            }

            // Append a new item to the current parent's list of children.
            parents.last()->appendChild(new TreeItem(columnData, parents.last()));
        }
        ++number;
    }
}
