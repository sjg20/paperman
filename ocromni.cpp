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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStringList>

#include "err.h"
#include "ocromni.h"
#include "utils.h"

#ifdef CONFIG_use_omnipage


#include "KernelApi.h"


#define SID 0

#include "../nuance_ocr/sample/sample.h"


Ocromni::Ocromni (void)
   {
   _engine = OCRE_omnipage;
   }


Ocromni::~Ocromni ()
   {
   }


err_info *Ocromni::init (void)
   {
   void *lib = dlopen ("libxocr.so", RTLD_LAZY);

   // try some other locations, so the user doesn't need LD_LIBRARY_PATH
   if (!lib)
      {
      // first try to find the directory
      QStringList dirlist = QString ("/usr/local/lib/,/usr/lib").split (',');
      QString fname;

      for (int i = 0; i < dirlist.size (); i++)
         {
         QDir dir (dirlist [i]);

         dir.setFilter (QDir::Dirs);
         const QString filter = "nuance-omnipage-csdk*";
         dir.setNameFilter (filter);

//          qDebug () << dir.path () << dir.count ();
         QStringList list = dir.entryList ();
         foreach (QString path, list)
            {
            fname = dir.path () + "/" + path + "/libxocr.so";
            QFile file (fname);

            if (file.exists (fname))
               break;
            fname.clear ();
            }
//          qDebug () << list.size ();
         }

      if (!fname.isEmpty ())
         lib = dlopen (fname.latin1 (), RTLD_LAZY);
      }
   if (!lib)
      return err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "omnipage", dlerror ());

   return omni_init ();
   }


err_info *Ocromni::checkerr (char *operation, int rc)
   {
   if (rc != REC_OK)
      return err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "omnipage", QString ("Op '%1', error code %2").arg (operation).arg (rc).latin1 ());
   return NULL;
   }


err_info *Ocromni::omni_init (void)
   {
   int              brightness = 45;    // Automatic conversion mode (default=50)
   HPAGE            hPage;
   IMG_CONVERSION   conversion;         // Conversion mode
   RECERR           rc;
   err_info *err;

#if (USE_OEM_LICENSE)
   rc = kRecSetLicense(LICENSE_PATH, LICENSE_KEY);
   if (rc != REC_OK)
   {
      err = err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "omnipage", QString ("Error code %1").arg (rc).latin1 ());
      kRecQuit();
      return err;
   }
#endif

   rc = kRecInit("SampleCompany", "SampleProduct");    // use your company and product name here
   if ((rc != REC_OK) && (rc != API_INIT_WARN))
   {
      err = err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "omnipage", QString ("Error code %1").arg (rc).latin1 ());
      kRecQuit();
      return err;
   }
   if (rc == API_INIT_WARN)
   {
      qDebug () << "Module error - For more information, see kRecGetModulesInfo()";
   }

   rc = kRecSettingSetToDefault(SID, NULL, TRUE);
   if (rc != REC_OK)
   {
      err = err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "omnipage", QString ("Error code %1").arg (rc).latin1 ());
      kRecQuit();
      return err;
   }
   return NULL;
   }


err_info *Ocromni::imageToText (QImage &image, QString &text)
   {
   // unfortunately Omnipage's API has hungarian disease, probably
   // an infection from Seattle. It also suffers from Windows
   // 'far' disease. Oh dear.
   tagIMG_INFO img;
   int rc;
   HPAGE page;
   err_info *err;

   img.Size.cx = image.width ();
   img.Size.cy = image.height ();
   img.DPI.cx = 300;
   img.DPI.cy = 300;
   img.BytesPerLine = image.bytesPerLine ();
   img.IsPalette  = 0;
   img.BitsPerPixel = image.depth ();
   img.DummyS = NULL;
   rc = kRecLoadImgM (SID, image.bits (), &img, &page);
   if (rc != REC_OK)
   {
      err = err_make (ERRFN, ERR_ocr_engine_not_present_or_broken2,
         "omnipage", QString ("Error code %1").arg (rc).latin1 ());
      return err;
   }
   rc = kRecPreprocessImg(SID, page);
   rc = kRecSetCodePage(SID, "Windows ANSI");
   rc = kRecRecognize(SID, page, NULL);
   rc = kRecSetDTXTFormat(SID, DTXT_TXTF);

   // in a bizarre twist, this function doesn't support memory output
   char fname [PATH_MAX + 1];

   CALL (util_get_tmp (fname));
   rc = kRecConvert2DTXT(SID, &page, 1, fname);
   QFile file (fname);
   if (!file.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, fname);

   QByteArray ba = file.readAll ();
   text = ba.constData ();
   file.remove ();
   kRecFreeImg (page);
   return NULL;
   }

#endif

