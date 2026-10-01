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
#include "ocrpage.h"



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
   OcrPage page;

   CALL (imageToPage (image, page));
   text = page.text ();
   return NULL;
   }


err_info *Ocrtess::imageToPage (QImage &image, OcrPage &page)
   {
   // this function should really use tesseract as a library

   QString exe = tesseractPath ();
   if (exe.isEmpty ())
      return err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "tesseract", "tesseract is not on the PATH");

   /* tesseract reads PNG directly and writes <base>.tsv, with a row for
      each word giving where it is, so give it a unique base name in the
      temporary directory */
   QTemporaryFile base (QDir::tempPath () + "/maxviewXXXXXX");
   if (!base.open ())
      return err_make (ERRFN, ERR_could_not_make_temporary_file);
   QString tmp = base.fileName () + ".png";
   QString out = base.fileName () + ".tsv";
   if (!image.save (tmp, "PNG"))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (tmp));

   QProcess proc;
   QString program = exe;
   QStringList args = QStringList () << tmp << base.fileName () << "tsv";

   if (_background)
      {
      QProcessEnvironment env = QProcessEnvironment::systemEnvironment ();

      env.insert ("OMP_THREAD_LIMIT", "1");
      proc.setProcessEnvironment (env);

      QString nice = QStandardPaths::findExecutable ("nice");
      if (!nice.isEmpty ())
         {
         args = QStringList () << "-n" << "15" << exe << args;
         program = nice;
         }
      }
   proc.start (program, args);
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

   QByteArray tsv = file.readAll ();
   file.close ();
   QFile::remove (out);
   if (!OcrPage::fromTsv (tsv, image.size (), page))
      return err_make (ERRFN, ERR_tesseract_not_present2, qPrintable (exe),
                       "its output could not be read");
   return NULL;
   }

