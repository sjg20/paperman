/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2011 Simon Glass, chch-kiwi@users.sourceforge.net
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


#include <QDebug>
#include <QFile>
#include <QImage>
#include <QProcess>

#include "filejpeg.h"



#define DPI 300.0



Filejpeg::Filejpeg (const QString &dir, const QString &filename, Desk *desk)
   : File (dir, filename, desk, Type_pdf)
   {
   _changed = false;
   }


Filejpeg::~Filejpeg ()
   {
   }

err_info *Filejpeg::load (void)
   {
   if (!_valid)
      _valid = _image.load (_pathname, "JPG");
   _changed = false;
   return NULL;
   }


  // was desk->ensureMax

/** create the file (on the filesystem). It doesn't need to be written to
      as we will call flush() later for that */
err_info *Filejpeg::create (void)
   {
   // We choose to do nothing here
   _changed = true;
   return NULL;
   }


err_info *Filejpeg::flush (void)
   {
   if (_changed && !_image.save (_pathname, "JPG"))
      return err_make (ERRFN, ERR_could_not_write_image_to_as2,
                       qPrintable (_pathname), "JPEG");
   return NULL;
   }




err_info *Filejpeg::remove (void)
   {
   //FIXME: perhaps this should be implemented by File
   // currently only works if the object is about to be destroyed
   QFile file (_dir + _filename);

   if (file.exists () && !file.remove ())
      return err_make (ERRFN, ERR_could_not_remove_file2,
                file.name ().latin1 (), file.errorString ().latin1 ());
   return NULL;
   }


int Filejpeg::pagecount (void)
   {
   return _image.isNull () ? 0 : 1;
   }


// accessing and changing metadata

err_info *Filejpeg::getPageTitle (int pagenum, QString &title)
   {
   if (pagenum == 0)
      title = _page_title;
   return NULL;
   }


static const char *tags [File::Annot_count] =
   {
   "Artist",
   "ImageDescription",
   "Keywords",
   "Notes"
   };


File::e_annot Filejpeg::fromTag (QString tag)
   {
   int annot ;

   for (annot = 0; annot < Annot_count; annot++)
      if (tag == tags [annot])
         break;
   return (e_annot)annot;
   }

QString Filejpeg::toTag (File::e_annot annot)
   {
   return tags [annot];
   }

static err_info *run_exiftool (QProcess &process, const char *operation,
                               QStringList &args)
   {
   qDebug () << args;
   process.start ("exiftool", args);
   if (!process.waitForFinished(500))
      return err_make (ERRFN, ERR_cannot_exiftool_error2,
                          "load annotations", "process stuck");
   if (process.exitStatus () != QProcess::NormalExit ||
         process.exitCode () != 0)
      return err_make (ERRFN, ERR_cannot_exiftool_error2, operation,
                       "process exited with error");
   return NULL;
   }

err_info *Filejpeg::load_annot (void)
   {
   QProcess process;
   QStringList args;

   args << "-t" << "-s" << "-Author" << "-ImageDescription" << "-Keywords"
           << "-Notes" << _pathname;
   CALL (run_exiftool (process, "load annotations", args));
   _annot_loaded = TRUE;

   for (int annot = 0; annot < Annot_count; annot++)
      _annot_data << "";

   QStringList list;
   do
      {
      QByteArray ba = process.readLine ();

      QString line = ba.constData ();
      list = line.split ('\t');

      qDebug () << list;
      if (list.size () == 2)
         {
         e_annot annot = fromTag (list [0]);

         if (annot != Annot_none)
            _annot_data [annot] = list [1];
         }
      } while (list.size () == 2);
   return NULL;
   }

//Artist, kImageDescription, kKeywords, kMakerNote

QString Filejpeg::getAnnot (e_annot type)
   {
   err_info *err = 0;
   QString str;

   if (!_valid)
      str = QString ("<error: no jpeg>");
   else
      {
      if (!_annot_loaded)
         err = load_annot ();
      Q_ASSERT (type >= 0 && type < Annot_count);
      if (err)
         str = QString ("<error %1>").arg (err->errstr);
      else if (type < _annot_data.size ())
         str = _annot_data [type];
      }
   return str;
   }



