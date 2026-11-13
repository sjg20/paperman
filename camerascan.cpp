/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
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
*/

#include "camera.h"
#include "camerascan.h"
#include "desktopmodel.h"
#include "err.h"
#include "filemax.h"
#include "paperstack.h"
#include "utils.h"


CameraScan::CameraScan (Desktopmodel *model, const QModelIndex &parent,
                        const QString &stack_name,
                        const QString &page_name, int dpi)
      : _model (model), _parent (parent), _stack_name (stack_name),
        _page_name (page_name), _dpi (dpi)
   {
   }


/** make a page of a JPEG, which is stored as it stands: this makes its
    thumbnail and nothing more

   \param page     the page to set up
   \param jpeg     the picture, a whole JPEG file
   \param name     the name the page is filed under
   \param dpi      the resolution to record
   \param pagenum  its number in its stack
   \returns error, or NULL if none */
static err_info *make_page (Filemaxpage &page, const QByteArray &jpeg,
                            const QString &name, int dpi, int pagenum)
   {
   QSize size;
   int depth;
   bool has_dht;
   QString title = Paperstack::pageTitle (name);
   QString bad = cameraJpegInfo (jpeg, size, depth, has_dht);

   if (!bad.isEmpty ())
      return err_make (ERRFN, ERR_unable_to_read_image_header_information1,
                       qPrintable (bad));
   page.setDpi (dpi, dpi);
   page.addData (size.width (), size.height (), depth,
                 size.width () * depth / 8, title, true, false, pagenum,
                 jpeg, -1);
   return page.compress ();
   }


err_info *CameraScan::begin (void)
   {
   if (!_open)
      {
      CALL (_model->beginScan (_parent, _stack_name));
      _open = true;
      }
   return NULL;
   }


File *CameraScan::file (void) const
   {
   return _open ? _model->scanFile () : nullptr;
   }


err_info *CameraScan::addPage (const QByteArray &jpeg)
   {
   Filemaxpage page;

   /* Do this before the stack is touched, so that a picture which
      cannot be kept leaves no trace */
   CALL (make_page (page, jpeg, _page_name, _dpi, _pages));
   CALL (begin ());

   // the page view makes a place for the page, then is given it
   _model->pageStarting ();
   CALL (_model->addPageToScan (&page, QString ()));
   _pages++;
   Paperstack::incrementName (_page_name);
   return NULL;
   }


err_info *CameraScan::finish (QString *fname, File **filep,
                              std::function<void (err_info *err)> uploaded)
   {
   if (fname)
      fname->clear ();
   if (filep)
      *filep = nullptr;
   if (!_open)
      return NULL;

   // a stack whose only page could not be added is not worth keeping
   if (!_pages)
      return cancel ();
   _open = false;
   return _model->confirmScan (fname, filep, uploaded);
   }


err_info *CameraScan::cancel (void)
   {
   if (!_open)
      return NULL;
   _open = false;
   _pages = 0;
   return _model->cancelScan ();
   }


CameraKeep::CameraKeep (const QString &dir, const QString &name,
                        const QString &page_name, int dpi)
      : _dir (dir.endsWith ('/') ? dir : dir + '/'), _name (name),
        _page_name (page_name), _dpi (dpi)
   {
   }


CameraKeep::~CameraKeep ()
   {
   finish ();
   }


err_info *CameraKeep::add (const QByteArray &jpeg)
   {
   Filemaxpage page;

   CALL (make_page (page, jpeg, _page_name, _dpi, _count));
   if (!_file)
      {
      QString base = util_findNextFilename (_name, _dir, ".max");

      if (base.isNull ())
         return err_make (ERRFN, ERR_no_unique_filename1,
                          qPrintable (_name));

      File *file = File::createFile (_dir, base + ".max", nullptr,
                                     File::Type_max);
      err_info *err = file->create ();

      if (err)
         {
         delete file;
         return err;
         }
      _file = file;
      _fname = base + ".max";
      }

   // written to disk at once, so that it is there whatever happens next
   CALL (_file->addPage (&page, true));
   _count++;
   Paperstack::incrementName (_page_name);
   return NULL;
   }


void CameraKeep::finish (void)
   {
   delete _file;
   _file = nullptr;
   }


err_info *CameraKeep::cancel (void)
   {
   err_info *err = _file ? _file->remove () : NULL;

   finish ();
   _fname.clear ();
   _count = 0;
   return err;
   }
