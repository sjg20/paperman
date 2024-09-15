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
   read the maxdesk.ini file
*/

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tiffio.h"

#include <QBitArray>
#include <QDateTime>
#include <QDebug>
#include <QLinkedList>

#include "qdir.h"
#include "qfile.h"
#include "qfileinfo.h"
#include "qimage.h"
#include "qmessagebox.h"
#include "qpainter.h"
#include "qpixmap.h"
#include "qstring.h"

#include "desk.h"
#include "file.h"
#include "filemax.h"
#include "fileother.h"
#include "filepdf.h"
#include "op.h"
#include "paperstack.h"
#include "utils.h"

extern "C" {
   #define HAVE_STDINT_H 1
   #include "md5.h"
   };

#include "err.h"
#include "mem.h"


#define DESK_FNAME ".paperdesk"



enum
   {
   POS_leftmargin  = 25,   //!< left-most position for new files
   POS_topmargin   = 35,   //!< top-most position for new files
   POS_xstep       = 140,  //!< distance between files across
   POS_ystep       = 250,  //!< distance between files down
   POS_xoffset     = 8,    //!< x offset between bunched files
   POS_yoffset     = 8,    //!< y offset between bunched files
   POS_bunch_count = 16,   //!< number of files per bunch
   POS_bunch_xstep = (POS_xoffset * (POS_bunch_count + 4))    //!< x distance between bunch
   };


Desk::Desk (void)
   {
   setup ();
   }


Desk::Desk(const QString &dirPath, const QString &trashPath, bool do_readDesk)
   {
   //qDebug () << "new Desk" << dirPath << -90 % 360;
   setup ();
   _do_writeDesk = true;
   _dir = dirPath;
   _rootDir = trashPath;
   if (!_dir.isEmpty ()) {
      if (_dir.right (1) != "/")
         _dir += "/";
   }

   if (!trashPath.isEmpty ())
      {
      QDir dir;
      _trash_dir = trashPath + ".maxview-trash";
      if (!dir.exists (_trash_dir))
         dir.mkdir (_trash_dir);
      }

   // read in any maxdesk file
   if (do_readDesk)
      readDesk(true);
   updateRowCount ();
   }


Desk::~Desk ()
   {
   if (_dir != QString() && _do_writeDesk && !writeDesk ())
      printf ("couldn't write maxdesk.ini\n");
   while (!_files.isEmpty ())
      delete _files.takeFirst ();
   }


void Desk::setup ()
   {
   resetPos ();
   _files.clear ();
   _rightMargin = 950;
   _debug_level = 0;
   _debug = 0;
   _do_writeDesk = false;
   _allow_dispose = true;
   _fbase_seq = 0;
   _row_count = 0;      // our record of the last valid row count
   _dirty = false;
   }


void Desk::clear (void)
   {
   _files.clear ();
   }


File *Desk::getFile (int itemnum)
   {
   return _files [itemnum];
   }


QString &Desk::dir (void)
   {
   return _dir;
   }


QString Desk::trashdir (void)
   {
   return _trash_dir;
   }


int Desk::rowCount (void)
   {
   return _row_count;
   }


void Desk::dirty (void)
   {
   if (!_dirty)
      {
//       qDebug () << "dirty";
      _dirty = true;
      }
   }


void Desk::updateRowCount (void)
   {
   if (_row_count != _files.size ())
      {
      _row_count = _files.size ();
      dirty ();
      }
   }

void Desk::addFiles(const QString &dirPath)
   {
   QDir dir (dirPath);
   dir.setFilter(QDir::Files | QDir::NoSymLinks);
   dir.setSorting(QDir::Name);

   const QFileInfoList list = dir.entryInfoList();
   if (!list.size ())
      {
      printf ("dir not found\n");
      return;
      }

   for (int i = 0; i < list.size(); i++)
      {
      QFileInfo fi = list.at(i);

      addFile(fi.fileName(), dirPath);
      }
   updateRowCount ();
   }

void Desk::addMatches(const QString &dirPath, const QStringList& matches)
{
      // don't write back the maxdesk.ini file
   _do_writeDesk = false;

   foreach (const QString& relname, matches) {
      QString pathname = dirPath + relname;
      QFileInfo fi(pathname);
      QDir dir(fi.dir());
      QString fname = fi.fileName();
      addFile(fi.fileName(), dir.path() + "/");
   }
   updateRowCount();
}

void Desk::resetPos (void)
   {
   _pos = QPoint (POS_leftmargin, POS_topmargin);
   }


/*
void Desk::disposeFile (file_info *f)
   {
   if (f->max)
      max_close (f->max);
   if (f->pixmap)
      delete f->pixmap;
   delete f;
   }
*/

/*
err_info *Desk::rescanFile (file_info *f)
   {
   if (f->max)
      {
      max_close (f->max);
      f->max = NULL;
      }
   f->err = NULL;
   return ensureMax (f);
   }
*/

/*
file_info *Desk::alloc_file (void)
   {
   file_info *f;

   f = new file_info;
   f->pagenum = f->pagecount = f->size;
   f->pixmap = 0;
   f->max = 0;
   f->err = NULL;
   return f;
   }
*/


File *Desk::createFile (const QString &dir, const QString fname)
   {
   File::e_type type;

   type = File::typeFromName (fname);
#if 0
   QFileInfo fi (dir, fname);
   QString ext = fi.suffix ();
   File *f;
   File::e_type type;

   //FIXME: there should be an associate config for doing this
   if (ext == "max")
      type = File::Type_max;
   else if (ext == "pdf")
      type = File::Type_pdf;
   else
      type = File::Type_other;
#endif

   return File::createFile (dir, fname, this, type);
   }


bool Desk::addToExistingFile (QString &fname)
{
   QString base, ext;
   int pagenum;

   if (!File::decodePageNumber (fname, base, pagenum, ext))
      return false;

   foreach (File *f, _files)
      {
      if (!f->extMatchesType (ext))
         continue;
      if (f->claimFileAsNewPage (fname, base, pagenum))
         return true;
      }

   return false;
}

