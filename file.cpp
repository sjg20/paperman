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


#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>

#include "config.h"
#include "desk.h"
#include "err.h"
#include "file.h"
#include "filejpeg.h"
#include "filemax.h"
#include "fileother.h"
#include "filepdf.h"
#include "maxview.h"
#include "mem.h"
#include "op.h"
#include "utils.h"


#if QT_VERSION >= 0x040400
#define USE_24BPP
#endif



static QPixmap *unknown = 0;


File::File (const QString &dir, const QString &fname, Desk *desk, e_type type)
      : QObject (0)
   {
   setup ();
   _dir = dir;    // must have trailing /
   updateFilename (fname);

   _desk = desk;
   _type = type;
   }


File::~File ()
   {
//    if (_max)
//       max_close (_max);
//    if (_pixmap)
//       delete _pixmap;
   }


File::File (File *orig, Desk *new_desk)
   {
   setup ();
   _desk = new_desk;
   _ref_to = orig;
   _type = orig->_type;
   _pixmap = orig->_pixmap;
   _filename = orig->_filename;
   _basename = orig->_basename;
   _leaf = orig->_leaf;
   _ext = orig->_ext;
   _pathname = orig->_pathname;
   _pagenum = orig->_pagenum;
   _size = orig->_size;
   _preview_maxsize = orig->_preview_maxsize;
   _title_maxsize = orig->_title_maxsize;
   _pagename_maxsize = orig->_pagename_maxsize;
   _valid = orig->_valid;
   }


void File::setup (void)
   {
   _pagenum = 0;
   _size = 0;
   _timestamp = QDateTime::currentDateTime ();
   _order = 0;
   _err = 0;
   _valid = false;
   _preview_maxsize = QSize (-1, -1);
   _title_maxsize = QSize (-1, -1);
   _pagename_maxsize = QSize (-1, -1);
   if (!unknown)
      unknown = new QPixmap (":images/images/unknown.xpm");
//    Q_ASSERT (!_no_access.isNull ());
   Q_ASSERT (!unknown->isNull ());
   _order = -1;
   _annot_loaded = false;
   _env_loaded = false;
   _ref_to = 0;
   }


QPixmap File::unknownPixmap (void)
   {
   return *unknown;
   }


Desk *File::desk (void)
   {
   return _desk;
   }


void File::setDesk (Desk *desk)
   {
   _desk = desk;
   }


File::e_type File::type (void)
   {
   return _type;
   }


void File::updateFilename (const QString &fname)
   {
   _filename = fname;
   _leaf = removeExtension (_filename, _ext);
   _pathname = _dir + _filename;
   if (_ext == ".max")
      _basename = _leaf;
   else
      _basename = _filename;
   }


QString File::typeName (e_type type)
   {
   return QString ("Other,Max,PDF,JPEG").section (',', type, type);
   }


QString File::typeName (void)
   {
   return typeName (_type);
   }

QString File::typeExt (e_type type)
   {
   return QString (",.max,.pdf,.jpg").section (',', type, type);
   }

File::e_type extToType (const QString &in_ext)
{
   QString ext = in_ext.toLower ();

   //FIXME: there should be an associate config for doing this
   if (ext == "max")
      return File::Type_max;
   else if (ext == "pdf")
      return File::Type_pdf;
   else if (ext == "jpg" || ext == "jpeg")
      return File::Type_jpeg;
   else
      return File::Type_other;
}

bool File::extMatchesType (const QString &ext)
{
   return type () == extToType (ext);
}

File::e_type File::typeFromName (const QString &fname)
   {
   QFileInfo fi (fname);
   QString ext = fi.suffix ();

   return extToType (ext);
   }


static QStringList env_names = QString ("from,to,subject,notes").split (',');


File::e_env envFromName (const QString &name)
   {
   for (int i = 0; i < File::Env_count; i++)
      if (name == env_names [i])
         return (File::e_env)i;
   return File::Env_count;
   }


QString File::envToName (File::e_env env)
   {
   return env_names [env];
   }


