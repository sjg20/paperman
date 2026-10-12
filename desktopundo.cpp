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
#include "mainwidget.h"
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

void Desktopundocmd::complain(err_info *err)
{
   Mainwidget::singleton()->complain(err);
}


void Desktopundocmd::deleteMade (const QString &dir,
                                 std::shared_ptr<QStringList> names)
{
   Desktopmodel *model = _model;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndexList list = model->listFromFilenames (*names, parent);

      model->opDeleteStacks (list, parent, done);
   });
}


void Desktopundocmd::queue (const QString &dir,
                            std::function<void (Desktopmodel::OpDone)> op)
{
   _model->queueOp (op, [] (err_info *err) {
      Mainwidget::singleton ()->complain (err);
   }, _model->isRemoteDir (dir));
}

UCTrashStacks::UCTrashStacks (Desktopmodel *model,
         QModelIndexList &list, QModelIndex &parent, QString dir, bool copy)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (parent);
   _filenames = std::make_shared<QStringList> (model->listToFilenames (list));
   _trashlist = std::make_shared<QStringList> ();
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
   Desktopmodel *model = _model;
   QString dir = _dir;
   auto filenames = _filenames;
   auto trashlist = _trashlist;
   QString destdir = _destdir;
   bool copy = _copy;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndexList list = model->listFromFilenames (*filenames, parent);

      trashlist->clear ();
      model->opTrashStacks (list, parent, trashlist, destdir, copy, done);
   });
   }


void UCTrashStacks::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   auto filenames = _filenames;
   auto trashlist = _trashlist;
   QList<pagepos_info> files = _files;
   QString destdir = _destdir;
   bool copy = _copy;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);

      model->opUntrashStacks (*trashlist, parent, filenames, files, true,
                              destdir, copy, done);
   });
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
   _newnames = std::make_shared<QStringList> ();
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
   Desktopmodel *model = _model;
   QString dir = _dir;
   QStringList filenames = _filenames;
   auto newnames = _newnames;
   File::e_type type = _type;
   int odd_even = _odd_even;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndexList list = model->listFromFilenames (filenames, parent);

      newnames->clear ();
      model->aboutToAdd (parent);
      model->opDuplicateStacks (list, parent, newnames, type, odd_even, done);
   });
   }


void UCDuplicate::undo (void)
   {
   deleteMade (_dir, _newnames);
   }


UCUnfoldBooklet::UCUnfoldBooklet (Desktopmodel *model, QModelIndexList &list,
      QModelIndex &parent)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (parent);
   _filenames = model->listToFilenames (list);
   _newnames = std::make_shared<QStringList> ();
   setText (QApplication::translate ("UCUnfoldBooklet",
      "Unfold booklet (%1)").arg (list.size ()));
   }


void UCUnfoldBooklet::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QStringList filenames = _filenames;
   auto newnames = _newnames;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndexList list = model->listFromFilenames (filenames, parent);

      newnames->clear ();
      model->aboutToAdd (parent);
      model->opUnfoldBooklets (list, parent, newnames, done);
   });
   }


void UCUnfoldBooklet::undo (void)
   {
   deleteMade (_dir, _newnames);
   }


UCUnstackStacks::UCUnstackStacks (Desktopmodel *model, QModelIndexList &list,
      QModelIndex &parent)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (parent);
   _filenames = model->listToFilenames (list);
   _newnames = std::make_shared<QList<QStringList> > ();
   setText (QApplication::translate("UCUnstackStacks",
      "Unstack stacks (%1)").arg (list.size ()));
   }


void UCUnstackStacks::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QStringList filenames = _filenames;
   auto newnames = _newnames;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndexList list = model->listFromFilenames (filenames, parent);

      newnames->clear ();
      model->aboutToAdd (parent);
      model->opUnstackStacks (list, parent, newnames, done);
   });
   }


void UCUnstackStacks::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QStringList filenames = _filenames;
   auto newnames = _newnames;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndexList list = model->listFromFilenames (filenames, parent);
      QList<QModelIndexList> pages;

      for (const QStringList &fname_list : *newnames)
         {
         QModelIndexList plist;

         for (const QString &fname : fname_list)
            plist << model->index (fname, parent);
         pages << plist;
         }

      /* restack to page 1, since we know that the destination stack has
         only 1 page */
      model->opRestackStacks (list, parent, pages,
                              std::make_shared<int> (1), done);
   });
   }


UCUnstackPage::UCUnstackPage (Desktopmodel *model, QModelIndex &ind,
      int pagenum, bool remove)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _filename = model->data (ind, Desktopmodel::Role_filename).toString ();
   _pagenum = std::make_shared<int> (pagenum);
   _newname = std::make_shared<QString> ();
   _remove = remove;
   setText (QApplication::translate("UCUnstackPage",
      "Unstack page"));
   }


void UCUnstackPage::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _filename;
   auto pagenum = _pagenum;
   auto newname = _newname;
   bool remove = _remove;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndex ind = model->index (fname, parent);

      model->aboutToAdd (parent);
      model->opUnstackPage (ind, pagenum, remove, newname, done);
   });
   }