void Desk::readDesk (bool read_sizes)
   {
   QFile oldfile;
   QFile file (_dir + DESK_FNAME);
   QString fname;

   // if there is a maxdesk.ini file, rename it to the official name
   fname = _dir + "/maxdesk.ini";
   oldfile.setFileName (fname);
   if (!oldfile.exists ())
      {
      fname = _dir + "/MaxDesk.ini";
      oldfile.setFileName (fname);
      }
   if (oldfile.exists ())
      {
      // if we have the official file, just remove this one
      if (file.exists ())
         oldfile.remove ();
      else
         oldfile.rename (_dir + DESK_FNAME);
      }

   QTextStream stream (&file);
   QString line;
   bool files = false;
   File *f;
   int pos;

   if (file.open (QIODevice::ReadOnly)) while (!stream.atEnd())
      {
      line = stream.readLine(); // line of text excluding '\n'

//      printf( "%s\n", line.latin1() );
      if (!line.isEmpty() && line [0] == '[')
         {
         files = line == "[Files]";
         continue;
         }

      // add a new file
      pos = line.indexOf ('=');
      if (files && pos != -1)
         {
         QString fname = line.left (pos);

         QFile test (_dir + fname);
         if (!test.exists ())
            continue;

         if (!addToExistingFile (fname))
            {
            f = createFile (_dir, fname);
            line = line.mid (pos + 1);
            f->decodeFile (line, read_sizes);
            _files << f;
            }
         }
      }

   // advance the position past these
   advance ();
   }


bool Desk::writeDesk (void)
   {
   QFile file;
   QString fname;

   // remove any old file
   fname = _dir + "/maxdesk.ini";
   file.setFileName (fname);
   if (file.exists ())
      file.remove ();
   fname = _dir + "/MaxDesk.ini";
   file.setFileName (fname);
   if (file.exists ())
      file.remove ();
   file.setFileName (_dir + DESK_FNAME);
   if (file.exists ())
      file.remove ();

   QTextStream stream( &file );
   QString line;

   if (!file.open (QIODevice::WriteOnly))
      return false;

   // write header
   stream << "[DesktopFile]" << '\n' ;
   stream << "File=" << '\n';
   stream << '\n';
   stream << "[Folder]" << '\n';
   stream << "Version=0x00060000" << '\n';
   stream << '\n';

   stream << "[Files]" << '\n';

   // output the file list
   foreach (File *f, _files)
      f->encodeFile (stream);

   _dirty = false; // we are clean again
   return true;
   }


void Desk::flush (void)
   {
   if (_dirty)
      writeDesk ();
   }


File *Desk::takeAt (int row)
   {
   dirty ();
   return _files.takeAt (row);
   }


File *Desk::findFile (QString fileName)
   {
   QString base, ext;
   int pagenum;

   File::decodePageNumber (fileName, base, pagenum, ext);

   foreach (File *f, _files)
      {
      if (f->filename () == fileName)
         return f;
      if (!f->extMatchesType (ext))
         continue;
      if (f->claimFileAsNewPage (fileName, base, pagenum))
         return f;
      }
   return 0;
   }


File *Desk::findFile (QString fileName, int &pos)
   {
   pos = 0;
   foreach (File *f, _files)
      {
      if (f->filename () == fileName)
         return f;
      pos++;
      }
   return 0;
   }


void Desk::addFile(const QString& fname, const QString& dir)
   {
   File *f;

   // if we already know about this file, ignore it
   f = findFile(fname);
   if (f)
      return;

   // if not a .max file, ignore for the moment
//   if (file->fileName ().right (4) != ".max")
//      return;

   // ignore maxdesk.ini and ppthumbs.ptn as these are special files
   if (fname.indexOf("maxdesk.ini", 0, Qt::CaseInsensitive) != -1
       || fname.indexOf("paperportsave.reg", 0,
                                    Qt::CaseInsensitive) != -1
       || fname.indexOf("ppthumbs.ptn", 0, Qt::CaseInsensitive) != -1)
      return;

   // if not then find a good position for it, and add it
//   printf ("not found %s\n", file->fileName ().latin1 ());
   f = createFile(dir, fname);
   f->setPos (_pos);
   _files << f;
   dirty ();

   // advance pos
   advanceOne ();
   }


bool Desk::clashes (QPoint &pos)
   {
   File *f;
   QRect base (pos.x (), pos.y (), POS_xstep, POS_ystep);

   foreach (f, _files)
      if (f->intersects (base))
         return true;
   return false;
   }


void Desk::advanceOne (void)
   {
   _pos.setX (_pos.x () + POS_xstep);
   if (_pos.x () >= _rightMargin)
      {
      _pos.setX (POS_leftmargin);
      _pos.setY (_pos.y () + POS_ystep);
      }
   }


void Desk::advance (void)
   {
   // start at top
   _pos = QPoint (POS_leftmargin, POS_topmargin);

   File *f;

   // put the pos level with the bottom most item
   // also find the rightmost item
   foreach (f, _files)
      {
      if (f->pos ().y () >= _pos.y ())
         {
         if (f->pos ().y () > _pos.y ())
            {
            _pos.setY (f->pos ().y ());
            _pos.setX (POS_leftmargin);
            }
         while (_pos.y () == f->pos ().y () && _pos.x () < f->pos ().x ())
            advanceOne ();
         }
      }

   // now keep advancing pos until it doesn't clash with an existing item
   for (int i = 0; i < 1000 && clashes (_pos); i++)
      advanceOne ();

/*
   _pos.setX (_pos.x () + POS_xstep);
   if (_pos.x () >= _rightMargin)
      {
      _pos.setX (0);
      _pos.setY (_pos.y () + POS_ystep);
      }
*/
   }

void Desk::refresh(void)
{
   removeRows(0, _row_count);
   readDesk();
   addFiles(_dir);
}

//template int QPtrList<file_info>::compareItems ( QPtrCollection::Item item1, QPtrCollection::Item item2 )

//FIXME: should port this to QT4

class myPtrList : public QList<File *>
   {
public:
     myPtrList () {}
     ~myPtrList () {}
     void setType (int ty) {_type = ty; }
     static bool lessThan (const File *f1, const File *f2);
     void sort ();

   private:
     static int _type;
   };


int myPtrList::_type;

bool myPtrList::lessThan (const File *f1, const File *f2)
   {
   switch (_type)
      {
      case 0: // pos
     return f1->order () < f2->order ();

      case 1 : // name
     return f1->filename () < f2->filename ();

      case 2 : // date
     return f1->time () < f2->time ();
      }
   return 0;
   }

void myPtrList::sort ()
   {
   std::stable_sort (begin (), end (), lessThan);
   }

