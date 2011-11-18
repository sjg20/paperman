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
#include "pdfio.h"


#define EXCEPTIONS


#ifdef EXCEPTIONS
#define mytry try
#else
#define mytry
#endif


using namespace PoDoFo;


Pdfio::Pdfio (const QString &fname)
   {
   _doc = 0;
#ifdef CONFIG_use_poppler
   _pop = 0;
#endif
   _pathname = fname;
   }


Pdfio::~Pdfio ()
   {
   if (_doc)
      delete _doc;
   }


#ifdef CONFIG_use_poppler

err_info *Pdfio::find_page (int pagenum, Poppler::Page *& page)
   {
   if (!_pop)
      return err_make (ERRFN, ERR_file_is_not_open1, _pathname.latin1 ());
   page = _pop->page (pagenum);

   if (!page)
      return err_make (ERRFN, ERR_could_not_find_image_chunk_for_page1, pagenum + 1);
   return NULL;
   }

#endif


err_info *Pdfio::open (void)
   {
#ifdef CONFIG_use_poppler
   _pop = Poppler::Document::load (_pathname);
   if (!_pop)
      return err_make (ERRFN, ERR_cannot_open_file1, _pathname.latin1 ());
   if (_pop->isLocked ())
      return err_make (ERRFN, ERR_cannot_open_document_as_it_is_locked1, _pathname.latin1 ());
#endif
   try
      {
      _doc = new PdfMemDocument ();
      _doc->Load (_pathname.latin1 ());
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#ifndef CONFIG_use_poppler
   return err_make (ERRFN, ERR_pdf_previewing_requires_poppler);
#endif
   return NULL;
   }


err_info *Pdfio::create (void)
   {
   try
      {
//       qDebug () << _pathname;
      _doc = new PdfMemDocument ();
      //FIXME: put proper fields in here
      _doc->GetInfo()->SetCreator ( PdfString("Maxview - manage your paper") );
      _doc->GetInfo()->SetAuthor  ( PdfString("Simon Glass") );
      _doc->GetInfo()->SetTitle   ( PdfString("") );
      _doc->GetInfo()->SetSubject ( PdfString("") );
      _doc->GetInfo()->SetKeywords( PdfString("sep;sep;") );
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
   return NULL;
   }


err_info *Pdfio::close (void)
   {
   try
      {
      _doc->Write (_pathname.latin1 ());
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#ifdef CONFIG_use_poppler
   if (_pop)
      {
      delete _pop;
      _pop = 0;
      CALL (open ());
      }
#endif
   return NULL;
   }


err_info *Pdfio::addPage (const Filepage *mp)
   {
   mytry
      {
      Q_ASSERT (_doc);
//       _doc = new PdfMemDocument (_fname.latin1 ());
      PdfPage *page;
      PdfPainter painter;

      page = _doc->CreatePage (PdfPage::CreateStandardPageSize (ePdfPageSize_A4));
      if (!page)
         PODOFO_RAISE_ERROR (ePdfError_InvalidHandle);

      QImage imthumb;
//       QByteArray ba = mp->getThumbnailJpeg (false, imthumb);
//       bool jpeg = ba.size () != 0;
      QByteArray ba = mp->getThumbnailRaw (false, imthumb, false);
      bool jpeg = false;

      PdfImage *thumb = new PdfImage (_doc);
      thumb->SetImageColorSpace (imthumb.depth () <= 8 ? ePdfColorSpace_DeviceGray
            : ePdfColorSpace_DeviceRGB);
      PdfMemoryInputStream *tinput;
      TVecFilters filters;
//       if (jpeg)
         tinput = new PdfMemoryInputStream ((const char *)ba.constData (), ba.size ());
//       else
//          tinput = new PdfMemoryInputStream ((const char *)imthumb.bits (), imthumb.numBytes ());

      if (!jpeg)
         filters.push_back (ePdfFilter_FlateDecode);

      thumb->SetImageData (imthumb.width (), imthumb.height (),
         imthumb.depth () == 1 ? 1 : 8, tinput, filters);
      delete tinput;
//       PdfObject *thumbobj = _doc->GetObjects().CreateObject (thumb);
//       PdfVariant thumbref (thumb->Reference ());

      PdfObject *obj;

      obj = thumb->GetObject ();
      if (jpeg)
         obj->GetDictionary().AddKey ("Filter", PdfName ("DCTDecode"));

      obj = page->GetObject ();

      obj->GetDictionary().AddKey ("Thumb", thumb->GetObjectReference ());


      // now the main image
      painter.SetPage (page);

      PdfImage *image = new PdfImage (_doc);

      ba = mp->copyData (mp->_depth == 1, true);
      PdfMemoryInputStream input ((const char *)ba.constData (), ba.size ());

      image->SetImageColorSpace (mp->_depth <= 8 ? ePdfColorSpace_DeviceGray
            : ePdfColorSpace_DeviceRGB);
      image->SetImageData (mp->_width, mp->_height, mp->_depth == 1 ? 1 : 8, &input);
      qDebug () << "pdfio: added image depth" << mp->_depth;
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
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   return NULL;
   }


err_info *Pdfio::make_error (const PdfError &eCode)
   {
   TDequeErrorInfo info = eCode.GetCallstack ();
   TCIDequeErrorInfo it = info.begin ();

   eCode.PrintErrorMsg();
   while (it != info.end ())
      {
      qDebug () << (*it).GetInformation ().c_str ();
      it++;
      }
   return err_make (ERRFN, ERR_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
   }


err_info *Pdfio::getPageTitle (int pagenum, QString &title)
   {
#ifdef CONFIG_use_poppler
   Poppler::Page *page;

   CALL (find_page (pagenum, page));
   title = page->label ();
   return NULL;
#else
   return err_make (ERRFN, ERR_pdf_previewing_requires_poppler);
#endif
   }


err_info *Pdfio::getAnnot (QString type, QString &str)
   {
#ifdef CONFIG_use_poppler
   if (!_pop)
      return err_make (ERRFN, ERR_cannot_read_pdf_file1, qPrintable (_pathname));
   str = _pop->info (type);
   return NULL;
#else
   return err_make (ERRFN, ERR_pdf_previewing_requires_poppler);
#endif
   }


err_info *Pdfio::getPageText (int pagenum, QString &str)
   {
#ifdef CONFIG_use_poppler
   Poppler::Page *page;

   CALL (find_page (pagenum, page));
   str = page->text (QRectF ());
   return NULL;
#else
   return err_make (ERRFN, ERR_pdf_previewing_requires_poppler);
#endif
   }


err_info *Pdfio::getImageSize (int pagenum, bool preview, QSize &size,
                               int &bpp)
   {
   mytry
      {
      const PdfDictionary *dict;
      const PdfObject *obj = 0;

      if (preview)
         obj = get_thumbnail_obj (pagenum, dict);
      else
         obj = get_image_obj (pagenum, dict);
      if (obj)
         {
         int width, height;

         get_image_details (dict, width, height, bpp);
         size = QSize (width, height);
         return NULL;
         }
      }
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   if (preview)
      size = QSize (); // we have no preview
   else
      {
#ifdef CONFIG_use_poppler
      Poppler::Page *page;
      QSizeF fsize;

      CALL (find_page (pagenum, page));
      fsize = page->pageSize ();
      fsize *= 300;
      fsize /= 72;      // make some assumptions!
      size = fsize.toSize ();
      bpp = 24;
#else
      return err_make (ERRFN, ERR_pdf_previewing_requires_poppler);
#endif
      }
   return NULL;
   }


void Pdfio::get_image_details (const PdfDictionary *dict, int &width, int &height, int &bpp) const
   {
   width = dict->GetKey( "Width" )->GetNumber();
   height = dict->GetKey( "Height" )->GetNumber();
   int bpc = dict->GetKey( "BitsPerComponent" )->GetNumber();
   QString cs = dict->GetKey( "ColorSpace" )->GetName().GetName ().c_str ();
   EPdfColorSpace space = cs == "DeviceRGB" ? ePdfColorSpace_DeviceRGB
         : cs == "DeviceGray" ? ePdfColorSpace_DeviceGray
         : ePdfColorSpace_DeviceGray;
   bpp = bpc;
   if (space == ePdfColorSpace_DeviceRGB)
      bpp *= 3;
   }


err_info *Pdfio::getImage (QString fname, int pagenum, QImage &image, double xscale,
      double yscale, bool preview)
   {
   // try to find the image with PoDoFo
   try
      {
      const PdfDictionary *dict;
      const PdfObject *obj = 0;

      if (preview)
         obj = get_thumbnail_obj (pagenum, dict);
      else
         obj = get_image_obj (pagenum, dict);

      if (obj)
         {
         int width, height, bpp;

         get_image_details (dict, width, height, bpp);
         qDebug () << "image" << width << height << bpp;
         char *buff;
         pdf_long len;
         obj->GetStream()->GetFilteredCopy (&buff, &len);
//          qDebug () << buff << len;
         int stride = (width * bpp + 7) / 8;
//         qDebug () << "width" << width << "bpp" << bpp << "stride" << stride
//                 << "expected" << stride * height << "got" << len;
         if (len < stride * height)
            return err_make (ERRFN, ERR_pdf_decoder_returned_too_little_data_for_page_expected_got4,
               qPrintable (fname), pagenum + 1, stride * height, len);
         Filepage::getImageFromLines (buff, width, height, bpp, stride,
               image, true, false, bpp == 1);
         return NULL;
         }
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }

#ifdef CONFIG_use_poppler
   Poppler::Page *page;

   CALL (find_page (pagenum, page));
   image = page->renderToImage (xscale, yscale);
#else
   return err_make (ERRFN, ERR_pdf_previewing_requires_poppler);
#endif
   return NULL;
   }


err_info *Pdfio::flush (void)
   {
   return close ();
   }


int Pdfio::numPages (void)
   {
#ifdef CONFIG_use_poppler
   // perhaps we don't know
   return _pop ? _pop->numPages () : 1;
#else
   return 1;
#endif
   }


const PdfObject *Pdfio::get_image_obj (int pagenum, const PdfDictionary *&dict)
   {
   if (!_doc)
      return 0;
   PdfPage *page = _doc->GetPage (pagenum);
   const PdfObject *obj = page->GetContents ();

   dict = 0;
//    qDebug () << "has stream" << _pathname << obj->HasStream ();
   PdfContentsTokenizer token (page); //stream->Get(), stream->GetLength());
   EPdfContentsType t;
   const char *text;
   char str [10], *s;
   const char *p;
   PdfVariant var;
   bool ok, image_only = true;
   QList <PdfVariant> stack;
   QString image_name;

//    qDebug () << "decoding file" << _pathname;
   QString allowed = ",cm,q,Q,Do,";
   *str = ',';
   while (ok = token.ReadNext (t, text, var), ok && image_only)
      {
      switch (t)
         {
         case ePdfContentsType_Keyword :
            for (s = str + 1, p = text; *p;)
               *s++ = *p++;
            *s++ = ',';
            *s = '\0';
            if (!allowed.contains (str))
               image_only = false;
//             qDebug () << "   keyword" << text << stack.size ();
            if (0 == strcmp (text, "Do") && stack.size () == 1
               && stack [0].IsName () && image_name.isEmpty ())
               image_name = stack [0].GetName ().GetName ().c_str ();
            stack.clear ();
            break;

         case ePdfContentsType_Variant :
            stack << var;
            break;
         }
      }
//    qDebug () << "decoding done" << image_only << image_name;
   if (!image_only)
      return 0;
   obj = page->GetResources ();
   PdfReference ref;
   QString refstr;

   if (obj->IsDictionary ())
      {
      const TKeyMap resmap = obj->GetDictionary ().GetKeys();

      for (TCIKeyMap itres = resmap.begin(); itres != resmap.end(); ++itres )
         {
         const PdfObject *o = itres->second;

//          qDebug () << itres->first.GetName ().c_str ();
         if (0 == strcmp (itres->first.GetName ().c_str (), "XObject"))
            {
//             qDebug () << "dict" << o << o->GetDataTypeString () << o->Reference ().ToString ().c_str ();
            const TKeyMap resmap2 = o->GetDictionary ().GetKeys();

            for (TCIKeyMap itres2 = resmap2.begin(); itres2 != resmap2.end(); ++itres2 )
               {
               const PdfObject *o2 = itres2->second;

               ref = o2->GetReference ();
//                qDebug () << itres2->first.GetName ().c_str ();
//                qDebug () << "obj" << o2->GetDataTypeString () << o2->GetReference ().ToString ().c_str ();
               }
            }
         }
      }

//    qDebug () << "ref" << ref.ToString ().c_str ();
   return get_xobject_image (ref, dict);
   }


#if 0
       PdfMemDocument input1( pszInput1 );
    printf("Reading file: %s\n", pszInput2 );
    PdfMemDocument input2( pszInput2 );

    input1.InsertPages( input2, 1, 2 );
#endif


const PdfObject *Pdfio::get_thumbnail_obj (int pagenum, const PdfDictionary *&dict)
   {
   if (!_doc)
      return 0;
   const PdfPage *page = _doc->GetPage (pagenum);
   const PdfObject *obj = page->GetObject ();
   const PdfObject *thumb;

   thumb = obj->GetDictionary().GetKey ("Thumb");
   if (!thumb || !thumb->IsReference())
      return 0;
   return get_xobject_image (thumb->GetReference (), dict);
   }



const PdfObject *Pdfio::get_xobject_image (const PdfReference &ref, const PdfDictionary *&dict)
   {
   PdfObject *obj;

   obj = _doc->GetObjects ().GetObject (ref);
//    qDebug () << "obj" << obj;
   if (!obj->IsDictionary())
      return 0;
   dict = &obj->GetDictionary();

   const PdfObject* pObjType = dict->GetKey( PdfName::KeyType );
   const PdfObject* pObjSubType = dict->GetKey( PdfName::KeySubtype );
   if( ( pObjType && pObjType->IsName() && ( pObjType->GetName().GetName() == "XObject" ) ) ||
      ( pObjSubType && pObjSubType->IsName() && ( pObjSubType->GetName().GetName() == "Image" ) ) )
      return obj;
   return 0;
   }


err_info *Pdfio::appendFrom (Pdfio *from)
{
   mytry
      {
      _doc->Append (*from->_doc);
      }
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   // close and write changes
   return close ();
}


err_info *Pdfio::insertPages (Pdfio *from, int start, int count)
{
   mytry
      {
      _doc->InsertPages (*from->_doc, start, count);
      }
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   // close and write changes
   return close ();
}


err_info *Pdfio::deletePages (int start, int count)
{
   mytry
      {
      _doc->DeletePages (start, count);
      }
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   // close and write changes
   return close ();
}

