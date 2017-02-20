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


#include <QDebug>
#include <QFile>

#include "filepdf.h"
#include "pdfio.h"



#define DPI 300.0



Filepdf::Filepdf (const QString &dir, const QString &filename, Desk *desk)
   : File (dir, filename, desk, Type_pdf)
   {
   _pdfio = 0;
   }


Filepdf::~Filepdf ()
   {
   if (_pdfio)
      delete _pdfio;
   }


#if 0
err_info *Filepdf::setup_pdf (void)
   {
   char str [200];
   FILE *f;
   int i, fd;
   page_info *page;
   char tmp [80];
   char *fname;

#if 1 // without pdflib
   // get page count
//   tmp = tmpnam (NULL);
   sprintf (tmp, "%s/maxviewXXXXXX", P_tmpdir);
   fd = mkstemp (tmp);
   if (fd < 0)
      {
      sprintf (str, "could not make temporary file %s\n", tmp);
      return err_make (ERRFN, ERR_unable_to_read_image_header_information1, str);
      }
   close (fd);

//! should use pdfinfo here?
   fname = max_get_shell_filename (max);
   if (err_systemf ("pdf2dsc %s %s", fname, tmp) != 0)
      {
      sprintf (str, "pdf2dsc failed on file %s\n", fname);
      return err_make (ERRFN, ERR_unable_to_read_image_header_information1, str);
      }
   f = fopen (tmp, "r");
   if (!f)
      {
      sprintf (str, "could not open pdf2dsc file %s\n", tmp);
      return err_make (ERRFN, ERR_unable_to_read_image_header_information1, str);
      }
   while (fgets (str, 80, f))
      {
      if (0 == strncmp (str, "%%Pages: ", 9))
         {
         _page_count = atoi (str + 9);
         break;
         }
      }
   fclose (f);
   unlink (tmp);

   _page = malloc (sizeof (page_info) * _page_count);

   // get titles
   for (i = 0, page = _page; i < _page_count;
        i++, page++)
      {
      sprintf (str, "Page %d", i + 1);
      page->titlestr = strdup (str);
      }
   return NULL;
#endif
   }


err_info *Filepdf::getPreviewPixmap (int pagenum, QString &title,
          QPixmap &pixmap, int &bpp, bool blank)
         }

      case FILET_pdf :
         pixmap = get_pdf_preview (f->max, pagenum, title, bpp);
         break;
#endif


err_info *Filepdf::load (void)
   {
   if (!_pdfio)
      _pdfio = new Pdfio (_pathname);
   if (!_valid)
      {
      CALL (_pdfio->open ());
      _valid = true;
      }
   return NULL;
   }


  // was desk->ensureMax

/** create the file (on the filesystem). It doesn't need to be written to
      as we will call flush() later for that */
err_info *Filepdf::create (void)
   {
   if (!_pdfio)
      _pdfio = new Pdfio (_pathname);
   return _pdfio->create ();
   }


err_info *Filepdf::flush (void)
   {
   return _pdfio->flush ();
   }




err_info *Filepdf::remove (void)
   {
   //FIXME: perhaps this should be implemented by File
   // currently only works if the object is about to be destroyed
   QFile file (_dir + _filename);

   if (file.exists () && !file.remove ())
      return err_make (ERRFN, ERR_could_not_remove_file2,
                file.fileName ().toLatin1 ().constData(),
                file.errorString ().toLatin1 ().constData());
   return NULL;
   }


int Filepdf::pagecount (void)
   {
   if (_pdfio)
      return _pdfio->numPages ();
   return 1;
   }


// accessing and changing metadata

err_info *Filepdf::getPageTitle (int pagenum, QString &title)
   {
   return _pdfio->getPageTitle (pagenum, title);
   }




err_info *Filepdf::getAnnot (e_annot type, QString &text)
   {
   QString name = getAnnotName (type);

   return _pdfio->getAnnot (name, text);
   }




err_info *Filepdf::putAnnot (QHash<int, QString> &)
   {
   return not_impl ();
   }


err_info *Filepdf::putEnvelope (QStringList &)
   {
   return not_impl ();
   }


err_info *Filepdf::getPageText (int pagenum, QString &str)
   {
   return _pdfio->getPageText (pagenum, str);
   }




/** gets the total size of a file in bytes. this should include data not
      yet flushed to the filesystem */
int Filepdf::getSize (void)
   {
   return _size;
   }




err_info *Filepdf::renamePage (int, QString &)
   {
   return not_impl ();
   }


err_info *Filepdf::getImageInfo (int pagenum, QSize &size,
      QSize &true_size, int &bpp, int &image_size, int &compressed_size,
      QDateTime &timestamp)
   {
   if (!_pdfio)
      return NULL;
   CALL (_pdfio->getImageSize (pagenum, false, size, bpp));
   true_size = size;
   image_size = size.width () * size.height () * bpp / 8;
   compressed_size = -1;
   timestamp = _timestamp;
   return NULL;
   }


err_info *Filepdf::getPreviewInfo (int pagenum, QSize &size, int &bpp)
   {
   return _pdfio->getImageSize (pagenum, true, size, bpp);
   }


// image related
QPixmap Filepdf::pixmap (bool recalc)
   {
   err_info *err = NULL;

   if (recalc)
      err = getPreviewPixmap (_pagenum, _pixmap, false);
   return err || _pixmap.isNull () ? unknownPixmap () : _pixmap;
   }


err_info *Filepdf::getPreviewPixmap (int pagenum, QPixmap &pixmap, bool blank)
   {
   QImage image;

   CALL (_pdfio->getImage (_filename, pagenum, image, DPI / 24, DPI / 24, true));
   if (blank)
      colour_image_for_blank (image);
   pixmap = QPixmap::fromImage(image);
//    qDebug () << "pixmap" << pixmap.width () << pixmap.height ();
   return NULL;
   }


err_info *Filepdf::getImage (int pagenum, bool,
            QImage &image, QSize &size, QSize &trueSize, int &bpp, bool blank)
   {
   // this gives us the page size at 72dpi, but does work for our DPI
//    CALL (_pdfio->getImageSize (pagenum, size));
   CALL (_pdfio->getImage (_filename, pagenum, image, DPI, DPI, false));
   if (blank)
      colour_image_for_blank (image);
   trueSize = size = image.size ();
   bpp = image.depth ();
//    qDebug () << "pixmap" << pixmap.width () << pixmap.height ();
   return NULL;
   }






// operations on files

err_info *Filepdf::addPage (const Filepage *mp, bool do_flush)
   {
   CALL (_pdfio->addPage (mp));
   if (do_flush)
      CALL (flush ());
   return NULL;
   }




err_info *Filepdf::removePages (QBitArray &,
      QByteArray &, int &)
   {
   return not_impl ();
   }




err_info *Filepdf::restorePages (QBitArray &,
   QByteArray &, int )
   {
   return not_impl ();
   }




err_info *Filepdf::unstackPages (int pagenum, int pagecount, bool remove,
            File *fdest)
   {
   Filepdf *dest = (Filepdf *)fdest;

   CALL (dest->_pdfio->insertPages (_pdfio, pagenum, pagecount));

   // now remove from src file if required
   if (remove)
      CALL (_pdfio->deletePages (pagenum, pagecount));

   return NULL;
   }




err_info *Filepdf::stackStack (File *fsrc)
   {
   Filepdf *src = (Filepdf *)fsrc;

   return _pdfio->appendFrom (src->_pdfio);
   }




err_info *Filepdf::duplicate (File *&, File::e_type, const QString &,
      int, Operation &, bool &supported)
   {
   supported = false;
   return NULL;
   }