void Desk::arrangeBy (int type)
   {
   std::list<File *> todo;
   myPtrList sorted;
   int order;

   // fill in the date & field
   File *f;

   /* FIXME: this sorts by the position of the top left of the object, which is
      not the same as the top left of the image */
   foreach (f, _files)
      {
      QFileInfo fi (f->pathname ());

      f->setTime (fi.lastModified ());
      todo.push_back (f);
      }

   // now the position info
   order = 0;

   /*
    * Create a list of files in the current order. This works by starting with
    * the top left position, and adding any files which are at or 'before' that
    * position. Then we move to the next position along and add files before
    * that. This proceeds until we have added all files to our list.
    */
   resetPos ();
    std::list<File *>::iterator iter = todo.begin();
   while (todo.size ())
      {
      // First add any files which are before the current x and y position
      iter = todo.begin ();
      while (iter != todo.end())
         {
         f = *iter++;
         if (_pos.y () >= f->pos ().y () && _pos.x () > f->pos ().x ())
            {
            f->setOrder (order++);
            iter = todo.erase (iter);
            }
         }

      // Now add any files which are before the current y position
      iter = todo.begin ();
      while (iter != todo.end())
         {
          f = *iter++;
         if (_pos.y () >= f->pos ().y ())
            {
            f->setOrder (order++);
            iter = todo.erase (iter);
            }
         }

      // Skip to the next position
      advanceOne ();
      }

   // we should now have an order for each item
   foreach (f, _files)
      sorted.append (f);

   /* sort by the given key */
   sorted.setType (type);
   sorted.sort ();

   // add them all at the right place
   resetPos ();
   for (int i = 0; i < sorted.size (); i++)
      {
      f = sorted [i];
      f->setPos (_pos);
      advanceOne ();
      }
   dirty ();

   }


QString Desk::findNextFilename (QString fname, QString dir, QString ext)
   {
   if (dir.isNull ())
      dir = _dir;
   if (ext.isNull ())
      ext = ".max";
   return util_findNextFilename (fname, dir, ext);
   }


int Desk::fileCount (void)
   {
   return _files.size ();
   }


#define PREVIEW_X 150


enum
   {
   TYPE_none,
   TYPE_pbm,
   TYPE_ppm,
   TYPE_jpeg,
   TYPE_png
   };


err_info *Desk::checksum()
   {
   QString fname = "checksums.md5";
   QFile file (fname);
   QTextStream stream(&file);
   File *f;
   int pagenum;
   unsigned md5 [4];
   char line [200];

   if (!file.open (QIODevice::WriteOnly))
      return err_make(ERRFN, ERR_cannot_open_file1, qPrintable(fname));

   QDir dir(_dir);
   if (!dir.exists())
      return err_make("checksum", ERR_directory_not_found1, qPrintable(_dir));
   qInfo() << "Checksumming" << _files.count() << "files to" << fname;
   int i = 1;
   QDebug deb = qDebug();

   int total_pages = 0;
   foreach (f, _files) {
//      if (f->type() != File::Type_max)
//         continue;

      printf("\r%d ", i++);
      fflush(stdout);
      err_info *err = f->load();
      if (err) {
         printf("File '%s': %s\n", qPrintable(f->filename()), err->errstr);
         continue;
      }

      // decode each page one by one
      for (pagenum = 0; pagenum < f->pagecount(); pagenum++) {
         QImage image;
         QSize size, trueSize;
         int bpp;
         err_info *err;

         err = f->getImage(pagenum, false, image, size, trueSize, bpp, false);
         if (err) {
            printf("Error: %s\n", qPrintable(err->errstr));
            break;
         }
         int image_size;
#if QT_VERSION >= 0x050a00
         image_size = image.sizeInBytes();
#else
         image_size = image.byteCount();
#endif
         md5_buffer((const char *)image.bits(), image_size, md5);
         sprintf(line, "%d %s %d %d %x %x %x %x\n", f->filename().length(),
                 qPrintable(f->filename()), pagenum, image_size,
                 md5 [0], md5 [1], md5 [2], md5 [3]);
         stream << line;
         total_pages++;
      }
   }

   printf("\r");
   fflush(stdout);
   qInfo() << total_pages << "pages processed";

   return NULL;
   }

int Desk::newFile (File *fnew, File *fbase, int seq)
   {
   // if positioning relative to an earlier file, work out position
   QPoint pos;
   if (fbase && seq != -1)
      {
      QPoint offset = QPoint (POS_xoffset, POS_yoffset);

      if (seq != _fbase_seq)
         {
         _fbase_pos = fbase->pos () + (offset * seq);
         _fbase_seq = seq;
         }

      // use current position
      pos = _fbase_pos;

      // update position for next item
      _fbase_seq++;
      _fbase_pos += offset;
      if ((seq % POS_bunch_count) == 0)
         {
         _fbase_pos += QPoint (-POS_xoffset * POS_bunch_count + POS_bunch_xstep,
               -POS_yoffset * POS_bunch_count);
         if (_fbase_pos.x () > _rightMargin)
            {
            _fbase_pos.setX (POS_leftmargin);
            _fbase_pos.setY (_pos.y () + POS_ystep);
            }
         }
      }

   // otherwise just position at the end
   else
      {
      pos = _pos;

      // advance pos
      advanceOne ();
//       qDebug () << "item positioned at" << pos << ", pos advanced to" << _pos;
      }

   fnew->setPos (pos);
   _files << fnew;
   dirty ();
   return _files.size () - 1; // item number we added it as
   }


err_info *Desk::deleteFromTrash (QString &trashname)
   {
   if (_trash_dir.isEmpty ())
      return err_make (ERRFN, ERR_no_trash_directory_is_defined);

   return deleteFromDir (_trash_dir, trashname);
   }


err_info *Desk::deleteFromDir (QString &src, QString &trashname)
   {
   QString path = src + trashname;
   QFile file (path);

   if (!file.remove ())
      return err_make (ERRFN, ERR_could_not_remove_file1,
                path.toLatin1 ().constData ());
   return NULL;
   }


err_info *Desk::moveFromTrash (QString &trashname, QString &filename,
      File *&fnew)
   {
   if (_trash_dir.isEmpty ())
      return err_make (ERRFN, ERR_no_trash_directory_is_defined);

   return moveFromDir (_trash_dir, trashname, filename, fnew);
   }


