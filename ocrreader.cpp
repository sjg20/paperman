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
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QtConcurrent>

#include "err.h"
#include "file.h"
#include "ocr.h"
#include "ocrreader.h"
#include "ocrtess.h"
#include "utils.h"


OcrReader::OcrReader (QObject *parent)
   : QObject (parent)
   {
   /* a scanner fills a stack far faster than one page at a time can be
      read, but leave most of the machine for whoever is using it */
   setWorkers (qBound (1, QThread::idealThreadCount () / 2, 4));
   }


OcrReader::~OcrReader ()
   {
   // the engine is given a copy of the image, so it only needs to finish
   for (Job &job : _jobs)
      {
      if (job.watcher->isRunning ())
         {
         QElapsedTimer timer;

         timer.start ();
         qCDebug (logOcr).noquote () << QString ("ocr: waiting for page %1 "
            "of %2 to be read").arg (job.pagenum + 1).arg (job.path);
         job.watcher->waitForFinished ();
         qCDebug (logOcr).noquote () << QString ("ocr: waited %1ms")
            .arg (timer.elapsed ());
         }
      delete job.watcher;
      }
   closeOwn ();
   delete _tess;
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


void OcrReader::setWorkers (int workers)
   {
   _workers = qMax (1, workers);
   _pool.setMaxThreadCount (_workers);
   }


bool OcrReader::isBusy () const
   {
   return !_jobs.isEmpty () || !_stacks.isEmpty ();
   }


QStringList OcrReader::pending () const
   {
   QStringList paths;

   // a page being read may send its stack round again, so count it too
   for (const Job &job : _jobs)
      if (!paths.contains (job.path))
         paths << job.path;
   for (const Stack &stack : _stacks)
      if (!paths.contains (stack.path))
         paths << stack.path;
   return paths;
   }


void OcrReader::setFinder (const Finder &finder)
   {
   _finder = finder;
   }


void OcrReader::addFile (File *file)
   {
   if (!file)
      return;
   queue (file->pathname (), file);
   next ();
   }


//! a path in the one form which paths are compared in
static QString cleanPath (const QString &path)
   {
   return QDir::cleanPath (path);
   }


void OcrReader::addPath (const QString &pathname)
   {
   queue (pathname, nullptr);
   next ();
   }


void OcrReader::queue (const QString &pathname, File *file)
   {
   QString path = cleanPath (pathname);

   for (Stack &stack : _stacks)
      if (stack.path == path)
         {
         // look through it again, since pages may have been added
         stack.next = 0;
         if (file)
            stack.file = file;
         return;
         }

   Stack stack;

   stack.path = path;
   stack.file = file;
   _stacks << stack;
   }


void OcrReader::stackMoved (const QString &from_path, const QString &to_path)
   {
   QString from = cleanPath (from_path), to = cleanPath (to_path);

   /* the File the desktop had for it may have gone with the move; it is
      found again at its new path */
   for (Stack &stack : _stacks)
      if (stack.path == from)
         stack.path = to;
   for (Job &job : _jobs)
      if (job.path == from)
         job.path = to;
   if (_own_path == from)
      closeOwn ();
   }


void OcrReader::closeOwn ()
   {
   delete _own;
   _own = nullptr;
   _own_path.clear ();
   }


File *OcrReader::fileFor (QString &path, QPointer<File> &file)
   {
   // the File knows best where the stack is, since it may be renamed
   if (file)
      path = cleanPath (file->pathname ());
   else if (_finder)
      file = _finder (path);
   if (file)
      {
      /* only one File may write to a stack, and the desktop now has one */
      if (_own_path == path)
         closeOwn ();

      // one which the desktop has not needed yet knows nothing of its pages
      if (!file->valid () && file->load ())
         {
         qCDebug (logOcr) << "ocr: cannot open" << path;
         return nullptr;
         }
      return file;
      }

   if (_own && _own_path == path)
      return _own;
   closeOwn ();

   QFileInfo fi (path);

   if (!fi.isFile ())
      return nullptr;
   _own = File::createFile (fi.absolutePath () + "/", fi.fileName (), nullptr,
                            File::typeFromName (fi.fileName ()));
   if (!_own || _own->load ())
      {
      qCDebug (logOcr) << "ocr: cannot open" << path;
      closeOwn ();
      return nullptr;
      }
   _own_path = path;
   return _own;
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
   int workers = _workers;

   _next_posted = false;
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
         if (_jobs.isEmpty ())
            emit idle ();
         return;
         }

      /* tesseract is run afresh for each page, so can read several at
         once: use one of our own, set to keep out of the way. Any other
         engine is the one everything shares, a page at a time */
      if (dynamic_cast<Ocrtess *> (ocr))
         {
         if (!_tess)
            {
            _tess = new Ocrtess ();
            _tess->setBackground (true);
            }
         ocr = _tess;
         }
      else
         workers = 1;
      engine = [ocr] (QImage &image, OcrPage &page)
         {
         err_info *err = ocr->imageToPage (image, page);

         return err ? QString (err->errstr) : QString ();
         };
      }

   int looked = 0;

   while (_jobs.size () < workers && !_stacks.isEmpty ())
      {
      /* a stack whose pages have mostly been read takes a while to look
         through, so let other things have a turn now and then */
      if (looked++ >= look_batch)
         {
         _next_posted = true;
         QTimer::singleShot (0, this, [this] () {
            if (_next_posted)
               next ();
         });
         return;
         }

      Stack &stack = _stacks.first ();
      File *file = fileFor (stack.path, stack.file);

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
      Job job;

      if (!markPage (file, pagenum, job.mark)
          || file->getImage (pagenum, false, image, size, true_size, bpp,
                             false)
          || image.isNull ())
         continue;

      job.path = stack.path;
      job.file = stack.file;
      job.pagenum = pagenum;
      job.timer.start ();
      job.watcher = new QFutureWatcher<Result> (this);

      QFutureWatcher<Result> *watcher = job.watcher;

      connect (watcher, &QFutureWatcher<Result>::finished, this,
               [this, watcher] () { readDone (watcher); });
      qCDebug (logOcr).noquote () << QString ("ocr: reading page %1 of %2")
         .arg (pagenum + 1).arg (job.path);
      _jobs << job;
      watcher->setFuture (QtConcurrent::run (&_pool, [engine, image] () mutable
         {
         Result result;

         result.error = engine (image, result.page);
         return result;
         }));
      }
   if (!isBusy ())
      {
      closeOwn ();
      emit idle ();
      }
   }


