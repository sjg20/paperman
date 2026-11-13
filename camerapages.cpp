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

#include <QBuffer>
#include <QImage>

#include "camera.h"
#include "camerapages.h"
#include "utils.h"


CameraPagesWorker::CameraPagesWorker (const QAtomicInt *current)
      : _current (current)
   {
   }


void CameraPagesWorker::process (int run, const QByteArray &jpeg, int mode,
                                 int quality, double focal)
   {
   bool found = false;
   QByteArrayList pages;

   // not wanted any more
   if (run != _current->loadAcquire ())
      return;
   pages = CameraPages::process (jpeg, (Pagefind::Mode)mode, quality, found,
                                 focal);
   emit done (run, pages, found);
   }


CameraPages::CameraPages (QObject *parent)
      : QObject (parent)
   {
   _worker = new CameraPagesWorker (&_run);
   _worker->moveToThread (&_thread);
   connect (&_thread, &QThread::finished, _worker, &QObject::deleteLater);

   /* the pictures wait in the thread's queue of calls, each as the
      camera's JPEG, and come back through this thread's queue */
   connect (this, &CameraPages::work, _worker, &CameraPagesWorker::process,
            Qt::QueuedConnection);
   connect (_worker, &CameraPagesWorker::done, this,
            &CameraPages::workDone, Qt::QueuedConnection);
   _thread.start ();
   }


CameraPages::~CameraPages ()
   {
   // have it pass over whatever is waiting, so that this is soon done
   _run.ref ();
   _thread.quit ();
   _thread.wait ();
   }


void CameraPages::add (const QByteArray &jpeg, Pagefind::Mode mode,
                       int quality, double focal)
   {
   _pending++;
   emit work (_run.loadAcquire (), jpeg, mode, quality, focal);
   }


void CameraPages::abandon (void)
   {
   _run.ref ();
   _pending = 0;
   }


void CameraPages::workDone (int run, const QByteArrayList &pages, bool found)
   {
   // forgotten while it was being done
   if (run != _run.loadAcquire ())
      return;
   _pending--;
   emit processed (pages, found);
   }


QByteArrayList CameraPages::process (const QByteArray &jpeg,
                                     Pagefind::Mode mode, int quality,
                                     bool &found, double focal)
   {
   QByteArrayList out;
   QSize size;
   int depth;
   bool has_dht;

   found = false;
   out << jpeg;
   if (mode == Pagefind::Mode_whole
       || !cameraJpegInfo (jpeg, size, depth, has_dht).isEmpty ())
      return out;

   /* Decode it with the decoder which reads pages, rather than Qt's,
      which refuses a picture as large as a camera's can be unless told
      otherwise for the whole program */
   QImage image (size, depth == 8 ? QImage::Format_Grayscale8
                                  : QImage::Format_RGB32);

   if (image.isNull ())
      return out;
   image.fill (Qt::white);
   jpeg_decode ((byte *)jpeg.constData (), jpeg.size (), image.bits (),
                image.bytesPerLine (), depth == 8 ? 8 : 32, size.width (),
                size.height ());

   QList<QImage> pages = Pagefind::pages (image, mode, nullptr, focal);
   QByteArrayList encoded;

   // it is large, and no longer needed
   image = QImage ();
   for (const QImage &page : pages)
      {
      QByteArray data;
      QBuffer buf (&data);

      if (!buf.open (QIODevice::WriteOnly)
          || !page.save (&buf, "JPG", quality))
         return out;
      encoded << data;
      }
   if (encoded.isEmpty ())
      return out;
   found = true;
   return encoded;
   }
