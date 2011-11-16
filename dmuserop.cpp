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
   File:       dmuserop.cpp
   Started:    26/6/09

   This file contains user operations (which can be undone) relating to Desktopmodel.
   Each of these operations is invoked from the user interface. Each is implemented
   by the Desktopundo file, which in turn calls the opXXX functions in dmop.cpp
*/


#include "desktopmodel.h"
#include "desktopundo.h"
#include "file.h"


err_info *Desktopmodel::stackItems (QModelIndex dest, QModelIndexList &list,
         QList<QPoint> *poslist)
   {
   _modelconv->assertIsSource (0, &dest, &list);
   if (checkScanStack (list, dest.parent ()))
      _undo->push (new UCStackStacks (this, dest, list, -1, poslist));
   return NULL;
   }


err_info *Desktopmodel::unstackStacks (QModelIndexList &list, QModelIndex parent)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      _undo->push (new UCUnstackStacks (this, list, parent));
   return NULL;
   }


err_info *Desktopmodel::duplicate (QModelIndexList &list, QModelIndex parent)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      _undo->push (new UCDuplicate (this, list, parent, File::Type_other));
   return NULL;
   }


void Desktopmodel::duplicateMax (QModelIndexList &list, QModelIndex parent, int odd_even)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      _undo->push (new UCDuplicate (this, list, parent, File::Type_max, odd_even));
   }


void Desktopmodel::duplicatePdf (QModelIndexList &list, QModelIndex parent)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      _undo->push (new UCDuplicate (this, list, parent, File::Type_pdf));
   }


void Desktopmodel::duplicateJpeg (QModelIndexList &list, QModelIndex parent)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      _undo->push (new UCDuplicate (this, list, parent, File::Type_jpeg));
   }


void Desktopmodel::trashStacks (QModelIndexList &list, QModelIndex parent)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      _undo->push (new UCTrashStacks (this, list, parent, "", false));
   }


void Desktopmodel::trashStack (QModelIndex &ind)
   {
   QModelIndexList list;

   list << ind;
   trashStacks (list, ind.parent ());
   }


void Desktopmodel::move (QModelIndexList &list, QModelIndex parent,
      QList<QPoint> &oldplist, QList<QPoint> &plist)
   {
//    int i;

//    qDebug () << "move items" << list.size ();
//    for (i = 0; i < list.size (); i++)
//       qDebug () << "row" << list [i].row () << "oldpos" << oldplist [i] << "newpos" << plist [i];
   _modelconv->assertIsSource (0, &parent, &list);
   _undo->push (new UCMove (this, list, parent, oldplist, plist));
   }


void Desktopmodel::unstackPage (QModelIndex &ind, int pagenum, bool remove)
   {
   QModelIndexList list;

   _modelconv->assertIsSource (ind.model (), &ind, 0);
   list << ind;
   if (checkScanStack (list, ind.parent ()))
      _undo->push (new UCUnstackPage (this, ind, pagenum, remove));
   }


void Desktopmodel::updateAnnot (QModelIndex &ind, QHash<int, QString> &updates)
   {
   _modelconv->assertIsSource (ind.model (), &ind, 0);

   _undo->push (new UCUpdateAnnot (this, ind, updates));
   }


void Desktopmodel::deletePages (QModelIndex &ind, QBitArray &pages)
   {
   QModelIndexList list;

   _modelconv->assertIsSource (ind.model (), &ind, 0);
   list << ind;
   if (checkScanStack (list, ind.parent ()))
      _undo->push (new UCDeletePages (this, ind, pages));
   }


void Desktopmodel::moveToDir (QModelIndexList &list, QModelIndex parent, QString &dir,
         QStringList &trashlist, bool copy)
   {
   _modelconv->assertIsSource (0, &parent, &list);
   if (checkScanStack (list, parent))
      {
      UCTrashStacks *op = new UCTrashStacks (this, list, parent, dir, copy);

      _undo->push (op);
      trashlist = op->trashlist ();
      }
   }


void Desktopmodel::changeDir (QString dirPath, QString rootPath,
         bool allow_undo)
   {
   if (allow_undo)
      _undo->push (new UCChangeDir (this, dirPath, rootPath, _dirPath, _rootPath));
   else
      opChangeDir (dirPath, rootPath);
   }


void Desktopmodel::renameStack (const QModelIndex &index, QString newname)
   {
   _modelconv->assertIsSource (0, &index, 0);
   File *f = getFile (index);

   newname += f->ext ();
   if (f->filename () != newname)
      _undo->push (new UCRenameStack (this, index, newname));
   }


void Desktopmodel::renamePage (const QModelIndex &index, QString newname)
   {
   _modelconv->assertIsSource (0, &index, 0);
   File *f = getFile (index);

   if (f && f->pageTitle (-1) != newname)
      _undo->push (new UCRenamePage (this, index, newname));
   }


