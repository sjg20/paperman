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
#include <QFileInfo>
#include <QUndoCommand>


#include "desktopmodel.h"
#include "desktopundo.h"
#include "mainwidget.h"
#include "maxview.h"
#include "pagecover.h"
#include "utils.h"


Desktopundostack::Desktopundostack (QObject *parent)
      : QUndoStack (parent)
   {
   _index = 0;
   _pushing = false;
   connect (this, &QUndoStack::indexChanged, this,
            [this] (int index) { noteIndex (index); });
   }


Desktopundostack::~Desktopundostack ()
   {
   }


void Desktopundostack::push (QUndoCommand *cmd)
   {
   qCDebug (logUndo).noquote () << QString ("undo: do '%1' (%2 done)")
      .arg (cmd->text ()).arg (index () + 1);
   _pushing = true;
   QUndoStack::push (cmd);
   _pushing = false;
   _index = index ();
   }


void Desktopundostack::noteIndex (int index)
   {
   /* push() says what it does, and a stack being emptied undoes nothing */
   if (_pushing)
      return;
   if (!count ())
      qCDebug (logUndo).noquote () << "undo: stack cleared";
   else if (index < _index)
      for (int i = _index - 1; i >= index; i--)
         qCDebug (logUndo).noquote () << QString ("undo: undo '%1' (%2 done)")
            .arg (text (i)).arg (i);
   else
      for (int i = _index; i < index; i++)
         qCDebug (logUndo).noquote () << QString ("undo: redo '%1' (%2 done)")
            .arg (text (i)).arg (i + 1);
   _index = index;
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


UCAddFiles::UCAddFiles (Desktopmodel *model, const QStringList &paths,
                        QModelIndex &parent, QPoint pos,
                        const QModelIndex &dest, int pagenum)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (parent);
   _paths = paths;
   _pos = pos;
   if (dest.isValid ())
      _destname = model->data (dest, Desktopmodel::Role_filename).toString ();
   _newnames = std::make_shared<QStringList> ();
   _pagenum = std::make_shared<int> (pagenum);
   _pagepos = std::make_shared<QList<pagepos_info> > ();

   // how many pages they come to, which is how many undo takes out
   _count = 0;
   if (dest.isValid ())
      for (const QString &path : paths)
         {
         QFileInfo fi (path);
         File::e_type type = File::typeFromName (path);
         int pages = 1;

         if (type != File::Type_jpeg)
            {
            File *f = File::createFile (fi.path () + "/", fi.fileName (),
                                        nullptr, type);

            pages = f->load () ? 0 : f->pagecount ();
            delete f;
            }
         _count += pages;
         }
   setText (dest.isValid ()
      ? QApplication::translate ("UCAddFiles", "Add files to stack (%1)")
            .arg (paths.size ())
      : QApplication::translate ("UCAddFiles", "Add files (%1)")
            .arg (paths.size ()));
   }


/** make a completion which says that a stack has new pages, once they
    are in, then passes on to another

   \param model     model the stack is in
   \param dir       directory of the stack
   \param destname  filename of the stack
   \param done      completion to pass on to
   \returns the completion */
static Desktopmodel::OpDone joined (Desktopmodel *model, const QString &dir,
                                    const QString &destname,
                                    Desktopmodel::OpDone done)
   {
   return [=] (err_info *err) {
      QModelIndex dest = model->index (destname,
                                       model->deskFromDirname (dir));

      if (!err && dest.isValid ())
         emit model->stackJoined (dest);
      done (err);
   };
   }


