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
   File:       pdfwrite.h
   Started:    2/7/09

   This file implmenents a simple PDF writer, sufficient for out purposes.
   It uses the PoDoFo library. Unfortunately libpdf's open source version
   doesn't handle modification of existing PDFs, which is needed
*/


#include <QString>

#include "err.h"

namespace PoDoFo 
   {
   class PdfMemDocument;
   };
   
class Filepage;


class Pdfwrite
   {
public :
   Pdfwrite (const QString &fname);
   ~Pdfwrite ();
   
   err_info *create (void);

   err_info *addPage (const Filepage *mp);
   
   err_info *open (void);
   
   err_info *close (void);
   
private:
   PoDoFo::PdfMemDocument *_doc; //!< document handle
   QString _fname;            //!< filename
   };
   
   