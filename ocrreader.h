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
#include <QFutureWatcher>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSize>

#include "ocrpage.h"

class File;

/** Reads the text of stacks' pages in the background, a page at a time,
    and keeps what it reads with each page. Pages which have been read
    are left alone, so a stack can be queued again at any time.

    The engine runs on another thread while the page's image is read and
    what was read is stored on this one, where the stack's File lives. A
    page can be moved, deleted or turned while it is being read, so each
    page is recognised by its image before what was read is stored */
class OcrReader : public QObject
   {
   Q_OBJECT

public:
   /** reads an image, on the reader's thread

      \param image   the page
      \param page    returns what was read
      \returns an error message, or an empty string if ok */
   typedef std::function<QString (QImage &image, OcrPage &page)> Engine;

   explicit OcrReader (QObject *parent = nullptr);
   ~OcrReader ();

   /** read the pages of a stack which have not been read yet

      \param file  the stack, which must be able to keep what is read
                   (see File::putPageOcr()) */
   void addFile (File *file);

   //! true while there are pages to read
   bool isBusy () const;

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

private slots:
   void readDone ();

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
      QPointer<File> file;
      int next = 0;         //!< the first page not yet looked at
      };

   //! add a stack to those to read, without starting
   void queue (File *file);

   //! start reading the next page which needs it
   void next ();

   //! find how a page can be recognised, returning false if it cannot
   bool markPage (File *file, int pagenum, Mark &mark);

   QList<Stack> _stacks;          //!< the stacks to read, the first now
   QPointer<File> _file;          //!< the stack of the page being read
   int _pagenum = -1;             //!< the page being read
   Mark _mark;                    //!< the page being read
   Engine _engine;
   QFutureWatcher<Result> _watcher;
   };
