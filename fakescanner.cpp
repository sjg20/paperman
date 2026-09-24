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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "fakescanner.h"
#include "test/fakescan/fakescan.h"


QString fakescanLibDir (void)
{
   QFileInfo self ("/proc/self/exe");

   return QFileInfo (self.canonicalFilePath ()).path () + "/test/fakescan";
}


/* libsane reads its configuration from where SANE_CONFIG_DIR says, if
   anywhere, else from the directory it was built for, which is under the
   prefix it was installed in. Tell which that is from where the library
   itself was loaded from */
QString fakescanSaneConfigDir (void)
{
   QFile maps ("/proc/self/maps");

   for (const QString &dir : QString::fromLocal8Bit (qgetenv ("SANE_CONFIG_DIR"))
                             .split (':', Qt::SkipEmptyParts))
      if (QFile::exists (dir + "/dll.conf"))
         return dir;

   if (maps.open (QIODevice::ReadOnly | QIODevice::Text))
      while (!maps.atEnd ())
         {
         QString line = QString::fromLatin1 (maps.readLine ());
         int pos = line.indexOf ('/');

         if (pos != -1 && line.mid (pos).contains ("/libsane.so"))
            {
            QString path = line.mid (pos).trimmed ();

            if (path.startsWith ("/usr/local/")
                && QFile::exists ("/usr/local/etc/sane.d"))
               return "/usr/local/etc/sane.d";
            break;
            }
         }

   return QFile::exists ("/etc/sane.d") ? "/etc/sane.d" : QString ();
}


QString fakescanOffer (const QString &libdir, const QString &dir,
                       const QString &sysconf)
{
#ifndef Q_OS_LINUX
   Q_UNUSED (libdir);
   Q_UNUSED (dir);
   Q_UNUSED (sysconf);
   return "the fake scanner is only built on Linux";
#else
   if (!QFile::exists (libdir + "/" FAKESCAN_LIB))
      return QString ("there is no fake scanner in %1").arg (libdir);

   /* libsane reads its configuration on every sane_init(), so this lasts
      as long as the program does */
   static QTemporaryDir conf;

   if (!conf.isValid ())
      return "cannot make a directory for libsane's configuration";

   /* libsane reads dll.conf from the first directory on its path which
      has one, and dll.d from the first which has that. To offer the fake
      scanner beside the others, copy both here with the fake one added,
      and have libsane look here first and then where it would anyway,
      for the rest of each back end's configuration */
   QByteArray list;

   // anything copied the last time is not wanted now
   QDir (conf.path () + "/dll.d").removeRecursively ();
   if (!sysconf.isEmpty ())
      {
      QFile old (sysconf + "/dll.conf");

      if (old.open (QIODevice::ReadOnly))
         list = old.readAll () + "\n";

      QDir olddlld (sysconf + "/dll.d");
      QDir dlld (conf.path () + "/dll.d");

      if (olddlld.exists ())
         {
         dlld.mkpath (".");
         for (const QString &name : olddlld.entryList (QDir::Files))
            QFile::copy (olddlld.filePath (name), dlld.filePath (name));
         }
      }
   list += FAKESCAN_BACKEND "\n";

   QFile conffile (conf.path () + "/dll.conf");

   if (!conffile.open (QIODevice::WriteOnly) || conffile.write (list) < 0)
      return "cannot write libsane's configuration";
   conffile.close ();

   QByteArray search = conf.path ().toLocal8Bit ();

   if (!sysconf.isEmpty ())
      {
      QByteArray was = qgetenv ("SANE_CONFIG_DIR");

      // a path ending in : has libsane's own directories added
      search += ":" + (was.isEmpty () ? QByteArray () : was);
      }
   qputenv ("SANE_CONFIG_DIR", search);

   // the dll back end looks along LD_LIBRARY_PATH for back ends
   QByteArray path = qgetenv ("LD_LIBRARY_PATH");

   qputenv ("LD_LIBRARY_PATH", libdir.toLocal8Bit ()
                               + (path.isEmpty () ? "" : ":" + path));

   if (!dir.isEmpty ())
      {
      if (!QDir (dir).mkpath ("hopper"))
         return QString ("cannot make %1/hopper").arg (dir);
      qputenv ("FAKESCAN_DIR", QFileInfo (dir).absoluteFilePath ()
                               .toLocal8Bit ());
      }

   return QString ();
#endif
}