void UCAddFiles::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QStringList paths = _paths;
   QPoint pos = _pos;
   QString destname = _destname;
   auto newnames = _newnames;
   auto pagenum = _pagenum;
   auto pagepos = _pagepos;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QModelIndex parent = model->deskFromDirname (dir);

      newnames->clear ();
      pagepos->clear ();
      model->aboutToAdd (parent);
      if (destname.isEmpty ())
         {
         model->opAddFiles (paths, parent, pos, newnames, done);
         return;
         }

      /* once they are stacks, they go into the one they are for. Those
         of another type than it are converted to its type first, as a
         stack takes only pages of its own type */
      auto stack = [=] (err_info *err) {
         QModelIndex parent = model->deskFromDirname (dir);
         QModelIndex dest = model->index (destname, parent);
         QModelIndexList list = model->listFromFilenames (*newnames, parent);

         if (err || !dest.isValid () || list.isEmpty ())
            {
            done (err);
            return;
            }
         for (const QModelIndex &ind : list)
            *pagepos << model->getPosData (ind);
         model->opRestackStacks (QModelIndexList () << dest, parent,
                                 QList<QModelIndexList> () << list, pagenum,
                                 joined (model, dir, destname, done));
      };

      model->opAddFiles (paths, parent, pos, newnames, [=] (err_info *err) {
         QModelIndex parent = model->deskFromDirname (dir);
         QModelIndex dest = model->index (destname, parent);
         File *fdest = model->getFile (dest);
         QStringList others;

         if (err || !fdest)
            {
            done (err);
            return;
            }

         File::e_type type = fdest->type ();

         for (const QString &name : *newnames)
            if (File::typeFromName (name) != type)
               others << name;
         if (others.isEmpty ())
            {
            stack (NULL);
            return;
            }

         // each is replaced by a copy of the stack's type
         auto copies = std::make_shared<QStringList> ();
         QModelIndexList list = model->listFromFilenames (others, parent);

         model->opDuplicateStacks (list, parent, copies, type, 3,
                                   [=] (err_info *err) {
            QModelIndex parent = model->deskFromDirname (dir);
            QModelIndexList list = model->listFromFilenames (others, parent);

            if (err || copies->size () != others.size ())
               {
               done (err);
               return;
               }
            for (int i = 0; i < others.size (); i++)
               (*newnames) [newnames->indexOf (others [i])] = (*copies) [i];
            model->opDeleteStacks (list, parent, stack);
         });
      });
   });
   }


void UCAddFiles::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString destname = _destname;
   auto newnames = _newnames;
   auto pagenum = _pagenum;
   auto pagepos = _pagepos;

   // the stacks made are still there, unless they went into another
   if (destname.isEmpty ())
      {
      deleteMade (_dir, _newnames);
      return;
      }

   /* they are pages of that stack now. On a server those pages are
      deleted from it, which leaves nothing else of them */
   int count = _count;

   if (model->isRemoteDir (dir))
      {
      queue (dir, [=] (Desktopmodel::OpDone done) {
         QModelIndex parent = model->deskFromDirname (dir);
         QModelIndex dest = model->index (destname, parent);

         if (!dest.isValid () || !count || pagepos->isEmpty ()
             || *pagenum < 0)
            {
            done (NULL);
            return;
            }

         QBitArray pages (*pagenum + count);

         pages.fill (true, *pagenum, *pagenum + count);
         model->opDeletePages (dest, pages, std::make_shared<QByteArray> (),
                               std::make_shared<int> (count), done);
      });
      return;
      }

   /* here they come out as the stacks they were, as when stacking is
      undone, and those are removed */
   queue (dir, [=] (Desktopmodel::OpDone done) {
      QModelIndex parent = model->deskFromDirname (dir);
      QModelIndex dest = model->index (destname, parent);
      QStringList names = *newnames;
      QList<pagepos_info> pos = *pagepos;

      if (!dest.isValid () || pos.isEmpty ())
         {
         done (NULL);
         return;
         }

      err_info *err = model->opUnstackFromStack (dest, names, *pagenum, pos);

      if (err)
         {
         done (err);
         return;
         }

      QModelIndexList list = model->listFromFilenames (names, parent);

      model->opDeleteStacks (list, parent, done);
   });
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


UCReorderPages::UCReorderPages (Desktopmodel *model, QModelIndex &ind,
                                const QVector<int> &order)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _filename = model->data (ind, Desktopmodel::Role_filename).toString ();
   _order = order;
   setText (QApplication::translate ("UCReorderPages", "Reorder pages"));
   }


void UCReorderPages::reorder (const QVector<int> &order)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _filename;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opReorderPages (ind, order, done);
   });
   }


void UCReorderPages::redo (void)
   {
   reorder (_order);
   }


