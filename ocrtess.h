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


#include "ocr.h"


class Ocrtess : public Ocr
   {
public:
   Ocrtess (void);
   ~Ocrtess ();

   /** convert an image to text */
   err_info *imageToText (QImage &image, QString &text);

   //! the words, with their boxes, from tesseract's TSV output
   err_info *imageToPage (QImage &image, OcrPage &page) override;

   err_info *init (void);

   /** Run tesseract as background work: at low priority, where the
       system can lower it, and on one thread, since several readers
       each running a thread per core only get in each other's way.
       OCR that someone is waiting for should not use this */
   void setBackground (bool background) { _background = background; }

private:
   bool _background = false;
   };