File *File::createFile (const QString &dir, const QString fname, Desk *desk, e_type type)
   {
//    QFileInfo fi (dir, fname);
//    QString ext = fi.suffix ();
   File *f;

   switch (type)
      {
      case Type_max :
         f = new Filemax (dir, fname, desk);
         break;

      case Type_pdf :
         f = new Filepdf (dir, fname, desk);
         break;

      case Type_jpeg :
         f = new Filejpeg (dir, fname, desk);
         break;

      case Type_other :
         f = new Fileother (dir, fname, desk);
         break;

      default :
         err_complain (err_make (ERRFN, ERR_file_type_unsupported1,
                                 qPrintable(typeName (type))));
         f = new Fileother (dir, fname, desk);
      }
   return f;
   }


Filepage *File::createPage (e_type type)
   {
   Filepage *fp;

   switch (type)
      {
      case Type_max :
         fp = new Filemaxpage ();
         break;

      case Type_jpeg :
         fp = new Filejpegpage ();
         break;

      case Type_pdf :
      case Type_other :
      default :
         fp = new Filepage ();
         break;
      }
   return fp;
   }


void File::setPos (QPoint pos)
   {
   /* QT crashes when any position is -ve because its
      int QDynamicListViewBase::itemIndex(const QListViewItem &item) const

      function returns -1 for an index. We then return QModelIndex() for its
      model index and then there is an assertion failure
      */
   if (pos.x () < 0)
      pos.setX (0);
   if (pos.y () < 0)
      pos.setY (0);
   _pos = pos;
   }


QString File::getAnnotName (e_annot type)
   {
   return QString ("Author,Title,Keywords,Notes").section (',', type, type);
   }


void File::setPagenum (int pagenum)
   {
   _pagenum = pagenum;
   }


void File::setPreviewMaxsize (QSize size)
   {
//    qDebug () << "setPreviewMaxsize" << size;
   _preview_maxsize = size;
   }


void File::setTitleMaxsize (QSize size)
   {
   _title_maxsize = size;
   }


void File::setPagenameMaxsize (QSize size)
   {
   _pagename_maxsize = size;
   }


QSize File::previewMaxsize (void)
   {
   return _preview_maxsize;
   }


QSize File::titleMaxsize (void)
   {
   return _title_maxsize;
   }


QSize File::pagenameMaxsize (void)
   {
   return _pagename_maxsize;
   }


void File::decodeFile (QString &line, bool read_sizes)
   {
   QStringList args = line.split (',');
   setPos (QPoint (args [0].toInt (), args [1].toInt ()));

   setPagenum (args [2].toInt ());
   if (args.size () >= 9 && read_sizes)
      {
      /* we can additionally store the pixmap maxsize, and text maxsize */
      setPreviewMaxsize (QSize (args [3].toInt (), args [4].toInt ()));
      setTitleMaxsize (QSize (args [5].toInt (), args [6].toInt ()));
      setPagenameMaxsize (QSize (args [7].toInt (), args [8].toInt ()));
      }
   }


void File::encodeFile (QTextStream &stream)
   {
   stream << _filename << '=' << _pos.x () << ',' << _pos.y () << "," << _pagenum << ",";
   stream << _preview_maxsize.width () << ',' << _preview_maxsize.height () << ',';
   stream << _title_maxsize.width () << ',' << _title_maxsize.height () << ',';
   stream << _pagename_maxsize.width () << ',' << _pagename_maxsize.height ();
   stream << '\n';
   }


QString File::filename (void) const
   {
   return _filename;
   }


QString &File::basename (void)
   {
   return _basename;
   }


QString &File::leaf (void)
{
   return _leaf;
}


QString &File::ext (void)
{
return _ext;
}


bool File::intersects (QRect &base)
   {
   QRect rect (_pos.x (), _pos.y (), base.width (), base.height ());

   return base.intersects (rect);
   }


QPoint &File::pos (void)
   {
   return _pos;
   }


QString File::pathname (void)
   {
   return _pathname;
   }


int File::order (void) const
   {
   return _order;
   }


void File::setOrder (int order)
   {
   _order = order;
   }


