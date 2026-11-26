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

#pragma once

#include <functional>

#include <QDateTime>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QThreadPool>

#include "ocrpage.h"

class File;
class Ocrtess;

/** Reads the text of stacks' pages in the background, a page at a time,
    and keeps what it reads with each page. Pages which have been read
    are left alone, so a stack can be queued again at any time.

    A stack waiting to be read is known by where it is, not by the File
    which the desktop has for it: that File goes when its folder is
    refreshed or left, and the stack still needs reading. The desktop's
    File is used while there is one, so that what is read shows up there,
    and is found again by the stack's path when it has been replaced (see
    setFinder()). A stack the desktop holds no File for is opened here
    instead, and let go of as soon as the desktop has one, so that only
    one File ever writes to a stack.

    The engine runs on other threads, a page on each, while each page's
    image is read and what was read is stored on this one, where the
    stack's File lives. A
    page can be moved, deleted or turned while it is being read, so each
    page is recognised by its image before what was read is stored */
class OcrReader : public QObject
   {
   Q_OBJECT

   friend class TestOcrSearch;

public:
   /** reads an image, on the reader's thread

      \param image   the page
      \param page    returns what was read
      \returns an error message, or an empty string if ok */
   typedef std::function<QString (QImage &image, OcrPage &page)> Engine;

   /** finds the File which the desktop has for a stack

      \param pathname  the stack's full path
      \returns the File, or nullptr if the desktop has none for it */
   typedef std::function<File *(const QString &pathname)> Finder;

   explicit OcrReader (QObject *parent = nullptr);
   ~OcrReader ();

   /** read the pages of a stack which have not been read yet

      \param file  the stack, which must be able to keep what is read
                   (see File::putPageOcr()) */
   void addFile (File *file);

   /** read the pages of a stack which have not been read yet, given
       where it is

      \param pathname  the stack's full path */
   void addPath (const QString &pathname);

   /** say how to find the File which the desktop has for a stack, so that
       a stack is read through that File whenever there is one. With no
       finder, a stack whose File goes is opened here */
   void setFinder (const Finder &finder);

   /** set how many pages are read at once; to begin with it is a few,
       fewer than the processors there are

      \param workers  how many, at least 1 */
   void setWorkers (int workers);

   //! true while there are pages to read
   bool isBusy () const;

   //! the full path of each stack which still has pages to look at
   QStringList pending () const;

   //! how many stacks still have pages to look at
   int stacksLeft () const;

   /** about how many pages are still to be looked at, some of which may
       turn out to have been read. A stack which has not been opened yet
       counts for none */
   int pagesLeft () const;

   /** keep a note in the settings of the stacks still to read, so that
       restore() can carry on with them after a restart

      \param remember  true to keep the note */
   void setRemembered (bool remember);

   /** queue the stacks which were still to be read when the note was
       last kept, such as when Paperman was last closed. Each is read from
       where it got to, since the pages read are kept in the stack */
   void restore ();

   /** allow or forbid any reader to keep the note. Tests forbid it, so
       that stacks left unread by one test are not read during another

      \param allow  true to allow it, which is how things start */
   static void allowRemembering (bool allow);

public slots:
   /** a stack has been moved or renamed, so is to be read where it is now

      \param from_path  the full path it had
      \param to_path    the full path it has now */
   void stackMoved (const QString &from_path, const QString &to_path);

public:

   //! use this engine rather than the usual one, e.g. for testing
   void setEngine (const Engine &engine);

   //! whether scanned stacks are read in the background
   static bool enabled ();
   static void setEnabled (bool enable);

signals:
   /** a page has been read and what was read has been stored with it

      \param file     the stack
      \param pagenum  the page, from 0 */
   void pageRead (File *file, int pagenum);

   //! there is nothing more to read
   void idle ();

   //! what there is still to read has changed
   void progress ();

private:
   //! what tells a page apart from the others in its stack
   struct Mark
      {
      QSize size;
      int compressed = 0;
      QDateTime timestamp;

      bool operator== (const Mark &other) const
         {
         return size == other.size && compressed == other.compressed
                && timestamp == other.timestamp;
         }
      };

   struct Result
      {
      OcrPage page;
      QString error;
      };

   struct Stack
      {
      QString path;         //!< where the stack is
      QPointer<File> file;  //!< the desktop's File for it, while it lasts
      int next = 0;         //!< the first page not yet looked at
      int pages = -1;       //!< how many pages it has, once it is opened
      };

   //! a page which the engine is reading
   struct Job
      {
      QString path;         //!< where the page's stack is
      QPointer<File> file;  //!< the desktop's File for it, while it lasts
      int pagenum = -1;     //!< the page, from 0
      Mark mark;            //!< how to recognise the page afterwards
      QElapsedTimer timer;  //!< how long the page has taken
      QFutureWatcher<Result> *watcher = nullptr;
      };

   //! add a stack to those to read, without starting
   void queue (const QString &pathname, File *file);

   //! start reading the next page which needs it
   void next ();

   //! deal with what the engine read from a page
   void readDone (QFutureWatcher<Result> *watcher);

   /** find the File to use for a stack: the desktop's if it has one, else
       one opened here

      \param path  where the stack is; updated if the desktop's File says
                   it has been renamed
      \param file  the desktop's File for it, if known; updated
      \returns the File, or nullptr if the stack has gone */
   File *fileFor (QString &path, QPointer<File> &file);

   //! let go of the File opened here, if any
   void closeOwn ();

   //! note what is still to read, and tell anything which shows it
   void changed ();

   //! find how a page can be recognised, returning false if it cannot
   bool markPage (File *file, int pagenum, Mark &mark);

   //! the most pages which next() looks at before letting other things run
   static constexpr int look_batch = 25;

   QList<Stack> _stacks;          //!< the stacks to read, the first now
   QList<Job> _jobs;              //!< the pages being read
   int _workers;                  //!< how many pages to read at once
   QThreadPool _pool;             //!< the threads which the engine runs on
   Ocrtess *_tess = nullptr;      //!< the reader's own engine, if it has one
   Finder _finder;                //!< finds the desktop's File for a stack
   File *_own = nullptr;          //!< a File opened here, for _own_path
   QString _own_path;             //!< the stack which _own is
   bool _next_posted = false;     //!< next() is due to be called again
   bool _remember = false;        //!< keep a note of the stacks to read
   QStringList _noted;            //!< the stacks in the note
   Engine _engine;
   };