void OcrReader::readDone (QFutureWatcher<Result> *watcher)
   {
   Job job;

   for (int i = 0; i < _jobs.size (); i++)
      if (_jobs [i].watcher == watcher)
         job = _jobs.takeAt (i);
   if (!job.watcher)
      return;

   Result result = watcher->result ();
   int pagenum = job.pagenum;
   Mark mark;

   watcher->deleteLater ();
   qCDebug (logOcr).noquote () << QString ("ocr: read page %1 in %2ms, "
      "%3 words").arg (pagenum + 1).arg (job.timer.elapsed ())
      .arg (result.page.words.size ());

   File *file = fileFor (job.path, job.file);

   if (!file)
      ;  // the stack has gone
   else if (!result.error.isEmpty ())
      qDebug () << "ocr: cannot read page" << pagenum + 1 << "of"
                << job.path << ":" << result.error;

   /* if the page has changed, look through the stack again: the pages
      read so far are left alone */
   else if (pagenum >= file->pagecount () || !markPage (file, pagenum, mark)
            || !(mark == job.mark))
      queue (job.path, job.file);
   else if (!result.page.isEmpty ())
      {
      err_info *err = file->putPageOcr (pagenum, result.page);

      if (err)
         qDebug () << "ocr: cannot keep what was read from page"
                   << pagenum + 1 << "of" << job.path << ":"
                   << err->errstr;

      // only the desktop's File has anything showing its pages
      else if (file != _own)
         emit pageRead (file, pagenum);
      }
   next ();
   }
