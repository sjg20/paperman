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
#include <QFile>

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


/* PoDoFo 1.x reworked its API: pages live in a PdfPageCollection,
   images are created by the document and described by PdfImageInfo,
   the content tokenizer became PdfContentStreamReader and most accessors
   return references rather than pointers. Debian and Ubuntu still ship
   0.9.8 while MSYS2 (used for the Windows build) has 1.x, so support
   both */
#if PODOFO_VERSION_MAJOR >= 1
#define PODOFO_1X
#endif


Pdfio::Pdfio (const QString &fname)
   {
   _doc = 0;
#ifdef CONFIG_use_poppler
   _pop = nullptr;
#endif
   _pathname = fname;
#ifndef PODOFO_1X
   PoDoFo::PdfError::EnableDebug(false);
#endif
   }


Pdfio::~Pdfio ()
   {
   if (_doc)
      delete _doc;
   }


#ifdef CONFIG_use_poppler

err_info *Pdfio::find_page (int pagenum, std::unique_ptr<Poppler::Page> &page)
   {
   if (!_pop)
      return err_make (ERRFN, ERR_file_is_not_open1,
                       _pathname.toLatin1 ().constData());
#if QT_VERSION >= 0x060000
   page = _pop->page (pagenum);
#else
   page.reset(_pop->page (pagenum));
#endif

   if (!page)
      return err_make (ERRFN, ERR_could_not_find_image_chunk_for_page1, pagenum + 1);
   return NULL;
   }

#endif


