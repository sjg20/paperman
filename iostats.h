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

#include <QElapsedTimer>
#include <QHash>
#include <QString>

/** Counts what the desktop reads from and writes to the files it shows,
    so that what opening a folder or showing a stack costs can be seen.

    The stack files count (.max, .pdf and .jpg), as do the folders' layout
    files. Bytes are those asked of the file, which for a .max is what it
    reads and for a PDF or JPEG, read whole by a library, is the size of
    the file each time it is opened. The counts are kept in all and for
    each file, from any thread.

    Set PAPERMAN_IOSTATS to have the desktop log what each folder opened
    and each stack shown has cost (see IoMeasure) */
class IoStats
   {
public:
   //! what has been read and written
   struct Counts
      {
      qint64 readBytes = 0;   //!< bytes read
      qint64 reads = 0;       //!< reads made
      qint64 writeBytes = 0;  //!< bytes written
      qint64 writes = 0;      //!< writes made
      qint64 opens = 0;       //!< times a file was opened
      qint64 ms = 0;          //!< time spent on the file, where noted

      Counts &operator+= (const Counts &other);
      Counts operator- (const Counts &other) const;
      bool isEmpty () const;
      };

   //! @p bytes have been read from the file at @p path
   static void read (const QString &path, qint64 bytes);

   //! @p bytes have been written to the file at @p path
   static void wrote (const QString &path, qint64 bytes);

   //! the file at @p path has been opened
   static void opened (const QString &path);

   //! @p ms have been spent on the file at @p path, e.g. building its stack
   static void spent (const QString &path, qint64 ms);

   //! everything counted so far
   static Counts total ();

   //! what has been counted for each file, by its path
   static QHash<QString, Counts> byPath ();

   //! what has been counted for one file
   static Counts forPath (const QString &path);

   //! forget everything counted
   static void reset ();

   //! true if each action's costs are to be logged (PAPERMAN_IOSTATS)
   static bool logging ();

   //! a few words for some counts, e.g. "read 3.2 MB in 812 reads"
   static QString describe (const Counts &counts);

   //! a size in bytes, as "812 B", "3.2 KB" or "1.4 MB"
   static QString size (qint64 bytes);
   };


/** One action being measured, such as opening a folder: what has been
    read and written since it started, in all and by file, and how long
    it has taken */
class IoMeasure
   {
public:
   //! start measuring an action, described by @p what
   void start (const QString &what);

   //! true once started
   bool isActive () const { return _timer.isValid (); }

   //! what the action is
   QString what () const { return _what; }

   //! how long since start(), in milliseconds
   qint64 elapsed () const;

   //! what has been counted since start()
   IoStats::Counts counts () const;

   //! what has been counted for each file since start()
   QHash<QString, IoStats::Counts> byPath () const;

   /** a line saying what the action has cost, then one for each of the
       @p top files which have cost most by bytes read, and by time taken
       where that is known */
   QString report (int top = 5) const;

private:
   QString _what;
   QElapsedTimer _timer;
   IoStats::Counts _start;
   QHash<QString, IoStats::Counts> _startPaths;
   };
