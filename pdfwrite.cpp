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


#include <QDebug>

#include "podofo/podofo.h"

#include "err.h"
#include "file.h"
#include "pdfwrite.h"

using namespace PoDoFo;


Pdfwrite::Pdfwrite (const QString &fname)
   {
   _doc = 0;
   _fname = fname;
   }


Pdfwrite::~Pdfwrite ()
   {
   if (_doc)
      delete _doc;
   }


err_info *Pdfwrite::open (void)
   {
   try
      {
      _doc = new PdfMemDocument ();
      _doc->Load (_fname.latin1 ());
      }
   catch (const PdfError &eCode)
      {
      return err_make (ERRFN, E_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
      }
   return NULL;
   }


err_info *Pdfwrite::close (void)
   {
   try
      {
      _doc->Write (_fname.latin1 ());
      }
   catch (const PdfError &eCode)
      {
      return err_make (ERRFN, E_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
      }
   return NULL;
   }


err_info *Pdfwrite::create (void)
   {
   try
      {
      qDebug () << _fname;
      _doc = new PdfMemDocument ();
      _doc->GetInfo()->SetCreator ( PdfString("examplahelloworld - A PoDoFo test application") );
      _doc->GetInfo()->SetAuthor  ( PdfString("Dominik Seichter") );
      _doc->GetInfo()->SetTitle   ( PdfString("Hello World") );
      _doc->GetInfo()->SetSubject ( PdfString("Testing the PoDoFo PDF Library") );
      _doc->GetInfo()->SetKeywords( PdfString("Test;PDF;Hello World;") );
      }
   catch (const PdfError &eCode)
      {
      return err_make (ERRFN, E_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
      }
#if 0
   try
      {
      _doc = new PdfStreamedDocument (_fname.latin1 ());
      PdfPage* pPage;
      PdfPainter painter;
      PdfFont* pFont;

      pPage = _doc->CreatePage( PdfPage::CreateStandardPageSize( ePdfPageSize_A4 ) );

      if( !pPage )
      {
         PODOFO_RAISE_ERROR( ePdfError_InvalidHandle );
      }

      painter.SetPage( pPage );

      pFont = _doc->CreateFont( "Arial" );

      if( !pFont )
      {
         PODOFO_RAISE_ERROR( ePdfError_InvalidHandle );
      }

      pFont->SetFontSize( 18.0 );

      painter.SetFont( pFont );
      painter.DrawText( 56.69, pPage->GetPageSize().GetHeight() - 56.69, "Hello World!" );

      painter.FinishPage();

      _doc->GetInfo()->SetCreator ( PdfString("examplahelloworld - A PoDoFo test application") );
      _doc->GetInfo()->SetAuthor  ( PdfString("Dominik Seichter") );
      _doc->GetInfo()->SetTitle   ( PdfString("Hello World") );
      _doc->GetInfo()->SetSubject ( PdfString("Testing the PoDoFo PDF Library") );
      _doc->GetInfo()->SetKeywords( PdfString("Test;PDF;Hello World;") );
      _doc->Close();
      }
   catch (const PdfError &eCode)
      {
      return err_make (ERRFN, E_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
      }
#endif
   return NULL;
   }


err_info *Pdfwrite::addPage (const Filepage *mp)
   {
   try
      {
      Q_ASSERT (_doc);
//       _doc = new PdfMemDocument (_fname.latin1 ());
      PdfPage *page;
      PdfPainter painter;

      page = _doc->CreatePage (PdfPage::CreateStandardPageSize (ePdfPageSize_A4));
      if (!page)
         PODOFO_RAISE_ERROR (ePdfError_InvalidHandle);
      painter.SetPage (page);

      PdfImage *image = new PdfImage (_doc);

      QByteArray ba = mp->invertData ();
      PdfMemoryInputStream input (ba.constData (), mp->_size);

      image->SetImageColorSpace (mp->_depth <= 8 ? ePdfColorSpace_DeviceGray
            : ePdfColorSpace_DeviceRGB);
      image->SetImageData (mp->_width, mp->_height, mp->_depth == 1 ? 1 : 8, &input);
      PdfRect rect = page->GetPageSize ();
      double w = image->GetWidth ();
      double h = image->GetHeight ();
      double xscale = rect.GetWidth () / w;
      double yscale = rect.GetHeight () / h;
      double scale = xscale;
      if (scale > yscale)
         scale = yscale;
      painter.DrawImage (rect.GetLeft (), rect.GetBottom (), image, scale, scale);
      painter.FinishPage();
      }
   catch (const PdfError &eCode)
      {
      return err_make (ERRFN, E_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
      }
   return NULL;
   }