void File::setTime (QDateTime dt)
   {
   _timestamp = dt;
   }


QDateTime File::time (void) const
   {
   return _timestamp;
   }


QString File::pageTitle (int pagenum)
   {
   err_info *err;
   QString title;

   if (!_valid)
      return ""; //tr ("<error: no maxdesk>");
   if (pagenum == -1)
      pagenum = _pagenum;
   err = getPageTitle (pagenum, title);
   if (err || title.isEmpty ())
      title = QString (tr ("Page %1")).arg (pagenum + 1);
   return title;
   }


bool File::valid (void)
   {
   return _valid;
   }


void File::setValid (bool valid)
   {
   _valid = valid;
   }


QPixmap File::pixmap (bool)
   {
   return *unknown;
   }


int File::pagenum (void)
   {
   return _pagenum;
   }


err_info *File::err (void)
   {
   return _err;
   }

err_info *File::setErr (err_info *err)
   {
   if (!err)
      _err = err;
   else
      {
      if (!_err)
         _err = &_serr;
      *_err = *err;
      }
   return err;
   }

/*
QString File::getAnnot (e_annot type)
   {
   return tr ("(not available for this file type)");
   }


err_info *File::getPageText (int pagenum, QString &str)
   {
   str = tr ("(not available for this file type)");
   return err_make (ERRFN, ERR_no_available_for_this_file_type);
   }
*/

void File::colour_image_for_blank (QImage &image)
   {
   QRgb *rgb, col;
   int i;

   Q_ASSERT (image.depth () == 32);
   for (i = 0; i < image.height (); i++)
      {
      rgb = (QRgb *)image.scanLine (i);
      for (int x = 0; x < image.width (); x++)
         {
         col = *rgb;
         *rgb = qRed (col) * CONFIG_preview_col_mult + qGreen (col) * CONFIG_preview_col_mult + qBlue (col);
         }
      }
   }


#define BUFF_SIZE 65536