err_info *Pdfio::open (void)
   {
#ifdef CONFIG_use_poppler
#if QT_VERSION >= 0x060000
   _pop = Poppler::Document::load (_pathname);
#else
   _pop.reset(Poppler::Document::load (_pathname));
#endif
   if (!_pop)
      return err_make (ERRFN, ERR_cannot_open_file1,
                       _pathname.toLatin1 ().constData());
   if (_pop->isLocked ())
      return err_make (ERRFN, ERR_cannot_open_document_as_it_is_locked1,
                       _pathname.toLatin1 ().constData());
#endif
   PoDoFo::PdfMemDocument *doc = 0;

   try
      {
      doc = new PdfMemDocument ();
      doc->Load (_pathname.toLatin1 ().constData());
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
   _doc = doc;
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
#ifdef PODOFO_1X
      PdfMetadata &meta = _doc->GetMetadata ();

      meta.SetCreator (PdfString ("Maxview - manage your paper"));
      meta.SetAuthor (PdfString ("Simon Glass"));
      meta.SetTitle (PdfString (""));
      meta.SetSubject (PdfString (""));
      meta.SetKeywords (std::vector<std::string> {"sep", "sep"});
#else
      _doc->GetInfo()->SetCreator ( PdfString("Maxview - manage your paper") );
      _doc->GetInfo()->SetAuthor  ( PdfString("Simon Glass") );
      _doc->GetInfo()->SetTitle   ( PdfString("") );
      _doc->GetInfo()->SetSubject ( PdfString("") );
      _doc->GetInfo()->SetKeywords( PdfString("sep;sep;") );
#endif
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
   return NULL;
   }


err_info *Pdfio::close (void)
   {
   /* PdfMemDocument loads objects on demand, so writing over the file
      it is still reading from fails with an unexpected end of file.
      Write to a temporary file and rename it into place */
   QString tmpname = _pathname + ".tmp";

   try
      {
#ifdef PODOFO_1X
      _doc->Save (tmpname.toLatin1 ().constData());
#else
      _doc->Write (tmpname.toLatin1 ().constData());
#endif
      }
   catch (const PdfError &eCode)
      {
      QFile::remove (tmpname);
      return make_error (eCode);
      }
   if (QFile::exists (_pathname))
      QFile::remove (_pathname);
   if (!QFile::rename (tmpname, _pathname))
      return err_make (ERRFN, ERR_could_not_rename_file2,
            qPrintable (tmpname), qPrintable (_pathname));

   // reopen both libraries' views of the new file
   delete _doc;
   _doc = 0;
#ifdef CONFIG_use_poppler
   _pop.reset();
#endif
   return open ();
   }


#ifdef PODOFO_1X

/* PoDoFo 1.x helpers */

/* scale an image to fill the page, keeping its aspect ratio, and draw it
   at the bottom left */
static void draw_scaled (PdfPage &page, const PdfImage &image)
   {
   PdfPainter painter;
   Rect rect = page.GetRect ();
   double xscale = rect.Width / image.GetWidth ();
   double yscale = rect.Height / image.GetHeight ();
   double scale = qMin (xscale, yscale);

   painter.SetCanvas (page);
   painter.DrawImage (image, rect.X, rect.Y, scale, scale);
   painter.FinishDrawing ();
   }


/* store uncompressed pixel data in an image, flate-compressing it. Uses
   the raw path so 1-bit images are supported */
static void set_image_pixels (PdfImage &image, const QByteArray &ba,
                              int width, int height, int depth)
   {
   PdfImageInfo info;

   info.Width = width;
   info.Height = height;
   info.BitsPerComponent = depth == 1 ? 1 : 8;
   info.ColorSpace = depth <= 8 ? PdfColorSpaceType::DeviceGray
         : PdfColorSpaceType::DeviceRGB;
   image.SetDataRaw (bufferview (ba.constData (), ba.size ()), info);

   /* SetDataRaw() stores the bytes as they are, so compress them now */
   SpanStreamDevice stream (bufferview (ba.constData (), ba.size ()));
   image.GetObject ().GetOrCreateStream ().SetData (stream,
         PdfFilterList {PdfFilterType::FlateDecode}, false);
   }

#endif


err_info *Pdfio::addPage (const Filepage *mp)
   {
   mytry
      {
      Q_ASSERT (_doc);
#ifdef PODOFO_1X
      PdfPage &page = _doc->GetPages ().CreatePage (PdfPageSize::A4);

      QImage imthumb;
      QByteArray ba = mp->getThumbnailRaw (false, imthumb, false);
      std::unique_ptr<PdfImage> thumb = _doc->CreateImage ();

      set_image_pixels (*thumb, ba, imthumb.width (), imthumb.height (),
                        imthumb.depth ());
      page.GetDictionary ().AddKey (PdfName ("Thumb"),
            PdfObject (thumb->GetObject ().GetIndirectReference ()));

      // now the main image
      std::unique_ptr<PdfImage> image = _doc->CreateImage ();

      ba = mp->copyData (mp->_depth == 1, true);
      set_image_pixels (*image, ba, mp->_width, mp->_height, mp->_depth);
      draw_scaled (page, *image);
#else
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
      // qDebug () << "pdfio: added image depth" << mp->_depth;
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
#endif
      }
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   return NULL;
   }


err_info *Pdfio::addPageJpeg(const QByteArray &jpegData, int width, int height,
                             bool colour)
   {
   mytry
      {
      Q_ASSERT (_doc);
#ifdef PODOFO_1X
      PdfPage &page = _doc->GetPages ().CreatePage (PdfPageSize::A4);
      std::unique_ptr<PdfImage> image = _doc->CreateImage ();
      PdfImageInfo info;

      /* Write the pre-compressed JPEG stream with DCTDecode filter */
      info.Width = width;
      info.Height = height;
      info.BitsPerComponent = 8;
      info.Filters = PdfFilterList {PdfFilterType::DCTDecode};
      info.ColorSpace = colour ? PdfColorSpaceType::DeviceRGB
            : PdfColorSpaceType::DeviceGray;
      image->SetDataRaw (bufferview (jpegData.constData (), jpegData.size ()),
                         info);
      draw_scaled (page, *image);
#else
      PdfPage *page;
      PdfPainter painter;

      page = _doc->CreatePage (PdfPage::CreateStandardPageSize (ePdfPageSize_A4));
      if (!page)
         PODOFO_RAISE_ERROR (ePdfError_InvalidHandle);

      painter.SetPage (page);

      PdfImage *image = new PdfImage (_doc);
      image->SetImageColorSpace (colour ? ePdfColorSpace_DeviceRGB
                                        : ePdfColorSpace_DeviceGray);

      /* Write the pre-compressed JPEG stream with DCTDecode filter */
      PdfObject *obj = image->GetObject ();
      obj->GetDictionary().AddKey ("Filter", PdfName ("DCTDecode"));

      PdfMemoryInputStream input (jpegData.constData (), jpegData.size ());
      image->SetImageDataRaw (width, height, 8, &input);

      PdfRect rect = page->GetPageSize ();
      double xscale = rect.GetWidth () / width;
      double yscale = rect.GetHeight () / height;
      double scale = qMin (xscale, yscale);
      painter.DrawImage (rect.GetLeft (), rect.GetBottom (), image,
                         scale, scale);
      painter.FinishPage();
#endif
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
#ifdef PODOFO_1X
   eCode.PrintErrorMsg ();
   for (const PdfErrorInfo &info : eCode.GetCallStack ())
      qDebug () << info.GetInformation ().c_str ();
   return err_make (ERRFN, ERR_pdf_creation_error1, eCode.what ());
#else
   TDequeErrorInfo info = eCode.GetCallstack ();
   TCIDequeErrorInfo it = info.begin ();

   eCode.PrintErrorMsg();
   while (it != info.end ())
      {
      qDebug () << (*it).GetInformation ().c_str ();
      it++;
      }
   return err_make (ERRFN, ERR_pdf_creation_error1, eCode.ErrorMessage (eCode.GetError()));
#endif
   }


/* returns the /Rotate value of a page, normalised to 0-359 */
int Pdfio::page_rotation (int pagenum) const
   {
#ifdef PODOFO_1X
   return (int)_doc->GetPages ().GetPageAt (pagenum).GetRotation () % 360;
#else
   return ((_doc->GetPage (pagenum)->GetRotation () % 360) + 360) % 360;
#endif
   }


err_info *Pdfio::getPageTitle (int pagenum, QString &title)
   {
#ifdef CONFIG_use_poppler
   std::unique_ptr<Poppler::Page> page;

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
   std::unique_ptr<Poppler::Page> page;

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

         /* a 90 or 270 degree /Rotate swaps the displayed dimensions,
            to match the rotated image apply_rotation() produces */
         int rot = page_rotation (pagenum);
         if (rot == 90 || rot == 270)
            size.transpose ();
         return NULL;
         }
      }
#ifdef EXCEPTIONS
   catch (const PdfError &)
      {
      /* if PoDoFo cannot handle the page, fall back to poppler below.
         For example PoDoFo 0.9.8 refuses to decode the streams of
         images with under 8 bits per pixel, since its predictor checks
         misread the image's BitsPerComponent as predictor parameters */
      }
#endif
   if (preview)
      size = QSize (); // we have no preview
   else
      {
#ifdef CONFIG_use_poppler
      std::unique_ptr<Poppler::Page> page;
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


QImage Pdfio::apply_rotation (int pagenum, const QImage &image) const
   {
   if (!_doc)
      return image;

   /* /Rotate is the clockwise rotation a viewer applies to the page,
      so map it to the matching image transform */
   int rotation = page_rotation (pagenum);
   switch (rotation)
      {
      case 90 :
         return File::transformImage (image, File::Transform_rotate90);
      case 180 :
         return File::transformImage (image, File::Transform_rotate180);
      case 270 :
         return File::transformImage (image, File::Transform_rotate270);
      }
   return image;
   }


void Pdfio::get_image_details (const PdfDictionary *dict, int &width, int &height, int &bpp) const
   {
   const PdfObject *pwidth = dict->GetKey( "Width" );
   const PdfObject *pheight = dict->GetKey( "Height" );
   const PdfObject *pbits = dict->GetKey( "BitsPerComponent" );
   const PdfObject *pcolor = dict->GetKey( "ColorSpace" );

   if (!pwidth) {
       width = 1;
       height = 1;
       bpp = 24;
       return;
   }
   width = pwidth->GetNumber();
   height = pheight->GetNumber();
   int bpc = pbits->GetNumber();
#ifdef PODOFO_1X
   QString cs = pcolor && pcolor->IsName ()
         ? QString::fromStdString (std::string (pcolor->GetName ().GetString ()))
         : QString ();
#else
   QString cs = pcolor->GetName().GetName ().c_str ();
#endif
   bpp = bpc;
   if (cs == "DeviceRGB")
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
         // qDebug () << "image" << width << height << bpp;
#ifdef PODOFO_1X
         /* GetCopy() decodes the stream and throws for filters it cannot
            handle, such as DCTDecode, which drops us into the poppler
            fallback below */
         charbuff copy = obj->MustGetStream ().GetCopy ();
         char *buff = copy.data ();
         long len = copy.size ();
#else
         char *buff;
         pdf_long len;
         obj->GetStream()->GetFilteredCopy (&buff, &len);
#endif
//          qDebug () << buff << len;
         int stride = (width * bpp + 7) / 8;
//         qDebug () << "width" << width << "bpp" << bpp << "stride" << stride
//                 << "expected" << stride * height << "got" << len;
         if (len < stride * height)
            return err_make (ERRFN, ERR_pdf_decoder_returned_too_little_data_for_page_expected_got4,
               qPrintable (fname), pagenum + 1, stride * height, len);
         Filepage::getImageFromLines (buff, width, height, bpp, stride,
               image, true, false, bpp == 1);

         /* podofo extracts the raw embedded scan, which does not
            reflect the page's /Rotate attribute; apply it so the
            rendered image matches what a PDF viewer (and the poppler
            fallback) shows */
         image = apply_rotation (pagenum, image);
         return NULL;
         }
      }
   catch (const PdfError &)
      {
      /* if PoDoFo cannot decode the page image, fall back to rendering
         with poppler below. For example PoDoFo 0.9.8 refuses to decode
         the streams of images with under 8 bits per pixel, since its
         predictor checks misread the image's BitsPerComponent as
         predictor parameters */
      }

#ifdef CONFIG_use_poppler
   std::unique_ptr<Poppler::Page> page;

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


err_info *Pdfio::rotatePage (int pagenum, int degrees)
   {
   if (!_doc)
      CALL (open ());
   try
      {
#ifdef PODOFO_1X
      PdfPageCollection &pages = _doc->GetPages ();

      if (pagenum < 0 || (unsigned)pagenum >= pages.GetCount ())
         return err_make (ERRFN, ERR_could_not_find_image_chunk_for_page1,
               pagenum + 1);
      PdfPage &page = pages.GetPageAt (pagenum);
      page.SetRotation ((page.GetRotation () + degrees) % 360);
#else
      PdfPage *page = _doc->GetPage (pagenum);

      if (!page)
         return err_make (ERRFN, ERR_could_not_find_image_chunk_for_page1,
               pagenum + 1);
      page->SetRotation ((page->GetRotation () + degrees) % 360);
#endif
      }
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }

   // write the change and reopen the renderer's view of the file
   return flush ();
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
   bool image_only = true;
   QString image_name;
   PdfReference ref;

   dict = 0;
#ifdef PODOFO_1X
   /* walk the content stream: the page is a plain image if it only
      uses the transform, state and Do operators */
   PdfPage &page = _doc->GetPages ().GetPageAt (pagenum);
   PdfContentReaderArgs args;

   // report Do as a plain operator rather than resolving the XObject
   args.Flags = (PdfContentReaderFlags)
         ((int)PdfContentReaderFlags::SkipFollowFormXObjects
          | (int)PdfContentReaderFlags::SkipHandleNonFormXObjects);
   PdfContentStreamReader reader (page, args);
   PdfContent content;

   while (image_only && reader.TryReadNext (content))
      {
      switch (content.GetType ())
         {
         case PdfContentType::Operator :
            {
            PdfOperator op = content.GetOperator ();
            const PdfVariantStack &stack = content.GetStack ();

            if (op != PdfOperator::q && op != PdfOperator::Q
                && op != PdfOperator::cm && op != PdfOperator::Do)
               image_only = false;
            if (op == PdfOperator::Do && stack.GetSize () == 1
                && stack [0].IsName () && image_name.isEmpty ())
               image_name = QString::fromStdString (
                     std::string (stack [0].GetName ().GetString ()));
            break;
            }

         case PdfContentType::UnexpectedKeyword :
            image_only = false;
            break;

         default :
            break;
         }
      }
   if (!image_only)
      return 0;

   /* find the XObject the page draws: the one named by Do if we saw it,
      otherwise the last one in the resources */
   const PdfResources &res = page.GetResources ();
   const PdfObject *xobjects = res.GetDictionary ().FindKey ("XObject");
   if (xobjects && xobjects->IsDictionary ())
      {
      for (auto &pair : xobjects->GetDictionary ())
         {
         if (!pair.second.IsReference ())
            continue;
         ref = pair.second.GetReference ();
         if (QString::fromStdString (std::string (pair.first.GetString ()))
             == image_name)
            break;
         }
      }
#else
   PdfPage *page = _doc->GetPage (pagenum);
   const PdfObject *obj = page->GetContents ();

//    qDebug () << "has stream" << _pathname << obj->HasStream ();
   PdfContentsTokenizer token (page); //stream->Get(), stream->GetLength());
   EPdfContentsType t;
   const char *text;
   char str [10], *s;
   const char *p;
   PdfVariant var;
   bool ok;
   QList <PdfVariant> stack;

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

         case ePdfContentsType_ImageData:
            break;
         }
      }