void UCUnstackPage::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _filename;
   auto pagenum = _pagenum;
   auto newname = _newname;
   bool remove = _remove;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndex ind = model->index (fname, parent);
      QModelIndex page = model->index (*newname, parent);

      // if we can't find something, skip
      if (ind == QModelIndex () || page == QModelIndex ())
         {
         done (NULL);
         return;
         }

      /* if we removed the page, then to undo we must stack it back; but if
         we just copied the page, then the stack can be left alone, and
         we just need to delete the page */
      if (remove)
         model->opRestackStacks (QModelIndexList () << ind, parent,
                                 QList<QModelIndexList> ()
                                    << (QModelIndexList () << page),
                                 std::make_shared<int> (*pagenum), done);
      else
         model->opDeleteStacks (QModelIndexList () << page, parent, done);
   });
   }


UCDeletePages::UCDeletePages (Desktopmodel *model, QModelIndex &ind, QBitArray &pages)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _filename = model->data (ind, Desktopmodel::Role_filename).toString ();
   int pagecount = model->data (ind, Desktopmodel::Role_pagecount).toInt ();
   _pages = pages;
   _del_info = std::make_shared<QByteArray> ();
   _count = std::make_shared<int> (0);
   for (int i = 0; i < pagecount; i++)
      if (pages.testBit (i))
         (*_count)++;
   setText (QApplication::translate("UCDeletePages",
      "Delete pages"));
   }


void UCDeletePages::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _filename;
   QBitArray pages = _pages;
   auto del_info = _del_info;
   auto count = _count;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opDeletePages (ind, pages, del_info, count, done);
   });
   }


void UCDeletePages::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _filename;
   QBitArray pages = _pages;
   auto del_info = _del_info;
   auto count = _count;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opUndeletePages (ind, pages, del_info, *count, done);
   });
   }


UCStackStacks::UCStackStacks (Desktopmodel *model, QModelIndex &dest,
      QModelIndexList &list, int pagenum, QList<QPoint> *poslist)
      : Desktopundocmd (model)
   {
   QModelIndex ind;

   _dir = model->deskToDirname (dest.parent ());
   _destname = model->data (dest, Desktopmodel::Role_filename).toString ();
   _filenames = model->listToFilenames (list);
   _pagenum = std::make_shared<int> (pagenum);
   int upto = 0;
   foreach (ind, list)
      {
      pagepos_info pp = model->getPosData (ind);

      // override the position if required
      if (poslist)
         pp.pos = (*poslist) [upto++];
      _pagepos << pp;
      // qDebug () << "position" << pp.pos;
      }
   setText (QApplication::translate("UCStackStacks",
      "Stack pages (%1)").arg (list.size ()));
   }


void UCStackStacks::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString destname = _destname;
   QStringList filenames = _filenames;
   auto pagenum = _pagenum;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndex dest = model->index (destname, parent);
      QModelIndexList list = model->listFromFilenames (filenames, parent);

      model->opRestackStacks (QModelIndexList () << dest, parent,
                              QList<QModelIndexList> () << list, pagenum,
                              done);
   });
   }


void UCStackStacks::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString destname = _destname;
   QStringList filenames = _filenames;
   auto pagenum = _pagenum;
   QList<pagepos_info> pagepos = _pagepos;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndex dest = model->index (destname, parent);
      QStringList names = filenames;
      QList<pagepos_info> pos = pagepos;

      done (model->opUnstackFromStack (dest, names, *pagenum, pos));
   });
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
   rename (_oldname, _newname);
   }


void UCRenameStack::undo (void)
   {
   rename (_newname, _oldname);
   }


void UCRenameStack::rename (const QString &from, const QString &to)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (from, model->deskFromDirname (path));

      model->opRenameStack (ind, to, done);
   });
   }


UCTransformPage::UCTransformPage (Desktopmodel *model, const QModelIndex &ind,
      int pagenum, File::e_transform op)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _fname = model->data (ind, Desktopmodel::Role_filename).toString ();
   _pagenum = pagenum;
   _op = op;
   setText (QApplication::translate("UCTransformPage",
      pagenum == -1 ? "Transform stack" : "Transform page"));
   }


void UCTransformPage::redo (void)
   {
   transform (_op);
   }


void UCTransformPage::undo (void)
   {
   transform (File::transformInverse (_op));
   }


void UCTransformPage::transform (File::e_transform op)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;
   int pagenum = _pagenum;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opTransformPage (ind, pagenum, op, done);
   });
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
   rename (_newname);
   }


void UCRenamePage::undo (void)
   {
   rename (_oldname);
   }


void UCRenamePage::rename (const QString &name)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;
   int pagenum = _pagenum;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opRenamePage (ind, pagenum, name, done);
   });
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
   update (_new);
   }


void UCUpdateAnnot::undo (void)
   {
   update (_old);
   }


void UCUpdateAnnot::update (const QHash<int, QString> &annots)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opUpdateAnnot (ind, annots, done);
   });
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