err_info *File::copyFile (QString from, QString to)
   {
   QFile in (from);
   QFile out (to);
   char *buff;
   int len;

   if (!in.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (from));
   if (!out.open (QIODevice::WriteOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (to));
   CALL (mem_alloc (CV &buff, BUFF_SIZE, "Desk::copyFile"));
   while (!in.atEnd ())
      {
      len = in.read (buff, BUFF_SIZE);
      if (len < 0)
         return err_make (ERRFN, ERR_failed_to_read_bytes1, BUFF_SIZE);
      if (out.write (buff, len) != len)
         return err_make (ERRFN, ERR_failed_to_write_bytes1, len);
      }
   mem_free (CV &buff);
   return NULL;
   }


err_info *File::rename (QString &fname, bool auto_rename)
   {
   QString oldname, newname, name, ext;
   int err;

   if (auto_rename && _desk)
      {
      QFileInfo fi (fname);

      ext = fi.suffix ();

      name = _desk->findNextFilename (fi.baseName (), QString(), "." + ext);
      if (name.isNull ())
         return err_make (ERRFN, ERR_no_unique_filename1, qPrintable (fname));
      name += "." + ext;
      }
   else
      name = fname;

   oldname = _dir + _filename;
   newname = _dir + name;
   if (oldname == newname)    // nothing changed?
      return NULL;

//   printf ("rename %s to %s\n", oldname.latin1 (), newname.latin1 ());

   QFile file (newname);

   if (file.exists ())
      return err_make (ERRFN, ERR_file_already_exists1,qPrintable (name));
   err = ::rename (qPrintable (oldname), qPrintable (newname));
   if (err)
      return err_make (ERRFN, ERR_could_not_execute1, "rename");
   updateFilename (name);
   fname = name;
   return NULL;
   }


#if 0
err_info *File::moveToTrash (QString &trashname)
   {
   if (_trash_dir.isEmpty ())
      return err_make (ERRFN, ERR_no_trash_directory_is_defined);

   // move to trash
   return move (_trash_dir, trashname);
   }
#endif


err_info *File::move (QString &newDir, QString &newName, bool copy)
   {
   QString old_fname = _filename;
   QFile file (_pathname);

   // get the full paths for the current file, and the new file it will become
   QString oldPath = _pathname;
   QString newPath = newDir + "/" + _filename;

   // the new name will hopefully be the same as the old
   newName = _filename;

   QDir dir;

   // if the new name exists in the destination directory, rename the file
   if (dir.exists (newPath) && _desk)
      {
      QString fname, ext, uniq;

      // find unique name
      uniq = _desk->findNextFilename (_leaf + "_move", newDir + "/", _ext);

      // get the new path and name
      newPath = newDir + "/" + uniq + _ext;
      newName = uniq + _ext;
      printf ("new name %s\n", qPrintable (newName));
      }

   if (copy)
      return err_subsume (ERRFN, copyFile (oldPath, newPath),
         ERR_could_not_copy_file2, qPrintable (oldPath),
                  qPrintable (newPath));
   if (!dir.rename (oldPath, newPath))
      return err_make (ERRFN, ERR_could_not_rename_file2, qPrintable (oldPath),
                  qPrintable (newPath));
   utilSetGroup(newPath);

   return NULL;
   }


err_info *File::stackItem (File *src)
   {
   if (src->type () != _type)
      return err_make (ERRFN, ERR_cannot_stack_type_onto_type2,
         qPrintable (typeName (src->_type)), qPrintable (typeName (_type)));
   load ();
   src->load ();
   return stackStack (src);
   }


err_info *File::unstackItems (int pagenum, int pagecount, bool remove,
               QString fname, File *&dest, int &itemnum, int seq)
   {
   QString uniq, str;

   load ();

   // work out new name for file
   QString ext;

   if (fname.isEmpty ())
      fname = pageTitle (pagenum);
   else
      fname = removeExtension (fname, ext);
   if (fname.isEmpty ())
      fname = pagecount == 1
         ? QString ("%1_page_%2").arg (_leaf).arg (pagenum + 1)
         : QString ("%1_pages_%2_to_%3").arg (_leaf).arg (pagenum + 1).arg (pagenum + pagecount);
   QString pagename = _desk->findNextFilename (fname);

   if (pagename.isNull ())
      return err_make (ERRFN, ERR_no_unique_filename1,
                       fname.toLatin1 ().constData());

   // get unique filename
   uniq = pagename + typeExt (_type);

   // create a new file of the correct type
   dest = createFile (_dir, uniq, _desk, _type);
   CALL (dest->create ());

   // unstack and remove the pages
   CALL (unstackPages (pagenum, pagecount, remove, dest));

   // add the newly created file to the desk
   itemnum = _desk->newFile (dest, this, seq);
   return NULL;
   }


err_info *File::not_impl (void)
   {
   return err_make (ERRFN, ERR_no_available_for_this_file_type);
   }


err_info *File::duplicateToDesk (Desk *desk, File::e_type type, QString &uniq,
            int odd_even, Operation &op, File *&fnew)
   {
   bool supported;

   // try the filetype-specific function first
   CALL (duplicate (fnew, type, uniq, odd_even, op, supported));
   if (supported)
      return NULL;

   // get file extension
   QString ext = typeExt (type);
   if (ext.isEmpty ())
      ext = _ext;

   // and directory (the same one as this file, or the temp dir)
   QString dir = _dir;
   if (!desk)
      dir = QString ("%1/").arg (P_tmpdir);

   // do a plain file copy if allowed to
   if (type == Type_other)
      {
      // handle a whole-file copy

   //   printf ("without ext '%s' (%s), copy '%s'\n", fname.latin1 (), ext.latin1 (), uniq.latin1 ());

      // copy the file with the new name
      uniq += ext;
      CALL (copyFile (dir + _filename, dir + uniq));

      // create a new max file
      if (desk)
         {
         fnew = desk->createFile (dir, uniq);
         desk->newFile (fnew, this, 1);
         }
      else
         fnew = createFile (dir, uniq, desk, type);
      uniq = dir + uniq;   // return final file
      return NULL;
      }

   // need to convert from the current file to whatever type is requested
   fnew = createFile (dir, uniq + ext, desk, type);
   if (!fnew)
      return not_impl ();
   CALL (fnew->create ());

   err_info *err = copyTo (fnew, odd_even, op);

   // if we got no pages, delete the file
   if (!fnew->valid () || !fnew->pagecount ())
   {
      fnew->remove ();
      delete fnew;
      fnew = 0;
      if (!err)
         return err_make (ERRFN, ERR_cannot_find_any_pages_in_source_document1, qPrintable (_pathname));
   }
   else if (desk)
      desk->newFile (fnew, this, 1);
   uniq = dir + uniq + ext;   // return final file
   return err;
   }


err_info *File::duplicateAny (File::e_type type, int odd_even, Operation &op, File *&fnew)
   {
   QString uniq;

   // find unique name
   QString ext = typeExt (type);
   if (ext.isEmpty ())
      ext = _ext;
   uniq = _desk->findNextFilename (_leaf + "_copy", QString(), ext);

   return duplicateToDesk (_desk, type, uniq, odd_even, op, fnew);
   }


err_info *File::copyTo (File *fnew, int odd_even, Operation &op, bool verbose)
   {
   int pagenum;
   int page_count = pagecount ();

   // we may generate fewer pages than we receive
   int out_page_count = 0;

   for (pagenum = 0; pagenum < page_count; pagenum++)
      {
      if (verbose) {
         printf ("\rpage %d/%d", pagenum + 1, page_count);
         fflush (stdout);
      }

      // are we doing this page?
      int do_this = odd_even & ((pagenum % 2) + 1);
      if (!do_this)
         continue;

      QImage image;
      QSize size, trueSize;
      int bpp;
      Filepage *fp = createPage (fnew->type ());

      CALL (getImage (pagenum, false, image, size, trueSize, bpp, false));

      // if no image, do the next page
      if (image.isNull ())
         continue;

      int image_size;
#if QT_VERSION >= 0x050a00
      image_size = image.sizeInBytes();
#else
      image_size = image.byteCount();
#endif
      QByteArray ba = QByteArray::fromRawData ((const char *)image.bits (),
                                               image_size);

//       int stride = (trueSize.width () * bpp + 7) / 8;
      int stride = image.bytesPerLine ();
//      mp->stride = (true_size.x * (bpp == 24 ? 32 : bpp) + 7) / 8;
      // changed from above line, since duplicating a normal colour max file created an error

      QString name = QString (tr ("Page %1")).arg (out_page_count + 1);
      fp->addData (image.width (), image.height (), image.depth (), stride,
            name, false, false, out_page_count, ba, ba.size ());

      fp->compress ();
      CALL (fnew->addPage (fp, false));
      out_page_count++;
      op.incProgress (1);
      }
   fnew->flush ();
   fnew->load ();
   return NULL;
   }

bool File::decodePageNumber (const QString &fname, QString &base, int &pagenum,
                             QString &ext)
{
   int pos;

   base = removeExtension (fname, ext);
   ext = ext.mid (1);   // remove .
   pos = fname.lastIndexOf (UTIL_PAGE_PREFIX);
   if (pos == -1)
      return false;

   pagenum = base.mid (pos + 2).toInt ();
   if (pagenum < 1 || pagenum > 9999)
      return false;

   base = base.left (pos);
   pagenum--;

   return true;
}

QString File::encodePageNumber (const QString &base, int pagenum)
   {
   pagenum++;
   return QString ("%1" UTIL_PAGE_PREFIX "%2%3").arg (base).arg (pagenum).
         arg (typeExt (type ()));
   }

bool File::claimFileAsNewPage (const QString &, QString &, int)
   {
   return false;
   }

#if 0
   QString old, uniq, path;

   old = _pathname;

   // find unique name
   uniq = findNextFilename (_leaf + "_copy");
   uniq += ".max";
   path = _dir + uniq;
   CALL (convertMax (old, path, false, false, false, odd_even));
   printf ("newf=%s\n", path.latin1 ());
   fnew = createFile (_dir, uniq);
   _desk->newFile (fnew, this, 1);
   return NULL;
#endif


/*************************** Filepage *******************************/

Filepage::Filepage ()
   {
   _timestamp = QDateTime::currentDateTime ();
//    mp->stack = NULL;
//    mp->chunk = NULL;
   _width = _height = _depth = _stride = _size = 0;
   _jpeg = _mark_blank = false;
   _pagenum = -1;
   _stack = 0;
   }


Filepage::~Filepage ()
   {
   }


void Filepage::setPaperstack (Paperstack *stack)
   {
   _stack = stack;
   }


void Filepage::addData (int width, int height, int depth, int stride,
      QString &name, bool jpeg, bool blank, int pagenum, QByteArray data, int size)
   {
   _width = width;
   _height = height;
   _depth = depth;
   _stride = stride;
   _name = name;
   _jpeg = jpeg;
   _mark_blank = blank;
   _pagenum = pagenum;
   _data = data;
   _size = size == -1 ? _data.size () : size;
   }


bool Filepage::markBlank (void) const
   {
   return _mark_blank;
   }


Paperstack *Filepage::stack (void) const
   {
   return _stack;
   }


int Filepage::pagenum (void) const
   {
   return _pagenum;
   }


void Filepage::getImageFromLines (const char *data, int width, int height, int depth,
      int stride, QImage &image, bool restride32, bool blank)
   {
   QImage::Format format;
   int new_stride;
   bool conv24 = false;

   // work out the format
#ifdef USE_24BPP
   if (depth == 24)
      format = QImage::Format_RGB888;
   else
#endif
      {
      format = depth == 1
         ? QImage::Format_Mono : depth == 8
         ? QImage::Format_Indexed8
         : QImage::Format_RGB32;
      if (depth == 24)
         {
         conv24 = true;   // convert 24bpp to 32bpp
         depth = 32;
         }
      }

   // sort out the stride (bytes per line)
   new_stride = (width * depth + 31) / 32 * 4;
//    qDebug () << "depth" << depth << "stride" << stride << "new_stride" << new_stride;
   if ((conv24 || restride32) && stride != new_stride && data)
      {
      // the stride needs to change, so we must do this manually
      image = QImage (width, height, format);
//       qDebug () << "convert stride" << "new" << new_stride << "old" << stride << "qimage" << image.bytesPerLine ();
      const char *p = data;
      for (int line = 0; line < height; line++, p += stride)
         if (conv24)
            {
            const char *in = p;
            char *out = (char *)image.scanLine (line);
            const char *end = out + new_stride;

            while (out < end)
               {
               out [0] = in [2];
               out [1] = in [1];
               out [2] = in [0];
               out [3] = 0xff;
               in += 3; out += 4;
               }
            }
         else
            memcpy (image.scanLine (line), p, new_stride);
      }
   else if (data)
      image = QImage ((const uchar *)data, width, height, stride, format);
   else if (image.width () != width || image.height () != height
      || format != image.format ())
      image = QImage (width, height, format);

//    if (new_stride != stride)

//    qDebug () << "maxdesk image" << image.width () << image.height ()<< image.format ();
   QVector <QRgb> table;
   switch (depth)
      {
      case 1 :
         // create a B&W palette
         table.resize (2);
         table [1] = qRgb (0, 0, 0);
         table [0] = blank ? qRgb (255 * CONFIG_preview_col_mult,
               255 * CONFIG_preview_col_mult, 255) : qRgb (255, 255, 255);
         break;

      case 8 :
         table.resize (256);
         for (int i = 0; i < 256; i++)
            table [i] = blank ? qRgb (i * CONFIG_preview_col_mult,
                  i * CONFIG_preview_col_mult, i) : qRgb (i, i, i);
         break;

      case 24 :
         if (blank)
            File::colour_image_for_blank (image);
         break;
      }
   if (depth <= 8)
      image.setColorTable (table);
   }



void Filepage::getImageFromLines (char *data, int width, int height, int depth,
      int stride, QImage &image, bool restride32, bool blank, bool invert)
   {
   getImageFromLines (data, width, height, depth, stride, image, restride32, blank);
   if (invert)
      image.invertPixels ();
   }


void Filepage::getImage (QImage &image) const
   {
   getImageFromLines ((const char *)_data.data (), _width ,_height, _depth,
      _stride, image, false);
   }


err_info *Filepage::compress ()
   {
   // we don't need to do anything here
   return NULL;
   }


QByteArray Filepage::copyData (bool invert, bool force_24bpp) const
   {
   QImage image;
   int size = _size;
   unsigned char *data = (unsigned char *)_data.data (), *pend, *p, *q;
   int stride = _stride;
   QByteArray conv;

   int stride8 = (_width * _depth + 7) / 8;
   if (force_24bpp && _depth == 32)
      {
      getImage (image);
      stride8 = _width * 3;
#ifdef USE_24BPP
      image = image.convertToFormat (QImage::Format_RGB888);
      data = image.bits ();
      int image_size;
#if QT_VERSION >= 0x050a00
      image_size = image.sizeInBytes();
#else
      image_size = image.byteCount();
#endif
      size = image_size;
      stride = image.bytesPerLine ();
#else
      stride = stride8;
      conv.resize (stride * image.height ());
      char *out = conv.data ();
      for (int line = 0; line < image.height (); line++, p += stride)
         {
         char *in = (char *)image.scanLine (line);
         const char *end = in + image.bytesPerLine ();

         while (in < end)
            {
            out [0] = in [2];
            out [1] = in [1];
            out [2] = in [0];
            out += 3; in += 4;
            }
         }
      data = (unsigned char *)conv.data ();
      size = conv.size ();
#endif
      }
   pend = data + size;
   if (invert) for (p = data; p < pend; p++)
      *p = 255 - *p;
   QByteArray ba = QByteArray ((const char *)data, size);
   if (stride != stride8)
      {
      qDebug () << "restride from" << stride << "to" << stride8;
      p = (unsigned char *)ba.data ();
      q = (unsigned char *)ba.data ();
      for (int line = 0; line < _height; line++, p += stride8, q += stride)
         memcpy (p, q, stride8);
      ba.resize (stride8 * _height);
      }
   return ba;
   }


QImage Filepage::getThumbnail (bool invert) const
   {
   QImage image;

   Q_ASSERT (!invert); // not supported
   getImage (image);
   QSize size = QSize (_width, _height);
   size /= CONFIG_preview_scale;
   QImage thumb = util_smooth_scale_image (image, size);

   //qDebug () << "f1" << image.format () << thumb.format ();
   //qDebug () << "width" << thumb.width () << "bpl" << thumb.bytesPerLine ();
   QPainter p (&thumb);

   // debugging
//    p.fillRect (QRect (10, 10, 20, 20), Qt::gray);
   p.end ();
   if (image.depth () > 8)
#ifdef USE_24BPP
      thumb = thumb.convertToFormat (QImage::Format_RGB888);
#else
      thumb = thumb.convertToFormat (QImage::Format_RGB32);
#endif
   else
      thumb = utilConvertImageToGrey (thumb);
   return thumb;
   }


QByteArray Filepage::getThumbnailRaw (bool invert, QImage &image,
                  bool word_align) const
   {
   image = getThumbnail (invert);
//    qDebug () << image.width () << image.bytesPerLine ();
//    image.save ("/tmp/1.jpg");
   QByteArray ba;
   int image_size;
#if QT_VERSION >= 0x050a00
   image_size = image.sizeInBytes();
#else
   image_size = image.byteCount();
#endif
   ba.reserve(image_size);
   QBuffer buffer(&ba);
   buffer.open(QIODevice::WriteOnly);
   int stride = image.bytesPerLine ();
   int req_stride = image.width () * image.depth () / 8;
   if (word_align || stride == req_stride)
      buffer.write ((const char *)image.bits (), image_size);
   else
      {
      // we must copy line by line
      for (int y = 0; y < image.height (); y++)
         buffer.write ((const char *)image.scanLine (y), req_stride);
      }
   //qDebug () << "thumbnail" << image_size << ba.size ();
   return ba;
   }

