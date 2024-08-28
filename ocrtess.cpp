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


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QImage>

#include "err.h"


#include "ocrtess.h"



Ocrtess::Ocrtess (void)
   {
   _engine = OCRE_tesseract;
   }


Ocrtess::~Ocrtess ()
   {
   }


err_info *Ocrtess::init (void)
   {
   QFile file ("/usr/bin/tesseract");

   if (!file.exists ())
      return err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "tesseract", "/usr/bin/tesseract does not exist");
   return NULL;
   }


err_info *Ocrtess::imageToText (QImage &image, QString &text)
   {
   char base [200], tmp [200], tmp2 [200], out [200];
   char cmd [430];
   int fd;

   // this function should really use tesseract as a library

   /* sadly:

     1. tesseract only accepts 1bpp uncompressed tiff
     2. QT only writes 8bpp tiff

     so we:

     1. Write png
     2. Convert to tif with 'convert' */
   sprintf (base, "%s/maxviewXXXXXX", P_tmpdir);
   fd = mkstemp (base);
   if (fd < 0)
      return err_make (ERRFN, ERR_could_not_make_temporary_file);
   close (fd);
   strncpy (tmp, base, sizeof(tmp));
   strncat (tmp, ".png", sizeof(tmp) - strlen(tmp) - 1);
   qDebug () << image.depth ();
   image.save (tmp, "PNG");

   strncpy (tmp2, base, sizeof(tmp2));
   strncat (tmp2, ".tif", sizeof(tmp2) - strlen(tmp2) - 1);
   int errnum = err_systemf ("convert %s %s", tmp, tmp2);
   if (errnum)
      return err_make (ERRFN, ERR_could_not_execute1, "convert");

   strncpy (out, base, sizeof(tmp));
   snprintf (cmd, sizeof(cmd), "/usr/bin/tesseract %s %s", tmp2, out);
   errnum = err_systemf (cmd);
   if (errnum)
      return err_make (ERRFN, ERR_tesseract_not_present2, cmd, strerror (errno));

   strncat (out, ".txt", sizeof(out) - strlen(out) - 1);
   QFile file (out);
   if (!file.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, out);

   QByteArray ba = file.readAll ();
   text = ba.constData ();
   return NULL;
   }

