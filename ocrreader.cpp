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
      read, but leave much of the machine for whoever is using it */
   setWorkers (workers ());
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


int OcrReader::autoWorkers ()
   {
   return qBound (1, QThread::idealThreadCount () / 2, int (max_auto_workers));
   }


int OcrReader::workersSetting ()
   {
   return qMax (0, QSettings ().value ("ocr/workers", 0).toInt ());
   }


void OcrReader::setWorkersSetting (int workers)
   {
   QSettings ().setValue ("ocr/workers", qMax (0, workers));
   }


int OcrReader::workers ()
   {
   int chosen = workersSetting ();

   return chosen ? chosen : autoWorkers ();
   }


//! a path in the one form which paths are compared in
static QString cleanPath (const QString &path)
   {
   return QDir::cleanPath (path);
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
      if (!job.dropped && !paths.contains (job.path))
         paths << job.path;
   for (const Stack &stack : _stacks)
      if (!paths.contains (stack.path))
         paths << stack.path;
   return paths;
   }


QList<OcrReader::Waiting> OcrReader::waiting () const
   {
   QList<Waiting> list;

   auto find = [&list] (const QString &path) -> Waiting & {
      for (Waiting &w : list)
         if (w.path == path)
            return w;
      list << Waiting {path, -1, 0, 0};
      return list.last ();
   };

   // those with pages being read come first, as in pending()
   for (const Job &job : _jobs)
      if (!job.dropped)
         find (job.path).reading++;
   for (const Stack &stack : _stacks)
      {
      Waiting &w = find (stack.path);

      w.pages = stack.pages;
      if (w.pages < 0 && stack.file && stack.file->valid ())
         w.pages = stack.file->pagecount ();
      w.left = qMax (0, w.pages - stack.next);
      }
   for (Waiting &w : list)
      w.left += w.reading;
   return list;
   }


void OcrReader::setPaused (bool paused)
   {
   if (paused == _paused)
      return;
   _paused = paused;
   qCDebug (logOcr) << "ocr:" << (paused ? "paused" : "carrying on");
   if (paused)
      changed ();
   else
      next ();
   }


void OcrReader::remove (const QString &pathname)
   {
   QString path = cleanPath (pathname);

   for (int i = _stacks.size () - 1; i >= 0; i--)
      if (_stacks [i].path == path)
         _stacks.removeAt (i);
   for (Job &job : _jobs)
      if (job.path == path)
         job.dropped = true;
   if (_own_path == path)
      closeOwn ();

   // this may leave nothing to do, which next() notices
   next ();
   }


void OcrReader::stop ()
   {
   _stacks.clear ();
   for (Job &job : _jobs)
      job.dropped = true;
   closeOwn ();
   next ();
   }


int OcrReader::stacksLeft () const
   {
   return pending ().size ();
   }


int OcrReader::pagesLeft () const
   {
   int pages = 0;

   for (const Job &job : _jobs)
      pages += !job.dropped;

   for (const Stack &stack : _stacks)
      {
      int count = stack.pages;

      // one waiting its turn is not opened just to count its pages
      if (count < 0 && stack.file && stack.file->valid ())
         count = stack.file->pagecount ();
      pages += qMax (0, count - stack.next);
      }
   return pages;
   }


//! whether a reader may keep a note of its stacks; see allowRemembering()
static bool remembering_allowed = true;

//! the setting which holds the note
#define PENDING_KEY "ocr/pending"

void OcrReader::allowRemembering (bool allow)
   {
   remembering_allowed = allow;
   }


void OcrReader::setRemembered (bool remember)
   {
   _remember = remember;
   if (_remember && remembering_allowed)
      _noted = QSettings ().value (PENDING_KEY).toStringList ();
   }


void OcrReader::restore ()
   {
   if (!_remember || !remembering_allowed)
      return;

   const QStringList paths = QSettings ().value (PENDING_KEY).toStringList ();

   for (const QString &path : paths)
      queue (path, nullptr);
   next ();
   }


void OcrReader::changed ()
   {
   if (_remember && remembering_allowed)
      {
      QStringList paths = pending ();

      // this is called for each page, but the stacks seldom change
      if (paths != _noted)
         {
         _noted = paths;
         QSettings ().setValue (PENDING_KEY, paths);
         }
      }
   emit progress ();
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
   if (_kept.contains (from))
      _kept [to] += _kept.take (from);
   if (_own_path == from)
      closeOwn ();
   changed ();
   }


