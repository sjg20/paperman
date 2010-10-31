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
/*
   Project:    Maxview
   Author:     Simon Glass
   Copyright:  2001-2009 Bluewater Systems Ltd, www.bluewatersys.com
   File:       ocr.h
   Started:    26/6/09

   This file contains the OCR (Optical Character Recognition) interface
*/


#ifndef __ocr_h
#define __ocr_h


class QImage;
class QString;


class Ocr
   {
public:
   Ocr (void);
   ~Ocr ();

   //! engine in use
   enum e_engine
      {
      OCRE_none,
      OCRE_tesseract,
      OCRE_omnipage
      };

   /** convert an image to text */
   virtual err_info *imageToText (QImage &image, QString &text) = 0;

   virtual err_info *init (void) = 0;

   /** sets up the best available Ocr engine and returns it

      \param err     error returned, NULL if ok
      \returns ocr engine, or 0 if none could be found */
   static Ocr *getOcr (err_info *&err);

protected:
   e_engine _engine;  //!< ocr engine we are using
   };


#endif