err_info *Filejpeg::create_annot (void)
   {
   QProcess process;
   QStringList args;

   for (int annot = 0; annot < Annot_count; annot++)
      args << QString ("-%1='%2'").arg (toTag ((e_annot)annot))
                  .arg (_annot_data [annot]);

   // Try to preserve original timestamp
#if defined(Q_OS_MAC) || defined(Q_OS_WIN32)
   args << "-overwrite_original_in_place";
#else
   args << "-overwrite_original" << "-preserve";
#endif
   args << _pathname;
   CALL (run_exiftool (process, "save annotations", args));
   return NULL;
   }


err_info *Filejpeg::putAnnot (QHash<int, QString> &updates)
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


err_info *Filejpeg::putEnvelope (QStringList &)
   {
   return not_impl ();
   }


err_info *Filejpeg::getPageText (int pagenum, QString &str)
   {
   QProcess process;
   QStringList args;

   args << "-t" << "-s" << "-TextLayerText" << _pathname;
   CALL (run_exiftool (process, "load annotations", args));
   str = "<no-TextLayerText>";
   QByteArray ba = process.readLine ();

   QString line = ba.constData ();
   QStringList list = line.split ('\t');
   if (list.size () == 2)
      str = list [1];
   return NULL;
   }


/** gets the total size of a file in bytes. this should include data not
      yet flushed to the filesystem */
int Filejpeg::getSize (void)
   {
   return _size;
   }


err_info *Filejpeg::renamePage (int, QString &name)
   {
   return not_impl ();
   }


err_info *Filejpeg::getImageInfo (int pagenum, QSize &size,
      QSize &true_size, int &bpp, int &image_size, int &compressed_size,
      QDateTime &timestamp)
   {
   size = _image.size ();
   true_size = size;
   bpp = _image.depth ();
   image_size = _image.byteCount ();
   compressed_size = -1;
   timestamp = _timestamp;
   return NULL;
   }


err_info *Filejpeg::getPreviewInfo (int pagenum, QSize &size, int &bpp)
   {
   size = _image.size () / 24;
   bpp = _image.depth ();
   return NULL;
   }


// image related
QPixmap Filejpeg::pixmap (bool recalc)
   {
   err_info *err = NULL;

   if (recalc)
      err = getPreviewPixmap (_pagenum, _pixmap, false);
   return err || _pixmap.isNull () ? unknownPixmap () : _pixmap;
   }


err_info *Filejpeg::getPreviewPixmap (int pagenum, QPixmap &pixmap, bool blank)
   {
   QSize size;
   QImage image;

   size = _image.size () / 24;
   image = _image.scaled (size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
   pixmap = QPixmap (image);
//    qDebug () << "pixmap" << pixmap.width () << pixmap.height ();
   return NULL;
   }


err_info *Filejpeg::getImage (int pagenum, bool do_scale,
            QImage &image, QSize &size, QSize &trueSize, int &bpp, bool blank)
   {
   image = _image;
   size = _image.size ();
   trueSize = size = image.size ();
   bpp = image.depth ();
//    qDebug () << "pixmap" << pixmap.width () << pixmap.height ();
   return NULL;
   }






// operations on files

err_info *Filejpeg::addPage (const Filepage *mp, bool do_flush)
   {
   if (pagecount () > 0)
      return err_make (ERRFN, ERR_file_type_only_supports_a_single_page1,
                       qPrintable(typeName ()));
   mp->getImage (_image);
   _image.bits ();   // force deep copy
   return NULL;
   }




err_info *Filejpeg::removePages (QBitArray &pages,
      QByteArray &del_info, int &count)
   {
   return not_impl ();
   }




err_info *Filejpeg::restorePages (QBitArray &pages,
   QByteArray &del_info, int count)
   {
   return not_impl ();
   }




err_info *Filejpeg::unstackPages (int pagenum, int pagecount, bool remove,
            File *fdest)
   {
   return not_impl ();
   }




err_info *Filejpeg::stackStack (File *fsrc)
   {
   return not_impl ();
   }




err_info *Filejpeg::duplicate (File *&fnew, File::e_type type, const QString &uniq,
      int odd_even, Operation &op, bool &supported)
   {
   supported = false;
   return NULL;
//    return not_impl ();
   }