err_info *Desk::moveFromDir (QString &, QString &trashname, QString &filename,
      File *&fnew)
   {
   QString ext;

   // remove .max from filename
   QString fname = removeExtension (filename, ext);

   // find unique name
   QString uniq = findNextFilename (fname, QString(), ext);

   // move the file with the new name
   filename = uniq + ext;
   QDir dir;
   QString from = trashname;
   QString to = _dir + filename;
   if (!dir.rename (from, to))
      return err_make (ERRFN, ERR_could_not_rename_file2, qPrintable (from),
            qPrintable (to));
   fnew = createFile (_dir, filename);
   newFile (fnew);
   return NULL;
   }


err_info *Desk::addPaperstack (const QString &stack_name, File *&fnew, int &item)
   {
   QString fname, path, name;

//   memtest ("1");

   name = stack_name;
   if (name.isEmpty ())
      {
      QDateTime dt = QDateTime::currentDateTime ();

      // use today's date
      name = dt.toString ("dd-MMM-yy");
      }
//   memtest ("2");
//   qDebug () << "name" << name;

   // work out the filename to use
   fname = findNextFilename (name);
   if (fname.isNull ())
      return err_make (ERRFN, ERR_no_unique_filename1,
                stack_name.toLatin1 ().constData ());
//   memtest ("3");

   // compress the pages from the stack into a file
   fname += ".max";
//   stack->compressMax (_dir + fname);
   fnew = createFile (_dir, fname);
//   qDebug () << "addPaperStack" << fnew;
   CALL (fnew->create ());

   // recalculate where to put the scan
   advance ();

   item = newFile (fnew);
   return NULL;
   }


#if 0
err_info *Desk::viewFile (file_info *f)
   {
   int err;

   err = err_systemf ("%s %s &", max_get_type (f->max) == FILET_pdf
             ? "acroread" : "kview",
             max_get_shell_filename (f->max));
   return NULL;
   }
#endif


#if 0

err_info *Desk::convert_to_jpeg (max_info *max, QString fname, int pagenum,
				    Operation *op)
   {
   err_info *err;
   int bpp;
   QImage *image;
   QSize size, trueSize;

   err = NULL;

   // get the image
   err = get_image_qimage (max, pagenum, false, &image, size, trueSize, bpp);
   if (!err)
      {
      // write out as a jpeg file
      if (!image->save (fname, "JPEG"))
         err = err_make (ERRFN, ERR_could_not_write_image_to_as2, "png");

      // delete the image
      // if pdf, this will also remove image->bits()
      delete image;
      }
   return err;
   }


err_info *Desk::convertJpeg (QString &old)
   {
   QString fname, ext;
   max_info *max;
   err_info *err;

   // remove .max from filename
   fname = removeExtension (old, ext);
   if (ext != ".max")
      {
      printf ("skipping non-max file '%s'\n", old.latin1 ());
      return NULL;
      }

   fname += ".jpg";
   printf ("%s -> %s\n", old.latin1 (), fname.latin1 ());
   CALL (max_open (old.latin1 (), &max));
   err = convert_to_jpeg (max, fname, 0, 0);
   max_close (max);
   return err;
   }


err_info *Desk::duplicateTiff (file_info *f, file_info **fnewp,
      Operation &op, QString &newname)
   {
   QString uniq, fname, ext;
   max_info *max;
   int page_count, pagenum;
   QImage *image;
   QSize size, trueSize;
   max_page_info mp;
   int bpp;

   char name [30];
   int type;
   err_info *err;

   CALL (ensureMax (f));

   // remove .max from filename
   fname = removeExtension (f->filename, ext);

   // find unique name
   uniq = findNextFilename (fname + "_copy", QString(), ".tiff");
   printf ("without ext '%s' (%s), copy '%s'\n", fname.latin1 (), ext.latin1 (), uniq.latin1 ());

   // create the file
   uniq += ".tiff";
   CALL (max_create (_dir + uniq, &max));
   newname = uniq;

   // add each of the pages
   page_count = max_get_pagecount (f->max);
   type = max_get_type (f->max);
   err = NULL;
   for (pagenum = 0; !err && pagenum < page_count; pagenum++)
      {
      if (op.setProgress (pagenum)) // if cancelled, stop
         {
         max_close (max);
         QFile file (_dir + uniq);

         file.remove ();
         return err_make (ERRFN, ERR_operation_cancelled1, "duplicate as .max");
         }

      // get the image
      err = getImageQImage (f, pagenum, false, &image, size, trueSize, bpp);
      if (err)
         break;

      // compress to max
      max_init_max_page (&mp);
      mp.width = image->width ();
      mp.height = image->height ();
      mp.depth = image->depth ();
      mp.stride = image->bytesPerLine ();
      sprintf (name, "Page %d", pagenum + 1);
      mp.titlestr = name;
      err = max_compress_page (&mp, image->bits (), image->numBytes ());
      if (!err)
         err = max_add_page (max, &mp, false);

      // delete the image
      // if pdf, this will also remove image->bits()
      delete image;

      // tell max to de-allocate the page
      max_unload (max, pagenum);
      }
   if (!err)
      err = max_flush (max);
   err = err_copy (err);
   CALL (newFile (uniq, max, max_get_pagecount (max), fnewp));
   return err;
   }

#endif //p


#if 0 //p

err_info *Desk::test1 (QString &fname)
   {
   max_info *max;
   cpoint size, true_size;
   int bpp;
   byte *im;
   max_page_info smp, *mp = &smp;

   // open a file and decompress the first page image
   CALL (max_open (fname.latin1 (), &max));

   QString ext;
   QString newname = removeExtension (fname, ext);

   newname += ".info";
   FILE *f = fopen (newname.latin1 (), "w");
   if (!f)
      {
      printf ("cannot open file '%s\n", newname.latin1 ());
      exit (1);
      }
   show_file (max, f);
   fclose (f);
   CALL (max_get_image (max, 0, &size, &true_size, &bpp, &im, 0));
   printf ("decoded %s: %dx%dx%d true %dx%d\n", fname.latin1 (),
       size.x, size.y, bpp, true_size.x, true_size.y);

   // show the file
//   show_file (max);

   // now compress it again
   max_init_max_page (mp);
   mp->width = size.x;
   mp->height = size.y;
   mp->depth = bpp;
   mp->stride = (true_size.x + 7) / 8;
   max_compress_page (mp, im, mp->height * mp->stride);
   mp->titlestr = "test title";

   QString name = "test/out.max";

   CALL (max_write (name.latin1 (), mp, 1));

   max_close (max);

   CALL (max_open (name.latin1 (), &max));
   newname = removeExtension (name, ext);
   newname += ".info";
   f = fopen (newname.latin1 (), "w");
   if (!f)
      {
      printf ("cannot open file '%s\n", newname.latin1 ());
      exit (1);
      }
   show_file (max, f);
   fclose (f);
   max_close (max);

   return NULL;
   }


