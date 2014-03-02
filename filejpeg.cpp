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
#include "utils.h"


#define DPI 300.0



Filejpeg::Filejpeg (const QString &dir, const QString &filename, Desk *desk)
       : File (dir, filename, desk, Type_jpeg)
   {
   QString base, ext;
   int pagenum;

   _has_pagenum = decodePageNumber (filename, _base_fname, pagenum, ext);
   addSubPage(filename, _has_pagenum ? pagenum : 0);
   }


Filejpeg::~Filejpeg ()
   {
   }

err_info *Filejpeg::load (void)
   {
   err_info *err = 0;

   if (!_valid && _pages.size ())
      {
      err = _pages [0]->load (_dir);
      _valid = err == 0;
      }

   return err;
   }


  // was desk->ensureMax

/** create the file (on the filesystem). It doesn't need to be written to
      as we will call flush() later for that */
err_info *Filejpeg::create (void)
   {
   // We choose to do nothing here

   return NULL;
   }


err_info *Filejpeg::flush (void)
   {
   foreach (Filejpegpage *page, _pages)
      {
      if (page)
         CALL (page->flush (_dir));
      }

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
   return _pages.size ();
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

   if (!process.waitForFinished(500) ||
         process.exitStatus () != QProcess::NormalExit ||
         process.exitCode () != 0)
      {
      QByteArray ba = process.readAllStandardError () + process.readAllStandardOutput ();
      QString err = ba.constData ();

      if (err.isEmpty ())
         err = "Have you installed libimage-exiftool-perl?";
      return err_make (ERRFN, ERR_cannot_exiftool_error2, operation,
                       qPrintable (err));
      }
   return NULL;
   }

err_info *Filejpeg::load_annot (void)
   {
   QProcess process;
   QStringList args;

   args << "-t" << "-s" << "-Author" << "-ImageDescription" << "-Keywords"
           << _pathname;
   CALL (run_exiftool (process, "load annotations", args));
   _annot_loaded = TRUE;

   for (int annot = 0; annot < Annot_count; annot++)
      _annot_data << "";

   QStringList list;
   do
      {
      QByteArray ba = process.readLine ();

      QString line = ba.constData ();

      line.chop (1);
      list = line.split ('\t');

      qDebug () << list;
      if (list.size () == 2)
         {
         e_annot annot = fromTag (list [0]);

         if (annot != Annot_none)
            _annot_data [annot] = utilRemoveQuotes (list [1]);
         }
      } while (list.size () == 2);

   // Now get the notes
   args.clear ();
   args << "-b" << "-Notes" << _pathname;
   CALL (run_exiftool (process, "load notes", args));
   _annot_loaded = TRUE;
   _annot_data [Annot_notes] = utilRemoveQuotes (process.readAllStandardOutput ());

   return NULL;
   }

