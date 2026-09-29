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

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/

#include <QDebug>
#include <QSettings>
#include <QtConcurrent>

#include "err.h"
#include "file.h"
#include "ocr.h"
#include "ocrreader.h"


OcrReader::OcrReader (QObject *parent)
   : QObject (parent)
   {
   connect (&_watcher, &QFutureWatcher<Result>::finished,
            this, &OcrReader::readDone);
   }


OcrReader::~OcrReader ()
   {
   // the engine is given a copy of the image, so it only needs to finish
   _watcher.waitForFinished ();
   }


bool OcrReader::enabled ()
   {
   return QSettings ().value ("ocr/background", true).toBool ();
   }


void OcrReader::setEnabled (bool enable)
   {
   QSettings ().setValue ("ocr/background", enable);
   }


void OcrReader::setEngine (const Engine &engine)
   {
   _engine = engine;
   }


bool OcrReader::isBusy () const
   {
   return _file || !_stacks.isEmpty ();
   }


void OcrReader::addFile (File *file)
   {
   queue (file);
   if (!_file)
      next ();
   }


void OcrReader::queue (File *file)
   {
   for (Stack &stack : _stacks)
      if (stack.file == file)
         {
         // look through it again, since pages may have been added
         stack.next = 0;
         return;
         }

   Stack stack;

   stack.file = file;
   _stacks << stack;
   }


bool OcrReader::markPage (File *file, int pagenum, Mark &mark)
   {
   QSize true_size;
   int bpp, image_size;

   return !file->getImageInfo (pagenum, mark.size, true_size, bpp,
                               image_size, mark.compressed, mark.timestamp);
   }


void OcrReader::next ()
   {
   Engine engine = _engine;

   if (!engine)
      {
      err_info *err;
      Ocr *ocr = Ocr::getOcr (err);

      // with no engine there is nothing to be done
      if (!ocr)
         {
         qDebug () << "ocr: no engine to read pages:"
                   << (err ? err->errstr : "");
         _stacks.clear ();
         emit idle ();
         return;
         }
      engine = [ocr] (QImage &image, OcrPage &page)
         {
         err_info *err = ocr->imageToPage (image, page);

         return err ? QString (err->errstr) : QString ();
         };
      }

   while (!_stacks.isEmpty ())
      {
      Stack &stack = _stacks.first ();
      File *file = stack.file;

      if (!file || stack.next >= file->pagecount ())
         {
         _stacks.removeFirst ();
         continue;
         }

      int pagenum = stack.next++;
      OcrPage have;

      // a page which has been read, or a file which cannot keep it
      if (file->getPageOcr (pagenum, have) || !have.isEmpty ())
         continue;

      QImage image;
      QSize size, true_size;
      int bpp;
      Mark mark;

      if (!markPage (file, pagenum, mark)
          || file->getImage (pagenum, false, image, size, true_size, bpp,
                             false)
          || image.isNull ())
         continue;

      _file = file;
      _pagenum = pagenum;
      _mark = mark;
      _watcher.setFuture (QtConcurrent::run ([engine, image] () mutable
         {
         Result result;

         result.error = engine (image, result.page);
         return result;
         }));
      return;
      }
   emit idle ();
   }


void OcrReader::readDone ()
   {
   Result result = _watcher.result ();
   File *file = _file;
   int pagenum = _pagenum;
   Mark mark;

   _file = nullptr;
   _pagenum = -1;
   if (!file)
      ;  // the stack has gone
   else if (!result.error.isEmpty ())
      qDebug () << "ocr: cannot read page" << pagenum + 1 << "of"
                << file->pathname () << ":" << result.error;

   /* if the page has changed, look through the stack again: the pages
      read so far are left alone */
   else if (pagenum >= file->pagecount () || !markPage (file, pagenum, mark)
            || !(mark == _mark))
      queue (file);
   else if (!result.page.isEmpty ())
      {
      err_info *err = file->putPageOcr (pagenum, result.page);

      if (err)
         qDebug () << "ocr: cannot keep what was read from page"
                   << pagenum + 1 << "of" << file->pathname () << ":"
                   << err->errstr;
      else
         emit pageRead (file, pagenum);
      }
   next ();
   }