void UCReorderPages::undo (void)
   {
   // the page now at _order [i] goes back to place _order [i]
   QVector<int> back (_order.size ());

   for (int i = 0; i < _order.size (); i++)
      back [_order [i]] = i;
   reorder (back);
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
   _stacked = std::make_shared<QStringList> ();
   _origs = std::make_shared<QStringList> ();
   _copies = std::make_shared<QStringList> ();
   _trashed = std::make_shared<QStringList> ();
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
   auto stacked = _stacked;
   auto origs = _origs;
   auto copies = _copies;
   auto trashed = _trashed;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndex dest = model->index (destname, parent);
      File *fdest = model->getFile (dest);

      *stacked = filenames;
      origs->clear ();
      copies->clear ();
      trashed->clear ();

      auto stack = [=] (err_info *err) {
         QModelIndex parent = model->deskFromDirname (dir);
         QModelIndex dest = model->index (destname, parent);
         QModelIndexList list = model->listFromFilenames (*stacked, parent);

         if (err)
            {
            done (err);
            return;
            }
         model->opRestackStacks (QModelIndexList () << dest, parent,
                                 QList<QModelIndexList> () << list, pagenum,
                                 joined (model, dir, destname, done));
      };

      // those of another type than the stack are converted to its type
      if (fdest && model->canStackOnto (dest))
         for (const QString &name : filenames)
            {
            File *f = model->getFile (model->index (name, parent));

            if (f && f->type () != fdest->type ())
               *origs << name;
            }
      if (origs->isEmpty ())
         {
         stack (NULL);
         return;
         }

      /* a copy of the stack's type goes in for each, which itself goes
         to the trash, for undo to bring back */
      QModelIndexList list = model->listFromFilenames (*origs, parent);

      model->opDuplicateStacks (list, parent, copies, fdest->type (), 3,
                                [=] (err_info *err) {
         QModelIndex parent = model->deskFromDirname (dir);
         QModelIndexList list = model->listFromFilenames (*origs, parent);

         if (err || copies->size () != origs->size ())
            {
            done (err);
            return;
            }
         for (int i = 0; i < origs->size (); i++)
            (*stacked) [stacked->indexOf ((*origs) [i])] = (*copies) [i];
         model->opTrashStacks (list, parent, trashed, QString (), false,
                               stack);
      });
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
   auto stacked = _stacked;
   auto origs = _origs;
   auto copies = _copies;
   auto trashed = _trashed;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex parent = model->deskFromDirname (path);
      QModelIndex dest = model->index (destname, parent);
      QStringList asked = stacked->isEmpty () ? filenames : *stacked;
      auto names = std::make_shared<QStringList> (asked);

      model->opUnstackFromStack (dest, names, *pagenum, pagepos,
                                 [=] (err_info *err) {
         QModelIndex parent = model->deskFromDirname (dir);

         if (err || origs->isEmpty () || trashed->size () != origs->size ())
            {
            done (err);
            return;
            }

         /* the copies made of stacks of another type have come out, each
            perhaps with a name of its own: remove them, and bring back
            the stacks they were made from, each to where it was */
         QList<pagepos_info> back;
         QStringList gone;

         for (const QString &name : *origs)
            back << pagepos [filenames.indexOf (name)];
         for (const QString &name : *copies)
            gone << names->value (asked.indexOf (name), name);

         QModelIndexList list = model->listFromFilenames (gone, parent);

         model->opDeleteStacks (list, parent, [=] (err_info *err) {
            QModelIndex parent = model->deskFromDirname (dir);

            if (err)
               {
               done (err);
               return;
               }
            model->opUntrashStacks (*trashed, parent, origs, back, true,
                                    QString (), false, done);
         });
      });
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


UCCropPage::UCCropPage (Desktopmodel *model, const QModelIndex &ind,
      int pagenum, const QRectF &area)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _fname = model->data (ind, Desktopmodel::Role_filename).toString ();
   _pagenum = pagenum;
   _area = area;
   _undo = std::make_shared<QByteArray> ();
   setText (QApplication::translate ("UCCropPage",
      pagenum == -1 ? "Crop stack" : "Crop page"));
   }


void UCCropPage::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;
   int pagenum = _pagenum;
   QRectF area = _area;
   auto undo = _undo;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opCropPage (ind, pagenum, area, undo, done);
   });
   }


void UCCropPage::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;
   int pagenum = _pagenum;
   auto undo = _undo;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opUncropPage (ind, pagenum, undo, done);
   });
   }


UCSetCover::UCSetCover (Desktopmodel *model, const QModelIndex &ind,
      int pagenum, const Pagecover &cover)
      : Desktopundocmd (model)
   {
   _dir = model->deskToDirname (ind.parent ());
   _fname = model->data (ind, Desktopmodel::Role_filename).toString ();
   _pagenum = pagenum;
   _cover = cover.toString ();
   _old = std::make_shared<Desktopmodel::PagePropsMap> ();
   setText (QApplication::translate ("UCSetCover",
      cover.isNull () ? "Clear cover" : "Set cover"));
   }


void UCSetCover::redo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;
   int pagenum = _pagenum;
   QString cover = _cover;
   auto old = _old;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opSetCover (ind, pagenum, cover, old, done);
   });
   }


void UCSetCover::undo (void)
   {
   Desktopmodel *model = _model;
   QString dir = _dir;
   QString fname = _fname;
   auto old = _old;

   queue (dir, [=] (Desktopmodel::OpDone done) {
      QString path = dir;
      QModelIndex ind = model->index (fname, model->deskFromDirname (path));

      model->opPutPageProps (ind, old, done);
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