err_info *Filejpeg::getAnnot (e_annot type, QString &text)
   {
   if (!_valid)
      return err_make (ERRFN, ERR_file_not_loaded_yet1,
                       qPrintable (_filename));
   if (!_annot_loaded)
      CALL (load_annot ());
   Q_ASSERT (type >= 0 && type < Annot_count);
   if (type < _annot_data.size ())
      text = _annot_data [type];
   return NULL;
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


err_info *Filejpeg::checkPage (int pagenum)
   {
   if (pagenum >= _pages.size () || !_pages [pagenum])
      return err_make (ERRFN, ERR_page_number_out_of_range2, pagenum, 0);

   return NULL;
   }

err_info *Filejpeg::getPage (int pagenum, const Filejpegpage *&page)
   {
   CALL (checkPage (pagenum));
   page = _pages [pagenum];

   return NULL;
   }

err_info *Filejpeg::getPage (int pagenum, Filejpegpage *&page)
   {
   CALL (checkPage (pagenum));
   page = _pages [pagenum];

   return NULL;
   }

err_info *Filejpeg::setFilename (int pagenum, QString fname)
{
   Filejpegpage *page;

   CALL (getPage (pagenum, page));
   page->setFilename (fname);

   return 0;
}

err_info *Filejpeg::getPageText (int pagenum, QString &str)
   {
   QProcess process;
   QStringList args;

   CALL (checkPage (pagenum));
   args << "-b" << "-TextLayerText" << _pathname;
   CALL (run_exiftool (process, "load ocr text (TextLayerText)", args));
   str = utilRemoveQuotes (process.readAllStandardOutput ());
   return NULL;
   }


/** gets the total size of a file in bytes. this should include data not
      yet flushed to the filesystem */
int Filejpeg::getSize (void)
   {
   _size = 0;
   foreach (const Filejpegpage *page, _pages)
      _size += page->size ();

   return _size;
   }


err_info *Filejpeg::renamePage (int pagenum, QString &)
   {
   CALL (checkPage (pagenum));
   return not_impl ();
   }

err_info *Filejpeg::loadPage (int pagenum, QImage &image)
{
   Filejpegpage *page;

   CALL (checkPage (pagenum));
   page = _pages [pagenum];
   CALL (page->load (_dir));

   image = page->getImage ();

   return 0;
}

err_info *Filejpeg::getImageInfo (int pagenum, QSize &size,
      QSize &true_size, int &bpp, int &image_size, int &compressed_size,
      QDateTime &timestamp)
   {
   QImage image;

   CALL (loadPage (pagenum, image));

   size = image.size ();
   true_size = size;
   bpp = image.depth ();
   image_size = image.byteCount ();
   compressed_size = -1;
   timestamp = _timestamp;
   return NULL;
   }


err_info *Filejpeg::getPreviewInfo (int pagenum, QSize &size, int &bpp)
   {
   QImage image;

   CALL (loadPage (pagenum, image));

   size = image.size () / 24;
   bpp = image.depth ();
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

   CALL (loadPage (pagenum, image));
   size = image.size () / 24;
   image = image.scaled (size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
   if (blank)
      colour_image_for_blank (image);
   pixmap = QPixmap (image);
//    qDebug () << "pixmap" << pixmap.width () << pixmap.height ();
   return NULL;
   }


err_info *Filejpeg::getImage (int pagenum, bool,
            QImage &image, QSize &size, QSize &trueSize, int &bpp, bool blank)
   {
   CALL (loadPage (pagenum, image));
   size = image.size ();
   trueSize = size = image.size ();
   bpp = image.depth ();
   if (blank)
      colour_image_for_blank (image);
//    qDebug () << "pixmap" << pixmap.width () << pixmap.height ();
   return NULL;
   }






// operations on files

err_info *Filejpeg::addPage (const Filepage *mp, bool do_flush)
   {
   int pagenum = pagecount ();
   QImage image;

   mp->getImage (image);

   QString fname = encodePageNumber (_base_fname, pagenum);

   addSubPage(fname, pagenum);
   _pages [pagenum]->setImage (image);

   if (do_flush)
      CALL (flush ());

   return NULL;
   }




err_info *Filejpeg::removePages (QBitArray &, QByteArray &, int &)
   {
   return not_impl ();
   }




err_info *Filejpeg::restorePages (QBitArray &, QByteArray &, int)
   {
   return not_impl ();
   }



err_info *Filejpeg::copyOrMovePageFile (Filejpeg *src, int src_pagenum,
                                        int dst_pagenum, Filejpeg::op_t op,
                                        QString &dest_fname)
   {
   const Filejpegpage *src_page;

   CALL (src->getPage (src_pagenum, src_page));
   QString src_path = src_page->pathname (src->_dir);
   dest_fname = encodePageNumber (_base_fname, dst_pagenum);
   QString dest_path = _dir + dest_fname;
   QFile file (src_path);

   if (op == op_copy)
      {
      if (!file.copy (dest_path))
         return err_make (ERRFN, ERR_could_not_copy_file2,
                          qPrintable (src_path), qPrintable (dest_path));
      }
   else
      {
      if (!file.rename (dest_path))
         return err_make (ERRFN, ERR_could_not_rename_file2,
                          qPrintable (src_path), qPrintable (dest_path));
      }

   return 0;
   }

err_info *Filejpeg::unstackPages (int pagenum, int pagecount, bool remove,
                                  File *fdest)
   {
   Filejpeg *dest = (Filejpeg *)fdest;
   int cur_page = pagenum;

   // No attempt is made to rollback on error, need to consider that.
   for (int i = 0; i < pagecount; i++)
      {
      QString dest_fname;

      CALL (dest->copyOrMovePageFile (this, cur_page, i,
                                      remove ? op_move : op_copy, dest_fname));

      if (remove)
         {
         /*
          * This is slow - we could instead create a new list as we go, but
          * this implementation is slightly better if we get an error along
          * the way. It's not clear that we will have JPEG stacks with
          * large numbers of pages, and the file rename cost may dominate. We
          * can optimise this if it becomes a problem.
          */
         _pages.removeAt (pagenum);
         }
      else
         {
         cur_page++;
         }
      dest->setFilename (i, dest_fname);
      }

   // Rename all the files beyond the ones that were removed
   if (remove)
      {
      for (; cur_page < _pages.size(); cur_page++)
         {
         QString dest_fname;

         CALL (copyOrMovePageFile (this, cur_page, cur_page, op_move,
                                   dest_fname));
         setFilename (cur_page, dest_fname);
         }
      }
   CALL (dest->flush ());

   return 0;
   }


err_info *Filejpeg::stackStack (File *fsrc)
   {
   Filejpeg *src = (Filejpeg *)fsrc;
   int count = src->_pages.size ();

   // 'Make space' by renaming files out of the way
   for (int pagenum = _pages.size() - 1; pagenum >= _pagenum; pagenum--)
      {
      QString dest_fname;
      int dest_pagenum = pagenum + count;

      CALL (copyOrMovePageFile (this, pagenum, dest_pagenum, op_move,
                                dest_fname));
      setFilename (pagenum, dest_fname);
      }

   for (int pagenum = 0; pagenum < src->_pages.size (); pagenum++)
      {
      int cur_page = _pagenum + pagenum;
      QString dest_fname;

      CALL (copyOrMovePageFile (src, pagenum, cur_page, op_copy, dest_fname));
      _pages.insert (_pages.begin () + cur_page, new Filejpegpage (dest_fname));
      }

   return 0;
   }




err_info *Filejpeg::duplicate (File *&, File::e_type, const QString &,
      int, Operation &, bool &supported)
   {
   supported = false;
   return NULL;
   }

bool Filejpeg::addSubPage(const QString &filename, int pagenum)
{
   while (pagenum > _pages.size())
      _pages << new Filejpegpage ();

   // Cannot overwrite a page
   if (_pages.size() > pagenum)
      {
      if (_pages[pagenum])
         return false;
      _pages[pagenum] = new Filejpegpage (filename);
      }
   else
      _pages << new Filejpegpage (filename);

   return true;
}

bool Filejpeg::claimFileAsNewPage (const QString &fname, QString &base_fname,
                                   int pagenum)
   {
   // We must have a page number ourselves
   if (!_has_pagenum)
      return false;

   if (base_fname != _base_fname)
      return false;

   // Ignore return value, since it just means we already have this page
   addSubPage (fname, pagenum);

   qDebug () << "claimed" << base_fname << pagenum;

   return true;
   }


Filejpegpage::Filejpegpage ()
   {
   _changed = false;
   }

Filejpegpage::Filejpegpage (const QString &fname)
   {
   _filename = fname;
   _changed = false;
   }

Filejpegpage::~Filejpegpage (void)
   {
   }

err_info *Filejpegpage::compress (void)
   {
   // Convert to JPEG if we need to, replacing the existing data
   if (!_jpeg)
      {
      QImage image;

      getImageFromLines (_data.constData (), _width, _height, _depth, _stride,
                         image, false, false);
      _data = QByteArray ((const char *)image.bits (), image.byteCount ());
      _stride = -1;
      _jpeg = true;
      }

   return 0;
   }

err_info *Filejpegpage::load (const QString &dir)
   {
   QString path = pathname (dir);

   if (!_image.load (path, "JPG"))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (path));

   _changed = false;

   return 0;
   }

err_info *Filejpegpage::flush (const QString &dir)
   {
   QString path = pathname (dir);

   if (_changed && !_image.save (path, "JPG"))
      return err_make (ERRFN, ERR_could_not_write_image_to_as2,
                       qPrintable (path), "JPEG");

   return 0;
   }

const QImage &Filejpegpage::getImage (void) const
{
   return _image;
}

void Filejpegpage::setImage (const QImage &image)
{
   _image = image;
   _image.bits ();   // force deep copy
}

int Filejpegpage::size (void) const
{
   return _image.byteCount ();
}

QString Filejpegpage::pathname (const QString &dir) const
{
   return dir + _filename;
}

void Filejpegpage::setFilename (const QString &fname)
{
   _filename = fname;
   _image = QImage ();
   _changed = false;
}