static void dump (const char *name, byte *start, int size, byte *other)
   {
   int i;

   printf ("%5s:", name);

   for (i = 0; i < size; i++)
      printf ("%c%02x", start [i] == other [i] ? ' ' : '.', start [i]);
   printf ("\n");
   }


/* open a .max file and compare it to the .tiff file */

err_info *Desk::test_compare_with_tiff (QString &fname)
   {
   max_info *max;
   cpoint size, true_size;
   int bpp, pagenum;
   byte *im;
   TIFF *tif;
	unsigned imagelength;
	byte *buf, *ptr;
	unsigned row;
	int stride, im_size, tif_size;
   int config, nstrips;
   int imageWidth, imageLength, tileWidth, tileLength;

   CALL (max_open (fname.latin1 (), &max));

   QString ext;
   QString newname = removeExtension (fname, ext);

   newname += ".tif";

   printf ("name=%s\n", newname.latin1 ());

   tif = TIFFOpen (newname.latin1 (), "r");

   // compare each page
   for (pagenum = 0; pagenum < max_get_pagecount (max); pagenum++)
      {
      printf ("page %d / %d: ", pagenum, max_get_pagecount (max));

	   if (pagenum && !TIFFReadDirectory(tif))
	      {
	      printf ("ran out of pages\n");
	      exit (1);
	      }   err_info *test_decomp_comp (QString &fname);


//      printf ("\nTIFF\n");
    	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength);
      TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);
   	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imageWidth);
	   TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imageLength);
//   	TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
//   	TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);

      tileWidth = tileLength = 0;

      nstrips = TIFFNumberOfStrips(tif);
//      printf ("strips=%d, image=%dx%d, tile=%dx%d: %d\n", nstrips, imageWidth, imageLength,
//            tileWidth, tileLength, TIFFNumberOfTiles(tif));

    	stride = ((TIFFScanlineSize(tif) + 3) & ~3);;
//      stride = imageWidth / 8;
    	tif_size = stride * imagelength;
     	buf = (byte *)malloc (tif_size);
     	printf ("   stride=%d, ", stride);
   	for (row = 0, ptr = buf; row < imagelength; row++, ptr += stride)
	      if (TIFFReadScanline(tif, ptr, row) != 1)
	         {
	         printf ("\nreadscanline error\n");
	         break;
	         }
      CALL (max_get_image (max, pagenum, &size, &true_size, &bpp, &im, &im_size));

   	for (row = 0, ptr = buf; row < imagelength; row++, ptr += stride)
   	   if (0 != memcmp (ptr, im + (ptr - buf), stride))
   	      {
   	      printf ("\n- error line %d\n", row);
   	      dump ("tif", ptr, stride, im + (ptr - buf));
   	      dump ("max", im + (ptr - buf), stride, ptr);
   	      }

	   free (buf);
	   printf ("    lines=%d/%d, %d %d\n", imagelength, row, im_size, tif_size);
	   }
   TIFFClose (tif);
   max_close (max);

   return NULL;
   }


static int compare_32bpp (byte *im, byte *im2, int im_size, cpoint *image_size)
   {
   int total, i, diff;
   int max = 0, count = 0;
   int x, y;
   double error;

   for (i = total = 0; i < im_size; i++)
      {
      if ((i & 3) == 3)
	 continue;
      diff = im [i] - im2 [i];
      if (diff < 0)
         diff = -diff;
      total += diff;
      if (diff > max)
         max = diff;
      if (diff > 30)
	 {
 	 im2 [(i & ~3) + 0] = 0;
 	 im2 [(i & ~3) + 1] = 255;
 	 im2 [(i & ~3) + 2] = 0;
 	 count++;
	 y = i / 4 / image_size->x;
	 x = i / 4 % image_size->x;
	 //         printf ("%d,%d: %d\n", x, y, diff);
	 }
      //      if (!(i & 63))
      // 	 printf ("\n");
      }
   error = (double)total / im_size;
   printf ("\ncompare: total=%d, max=%d, error=%1.5lf, count=%d\n", total, max,
          error, count);
   return error < 3;
   }


