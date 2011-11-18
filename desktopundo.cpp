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


#include <QApplication>
#include <QDebug>
#include <QUndoCommand>


#include "desktopmodel.h"
#include "desktopundo.h"

#include "maxview.h"


Desktopundostack::Desktopundostack (QObject *parent)
      : QUndoStack (parent)
   {
   }


Desktopundostack::~Desktopundostack ()
   {
   }


Desktopundocmd::Desktopundocmd (Desktopmodel *model)
      : QUndoCommand ()
   {
   _seq = 1;  // need to global alloc
   _model = model;
   _model->resetAdded ();
   }


Desktopundocmd::~Desktopundocmd ()
   {
   }


void Desktopundocmd::aboutToAdd (QModelIndex parent)
   {
   // we are about to add some items
   _model->aboutToAdd (parent);
   }


void Desktopundocmd::complain (err_info *err)
   {
   err_complain (err);
   }


UCTrashStacks::UCTrashStacks (Desktopmodel *model,
         QModelIndexList &list, QModelIndex &parent, QString dir, bool copy)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (parent);
   _filenames = model->listToFilenames (list);
   _destdir = dir;
   _copy = copy;
   foreach (ind, list)
      if (ind.isValid ())
         _files << model->getPosData (ind);
   setText (QApplication::translate(dir.isEmpty () ? "UCDeleteStacks" : "UCMoveStacksToDir",
      dir.isEmpty () ? "Delete stacks (%1)" : "Move stacks to dir (%1)").arg (list.size ()));
   }


void UCTrashStacks::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);

   _trashlist.clear ();
   complain (_model->opTrashStacks (list, parent, _trashlist, _destdir, _copy));
   }


void UCTrashStacks::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);

   complain (_model->opUntrashStacks (_trashlist, parent, _filenames, &_files, 
         _destdir, _copy));
   }


UCMove::UCMove (Desktopmodel *model, QModelIndexList &list, QModelIndex &parent,
      QList<QPoint> &oldpos, QList<QPoint> &newpos)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (parent);
   _filenames = model->listToFilenames (list);
   _oldpos = oldpos;
   _newpos = newpos;
   setText (QApplication::translate("UCMove",
      "Move stacks (%1)").arg (list.size ()));
   }


void UCMove::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);

   _model->opMoveStacks (list, parent, _newpos);
   }


void UCMove::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);

   _model->opMoveStacks (list, parent, _oldpos);
   }


UCDuplicate::UCDuplicate (Desktopmodel *model, QModelIndexList &list,
      QModelIndex &parent, File::e_type type, int odd_even)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (parent);
   _filenames = model->listToFilenames (list);
   _type = type;
   _odd_even = odd_even;

   QString str = type == File::Type_other
      ? QApplication::translate("UCDuplicate1", "Duplicate stacks %1").arg (list.size ())
      : QApplication::translate("UCDuplicate2", "Duplicate stacks as %1 (%2)")
            .arg (File::typeName (type)).arg (list.size ());
   setText (str);
   }


void UCDuplicate::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);

   _newnames.clear ();
   aboutToAdd (parent);
   complain (_model->opDuplicateStacks (list, parent, _newnames, _type, _odd_even));
   }


void UCDuplicate::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndexList list = _model->listFromFilenames (_newnames, parent);

   complain (_model->opDeleteStacks (list, parent));
   }


UCUnstackStacks::UCUnstackStacks (Desktopmodel *model, QModelIndexList &list,
      QModelIndex &parent)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (parent);
   _filenames = model->listToFilenames (list);
   setText (QApplication::translate("UCUnstackStacks",
      "Unstack stacks (%1)").arg (list.size ()));
   }


void UCUnstackStacks::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);

   _newnames.clear ();
   aboutToAdd (parent);
   complain (_model->opUnstackStacks (list, parent, _newnames));
   }


void UCUnstackStacks::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QStringList fname_list;
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);
   QList<QModelIndexList> pages;
   QString fname;

   foreach (fname_list, _newnames)
      {
      QModelIndexList plist;

      foreach (fname, fname_list)
         {
         QModelIndex ind = _model->index (fname, parent);
         plist << ind;
         }
      pages << plist;
      }

   /* restack to page 1, since we know that the destination stack has
      only 1 page */
   int pagenum = 1;
   complain (_model->opRestackStacks (list, parent, pages, pagenum));
   }


UCUnstackPage::UCUnstackPage (Desktopmodel *model, QModelIndex &ind,
      int pagenum, bool remove)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _filename = model->data (ind, Desktopmodel::Role_filename).toString ();
   _pagenum = pagenum;
   _remove = remove;
   setText (QApplication::translate("UCUnstackPage",
      "Unstack page"));
   }


void UCUnstackPage::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex index = _model->index (_filename, parent);

   aboutToAdd (parent);
   complain (_model->opUnstackPage (index, _pagenum, _remove, _newname));
   }


void UCUnstackPage::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex index = _model->index (_filename, parent);
   QModelIndex page = _model->index (_newname, parent);
   QModelIndexList list;

   list << page;

   // if we can't find something, skip
   if (index == QModelIndex () || page == QModelIndex ())
      return;

   // if we removed the page, then to undo we must stack it back
   if (_remove)
      complain (_model->opStackStacks (index, list, _pagenum));

   /* but if we just copied the page, then the stack can be left alone, and
      we just need to delete the page */
   else
      complain (_model->opDeleteStacks (list, parent));
   }