void OcrReader::fileGoing (File *file)
   {
   QMutexLocker locker (&_pin_mutex);

   _watched.remove (file);
   for (Job &job : _jobs)
      if (job.source == file)
         job.pin->gone = true;

   // a thread which is in the File comes out when it has its picture
   auto busy = [this, file] () {
      for (const Job &job : _jobs)
         if (job.source == file && job.pin->using_file)
            return true;
      return false;
   };

   while (busy ())
      _pin_done.wait (&_pin_mutex);
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
         changed ();
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

   // while paused, the pages being read are finished and no more started
   while (!_paused && _jobs.size () < workers && !_stacks.isEmpty ())
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
         changed ();
         return;
         }

      Stack &stack = _stacks.first ();
      File *file = fileFor (stack.path, stack.file);

      if (!file || stack.next >= file->pagecount ())
         {
         _stacks.removeFirst ();
         continue;
         }

      stack.pages = file->pagecount ();

      int pagenum = stack.next++;
      OcrPage have;

      // a page which has been read, or a file which cannot keep it
      if (file->getPageOcr (pagenum, have) || !have.isEmpty ())
         continue;

      /* a stack can be looked through again while some of its pages are
         still being read, and those have no words yet: leave them to the
         read which is under way, and do not start another */
      bool reading = false;

      for (const Job &job : _jobs)
         reading |= !job.dropped && job.pagenum == pagenum
            && job.path == stack.path;
      if (reading)
         continue;

      QImage image;
      Job job;

      if (!markPage (file, pagenum, job.mark))
         continue;

      /* Making the picture of a page takes a while, and with several
         pages read at once it would keep this thread, which draws the
         display, busy most of the time. A max file can be used from
         another thread, so the thread which reads the page gets the
         picture too. Any other type gives its picture here */
      if (file->type () == File::Type_max)
         {
         job.source = file;
         job.pin = std::make_shared<Pin> ();
         if (!_watched.contains (file))
            {
            connect (file, &File::goingAway, this, &OcrReader::fileGoing,
                     Qt::DirectConnection);
            _watched << file;
            }
         }
      else
         {
         QSize size, true_size;
         int bpp;

         if (file->getImage (pagenum, false, image, size, true_size, bpp,
                             false)
             || image.isNull ())
            continue;
         }

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
      File *source = job.source;
      std::shared_ptr<Pin> pin = job.pin;

      watcher->setFuture (QtConcurrent::run (&_pool,
            [this, engine, image, source, pin, pagenum] () mutable
         {
         Result result;

         if (source)
            {
            // the File may be going, or gone: it says so through the pin
            {
               QMutexLocker locker (&_pin_mutex);

               if (pin->gone)
                  {
                  result.gone = true;
                  return result;
                  }
               pin->using_file = true;
            }

            QSize size, true_size;
            int bpp;
            err_info *err = source->getImage (pagenum, false, image, size,
                                              true_size, bpp, false);

            if (err)
               result.error = err->errstr;
            else if (image.isNull ())
               result.error = "no picture";
            {
               QMutexLocker locker (&_pin_mutex);

               pin->using_file = false;
               _pin_done.wakeAll ();
            }
            if (!result.error.isEmpty ())
               return result;
            }
         result.error = engine (image, result.page);
         return result;
         }));
      }
   changed ();
   noteRead ();
   if (!isBusy ())
      {
      closeOwn ();
      emit idle ();
      }
   }


void OcrReader::noteRead ()
   {
   // a stack is done with once it is neither waiting nor being read
   const QStringList paths = _kept.keys ();

   for (const QString &path : paths)
      {
      bool busy = false;

      for (const Stack &stack : _stacks)
         busy |= stack.path == path;
      for (const Job &job : _jobs)
         busy |= job.path == path;
      if (!busy)
         emit stackRead (path, _kept.take (path));
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

   File *file = job.dropped ? nullptr : fileFor (job.path, job.file);

   if (!file)
      ;  // the stack has gone, or been forgotten

   /* the File went before the page's picture was got, as when the folder
      is refreshed: the stack is still there, so look through it again */
   else if (result.gone)
      queue (job.path, job.file);
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

      else
         {
         _kept [job.path]++;

         // only the desktop's File has anything showing its pages
         if (file != _own)
            emit pageRead (file, pagenum);
         }
      }
   next ();
   }