err_info *Desk::test_decomp_comp (QString &fname)
   {
   max_info *max, *tmax;
   byte *im, *im2;
   int im_size, im_size2;
   cpoint size, true_size;
   int bpp, pagenum;
   max_page_info *max_page, *mp;
   FILE *outf;  //, *compf;
   debug_info *debug, *debug2, *debugc;
   int num_pages, page_end;

//      dump_tables ();
   CALL (max_open (fname.latin1 (), &max));

   // find 8bpp images
   CALL (max_get_image (max, 0, &size, &true_size, &bpp, &im, &im_size));
   if (bpp == 8)
      printf ("\r%-70s\n", fname.latin1 ());
   else
      printf ("\r%-70s", fname.latin1 ());
   max_close (max);
   return NULL;


#define STEPS  100000
#define FIRST  0
#define NUM    INT_MAX
#define PAGE_START  0
#define PAGE_END   INT_MAX
#define LEVEL 0
//#define COMPF

   outf = fopen ("1", "w");
   assert (outf);

   // debug level, max steps, first tile, numtiles
   debug = max_new_debug (outf, LEVEL, STEPS, FIRST, NUM);
#ifdef COMPF
   compf = fopen ("1.comp", "wb");
   assert (outf);
   max_set_debug_compf (debug, compf);
#endif
   max_set_debug (max, debug);

//debugc = max_new_debug (stdout, 2  /*LEVEL*/, STEPS, FIRST, NUM);
   debugc = max_new_debug (stdout, LEVEL, STEPS, FIRST, NUM);

   num_pages = max_get_pagecount (max);
   page_end = num_pages - 1;
   max_page = (max_page_info *)malloc (sizeof (max_page_info) * num_pages);

   if (page_end > PAGE_END)
      page_end = PAGE_END;
   for (mp = max_page, pagenum = PAGE_START; pagenum <= page_end; pagenum++, mp++)
      {
      printf ("\rread %d/%d", pagenum, num_pages); fflush (stdout);
      CALL (max_get_image (max, pagenum, &size, &true_size, &bpp, &im, &im_size));
/*
   stride = true_size.x / 8;
   printf ("stride=%d, size.y=%d\n", stride, true_size.y);
	for (row = 0, ptr = im; row < 8; row++, ptr += stride)
	   {
	   sprintf (str, "%db
", row);
      dump (str, ptr, stride, ptr);
      }
*/
      max_init_max_page (mp);
      mp->width = size.x;
      mp->height = size.y;
      mp->depth = bpp;
      mp->stride = (true_size.x * (bpp == 24 ? 32 : bpp) + 7) / 8;

      mp->debug = debugc;
      mp->titlestr = "test title";

      printf ("    \rwrite %d/%d", pagenum, num_pages); fflush (stdout);
      max_compress_page (mp, im, mp->height * mp->stride);
      max_free_image (max, pagenum);
      }

   fclose (outf);

   QString name = "test/out.max";
   printf ("    \rbuild"); fflush (stdout);
   CALL (max_write (name.latin1 (), max_page, page_end - PAGE_START + 1));

   CALL (max_open (name.latin1 (), &tmax));

   // debug level, max steps, first tile, numtiles
   outf = fopen ("2", "w");
   assert (outf);
   debug2 = max_new_debug (outf, LEVEL, STEPS, 0, INT_MAX);
   max_set_debug (tmax, debug2);
#ifdef COMPF
   compf = fopen ("2.comp", "wb");
   assert (outf);
   max_set_debug_compf (debug2, compf);
#endif

   for (pagenum = PAGE_START; pagenum <= page_end; pagenum++)
      {
      printf ("       \rread old %d/%d", pagenum, num_pages); fflush (stdout);
      CALL (max_get_image (tmax, pagenum, &size, &true_size, &bpp, &im, &im_size));
      printf ("       \rread new %d/%d", pagenum, num_pages); fflush (stdout);
      CALL (max_get_image (max, pagenum, &size, &true_size, &bpp, &im2, &im_size2));
      if (im_size != im_size2 || 0 != memcmp (im, im2, im_size))
	 {
         if (bpp == 24)
	    {
	    if (!compare_32bpp (im, im2, im_size, &size))
	       printf ("\rpage %d differs      \n", pagenum);
	    mp = max_page;
	    max_init_max_page (mp);
	    mp->width = size.x;
	    mp->height = size.y;
	    mp->depth = bpp;
	    mp->stride = (true_size.x * (bpp == 24 ? 32 : bpp) + 7) / 8;

	    mp->debug = debugc;
	    mp->titlestr = "test title";

	    max_compress_page (mp, im2, mp->height * mp->stride);
	    QString name = "test/out2.max";
	    CALL (max_write (name.latin1 (), mp, 1));
	    }
	 else
	    printf ("\rpage %d differs      \n", pagenum);
	 }
      max_free_image (max, pagenum);
      max_free_image (tmax, pagenum);
      }

   fclose (outf);

   max_close (tmax);
   max_close (max);

   printf ("\r                     \r"); fflush (stdout);
   return NULL;
   }


#endif //p


#if 0//p

err_info *Desk::convertMax (QString &old, QString newf, bool verbose,
			       bool force, bool reloc, int odd_even)
   {
   QString fname, ext;
   max_info *max = NULL;
   int err;
   max_page_info *mp, *max_page;
   int page_count, pagenum;
   char tmp [256], *term;
   cpoint size, true_size;
   int bpp, im_size;
   byte *im;
   QImage *image = 0, grey;
   char pagestr [20];
   QRgb *table = new QRgb [256];
   char shellname [256];
   err_info *e;

   // create a greyscale palette
   for (int i = 0; i < 256; i++)
      table [i] = qRgb (i, i, i);

   // remove .pdf from filename
   fname = removeExtension (old, ext);
   if (newf == QString())
      {
      if (ext == ".max")
	 fname += ".new.max";
      else
	 fname += ".max";
      }
   else
      fname = newf;
   printf ("convert to %s\n", fname.latin1 ());

   QFile file (fname);

   if (file.exists (fname))
      {
      if (verbose)
         printf ("%s already exists - %s\n", fname.latin1 (),
            force ? "overwriting" : "skipping");
      if (!force)
         return NULL;
      }
   if (verbose)
      printf ("%s -> %s\n", old.latin1 (), fname.latin1 ());
   if (ext == ".pdf" || ext == ".max")
      {
      if (verbose)
         printf ("\rscanning..."); fflush (stdout);

      // use our own routines - convert may ignore the resolution
      CALL (max_open (old, &max));
      if (!max)
         {
         printf ("skipping  unreadable pdf file '%s'\n", old.latin1 ());
         return NULL;
         }
      page_count = max_get_pagecount (max);
      }

   // if not pdf, it is safe to just use convert to get the images
   else
      {
      if (verbose)
         printf ("\rconverting..."); fflush (stdout);

      CALL (get_tmp (tmp));
      strcat (tmp, ".png");

      // this will either product a single .png file
      // or multiple .png.0 .png.1 ... files
      err_fix_filename (old.latin1 (), shellname);
      err = err_systemf ("convert %s %s", shellname, tmp);
      if (err < 0)
         return err_make (ERRFN, ERR_could_not_execute1, "pdfimages");
      QFile f (tmp);
      if (f.exists ())
         page_count = 1;
      else
         {
         QFile f2;
         int i;

         term = tmp + strlen (tmp);

         for (i = 0; i < 9999; i++)
            {
            sprintf (term, ".%d", i);
            f2.setName (tmp);
            if (!f2.exists ())
               break;
            }
         page_count = i;
         if (page_count < 2)
            return err_make (ERRFN, ERR_could_not_read_multipage_images1, tmp);
         }
      }

   // now we know the page count
   CALL (mem_allocz (CV &max_page, sizeof (*max_page) * page_count,
           "convertMax"));

   if (verbose)
      printf ("\r%-40s\r", ""); fflush (stdout);

   // we may generate fewer pages than we receive
   int out_page_count = 0;

   for (pagenum = 0, mp = max_page; pagenum < page_count; pagenum++)
      {
      if (verbose)
         printf ("\rpage %d/%d", pagenum + 1, page_count); fflush (stdout);

      // are we doing this page?
      int do_this = odd_even & ((pagenum % 2) + 1);

      image = 0;

      // get the page image
      if (max && max_get_type (max) == FILET_max)
         {
         QSize Size, trueSize;

         if (do_this)
            CALL (get_image_qimage (max, pagenum, false, &image, Size,
                        trueSize, bpp));
         }
      else if (max)
      {
         if (do_this)
            {
            e = get_pdf_image (max, pagenum, false, false, &image, bpp);
            if (e) return e;

            // this may produce a colour image when a grey one would do
            // only way I think is to check the image
            if (bpp == 32 && image->allGray ())
               {
               grey = image->convertDepthWithPalette (8, table, 256);
               delete image;
               image = &grey;
               if (!image)
                  return err_make (ERRFN,
                             ERR_could_not_convert_image_to_greyscale1, tmp);
               bpp = image->depth ();
               }
            }
         }
      else
         {
         sprintf (term, ".%d", pagenum);
         if (do_this)
            {
            image = new QImage (tmp);
            bpp = image->depth ();
            }
         unlink (tmp);
         }

      // if no image, do the next page
      if (!image)
         continue;

      im = image->bits ();
      im_size = image->numBytes ();
      size.x = image->width ();
      size.y = image->height ();
      true_size = size;

//      image->save ("test.jpg", "JPEG");
      max_init_max_page (mp);
      mp->width = size.x;
      mp->height = size.y;
      mp->depth = bpp; //bpp == 32 ? 24 : bpp;
//      mp->stride = (true_size.x * (bpp == 24 ? 32 : bpp) + 7) / 8;
      // changed from above line, since duplicating a normal colour max file created an error
      mp->stride = (true_size.x * bpp + 7) / 8;

      sprintf (pagestr, "Page %d", out_page_count + 1);
      mp->titlestr = strdup (pagestr);

      max_compress_page (mp, im, mp->height * mp->stride);
      if (image != &grey)
         delete image;
      out_page_count++;
      mp++;
      }
   if (verbose)
      printf ("\r%-40s\r", ""); fflush (stdout);

   CALL (max_write (fname.latin1 (), max_page, out_page_count));

   if (max)
      max_close (max);

   if (reloc)
      {
      QDir dir (old);
      QString str, leaf;
      int pos = old.findRev ('/');
      bool ok;

      leaf = old.mid (pos);
      dir.cdUp ();
      //      printf ("cdup: %d\n", dir.cdUp ());
      //      printf ("path %s, leaf %s\n", dir.path ().latin1 (), leaf.latin1 ());
      ok = dir.mkdir ("old.xxx");
      str = dir.path () + "/old.xxx" + leaf;

      QDir dirr;
      ok = dirr.rename (old, str);
      //      printf ("rename %s %s: %d\n", old.latin1 (), str.latin1 (), ok);
      if (ok && verbose)
	 printf ("   (moved old file to old.xxx");
      if (ext == ".max")
	 {
	 fname = removeExtension (old, ext);

	 ok = dirr.rename (fname + ".new.max", fname + ".max");
	 //	 printf ("rename %s%s %s%s: %d\n", fname.latin1 (), ".new.max",
	 //		 fname.latin1 (), ".max", ok);
	 if (ok && verbose)
	    printf (", replaced old file");
	 }
      if (verbose)
	 {
	 if (ok)
	    printf (")\n");
	 else
	    printf (" ** relocation failed)\n");
	 }
      }
   return NULL;
   }