//    qDebug () << "decoding done" << image_only << image_name;
   if (!image_only)
      return 0;
   obj = page->GetResources ();
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
#endif

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
#ifdef PODOFO_1X
   const PdfObject *obj = &_doc->GetPages ().GetPageAt (pagenum).GetObject ();
#else
   const PdfPage *page = _doc->GetPage (pagenum);
   const PdfObject *obj = page->GetObject ();
#endif
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

   const PdfObject* pObjType = dict->GetKey( "Type" );
   const PdfObject* pObjSubType = dict->GetKey( "Subtype" );
#ifdef PODOFO_1X
   if( ( pObjType && pObjType->IsName() && ( pObjType->GetName().GetString() == "XObject" ) ) ||
      ( pObjSubType && pObjSubType->IsName() && ( pObjSubType->GetName().GetString() == "Image" ) ) )
      return obj;
#else
   if( ( pObjType && pObjType->IsName() && ( pObjType->GetName().GetName() == "XObject" ) ) ||
      ( pObjSubType && pObjSubType->IsName() && ( pObjSubType->GetName().GetName() == "Image" ) ) )
      return obj;
#endif
   return 0;
   }


err_info *Pdfio::appendFrom (Pdfio *from)
{
   mytry
      {
#ifdef PODOFO_1X
      _doc->GetPages ().AppendDocumentPages (*from->_doc);
#else
      _doc->Append (*from->_doc);
#endif
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


err_info *Pdfio::appendPages (Pdfio *from)
{
   mytry
      {
#ifdef PODOFO_1X
      _doc->GetPages ().AppendDocumentPages (*from->_doc);
#else
      _doc->Append (*from->_doc);
#endif
      }
#ifdef EXCEPTIONS
   catch (const PdfError &eCode)
      {
      return make_error (eCode);
      }
#endif
   return NULL;
}


err_info *Pdfio::insertPages (Pdfio *from, int start, int count)
{
   mytry
      {
#ifdef PODOFO_1X
      _doc->GetPages ().AppendDocumentPages (*from->_doc, start, count);
#else
      _doc->InsertPages (*from->_doc, start, count);
#endif
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
#ifdef PODOFO_1X
      for (int i = 0; i < count; i++)
         _doc->GetPages ().RemovePageAt (start);
#else
      _doc->DeletePages (start, count);
#endif
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

