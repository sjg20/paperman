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
/*
   Project:    Maxview
   Author:     Simon Glass
   Copyright:  2001-2009 Bluewater Systems Ltd, www.bluewatersys.com
   File:       pdfwrite.h
   Started:    2/7/09

   This file implmenents a simple PDF writer, sufficient for out purposes.
   It uses the PoDoFo library. Unfortunately libpdf's open source version
   doesn't handle modification of existing PDFs, which is needed
*/


#include <QString>

#include "config.h"
#include "err.h"


#ifdef CONFIG_use_poppler
# if QT_VERSION >= 0x050000
#include <poppler-qt5.h>
# else
#include <poppler-qt4.h>
# endif
#endif


#include "podofo/podofo.h"


namespace PoDoFo
   {
   class PdfMemDocument;
   class PdfObject;
   };

class Filepage;


class Pdfio
   {
public :
   Pdfio (const QString &fname);
   ~Pdfio ();

   err_info *create (void);

   err_info *addPage (const Filepage *mp);

   err_info *open (void);

   err_info *close (void);

   err_info *flush (void);

   int numPages (void);

   err_info *getImage (QString fname, int pagenum, QImage &image, double xscale,
         double yscale, bool preview);

   /** looks at the image/preview for the given page and returns its size
       in pixels, or QSize() if none

       For preview = false:
          If the page consists of a single image only, then we will return the
          size of that image. If the page needs rendering, then we will return
          the image size of that page at 300dpi (so for A4 about 2400x3500).
          We will never return a zero size.

       For preview = true:
          If the page has a preview image, we return its size. If not, we
          return QSize ()

       \param pagenum   page number to check
       \param preview   true to return info on preview, false to look at image
       \param size      returns size
       \param bpp       returns bits per pixels
       \returns error, or NULL if ok */
   err_info *getImageSize (int pagenum, bool preview, QSize &size, int &bpp);
   err_info *getPageText (int pagenum, QString &str);
   err_info *getPageTitle (int pagenum, QString &title);
   err_info *getAnnot (QString type, QString &str);

   /** appends a pdf file into another file

      \param from    source PDF files
      \returns error, or NULL if ok */
   err_info *appendFrom (Pdfio *from);

   /** append pages from another document without writing to disk

      Use flush() to write the combined result after all pages are
      appended.

      \param from    source PDF document
      \returns error, or NULL if ok */
   err_info *appendPages (Pdfio *from);

   /** inserts pages from a pdf file into another

      \param from    source PDF file
      \param start   start page (0-based)
      \param count   number of pages */
   err_info *insertPages (Pdfio *from, int start, int count);

   /** delete pages from a pdf file

      \param start   first page to delete
      \param count   number of pages to delete */
   err_info *deletePages (int start, int count);

protected:
#ifdef CONFIG_use_poppler
   /** finds the given page

      \param pagenum    page number to find
      \param page       returns pointer to page
      \returns error if any, else NULL */
   err_info *find_page (int pagenum, Poppler::Page *&page);
#endif

   /** looks up a page number in the PDF file to see if it consists
       solely of an image. If so, return the object containing that
       image and its dictionary. This function will ignore objects
       which have other drawing commands, or don't have a dictionary

       \param pagenum   page number to look for (0...n-1)
       \param dict      returns object dictionary if found
       \return pointer to object, or 0 if nothing suitable found */
   const PoDoFo::PdfObject *get_image_obj (int pagenum,
         const PoDoFo::PdfDictionary *&dict);

   const PoDoFo::PdfObject *get_xobject_image (const PoDoFo::PdfReference &ref,
      const PoDoFo::PdfDictionary *&dict);

   const PoDoFo::PdfObject *get_thumbnail_obj (int pagenum,
         const PoDoFo::PdfDictionary *&dict);

   /** given an image dictionary, get the image details from it.

      We only support RGB images with 24bpp and grey images with 1 or 8bpp

      So for example CMYK images will return garbage

      \param dict    dictionary to look at
      \param width   returns image width
      \param height  returns image height
      \param bpp     returns bits per pixel (1, 8 or 24) */
   void get_image_details (const PoDoFo::PdfDictionary *dict, int &width, int &height,
         int &bpp) const;

   err_info *make_error (const PoDoFo::PdfError &eCode);

private:
   PoDoFo::PdfMemDocument *_doc; //!< document handle
#ifdef CONFIG_use_poppler
   Poppler::Document *_pop;
#endif
   QString _pathname;            //!< filename (full path)
   };