err_info *Desk::dumpInfo (QString &fname, int debug_level)
   {
   max_info *max;
   debug_info *debug;
//    FILE *f;

   //   f = fopen ("1", "w");
   //   assert (f);

   CALL (max_new (&max));
   debug = max_new_debug (stdout, debug_level, INT_MAX, 0, INT_MAX);
   max_set_debug (max, debug);
   CALL (max_open_file (max, fname.latin1 ()));
   CALL (show_file (max, stdout));
   max_close (max);
   //   fclose (f);
   return NULL;
   }


err_info *Desk::do_convert (QImage *image, QImage &newimage,
			     operation_t type, int ival)
   {
   char tmp [256];
   char out [256];
   char f1 [256], f2 [256];
   int err;

   CALL (get_tmp (tmp));
   strcpy (out, tmp);
   strcat (tmp, "1.png");
   strcat (out, "2.png");
   if (!image->save (tmp, "PNG"))
     return err_make (ERRFN, ERR_cannot_open_file1, tmp);

   // fix filenames
   err_fix_filename (tmp, f1);
   err_fix_filename (out, f2);

   err = err_systemf ("convert -rotate %d %s %s", ival, f1, f2);
   if (err)
      return err_make (ERRFN, ERR_could_not_execute1, "convert");

   if (!newimage.load (out))
     return err_make (ERRFN, ERR_cannot_open_file1, out);
   return NULL;
   }


err_info *Desk::operation (file_info *f, int pagenum, operation_t type,
			      int ival)
   {
   QSize size, trueSize;
   max_info *max = f->max;
   QImage *image, newimage;
   int bpp;
   int page;
   err_info *err;

   printf ("%s page %d: operation=%d, ival=%d\n", f->filename.latin1 (),
	   pagenum, type, ival);

   // work out starting page
   page = pagenum == -1 ? 0 : pagenum;

   for (; page == pagenum || (pagenum == -1 && page < f->pagecount); page++)
      {
      err = get_image_qimage (max, pagenum, false, &image, size, trueSize,
                  bpp);
      CALL (err);
      switch (type)
	 {
	 case op_hflip : newimage = image->mirror (true, false); break;
	 case op_vflip : newimage = image->mirror (false, true); break;
	 default : CALL (do_convert (image, newimage, type, ival));
	 }

      // save the image back
      err = max_update_page_image (max, pagenum, newimage.width (),
           newimage.height (), newimage.depth (), newimage.bytesPerLine (),
	   newimage.bits (), newimage.numBytes ());
      delete image;
      if (err)
         break;


      }
   return err;
   }

#endif


#if 0
file_info *Desk::find_file (filelist_info &fl, QString &fname)
   {
   std::vector<file_info>::iterator it;
   file_info *f;

   if (fl.last >= 0 && fl.last < (int)fl.file.size () - 1)
      {
      fl.last++;
      if (fl.file [fl.last].filename == fname)
	 return &fl.file [fl.last];
      }

   // check if the file is already present
   for (f = 0, it = fl.file.begin (); it != fl.file.end (); it++, f = 0)
      {
      f = &*it;
   //      printf ("name %s\n", f->filename.latin1 ());
      if (f->filename == fname)
         return f;
      }
   return 0;
   }


