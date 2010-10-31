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
#include <unistd.h>

#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QImage>

#include "err.h"

#include "config.h"

#include "ocr.h"
#include "ocromni.h"
#include "ocrtess.h"


static Ocr *static_ocr = 0;


Ocr::Ocr (void)
   {
   _engine = OCRE_none;
   }


Ocr::~Ocr ()
   {
   }


Ocr::Ocr *Ocr::getOcr (err_info *&err)
   {
   Ocr *ocr;

   err = NULL;
#ifdef CONFIG_use_omnipage
   if (!static_ocr)
      {
      // should support dynamic loading for this
      ocr = new Ocromni ();
      err = ocr->init ();
      if (err)
         {
         qDebug () << "Omnipage enginer error" << err->errstr;
         delete ocr;
         }
      else
         static_ocr = ocr;
      }
#endif
   if (!static_ocr)
      {
      ocr = new Ocrtess ();
      err = ocr->init ();
      if (err)
         delete ocr;
      else
         static_ocr = ocr;
      }
   return static_ocr;
   }


