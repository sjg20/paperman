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


#include <QFile>
#include <QPixmap>

#include "fileother.h"



Fileother::Fileother (const QString &dir, const QString &filename, Desk *desk)
   : File (dir, filename, desk, Type_other)
   {
   }


err_info *Fileother::load (void)
   {
   if (!_valid)
      return not_impl ();
   return NULL;
   }


  // was desk->ensureMax

/** create the file (on the filesystem). It doesn't need to be written to
      as we will call flush() later for that */
err_info *Fileother::create (void)
   {
   return not_impl ();
   }




err_info *Fileother::flush (void)
   {
   return not_impl ();
   }




err_info *Fileother::remove (void)
   {
   QFile file (_dir + _filename);

   if (!file.remove ())
      return err_make (ERRFN, ERR_could_not_remove_file2,
                file.name ().latin1 (), file.errorString ().latin1 ());
   return NULL;
   }


// accessing and changing metadata

err_info *Fileother::getPageTitle (int, QString &)
   {
   return not_impl ();
   }


err_info *Fileother::getAnnot (e_annot, QString &text)
   {
   text = "";
   return NULL;
   }



err_info *Fileother::putAnnot (QHash<int, QString> &)
   {
   return not_impl ();
   }


err_info *Fileother::putEnvelope (QStringList &)
   {
   return not_impl ();
   }

err_info *Fileother::getPageText (int, QString &)
   {
   return not_impl ();
   }




/** gets the total size of a file in bytes. this should include data not
      yet flushed to the filesystem */
int Fileother::getSize (void)
   {
   return 0;
   }




err_info *Fileother::renamePage (int, QString &)
   {
   return not_impl ();
   }




err_info *Fileother::getImageInfo (int, QSize &,
      QSize &, int &, int &, int &,
      QDateTime &)
   {
   return not_impl ();
   }




err_info *Fileother::getPreviewInfo (int, QSize &, int &)
   {
   return not_impl ();
   }


// image related
QPixmap Fileother::pixmap (bool)
   {
   return unknownPixmap ();
   }




err_info *Fileother::getPreviewPixmap (int, QPixmap &, bool)
   {
   return not_impl ();
   }




err_info *Fileother::getImage (int, bool,
            QImage &, QSize &, QSize &, int &, bool)
   {
   return not_impl ();
   }






// operations on files

err_info *Fileother::addPage (const Filepage *, bool)
   {
   return not_impl ();
   }




err_info *Fileother::removePages (QBitArray &,
      QByteArray &, int &)
   {
   return not_impl ();
   }




err_info *Fileother::restorePages (QBitArray &,
   QByteArray &, int)
   {
   return not_impl ();
   }




err_info *Fileother::unstackPages (int, int, bool,
            File *)
   {
   return not_impl ();
   }




err_info *Fileother::stackStack (File *)
   {
   return not_impl ();
   }




err_info *Fileother::duplicate (File *&, File::e_type, const QString &,
      int, Operation &, bool &supported)
   {
   supported = false;
   return NULL;
   }


int Fileother::pagecount (void)
   {
   // assume that there is only one page
   return 1;
   }

