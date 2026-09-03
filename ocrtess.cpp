/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
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


#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>

#include "err.h"


#include "ocrtess.h"



Ocrtess::Ocrtess (void)
   {
   _engine = OCRE_tesseract;
   }


Ocrtess::~Ocrtess ()
   {
   }


/* tesseract is found on the PATH, so this works with a distribution
   package on Linux and an MSYS2 or installer package on Windows */
static QString tesseractPath (void)
   {
   return QStandardPaths::findExecutable ("tesseract");
   }


err_info *Ocrtess::init (void)
   {
   if (tesseractPath ().isEmpty ())
      return err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "tesseract", "tesseract is not on the PATH");
   return NULL;
   }


err_info *Ocrtess::imageToText (QImage &image, QString &text)
   {
   // this function should really use tesseract as a library

   QString exe = tesseractPath ();
   if (exe.isEmpty ())
      return err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "tesseract", "tesseract is not on the PATH");

   /* tesseract reads PNG directly and writes <base>.txt, so give it a
      unique base name in the temporary directory */
   QTemporaryFile base (QDir::tempPath () + "/maxviewXXXXXX");
   if (!base.open ())
      return err_make (ERRFN, ERR_could_not_make_temporary_file);
   QString tmp = base.fileName () + ".png";
   QString out = base.fileName () + ".txt";
   if (!image.save (tmp, "PNG"))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (tmp));

   QProcess proc;
   proc.start (exe, QStringList () << tmp << base.fileName ());
   bool ok = proc.waitForFinished (-1) && proc.exitStatus () == QProcess::NormalExit
             && proc.exitCode () == 0;
   QFile::remove (tmp);
   if (!ok)
      {
      QFile::remove (out);
      return err_make (ERRFN, ERR_tesseract_not_present2, qPrintable (exe),
                       proc.readAllStandardError ().constData ());
      }

   QFile file (out);
   if (!file.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (out));

   QByteArray ba = file.readAll ();
   file.close ();
   QFile::remove (out);
   text = QString::fromUtf8 (ba);
   return NULL;
   }

