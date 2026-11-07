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

#include <algorithm>

#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include "iostats.h"

namespace
   {
   QMutex mutex;
   IoStats::Counts totals;
   QHash<QString, IoStats::Counts> paths;
   }


IoStats::Counts &IoStats::Counts::operator+= (const Counts &other)
   {
   readBytes += other.readBytes;
   reads += other.reads;
   writeBytes += other.writeBytes;
   writes += other.writes;
   opens += other.opens;
   ms += other.ms;
   return *this;
   }


IoStats::Counts IoStats::Counts::operator- (const Counts &other) const
   {
   Counts out = *this;

   out.readBytes -= other.readBytes;
   out.reads -= other.reads;
   out.writeBytes -= other.writeBytes;
   out.writes -= other.writes;
   out.opens -= other.opens;
   out.ms -= other.ms;
   return out;
   }


bool IoStats::Counts::isEmpty () const
   {
   return !readBytes && !reads && !writeBytes && !writes && !opens && !ms;
   }


static void add (const QString &path, const IoStats::Counts &counts)
   {
   QMutexLocker locker (&mutex);

   totals += counts;
   paths[path] += counts;
   }


void IoStats::read (const QString &path, qint64 bytes)
   {
   Counts counts;

   counts.readBytes = bytes;
   counts.reads = 1;
   add (path, counts);
   }


void IoStats::wrote (const QString &path, qint64 bytes)
   {
   Counts counts;

   counts.writeBytes = bytes;
   counts.writes = 1;
   add (path, counts);
   }


void IoStats::opened (const QString &path)
   {
   Counts counts;

   counts.opens = 1;
   add (path, counts);
   }


void IoStats::spent (const QString &path, qint64 ms)
   {
   Counts counts;

   counts.ms = ms;
   add (path, counts);
   }


IoStats::Counts IoStats::total ()
   {
   QMutexLocker locker (&mutex);

   return totals;
   }


QHash<QString, IoStats::Counts> IoStats::byPath ()
   {
   QMutexLocker locker (&mutex);

   return paths;
   }


IoStats::Counts IoStats::forPath (const QString &path)
   {
   QMutexLocker locker (&mutex);

   return paths.value (path);
   }


void IoStats::reset ()
   {
   QMutexLocker locker (&mutex);

   totals = Counts ();
   paths.clear ();
   }


bool IoStats::logging ()
   {
   static const bool on = qEnvironmentVariableIsSet ("PAPERMAN_IOSTATS");

   return on;
   }


QString IoStats::size (qint64 bytes)
   {
   if (bytes < 1000)
      return QString ("%1 B").arg (bytes);
   if (bytes < 1000 * 1000)
      return QString ("%1 KB").arg (bytes / 1000.0, 0, 'f', 1);
   if (bytes < 1000LL * 1000 * 1000)
      return QString ("%1 MB").arg (bytes / 1e6, 0, 'f', 1);
   return QString ("%1 GB").arg (bytes / 1e9, 0, 'f', 2);
   }


QString IoStats::describe (const Counts &counts)
   {
   QStringList parts;

   parts << QString ("read %1 in %2 reads").arg (size (counts.readBytes))
                                           .arg (counts.reads);
   if (counts.writes)
      parts << QString ("wrote %1 in %2 writes")
                  .arg (size (counts.writeBytes)).arg (counts.writes);
   parts << QString ("%1 opens").arg (counts.opens);
   if (counts.ms)
      parts << QString ("%1 ms").arg (counts.ms);
   return parts.join (", ");
   }


void IoMeasure::start (const QString &what)
   {
   _what = what;
   _start = IoStats::total ();
   _startPaths = IoStats::byPath ();
   _timer.start ();
   }


qint64 IoMeasure::elapsed () const
   {
   return _timer.isValid () ? _timer.elapsed () : 0;
   }


IoStats::Counts IoMeasure::counts () const
   {
   return IoStats::total () - _start;
   }


QHash<QString, IoStats::Counts> IoMeasure::byPath () const
   {
   QHash<QString, IoStats::Counts> out;
   const QHash<QString, IoStats::Counts> now = IoStats::byPath ();

   for (auto it = now.cbegin (); it != now.cend (); ++it)
      {
      IoStats::Counts diff = it.value () - _startPaths.value (it.key ());

      if (!diff.isEmpty ())
         out.insert (it.key (), diff);
      }
   return out;
   }


QString IoMeasure::report (int top) const
   {
   const QHash<QString, IoStats::Counts> files = byPath ();
   QStringList lines;

   lines << QString ("%1: %2 files in %3 ms: %4").arg (_what)
               .arg (files.size ()).arg (elapsed ())
               .arg (IoStats::describe (counts ()));

   // the files which cost most: by what was read, then by time taken
   auto list = [&] (const char *title, qint64 IoStats::Counts::*field)
      {
      QList<QString> names;

      for (auto it = files.cbegin (); it != files.cend (); ++it)
         if (it.value ().*field)
            names << it.key ();
      std::sort (names.begin (), names.end (),
                 [&files, field] (const QString &a, const QString &b)
         {
         return files[a].*field > files[b].*field;
         });
      if (!names.isEmpty ())
         lines << QString ("  %1:").arg (title);
      for (int i = 0; i < names.size () && i < top; i++)
         lines << QString ("    %1: %2")
                     .arg (QFileInfo (names[i]).fileName (),
                           IoStats::describe (files[names[i]]));
      };

   list ("most read", &IoStats::Counts::readBytes);
   list ("slowest", &IoStats::Counts::ms);
   return lines.join ("\n");
   }