err_info *Desk::scan_file (QString dir_name, QFileInfo *fi,
                  filelist_info &old, filelist_info &file)
   {
   QString fname = dir_name + "/" + fi->fileName ();
   file_info *of, newf;

   // check for this file in the old list
   of = find_file (old, fname);
   if (of && (unsigned)of->time != fi->lastModified ().toTime_t ())
      of = 0;

   // if no match, create a new entry
   if (!of)
      {
      of = &newf;
      of->filename = fname;
      of->pos = QPoint (0, 0);

      // get this info from the maxdesk file in the directory
      std::list<file_info *>::iterator it;
      file_info *f;

      // check if the file is already present
      for (f = 0, it = _file.begin (); it != _file.end (); it++, f = 0)
         {
         f = *it;
   //      printf ("name %s\n", f->filename.latin1 ());
         if (f->filename == fi->fileName ())
            break;
         }

      of->pagenum = 0;
      of->pagecount = 0;
      of->preview_maxsize = QSize (0, 0);
      if (f)
         {
         of->pagenum = f->pagenum;
         of->pagecount = f->pagecount;
         of->preview_maxsize = f->preview_maxsize;
         }
      else
         {
         // could not find in maxdesk file, so read the file to check it
         max_info *max;
         cpoint size;

         CALL (max_open (fname.latin1 (), &max));
         of->pagecount = max_get_pagecount (f->max);
//p          max_get_preview_maxsize (f->max, &size);
         of->preview_maxsize = QSize (size.x, size.y);
         max_close (max);
         }

      of->max = 0;
      of->pixmap = 0;
      of->time = fi->lastModified ().toTime_t ();
      of->order = -1;
      of->ipos = QPoint (0, 0);
      }

   file.file.push_back (*of);
   return NULL;
   }


/* scan a directory and all subdirectories for files and create a list of them
in 'file'. 'old' contains the previous list, which will have current
information for files which have not changed. Note that errors are just
ignored */

err_info *Desk::scan_dir (QString dir_name, filelist_info &old,
                     filelist_info &file)
   {
   QDir dir (dir_name);
   err_info *err;

   dir.setFilter (QDir::NoSymLinks);
   dir.setSorting (QDir::Name);

   const QFileInfoList list = dir.entryInfoList();
   if (list.size ())
      return err_make (ERRFN, ERR_directory_not_found1, dir_name.latin1 ());

   // read in the Desk.ini file in case we need it
   _dir = dir_name;
   _file.clear ();
   readDesk ();

   // do dirs first
   for (int i = 0; i < list.size (); i++)
      {
      QFileInfo fi = list.at (i);
//      printf ("%s\n", fi->fileName ().latin1 ());
      if (fi.isDir ())
         err = scan_dir (dir_name + "/" + fi.fileName (), old, file);
      }

   // now files
   for (int i = 0; i < list.size (); i++)
      {
      QFileInfo fi = list.at (i);
//      printf ("%s\n", fi->fileName ().latin1 ());
      if (!fi.isDir ())
         err = scan_file (dir_name, &fi, old, file);
      }

   return NULL;
   }


/** build an index file for the all the files in the given directory (and all
subdirectories) */

err_info *Desk::buildIndex (QString fname, QString dir)
   {
   char tmp [256];
   int err;

   // read the existing index
   filelist_info old, fl;

   old.last = -1;

// scan all files
   scan_dir (dir, old, fl);

#ifdef Q_WS_X11
   // create a new temp file in /tmp
   CALL (get_tmp (tmp));

   QFile file (tmp);
#else
   QFile file (fname);
#endif

   Q3TextStream stream (&file);
   QString line;

   if (!file.open (QIODevice::WriteOnly))
      return err_make (ERRFN, ERR_could_not_make_temporary_file);

   // write header
   stream << "[IndexFile]" << '\n';

   stream << "Version=1" << '\n';
   stream << '\n';

   std::vector<file_info>::iterator it;
   file_info *f;

   // check if the file is already present
   for (it = fl.file.begin (); it != fl.file.end (); it++)
      {
      f = &*it;
      stream
         << f->filename
         << '='
         << f->pagenum
         << f->pagecount
         << f->preview_maxsize.width ()
         << f->preview_maxsize.height ()
         << f->time
         << '\n';
      }

   file.close ();
#ifdef Q_WS_X11
   // atomic update
   err = err_systemf ("mv %s %s", tmp, fname.latin1 ());
   if (err != 0)
      return err_make (ERRFN, ERR_could_not_execute1, "mv");
#endif
   return NULL;
   }
#endif


void Desk::setDebugLevel (int level)
   {
   _debug_level = level;
   }


#if 0
file_info *Desk::findNextFile (file_info *old)
   {
   std::list<file_info *>::iterator it;
   file_info *f;
   bool found = false;

   // check if the file is already present
   for (f = 0, it = _file.begin (); it != _file.end (); it++, f = 0)
      {
      f = *it;
      if (found)
         break;
   //      printf ("name %s\n", f->filename.latin1 ());
      if (f == old)
         found = true;
      }
   if (!f)
       f = *_file.begin ();
   return f;
   }


file_info *Desk::findPrevFile (file_info *old)
   {
   std::list<file_info *>::iterator it;
   file_info *f;
   bool found = false;

   // check if the file is already present
   for (f = 0, it = _file.end (), it--; it != _file.begin (); it--, f = 0)
      {
      f = *it;
      if (found)
         break;
   //      printf ("name %s\n", f->filename.latin1 ());
      if (f == old)
         found = true;
      }
   if (!f)
      {
      f = found
         ? _file.front ()
         : _file.back ();
      }
   return f;
   }
#endif


#if 0 //p

void Desk::testInc (QString in, QString out)
   {
   QString test;

   test = in;
   incrementFilename (test);
   if (test != out)
      printf ("testInc failed: %s gave '%s', should be '%s'\n", in.latin1 (), test.latin1 (),
            out.latin1 ());
   }


void Desk::runTests (void)
   {
   testInc ("1", "2");
   testInc ("2007", "2007_1");
   testInc ("test07", "test07_1");
   testInc ("test07_9", "test07_10");
   testInc ("test07_10", "test07_11");
   }
#endif


void Desk::setAllowDispose (bool allow)
   {
   _allow_dispose = allow;
   }


bool Desk::getAllowDispose ()
   {
   return _allow_dispose;
   }


void Desk::removeRows (int row, int count)
   {
   for (int i = 0; i < count; i++)
      delete _files.takeAt (row);
   }