UCDeletePages::UCDeletePages (Desktopmodel *model, QModelIndex &ind, QBitArray &pages)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _filename = model->data (ind, Desktopmodel::Role_filename).toString ();
   int pagecount = model->data (ind, Desktopmodel::Role_pagecount).toInt ();
   _pages = pages;
   _count = 0;
   for (int i = 0; i < pagecount; i++)
      if (pages.testBit (i))
         _count++;
   setText (QApplication::translate("UCDeletePages",
      "Delete pages"));
   }


void UCDeletePages::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex index = _model->index (_filename, parent);

   complain (_model->opDeletePages (index, _pages, _del_info, _count));
   }


void UCDeletePages::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex index = _model->index (_filename, parent);

   complain (_model->opUndeletePages (index, _pages, _del_info, _count));
   }


UCStackStacks::UCStackStacks (Desktopmodel *model, QModelIndex &dest,
      QModelIndexList &list, int pagenum, QList<QPoint> *poslist)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (dest.parent ());
   _destname = model->data (dest, Desktopmodel::Role_filename).toString ();
   _filenames = model->listToFilenames (list);
   _pagenum = pagenum;
   int upto = 0;
   foreach (ind, list)
      {
      pagepos_info pp = model->getPosData (ind);

      // override the position if required
      if (poslist)
         pp.pos = (*poslist) [upto++];
      _pagepos << pp;
      qDebug () << "position" << pp.pos;
      }
   setText (QApplication::translate("UCStackStacks",
      "Stack pages (%1)").arg (list.size ()));
   }


void UCStackStacks::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex dest = _model->index (_destname, parent);
   QModelIndexList list = _model->listFromFilenames (_filenames, parent);

   complain (_model->opStackStacks (dest, list, _pagenum));
   }


void UCStackStacks::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex dest = _model->index (_destname, parent);

   complain (_model->opUnstackFromStack (dest, _filenames, _pagenum, _pagepos));
   }


UCChangeDir::UCChangeDir (Desktopmodel *model, QString dirPath, QString rootPath,
         QString oldPath, QString oldRoot)
      : Desktopundocmd (model)
   {
   _oldpath = oldPath;
   _newpath = dirPath;
   _oldroot = oldRoot;
   _newroot = rootPath;
   setText (QApplication::translate("UCChangeDir",
      "Change Directory"));
   }


void UCChangeDir::redo (void)
   {
   _model->opChangeDir (_newpath, _newroot);
   }


void UCChangeDir::undo (void)
   {
   _model->opChangeDir (_oldpath, _oldroot);
   }


UCRenameStack::UCRenameStack (Desktopmodel *model, const QModelIndex &ind, QString newname)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _oldname = model->data (ind, Desktopmodel::Role_filename).toString ();
   _newname = newname;
   setText (QApplication::translate("UCRenameStack",
      "Rename stack"));
   }


void UCRenameStack::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex ind = _model->index (_oldname, parent);

   _model->opRenameStack (ind, _newname);
   }


void UCRenameStack::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex ind = _model->index (_newname, parent);

   _model->opRenameStack (ind, _oldname);
   }


UCRenamePage::UCRenamePage (Desktopmodel *model, const QModelIndex &ind,
      QString newname)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _fname = model->data (ind, Desktopmodel::Role_filename).toString ();
   _pagenum = model->data (ind, Desktopmodel::Role_pagenum).toInt ();
   _oldname = model->data (ind, Desktopmodel::Role_pagename).toString ();
   _newname = newname;
   setText (QApplication::translate("UCRenamePage",
      "Rename page"));
   }


void UCRenamePage::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex ind = _model->index (_fname, parent);

   _model->opRenamePage (ind, _pagenum, _newname);
   }


void UCRenamePage::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex ind = _model->index (_fname, parent);

   _model->opRenamePage (ind, _pagenum, _oldname);
   }


UCUpdateAnnot::UCUpdateAnnot (Desktopmodel *model, const QModelIndex &ind,
      QHash<int, QString> &updates)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _fname = model->data (ind, Desktopmodel::Role_filename).toString ();
   _new = updates;
   QHashIterator<int, QString> i (updates);
   while (i.hasNext())
      {
      i.next();
      _old [i.key ()] = model->getAnnot (ind, (File::e_annot)i.key ());
//       qDebug () << "old" << i.key () << model->getAnnot (ind, i.key ());
      }
   setText (QApplication::translate("UCUpdateAnnot",
      "Update stack summary"));
   }


void UCUpdateAnnot::redo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex ind = _model->index (_fname, parent);

   err_complain (_model->opUpdateAnnot (ind, _new));
   }


void UCUpdateAnnot::undo (void)
   {
   QModelIndex parent = _model->deskFromDirname (_dir);
   QModelIndex ind = _model->index (_fname, parent);

   err_complain (_model->opUpdateAnnot (ind, _old));
   }


UCAddRepository::UCAddRepository (Desktopmodel *model, QString dirPath)
      : Desktopundocmd (model)
   {
   _dirpath = dirPath;
   setText (QApplication::translate("UCAddRepository", "Add Repository"));
   }


void UCAddRepository::redo (void)
   {
   _model->opUpdateRepositoryList (_dirpath, true);
   }


void UCAddRepository::undo (void)
   {
   _model->opUpdateRepositoryList (_dirpath, false);
   }


UCRemoveRepository::UCRemoveRepository (Desktopmodel *model, QString dirPath)
      : Desktopundocmd (model)
   {
   _dirpath = dirPath;
   setText (QApplication::translate("UCRemoveRepository", "Remove Repository"));
   }


void UCRemoveRepository::redo (void)
   {
   _model->opUpdateRepositoryList (_dirpath, false);
   }


void UCRemoveRepository::undo (void)
   {
   _model->opUpdateRepositoryList (_dirpath, true);
   }
