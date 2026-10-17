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
   File:       dmop.cpp
   Started:    26/6/09

   This file contains operations relating to Desktopmodel.
   These operations are typically invoked by the undo system in desktopundo
*/

#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QTimer>
#include <QJsonObject>
#include <QMimeData>
#include <QProcess>
#include <QUrl>

#include "blockwatch.h"
#include "desktopmodel.h"
#include "desk.h"
#include "file.h"
#include "ocr.h"
#include "ocrpage.h"
#include "dirmodel.h"
#include "searchindex.h"
#include <QDir>

#include "op.h"
#include "filemax.h"
#include "remotebackend.h"
#include "utils.h"


err_info *Desktopmodel::opUnstackFromStack (QModelIndex &src, QStringList &newnames,
      int srcpagenum, QList<pagepos_info> &pagepos)
   {
   QString newname;
   err_info *e = NULL;
   File *f = getFile (src);
   File *fnew;
   QList<File *> flist;
   int upto = 0, row;

   // remove every page from the stack except the first
   foreach (newname, newnames)
      {
      pagepos_info &pp = pagepos [upto++];

      // unstack the right number of pages from the source into a fresh stack
      CALLB (f->unstackItems (srcpagenum, pp.pagecount, true, newname, fnew, row));
      fnew->setPos (pp.pos);
      fnew->setPagenum (pp.pagenum);
      flist << fnew;
      }

   buildItem (src);
   insertRows (flist, src.parent ());
   return e;
   }


err_info *Desktopmodel::opUnstackStacks (QModelIndexList &list, QModelIndex parent,
      QList<QStringList> &_newnames)
   {
   QModelIndexList newlist;
   QModelIndex ind;
   err_info *err = NULL;

   Desk *rdesk = getDesk (parent);
   if (rdesk && dynamic_cast<RemoteBackend *> (rdesk->backend ()))
      {
      auto names = std::make_shared<QList<QStringList> > ();

      err = waitForOp ("unstack", [&] (OpDone done) {
         opUnstackStacks (list, parent, names, done);
      });
      _newnames << *names;
      return err;
      }

   // work through each stack in the list
   // note that some items might only have one page, in which case we do nothing
   foreach (ind, list)
      {
      File *f = getFile (ind);
      File *fnew;
      int pagenum = 1, row;

//       printf ("unstack %s, row %d\n", f->filename ().latin1 (), ind.row ());

      QList<File *> flist;
      QStringList stack_names;

      CALL(f->load());
      // remove every page from the stack except the first
      while (f->pagecount () > 1)
         {
         // for each new page, create a fresh stack
         err = f->unstackItems (0, 1, true, "", fnew, row, pagenum++);
         if (err)
            break;
         stack_names << fnew->filename ();
         flist << fnew;
         }

      // we leave one page on the original stack
//       printf ("unstack done, row %d, flist size = %d\n", ind.row (), flist.size ());
      if (flist.size ())   // any pages unstacked?
         {
         buildItem (ind);
         insertRows (flist, parent);
         }
      _newnames << stack_names;
      }
   return err;
   }


err_info *Desktopmodel::opStackStacks (QModelIndex &index, QModelIndexList &list, int &pagenum)
   {
   QList<QModelIndexList> pages_list;
   QModelIndexList dest;

   dest << index;
   pages_list << list;
   return opRestackStacks (dest, index.parent (), pages_list, pagenum);
   }



err_info *Desktopmodel::opRestackStacks (QModelIndexList &list, QModelIndex parent,
      QList<QModelIndexList> &pages_list, int &in_destpage)
   {
   QModelIndexList delete_list;
   QModelIndex ind;
   QString fname;
   err_info *e = NULL;
   int upto = 0;
   QModelIndex pind;
   int out_destpage = in_destpage; // destination page for first stack

   Desk *rdesk = getDesk (parent);
   RemoteBackend *remote = rdesk
         ? dynamic_cast<RemoteBackend *> (rdesk->backend ()) : nullptr;

   if (remote)
      {
      auto page = std::make_shared<int> (in_destpage);

      e = waitForOp ("stack", [&] (OpDone done) {
         opRestackStacks (list, parent, pages_list, page, done);
      });
      in_destpage = *page;
      return e;
      }


   // work through each stack in the list
   foreach (ind, list)
      {
      File *f = getFile (ind);
//       File *fnew;
//       int pagenum = 1;
      int destpage = in_destpage == -1 ? f->pagenum () : in_destpage;

      if (out_destpage == -1)
         out_destpage = destpage;
      QList<File *> flist;

      // add each page back onto the stack
      int old_pagenum = f->pagenum ();
      foreach (pind, pages_list [upto])
         {
         if (pind != QModelIndex ())   // not empty
            {
            File *pdel = getFile (pind);

            // get page to stack to, and work out where the next lot will go
            f->setPagenum (destpage);
            destpage += pdel->pagecount ();

            // stack this page, and remember to remove from model later
            CALLB (f->stackItem (pdel));
            delete_list << pind;
            }
         }
      f->setPagenum (old_pagenum);

      buildItem (ind);
      upto++;
      }

   // record destination page
   in_destpage = out_destpage;

   // we lose the original error
   err_info *e2 = opDeleteStacks (delete_list, parent);

   if (!e)
      e = e2;
   return e;
   }


err_info *Desktopmodel::opDuplicateStacks (QModelIndexList &list, QModelIndex parent,
      QStringList &namelist, File::e_type type, int odd_even)
   {
   QModelIndexList newlist;
   QModelIndex ind;
   QList<File *> flist;
   err_info *err = NULL;
   File *fnew, *f;
   int count;

   Desk *rdesk = getDesk (parent);
   RemoteBackend *remote = rdesk
         ? dynamic_cast<RemoteBackend *> (rdesk->backend ()) : nullptr;

   /* a server converts or copies the stack in place, or else a copy is
      converted here and uploaded */
   if (remote)
      {
      auto names = std::make_shared<QStringList> ();

      err = waitForOp ("duplicate", [&] (OpDone done) {
         opDuplicateStacks (list, parent, names, type, odd_even, done);
      });
      namelist << *names;
      return err;
      }

   // duplicate each stack, creating a list of new files
   QString opname = type == File::Type_other ? tr ("Copy files") :
      QString (tr ("Convert to %1")).arg (File::typeName (type));
   count = type == File::Type_other ? list.size () : listPagecount (list);
   Operation op (opname, count, 0);
   foreach (ind, list)
      {
      f = getFile (ind);
      CALL(f->load());
      err = f->duplicateAny (type, odd_even, op, fnew);
      if (err)
         break;
      flist << fnew;
      namelist << fnew->filename ();
      }

   // insert the files
   insertRows (flist, parent);
   return err;
   }


err_info *Desktopmodel::opUnfoldBooklets (QModelIndexList &list,
      QModelIndex parent, QStringList &namelist)
   {
   QList<File *> flist;
   err_info *err = NULL;
   QModelIndex ind;
   File *fnew, *f;

   // each booklet page splits into two, so the work is twice the page count
   Operation op (tr ("Unfold booklet"), 2 * listPagecount (list), 0);
   Desk *rdesk = getDesk (parent);
   RemoteBackend *remote = rdesk
         ? dynamic_cast<RemoteBackend *> (rdesk->backend ()) : nullptr;

   /* a server which converts unfolds a booklet in place, or else a copy
      is unfolded here and uploaded */
   if (remote)
      {
      auto names = std::make_shared<QStringList> ();

      err = waitForOp ("unfold", [&] (OpDone done) {
         opUnfoldBooklets (list, parent, names, done);
      });
      namelist << *names;
      return err;
      }

   foreach (ind, list)
      {
      f = getFile (ind);
      CALL (f->load ());
      err = f->unfoldBooklet (op, fnew);
      if (err)
         break;
      flist << fnew;
      namelist << fnew->filename ();
      }

   // insert the files
   insertRows (flist, parent);
   return err;
   }


err_info *Desktopmodel::opDeleteStack (QModelIndex &index)
   {
   QModelIndexList list;   // list of indexes to delete

   list << index;
   return opDeleteStacks (list, index.parent ());
   }


err_info *Desktopmodel::opDeleteStacks (QModelIndexList &list, QModelIndex parent)
   {
   err_info *e = NULL;
   QModelIndex ind;
   QModelIndexList del_list;   // list of indexes to delete
//    Desk *desk = getDesk (parent);

   for (const QModelIndex &first : list)
      if (getFile (first))
         {
         if (remoteForFile (getFile (first)))
            return waitForOp ("delete", [&] (OpDone done) {
               opDeleteStacks (list, parent, done);
            });
         break;
         }
//    _model_invalid = true;
   foreach (ind, list) if (getFile (ind))
      {
      File *f = getFile (ind);

      CALLB (f->remove ());
      del_list << ind;
      }

   // sort in reverse order so the row numbers don't change on us
   sortForDelete (del_list);
   foreach (ind, del_list)
      removeRow (ind.row (), ind.parent ());
//       delete desk->takeAt (ind.row ());
//    _model_invalid = false;

   return e;
   }


err_info *Desktopmodel::opTrashStacks (QModelIndexList &list, QModelIndex parent,
      QStringList &trashlist, QString &dest, bool copy)
   {
   err_info *e = NULL;
   QModelIndex ind;
   Desk *desk = getDesk (parent);
   QModelIndexList del_list;   // list of indexes to delete

//    savePersistentIndexes ();
//    emit layoutAboutToBeChanged();

   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   if (remote)
      {
      auto names = std::make_shared<QStringList> ();

      e = waitForOp ("trash", [&] (OpDone done) {
         opTrashStacks (list, parent, names, dest, copy, done);
      });
      trashlist = *names;
      return e;
      }

   if (dest.isEmpty ())
      dest = desk->trashdir ();
   if (dest.isEmpty ())
      return err_make (ERRFN, ERR_no_trash_directory_is_defined);

   // we don't know how many rows will be removed, so let's say all of them for now
//    beginRemoveRows (parent, 0, rowCount (parent));
//    _model_invalid = true;
   foreach (ind, list)
      {
      File *f = getFile (ind);
      QString trashname;

      // skip missing items
      if (!ind.isValid ())
         continue;
      // printf ("   row %d: %p, delete %s\n", ind.row (), f,
                  // f->filename ().toLatin1 ().constData());
      e = f->move (dest, trashname, copy);
      if (e)
         break;
      del_list << ind;
//       removeRow (ind.row (), ind.parent ());
      trashlist << trashname;
      }
   if (!copy)
      {
      sortForDelete (del_list);
      foreach (ind, del_list)
         {
         Q_ASSERT (ind.isValid ());
         removeRows (ind.row (), 1, parent);
         }
      }

   /* if we have the destination desk loaded, alert it about the new files */
   addFilesToDesk (dest, trashlist);

   /** this seems to be a bad idea as it causes a crash in the proxy.We are not
       actually sorting the model, so perhaps we shouldn't emit it anyway. Not
       emitting it doesn't seem to cause any problem */
//    emit layoutChanged();
   return e;
   }


err_info *Desktopmodel::opUntrashStacks (QStringList &trashlist, QModelIndex parent,
         QStringList &filenames, QList<pagepos_info> *pagepos, QString &src, bool copy)
   {
   Desk *desk = getDesk (parent);
   QModelIndexList list;
   QString trashname;
   QModelIndex ind;
   File *fnew;
   err_info *e = NULL;
   QList<File *> flist;
   int i;

   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   if (remote)
      {
      auto names = std::make_shared<QStringList> (filenames);

      e = waitForOp ("untrash", [&] (OpDone done) {
         opUntrashStacks (trashlist, parent, names,
                          pagepos ? *pagepos : QList<pagepos_info> (),
                          pagepos != nullptr, src, copy, done);
      });
      filenames = *names;
      return e;
      }

   /* the source desk, if loaded, still has the files open: let go of
      them so they can be renamed */
   if (!copy && !src.isEmpty ())
      releaseFilesInDesk (src, trashlist);

   /* first, move all the trashed files back into the current directory,
      creating a list of the new File * records thus created. If
      an error occurs, stop where we got up to (with the one that caused
      an error not added). For a copy, we just remove from the trash */
   for (i = 0; i < trashlist.size (); i++)
      {
      trashname = trashlist [i];
      QString fname = filenames [i];
      // printf ("undelete %s to %s\n", trashname.toLatin1 ().constData(),
              // fname.toLatin1 ().constData());
      if (src.isEmpty ())
         e = copy
            ? desk->deleteFromTrash (trashname)
            : desk->moveFromTrash (trashname, fname, fnew);
      else
         e = copy
            ? desk->deleteFromDir (src, trashname)
            : desk->moveFromDir (src, trashname, fname, fnew);
      if (e)
         break;

      // if we have position/page number information, restore it
      if (!copy && pagepos)
         {
         fnew->setPos ((*pagepos) [i].pos);
         fnew->setPagenum ((*pagepos) [i].pagenum);
         }
      filenames [i] = fname;  // update the filename
      flist << fnew;
      }

   if (!copy)
      insertRows (flist, parent);

   /* if we have the source desk loaded, alert it about the removed files */
   removeFilesFromDesk (src, trashlist);

   return e;
   }


void Desktopmodel::opMoveStacks (QModelIndexList &list, QModelIndex parent,
      QList<QPoint> &newpos)
   {
   Desk *desk = getDesk (parent);
   QModelIndex ind;
   int i = 0;

   foreach (ind, list) if (ind.isValid ())
      {
      getFile (ind)->setPos (newpos [i++]);
      emit dataChanged (ind, ind);
      }
   desk->dirty ();

   /* positions on a remote desk are shared through the server's desk
      file; push them at once so other clients see the new layout */
   if (desk->isRemote ())
      desk->flush ();
   }


err_info *Desktopmodel::opUnstackPage (QModelIndex &ind, int &pagenum,
      bool remove, QString &newname)
   {
   File *fnew, *f = getFile (ind);
   err_info *e = NULL;
   int row;

   if (pagenum == -1)
      pagenum = f->pagenum ();

   if (remoteForFile (f))
      {
      auto page = std::make_shared<int> (pagenum);
      auto name = std::make_shared<QString> (newname);

      e = waitForOp ("unstack", [&] (OpDone done) {
         opUnstackPage (ind, page, remove, name, done);
      });
      pagenum = *page;
      newname = *name;
      return e;
      }

   if ((!remove || f->pagecount () > 1)
      && (e = f->unstackItems (pagenum, 1, remove, "", fnew, row, 1), !e))
      {
      newname = fnew->filename ();
      // if removing, update the view of the source stack
      if (remove)
         buildItem (ind);
      QModelIndexList list;
      newItem (row, ind.parent (), list);
      }
   return e;
   }


err_info *Desktopmodel::opUpdateAnnot (QModelIndex &ind, QHash<int, QString> &updates)
   {
   return waitForOp ("annotation", [&] (OpDone done) {
      opUpdateAnnot (ind, updates, done);
   });
   }


void Desktopmodel::opUpdateAnnot (const QModelIndex &ind,
                                  const QHash<int, QString> &updates,
                                  OpDone done)
   {
   _modelconv->assertIsSource (ind.model (), &ind, 0);
   File *f = getFile (ind);
   QHash<int, QString> changes = updates;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString dir = desk->dir ();
         QString fname = f->filename ();
         QJsonObject wire;

         for (auto it = updates.constBegin (); it != updates.constEnd ();
              ++it)
            wire [File::annotWireName ((File::e_annot)it.key ())]
                  = it.value ();
         remote->updateAnnotations (desk->repoName (),
                                    remoteStackPath (desk, f), wire, this,
                                    [this, dir, fname, changes, done] (
                                       const RemoteBackend::Result &result) {
            if (_closing)
               return;      // the model is going, with what it showed
            if (!result.ok)
               {
               done (err_make (ERRFN, ERR_remote_op_failed2, "annotation",
                               qPrintable (result.error)));
               return;
               }

            /* A stack held a page at a time keeps its annotations in
               info.json, so fetch that again: 50KB, and it cannot drift
               from the server.  A stack fetched whole is mirrored, so the
               annotations pane shows the new values without a refetch. */
            QModelIndex ind = stackFor (dir, fname);
            File *f = getFile (ind);
            QHash<int, QString> updates = changes;

            if (f && sparseFor (f, remoteForFile (f), f->desk ()))
               stackChangedLater (ind, QList<int> () << -1);
            else if (f && f->remoteChecked ())
               {
               f->putAnnot (updates);
               f->flush ();
               }
            done (NULL);
         });
         return;
         }

      f->putAnnot (changes);
      f->flush ();
      }
   done (NULL);
   }



err_info *Desktopmodel::opDeletePages (QModelIndex &ind, QBitArray &pages,
      QByteArray &del_info, int &count)
   {
   auto info = std::make_shared<QByteArray> (del_info);
   auto num = std::make_shared<int> (count);
   err_info *e = waitForOp ("page delete", [&] (OpDone done) {
      opDeletePages (ind, pages, info, num, done);
   });

   del_info = *info;
   count = *num;
   return e;
   }


void Desktopmodel::opDeletePages (const QModelIndex &ind,
                                  const QBitArray &pages,
                                  std::shared_ptr<QByteArray> del_info,
                                  std::shared_ptr<int> count, OpDone done)
   {
   File *f = getFile (ind);
   err_info *e = NULL;
   QBitArray which = pages;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString dir = desk->dir ();
         QString fname = f->filename ();
         QList<int> pageList;

         for (int i = 0; i < pages.size (); i++)
            if (pages.testBit (i))
               pageList << i + 1;

         remote->deletePages (desk->repoName (), remoteStackPath (desk, f),
                              pageList, this,
                              [this, dir, fname, which, pageList, del_info,
                               count, done] (
                                 const RemoteBackend::Result &result) {
            if (_closing)
               return;      // the model is going, with what it showed
            if (!result.ok)
               {
               done (err_make (ERRFN, ERR_remote_op_failed2, "page delete",
                               qPrintable (result.error)));
               return;
               }

            /* The undo record's blob carries the server's token alongside
               any local recovery data, so opUndeletePages() can restore
               both sides */
            QString undoId = result.json ().value ("undoId").toString ();
            QModelIndex ind = stackFor (dir, fname);
            File *f = getFile (ind);
            RemoteBackend *remote = f ? remoteForFile (f) : nullptr;
            QByteArray localInfo;
            err_info *e = NULL;

            *count = pageList.size ();
            if (f && remote)
               {
               /* A stack held a page at a time is fetched again, since its
                  pages have been renumbered.  A stack fetched whole has
                  the removal mirrored onto the cached copy, so the view
                  stays in step without a refetch; one not fetched yet is
                  fetched afresh when next read */
               if (sparseFor (f, remote, f->desk ()))
                  stackChangedLater (ind, QList<int> ());
               else if (f->remoteChecked () && f->pagecount ())
                  {
                  QBitArray bits = which;

                  e = f->removePages (bits, localInfo, *count);
                  buildItem (ind);
                  }
               else
                  {
                  remote->invalidateCachedFile (f->desk ()->repoName (),
                                                remoteStackPath (f->desk (),
                                                                 f));
                  f->setRemoteChecked (false);
                  }
               }
            del_info->clear ();
            QDataStream ds (del_info.get (), QIODevice::WriteOnly);
            ds << undoId << localInfo;
            done (e);
         });
         return;
         }

      e = f->removePages (which, *del_info, *count);
      buildItem (ind);
      getDesk (ind.parent ())->dirty ();
      }
   done (e);
   }


err_info *Desktopmodel::opUndeletePages (QModelIndex &ind, QBitArray &pages,
      QByteArray &del_info, int count)
   {
   auto info = std::make_shared<QByteArray> (del_info);

   return waitForOp ("page undelete", [&] (OpDone done) {
      opUndeletePages (ind, pages, info, count, done);
   });
   }


void Desktopmodel::opUndeletePages (const QModelIndex &ind,
                                    const QBitArray &pages,
                                    std::shared_ptr<QByteArray> del_info,
                                    int count, OpDone done)
   {
   File *f = getFile (ind);
   err_info *e = NULL;
   QBitArray which = pages;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString dir = desk->dir ();
         QString fname = f->filename ();
         QString undoId;
         QByteArray localInfo;
         QDataStream ds (del_info.get (), QIODevice::ReadOnly);

         ds >> undoId >> localInfo;
         remote->undeletePages (desk->repoName (), remoteStackPath (desk, f),
                                undoId, this,
                                [this, dir, fname, which, localInfo, count,
                                 done] (
                                   const RemoteBackend::Result &result) {
            if (_closing)
               return;      // the model is going, with what it showed
            if (!result.ok)
               {
               done (err_make (ERRFN, ERR_remote_op_failed2,
                               "page undelete", qPrintable (result.error)));
               return;
               }

            QModelIndex ind = stackFor (dir, fname);
            File *f = getFile (ind);
            RemoteBackend *remote = f ? remoteForFile (f) : nullptr;
            err_info *e = NULL;

            // as in opDeletePages()
            if (f && remote)
               {
               if (sparseFor (f, remote, f->desk ()))
                  stackChangedLater (ind, QList<int> ());
               else if (f->remoteChecked () && !localInfo.isEmpty ())
                  {
                  QBitArray bits = which;
                  QByteArray info = localInfo;

                  e = f->restorePages (bits, info, count);
                  buildItem (ind);
                  }
               else
                  {
                  remote->invalidateCachedFile (f->desk ()->repoName (),
                                                remoteStackPath (f->desk (),
                                                                 f));
                  f->setRemoteChecked (false);
                  }
               }
            done (e);
         });
         return;
         }

      e = f->restorePages (which, *del_info, count);
      buildItem (ind);
      }
   done (e);
   }


void Desktopmodel::opChangeDir (QString &dirPath, QString &rootPath)
   {
   QModelIndex ind = showDir(dirPath, rootPath, nullptr);
   emit dirChanged (dirPath, ind);
   }


err_info *Desktopmodel::opRenameStack (const QModelIndex &index, QString &newname)
   {
   return waitForOp ("rename", [&] (OpDone done) {
      opRenameStack (index, newname, done);
   });
   }


void Desktopmodel::opRenameStack (const QModelIndex &index,
                                  const QString &newname, OpDone done)
   {
   File *f = getFile (index);
   err_info *e = NULL;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString dir = desk->dir ();
         QString oldname = f->filename ();

         remote->renameStack (desk->repoName (), remoteStackPath (desk, f),
                              newname, true, this,
                              [this, dir, oldname, newname, done] (
                                 const RemoteBackend::Result &result) {
            if (_closing)
               return;      // the model is going, with what it showed
            if (!result.ok)
               {
               done (err_make (ERRFN, ERR_remote_op_failed2, "rename",
                               qPrintable (result.error)));
               return;
               }

            /* carry the cached copy (and its validator) over to the new
               name, then update the file object to match the server */
            QModelIndex ind = stackFor (dir, oldname);
            File *f = getFile (ind);

            if (f)
               {
               QString oldCache = f->pathname ();

               f->updateFilename (result.json ().value ("name")
                                     .toString (newname));
               QFile::rename (oldCache, f->pathname ());
               QFile::rename (oldCache + ".etag", f->pathname () + ".etag");
               buildItem (ind);
               }
            done (NULL);
         });
         return;
         }

      QString name = newname;

      e = f->rename (name, true);
      buildItem (index);
      getDesk (index.parent ())->dirty ();
      }
   done (e);
   }


err_info *Desktopmodel::opRenamePage (const QModelIndex &index, int pagenum, QString &newname)
   {
   return waitForOp ("rename page", [&] (OpDone done) {
      opRenamePage (index, pagenum, newname, done);
   });
   }


void Desktopmodel::opRenamePage (const QModelIndex &index, int pagenum,
                                 const QString &newname, OpDone done)
   {
   _modelconv->assertIsSource (0, &index, 0);
   File *f = getFile (index);
   err_info *e = NULL;
   QString name = newname;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString dir = desk->dir ();
         QString fname = f->filename ();

         remote->renamePage (desk->repoName (), remoteStackPath (desk, f),
                             pagenum + 1, newname, this,
                             [this, dir, fname, pagenum, newname, done] (
                                const RemoteBackend::Result &result) {
            if (_closing)
               return;      // the model is going, with what it showed
            if (!result.ok)
               {
               done (err_make (ERRFN, ERR_remote_op_failed2, "page rename",
                               qPrintable (result.error)));
               return;
               }

            QModelIndex ind = stackFor (dir, fname);
            File *f = getFile (ind);
            QString name = newname;

            if (!f)
               {
               done (NULL);
               return;
               }

            /* a stack held a page at a time has the title in both the
               page file and info.json, so both are fetched again */
            if (sparseFor (f, remoteForFile (f), f->desk ()))
               {
               stackChangedLater (ind, QList<int> () << pagenum);
               f->setPagenum (pagenum);
               done (NULL);
               return;
               }

            /* mirror the change onto the cached copy so the page view
               stays in step without a refetch */
            if (pagenum >= 0 && pagenum < f->pagecount ())
               {
               f->renamePage (pagenum, name);
               f->setPagenum (pagenum);
               }
            buildItem (ind);
            done (NULL);
         });
         return;
         }

      if (pagenum >= 0 && pagenum < f->pagecount ()) // which it should be!
         {
         e = f->renamePage (pagenum, name);
         f->setPagenum (pagenum);
         }
      buildItem (index);
      }
   done (e);
   }


err_info *Desktopmodel::opTransformPage (const QModelIndex &index,
      int pagenum, File::e_transform op)
   {
   return waitForOp ("transform", [&] (OpDone done) {
      opTransformPage (index, pagenum, op, done);
   });
   }


void Desktopmodel::opTransformPage (const QModelIndex &index, int pagenum,
                                    File::e_transform op, OpDone done)
   {
   _modelconv->assertIsSource (0, &index, 0);
   File *f = getFile (index);
   err_info *e = NULL;

   /* A remote stack opens as a stub whose pages aren't reachable here,
      so ask the server that holds it to do the transform instead. */
   Desk *desk = f ? f->desk () : NULL;
   if (desk)
      {
      RemoteBackend *remote = dynamic_cast<RemoteBackend *> (desk->backend ());
      if (remote)
         {
         opTransformPageRemote (f, desk, remote, pagenum, op, done);
         return;
         }
      }

   if (f)
      {
      /* the file may not have been loaded yet, in which case it reports
         no pages and the transform would be silently skipped */
      f->load ();
      if (!f->pagecount ())
         /* an empty stack has nothing to transform; this also happens
            for stacks whose pages are not accessible, such as those in
            a remote repository */
         e = err_make (ERRFN, ERR_stack_has_no_pages);
      else if (pagenum == -1)
         {
         // transform every page of the stack
         for (int page = 0; !e && page < f->pagecount (); page++)
            e = f->transformPage (page, op);

         /* the page view refreshes through pageContentChanged(), so
            mark the item update as minor to avoid it also doing a full
            rebuild, which loses the page selection */
         _minor_change = true;
         buildItem (index);
         _minor_change = false;
         if (!e)
            emit pageContentChanged (index, -1);
         }
      else if (pagenum >= 0 && pagenum < f->pagecount ())
         {
         e = f->transformPage (pagenum, op);
         f->setPagenum (pagenum);
         _minor_change = true;
         buildItem (index);
         _minor_change = false;
         if (!e)
            emit pageContentChanged (index, pagenum);
         }
      else
         e = f->not_impl ();
      }
   done (e);
   }


RemoteBackend *Desktopmodel::remoteForFile (File *file) const
   {
   Desk *desk = file ? file->desk () : NULL;

   return desk ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;
   }


QString Desktopmodel::remoteRelDir (Desk *desk, QString dir) const
   {
   QString root = desk->rootDir ();

   if (dir.startsWith (root))
      dir = dir.mid (root.length ());
   while (dir.endsWith ('/'))
      dir.chop (1);
   if (dir.startsWith ('/'))
      dir = dir.mid (1);
   return dir;
   }


QString Desktopmodel::stackLocation (File *file) const
   {
   /* a stack on a server is where it is there, as the folder tree names
      the server, not where its copy is kept here */
   RemoteBackend *remote = remoteForFile (file);
   Desk *desk = file->desk ();

   if (!remote || !desk)
      return file->pathname ();
   return tr ("%1/%2 on %3").arg (desk->repoName (),
                                  remoteStackPath (desk, file),
                                  remote->baseUrl ().authority ());
   }


QString Desktopmodel::remoteStackPath (Desk *desk, File *file) const
   {
   /* the stack's file is in the backend's cache, at its path within the
      repository; this holds whichever folder it is in, which is not the
      desk's when the desk holds the stacks a search found */
   RemoteBackend *remote = dynamic_cast<RemoteBackend *> (desk->backend ());
   QString cache = remote ? remote->cacheDirFor (desk->repoName (), QString ())
                          : QString ();
   QString path = file->pathname ();

   if (!cache.isEmpty () && path.startsWith (cache + "/"))
      return path.mid (cache.length () + 1);

   /* otherwise it is a stub at its place below the repository's root,
      or failing that, in the desk's folder.  desk->dir() and
      desk->rootDir() are absolute and both end in '/' */
   QString root = desk->rootDir ();

   if (!root.isEmpty () && path.startsWith (root))
      return path.mid (root.length ());

   QString dirInRepo = desk->dir ();
   if (dirInRepo.startsWith (root))
      dirInRepo = dirInRepo.mid (root.length ());
   if (dirInRepo.endsWith ('/'))
      dirInRepo.chop (1);
   return dirInRepo.isEmpty () ? file->filename ()
                               : dirInRepo + "/" + file->filename ();
   }


void Desktopmodel::refreshRemoteThumbnail (Desk *desk, RemoteBackend *backend,
      File *file)
   {
   if (!_connectedBackends.contains (backend))
      {
      connect (backend, &RemoteBackend::thumbnailReady,
               this, &Desktopmodel::onThumbnailReady);
      _connectedBackends.insert (backend);
      }
   /* the page the stack is turned to, if it is known to have it */
   int pagenum = qMax (file->pagenum (), 0);
   int page = pagenum < file->knownPagecount () ? pagenum + 1 : 1;
   quint64 token = backend->fetchThumbnailAsync (desk->repoName (),
         remoteStackPath (desk, file), page, /*size=*/"small");
   _pendingThumbnails.insert (token, file);
   }


bool Desktopmodel::fetchRemoteThumbnail (File *file)
   {
   RemoteBackend *remote = remoteForFile (file);

   if (!remote)
      return false;
   Desk *desk = file->desk ();
   if (file->remoteChecked ())
      {
      Filemax *max = sparseFor (file, remote, desk);

      if (!max || max->hasPage (file->pagenum ()))
         return false;      // here to draw from
      }
   refreshRemoteThumbnail (desk, remote, file);
   return true;
   }


err_info *Desktopmodel::ocrPage (const QModelIndex &ind, int pagenum,
      QString &text)
   {
   return waitForOp ("ocr", [&] (OpDone done) {
      ocrPageLater (ind, pagenum, [&text, done] (err_info *err,
                                                 const QString &str) {
         text = str;
         done (err);
      });
   });
   }


err_info *Desktopmodel::ocrLocalPage (const QModelIndex &ind, int pagenum,
                                      QString &text)
   {
   File *f = getFile (ind);

   if (!f)
      return NULL;

   CALL (ensureContent (ind));

   QImage image;
   QSize size, trueSize;
   int bpp;
   CALL (getImage (ind, pagenum, false, image, size, trueSize, bpp));

   err_info *err;
   Ocr *ocr = Ocr::getOcr (err);

   if (!ocr)
      return err;

   // keep the page's words with it, where the stack can
   OcrPage page;

   CALL (ocr->imageToPage (image, page));
   text = page.text ();
   if (keepsOcr (f))
      CALL (f->putPageOcr (pagenum, page));
   return NULL;
   }


err_info *Desktopmodel::askServerToRead (const QModelIndex &ind)
   {
   return waitForOp ("read", [&] (OpDone done) {
      askServerToReadLater (ind, done);
   });
   }


bool Desktopmodel::keepsOcr (File *f) const
   {
   return f && f->type () == File::Type_max && !remoteForFile (f);
   }


err_info *Desktopmodel::findText (const QString &path,
      const QString &rootPath, const QString &text, Measure *meas,
      QModelIndex &found, int &count, Operation *op, bool *complete)
   {
   return waitForOp ("search", [&] (OpDone done) {
      findTextLater (path, rootPath, text, meas, op,
                     [&found, &count, complete, done] (err_info *err,
                        const QModelIndex &ind, int num, bool all) {
         found = ind;
         count = num;
         if (complete)
            *complete = all;
         done (err);
      });
   });
   }


/** Set @p root to @p rootPath without a trailing slash, and return the
    folder @p path relative to it, with no slash at either end */
static QString folderInRepo (const QString &path, const QString &rootPath,
                             QString &root)
   {
   root = rootPath;
   while (root.endsWith ('/'))
      root.chop (1);

   QString dirRel = path;

   if (dirRel.startsWith (root))
      dirRel = dirRel.mid (root.length ());
   while (dirRel.startsWith ('/'))
      dirRel.remove (0, 1);
   while (dirRel.endsWith ('/'))
      dirRel.chop (1);
   return dirRel;
   }


bool Desktopmodel::findNamesLater (const QString &path,
      const QString &rootPath, const QString &text, Measure *meas,
      Found done)
   {
   QString root;
   QString dirRel = folderInRepo (path, rootPath, root);
   RemoteBackend *remote = _dirmodel ? dynamic_cast<RemoteBackend *> (
         _dirmodel->backendForRoot (root)) : nullptr;

   if (!remote)
      return false;

   QString prefix = dirRel.isEmpty () ? QString () : dirRel + "/";

   remote->searchNames (QFileInfo (root).fileName (), text, dirRel, this,
                        [this, path, rootPath, prefix, meas, done] (
                           const QString &error, const QStringList &paths) {
      if (_closing)
         return;
      if (!error.isEmpty ())
         {
         done (err_make (ERRFN, ERR_remote_op_failed2, "search",
                         qPrintable (error)), QModelIndex (), 0, true);
         return;
         }

      // the stacks found, relative to the folder searched
      QStringList matches;

      for (const QString &found : paths)
         if (found.startsWith (prefix))
            matches << found.mid (prefix.length ());

      QModelIndex desk = finishFileSearch (path, rootPath, matches, meas);

      done (NULL, desk, matches.size (), true);
   });
   return true;
   }


void Desktopmodel::findTextLater (const QString &path,
      const QString &rootPath, const QString &text, Measure *meas,
      Operation *op, Found done)
   {
   QString root;

   // the folder searched, relative to the repository
   QString dirRel = folderInRepo (path, rootPath, root);

   RemoteBackend *remote = _dirmodel ? dynamic_cast<RemoteBackend *> (
         _dirmodel->backendForRoot (root)) : nullptr;
   QString query = SearchIndex::matchQuery (text);
   QString prefix = dirRel.isEmpty () ? QString () : dirRel + "/";

   if (!remote)
      {
      QModelIndex found;
      int count = 0;
      err_info *err = findLocalText (path, root, rootPath, query, meas, op,
                                     found, count);

      done (err, found, count, true);
      return;
      }

   // the stacks found, relative to the folder searched
   auto finish = [this, path, rootPath, prefix, meas, done] (
                    const QList<RemoteBackend::TextHit> &hits, bool complete) {
      QStringList matches;
      QHash<QString, int> pages;    // by the stack's path in the repository

      for (const RemoteBackend::TextHit &hit : hits)
         if (!pages.contains (hit.path) && hit.path.startsWith (prefix))
            {
            pages.insert (hit.path, hit.page);
            matches << hit.path.mid (prefix.length ());
            }
      QModelIndex found = finishFileSearch (path, rootPath, matches, meas,
                                            pages);

      done (NULL, found, matches.size (), complete);
   };

   if (query.isEmpty ())
      {
      finish (QList<RemoteBackend::TextHit> (), true);
      return;
      }

   bool can = remote->hasTextSearch ();

   remote->searchText (QFileInfo (root).fileName (), text, dirRel, 1000, this,
                       [this, can, finish, done] (const QString &error,
                          const QList<RemoteBackend::TextHit> &hits,
                          bool complete) {
      if (_closing)
         return;
      if (!error.isEmpty ())
         {
         done (err_make (ERRFN, ERR_remote_op_failed2, "search",
               qPrintable (can ? error
                  : tr ("the server does not search the text on the pages; "
                        "it needs a newer paperman-server, run with "
                        "--index"))), QModelIndex (), 0, true);
         return;
         }
      finish (hits, complete);
   });
   }


err_info *Desktopmodel::findLocalText (const QString &path,
      const QString &root, const QString &rootPath, const QString &query,
      Measure *meas, Operation *op, QModelIndex &found, int &count)
   {
   QStringList matches;          // relative to the folder searched
   QHash<QString, int> pages;    // by the stack's path in the repository

   count = 0;

   // a local repository keeps its index at the top, next to the stacks
   if (!QFileInfo (root).isDir ())
      return err_make (ERRFN, ERR_remote_op_failed2, "search",
            qPrintable (tr ("cannot search the text in %1").arg (root)));

   SearchIndex search;
   QList<SearchResult> results;

   CALL (search.init (root));
   if (!query.isEmpty ())
      {
      CALL (search.sync (path, [op] (int done, int total) {
         if (!op)
            return true;
         op->setCount (total);
         return !op->setProgress (done);
         }));
      CALL (search.search (query, results, 1000, path));
      }

   QString top = QDir (root).absolutePath () + "/";
   QString dir = QDir (path).absolutePath () + "/";

   for (const SearchResult &res : results)
      {
      QString rel = res.filepath.mid (top.length ());

      if (pages.contains (rel) || !res.filepath.startsWith (dir))
         continue;
      pages.insert (rel, res.pagenum);
      matches << res.filepath.mid (dir.length ());
      }

   found = finishFileSearch (path, rootPath, matches, meas, pages);

   // a local stack is read, which makes its preview, and turned to its page
   for (int row = 0; row < rowCount (found); row++)
      {
      QModelIndex ind = index (row, 0, found);
      File *f = getFile (ind);
      QString rel = f ? QFileInfo (f->pathname ()).absoluteFilePath ()
                           .mid (top.length ()) : QString ();

      if (pages.contains (rel))
         showAtPage (ind, pages.value (rel));
      }
   count = matches.size ();
   return NULL;
   }


void Desktopmodel::showAtPage (const QModelIndex &ind, int pagenum)
   {
   File *f = getFile (ind);

   // a stack not read yet has no pages, and is not turned to one
   if (f && !f->valid ())
      buildItem (ind);
   setData (ind, pagenum, Role_pagenum);
   }


void Desktopmodel::getPageOcr (const QModelIndex &ind, int pagenum,
                               OcrPage &page)
   {
   File *f = getFile (ind);

   page = OcrPage ();
   /* a remote stack may not have been fetched yet, nor this page of it;
      the page view asks again once it has */
   if (f && pageHere (ind, pagenum) && !ensureContent (ind)
       && !ensurePage (ind, pagenum))
      f->getPageOcr (pagenum, page);
   }


bool Desktopmodel::pageHere (const QModelIndex &ind, int pagenum)
   {
   File *f = getFile (ind);
   RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

   if (!remote)
      return true;
   if (!f->remoteChecked ())
      return false;

   Filemax *max = sparseFor (f, remote, f->desk ());
   return !max || max->hasPage (pagenum);
   }


void Desktopmodel::dropWorkCopy (File *copy)
   {
   if (!copy)
      return;

   QString pathname = copy->pathname ();

   delete copy;
   QFile::remove (pathname);
   }


void Desktopmodel::uploadScanStack (File *f, OpDone done)
   {
   Desk *desk = f->desk ();
   QPointer<RemoteBackend> remote (remoteForFile (f));
   QString repo = desk->repoName ();
   QString path = remoteStackPath (desk, f);
   QString dir = desk->dir ();
   QString name = f->filename ();
   QString cached = f->pathname ();

   // the stack is all here, so nothing need fetch it while it goes up
   f->setRemoteChecked (true);

   queueOp ([this, remote, repo, path, dir, name, cached] (OpDone opDone) {
      QFile in (cached);

      if (!remote)
         {
         opDone (NULL);
         return;
         }
      if (!in.open (QIODevice::ReadOnly))
         {
         opDone (err_make (ERRFN, ERR_cannot_open_file1,
                           qPrintable (cached)));
         return;
         }
      remote->uploadFile (repo, path, in.readAll (), false, this,
                          [this, dir, name, opDone] (
                             const RemoteBackend::Result &result) {
         if (!result.ok)
            {
            opDone (err_make (ERRFN, ERR_remote_op_failed2, "upload",
                              qPrintable (result.error)));
            return;
            }

         QJsonObject reply = result.json ();
         QString finalName = reply.value ("name").toString ();
         QString etag = reply.value ("etag").toString ();
         File *f = getFile (stackFor (dir, name));

         if (_closing || !f)
            {
            opDone (NULL);
            return;
            }
         if (!finalName.isEmpty () && finalName != f->filename ())
            {
            QString oldCache = f->pathname ();

            f->updateFilename (finalName);
            QFile::rename (oldCache, f->pathname ());
            }

         // record the server's validator so the next open costs a 304
         if (!etag.isEmpty ())
            {
            QFile ef (f->pathname () + ".etag");

            if (ef.open (QIODevice::WriteOnly))
               ef.write (etag.toUtf8 ());
            }
         opDone (NULL);
      });
   }, done, true);
   }


void Desktopmodel::finishOps (int timeoutMs)
   {
   if (!opsPending ())
      return;

   QElapsedTimer timer;
   QEventLoop loop;
   QTimer poll;

   timer.start ();
   connect (&poll, &QTimer::timeout, &loop, [&] () {
      if (!opsPending () || timer.elapsed () >= timeoutMs)
         loop.quit ();
   });
   poll.start (20);
   BlockWatch::note ("finish changes");
   loop.exec ();
   }


err_info *Desktopmodel::ensureContent (const QModelIndex &ind)
   {
   File *f = getFile (ind);

   if (!f)
      return NULL;
   Desk *desk = f->desk ();
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;
   if (!remote)
      return NULL;   // local desk: buildItem() has already loaded it

   // checked once already this session; the parsed state is current
   if (f->remoteChecked ())
      return NULL;

   /* A server that serves pages needs only the stack's structure
      fetched here; a page is fetched when something reads it.  This is
      the blocking path, used by operations that run on the GUI thread
      and can wait; the view uses requestContent() instead. */
   if (sparseFor (f, remote, desk))
      {
      if (remote->fetchStackInfo (desk->repoName (),
                                  remoteStackPath (desk, f)).isEmpty ())
         return err_make (ERRFN, ERR_remote_fetch_failed1,
                          qPrintable (remote->lastError ()));
      CALL (f->reload ());
      f->setValid (true);
      _minor_change = true;
      buildItem (ind);
      _minor_change = false;
      f->setRemoteChecked (true);
      return NULL;
      }

   bool refreshed = false;

   if (remote->ensureCachedFile (desk->repoName (),
                                 remoteStackPath (desk, f),
                                 &refreshed).isEmpty ())
      return err_make (ERRFN, ERR_remote_fetch_failed1,
                       qPrintable (remote->lastError ()));

   /* Parse the cached bytes.  A stack shown before ever being fetched
      is a valid-but-empty shell (see buildItem()), and one cached by
      an earlier session may have been parsed before this revalidation
      replaced the bytes: reload() covers both, discarding any stale
      state.  If the copy was already current and parsed, it's a
      no-op-ish re-read of an unchanged local file. */
   if (refreshed || !f->pagecount ())
      {
      CALL (f->reload ());
      f->setValid (true);

      /* the grid item now knows its page count and can render its
         own preview; refresh it without disturbing any selection */
      _minor_change = true;
      buildItem (ind);
      _minor_change = false;
      }
   f->setRemoteChecked (true);
   return NULL;
   }


/* Fetch one page of a stack held a page at a time, if it is not here.
   The blocking companion of requestPage(), for readers on the GUI
   thread that need the page now; the render thread must never get
   here, and getScaledImageData() sees to that. */
err_info *Desktopmodel::ensurePage (const QModelIndex &ind, int pagenum)
   {
   File *f = getFile (ind);

   if (!f)
      return NULL;
   Desk *desk = f->desk ();
   RemoteBackend *remote = remoteForFile (f);
   Filemax *max = sparseFor (f, remote, desk);
   if (!max || max->hasPage (pagenum))
      return NULL;

   if (remote->fetchPage (desk->repoName (), remoteStackPath (desk, f),
                          pagenum + 1).isEmpty ())
      return err_make (ERRFN, ERR_remote_fetch_failed1,
                       qPrintable (remote->lastError ()));
   max->pageArrived (pagenum);
   return NULL;
   }


err_info *Desktopmodel::remoteStackChanged (const QModelIndex &ind,
                                            const QList<int> &pages)
   {
   File *f = getFile (ind);

   if (!f)
      return NULL;
   Desk *desk = f->desk ();
   RemoteBackend *remote = remoteForFile (f);
   Filemax *max = sparseFor (f, remote, desk);
   if (!max)
      return NULL;

   // as in stackChangedLater()
   f->setListedVersion (QString ());

   QString path = remoteStackPath (desk, f);

   if (pages.isEmpty ())
      remote->invalidatePageDir (desk->repoName (), path);
   else
      for (int page : pages)
         remote->invalidatePage (desk->repoName (), path, page + 1);

   if (remote->fetchStackInfo (desk->repoName (), path).isEmpty ())
      {
      f->setRemoteChecked (false);   // fetched again on the next read
      return err_make (ERRFN, ERR_remote_fetch_failed1,
                       qPrintable (remote->lastError ()));
      }
      {
      QMutexLocker locker (&_imageMutex);

      CALL (max->reload ());
      }
   f->setRemoteChecked (true);
   _minor_change = true;
   buildItem (ind);
   _minor_change = false;
   return NULL;
   }


void Desktopmodel::opTransformPageRemote (File *file, Desk *desk,
      RemoteBackend *backend, int pagenum, File::e_transform op, OpDone done)
   {
   /* The wire page number is 1-based; a value below 1 means every page,
      which is how the all-pages case (pagenum == -1) is expressed. */
   int wirePage = pagenum < 0 ? 0 : pagenum + 1;
   QString dir = desk->dir ();
   QString fname = file->filename ();

   backend->transformPage (desk->repoName (), remoteStackPath (desk, file),
                           wirePage, File::transformName (op), this,
                           [this, dir, fname, pagenum, op, done] (
                              const RemoteBackend::Result &result) {
      if (_closing)
         return;      // the model is going, with what it showed
      if (!result.ok)
         {
         done (err_make (ERRFN, ERR_remote_transform_failed1,
                         qPrintable (result.error)));
         return;
         }

      QModelIndex index = stackFor (dir, fname);
      File *file = getFile (index);
      if (!file)
         {
         done (NULL);
         return;
         }
      Desk *desk = file->desk ();
      RemoteBackend *backend = remoteForFile (file);
      QString stackPath = remoteStackPath (desk, file);

      /* A stack held a page at a time cannot be turned locally: its
         changed page comes back from the server on the next read. */
      if (sparseFor (file, backend, desk))
         {
         QList<int> pages;

         if (pagenum >= 0)
            pages << pagenum;
         stackChangedLater (index, pages);
         refreshRemoteThumbnail (desk, backend, file);
         done (NULL);
         return;
         }

      /* Keep the cached copy in step with the server so an open page
         view shows the turn at once: apply the same transform to the
         already-parsed local bytes.  If the stack hasn't been fetched
         (or the local apply fails), drop any cached copy instead; the
         next ensureContent() downloads the transformed file. */
      if (file->valid () && file->pagecount ())
         {
         err_info *e = NULL;

         if (pagenum == -1)
            {
            for (int page = 0; !e && page < file->pagecount (); page++)
               e = file->transformPage (page, op);
            }
         else
            e = file->transformPage (pagenum, op);
         if (e)
            {
            qInfo () << "local cache transform failed:" << e->errstr;
            backend->invalidateCachedFile (desk->repoName (), stackPath);
            file->setRemoteChecked (false);
            }
         }
      else
         {
         backend->invalidateCachedFile (desk->repoName (), stackPath);
         file->setRemoteChecked (false);
         }

      /* refresh the grid thumbnail from the server, and let any open page
         view know the content changed */
      refreshRemoteThumbnail (desk, backend, file);
      emit pageContentChanged (index, pagenum);
      done (NULL);
   });
   }


err_info *Desktopmodel::copyFilesToClipboard (QString &fname,
      QStringList &fnamelist, bool &can_delete)
{
   can_delete = true;

// if we have more than one file, use a zip
   if (fnamelist.size () == 1)
      {
      fname = fnamelist [0];
      can_delete = false;   // we need to keep the file on the clipboard
      }
   else
      CALL (util_buildZip (fname, fnamelist));

   QUrl url = QUrl::fromLocalFile (fname);

   // Put the file on the clipboard as a text/uri-list so Gmail compose
   // (and any other webmail that accepts file paste) treats Ctrl+V as a
   // file attach.  mailto: URLs cannot carry attachments per RFC 6068,
   // so this is the only path that lands a real attachment in Gmail
   // without a per-user OAuth dance.
   QMimeData *mime = new QMimeData ();
   mime->setUrls (QList<QUrl> () << url);

   /* A bare text/uri-list is enough for a browser, but file managers
      need a copy/cut marker to paste the file.  Provide both the GNOME
      and KDE flavours so Files, Nautilus, Dolphin and the like treat
      Ctrl+V as a file copy. */
   mime->setData ("x-special/gnome-copied-files",
         QByteArray ("copy\n") + url.toEncoded ());
   mime->setData ("application/x-kde-cutselection", QByteArray ("0"));

   QApplication::clipboard ()->setMimeData (mime);

   return NULL;
}


err_info *Desktopmodel::emailFiles (QString &fname, QStringList &fnamelist, bool &can_delete)
{
   CALL (copyFilesToClipboard (fname, fnamelist, can_delete));

   // Open Gmail's compose URL with a pre-filled subject and body.  The
   // attachment cannot ride on the URL, but the user pastes it with
   // Ctrl+V once the compose window is up.
   QUrl compose ("https://mail.google.com/mail/?view=cm&fs=1"
                 "&su=Files&body=see+attachment+(paste+with+Ctrl%2BV)");
   if (url_capture)
      *url_capture = compose;
   else
      QDesktopServices::openUrl (compose);

   return NULL;
}


err_info *Desktopmodel::packageFiles (QModelIndexList &slist,
      File::e_type type, const QHash<QString, QString> &copies,
      Operation &op, QString &fname, QStringList &fname_list,
      QStringList &tmp_list)
{
   int upto = 0;

   // name of first file will be used for the zip name
   fname = getFile (slist [0])->filename ();
   QString dir = QDir::tempPath () + "/";

   // put the path on front of each file
   foreach (QModelIndex ind, slist)
   {
      File *f = getFile (ind);
      File *fnew;
      File *copy = nullptr;

      /* a remote stack may not be here, or only some of its pages, so
         work from the copy of the whole of it fetched first */
      if (remoteForFile (f))
         {
         QFileInfo fi (copies.value (f->pathname ()));

         if (fi.fileName ().isEmpty ())
            return err_make (ERRFN, ERR_cannot_open_file1,
                             qPrintable (f->filename ()));
         copy = File::createFile (fi.absolutePath () + "/", fi.fileName (),
                                  nullptr, File::typeFromName (fi.fileName ()));
         if (!copy)
            return err_make (ERRFN, ERR_cannot_open_file1,
                             qPrintable (fi.filePath ()));
         tmp_list << copy->pathname ();
         if (type != File::Type_other)
            {
            err_info *err = copy->load ();

            if (err)
               {
               delete copy;
               return err;
               }
            f = copy;
            }
         }

      // if just copying, then get the filename
      if (type == File::Type_other)
         {
         fname_list << (copy ? copy->pathname () : f->pathname ());
         delete copy;   // the file stays, until it has been sent
         }

      // else convert this file into a temporary one of the right type
      else
      {
         QString uniq;

         QString ext = File::typeExt (type);
         uniq = util_findNextFilename (f->leaf (), dir, ext);
         err_info *err = f->duplicateToDesk (0, type, uniq, 3, op, fnew);

         if (copy)
            dropWorkCopy (copy);
         CALL (err);
         fname_list << uniq;
         tmp_list << uniq;
      }
      op.setProgress (upto++);
   }
   return NULL;
}


void Desktopmodel::workCopiesLater (const QModelIndexList &list,
      std::function<void (err_info *err,
                          const QHash<QString, QString> &copies)> done)
   {
   struct Want
      {
      QString pathname;    //!< the stack, as the model knows it
      QString repo;
      QString path;        //!< in the repository
      QString leaf;
      QString ext;
      };
   QPointer<RemoteBackend> server;
   auto wants = std::make_shared<QList<Want> > ();
   auto copies = std::make_shared<QHash<QString, QString> > ();

   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);
      RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

      if (!remote)
         continue;
      server = remote;
      *wants << Want {f->pathname (), f->desk ()->repoName (),
                      remoteStackPath (f->desk (), f), f->leaf (), f->ext ()};
      }

   /* one at a time, so that each copy is there before the next is
      named, and two stacks of the same name get copies of their own.
      Each download holds the chain until it is done */
   auto next = std::make_shared<std::function<void ()> > ();
   std::weak_ptr<std::function<void ()> > weak = next;

   *next = [this, server, wants, copies, done, weak] () {
      auto self = weak.lock ();

      if (wants->isEmpty ())
         {
         done (NULL, *copies);
         return;
         }
      if (!server)
         {
         done (err_make (ERRFN, ERR_remote_fetch_failed1,
                         "the server has gone"), *copies);
         return;
         }

      Want want = wants->takeFirst ();
      QString dir = QDir::tempPath () + "/";
      QString dest = dir + util_findNextFilename (want.leaf, dir, want.ext)
                     + want.ext;

      // hold the file's name, so the next copy does not take it
      QFile (dest).open (QIODevice::WriteOnly);
      copies->insert (want.pathname, dest);
      server->downloadFile (want.repo, want.path, dest, this,
                            [copies, done, self] (
                               const RemoteBackend::Result &result) {
         if (!result.ok)
            done (err_make (ERRFN, ERR_remote_fetch_failed1,
                            qPrintable (result.error)), *copies);
         else
            (*self) ();
      });
   };
   (*next) ();
   }


err_info *Desktopmodel::sendFiles (QModelIndexList &slist, File::e_type type,
      const QHash<QString, QString> &copies, bool clipboard)
{
   QString fname;
   QStringList fname_list, tmp_list;
   Operation op ("Packaging", slist.size (), 0);

   err_info *e = packageFiles (slist, type, copies, op, fname, fname_list,
                               tmp_list);
   bool can_delete = true;

   if (!e)
      e = clipboard ? copyFilesToClipboard (fname, fname_list, can_delete)
                    : emailFiles (fname, fname_list, can_delete);

   /* delete the files if we are allowed, with any copy fetched from a
      server which was not used */
   if (can_delete)
      for (const QString &name : tmp_list)
         QFile::remove (name);
   for (const QString &copy : copies)
      if (!tmp_list.contains (copy))
         QFile::remove (copy);
   return e;
}


void Desktopmodel::sendFilesLater (const QModelIndexList &slist,
      File::e_type type, bool clipboard, OpDone done)
   {
   QList<QPersistentModelIndex> stacks;

   for (const QModelIndex &ind : slist)
      stacks << QPersistentModelIndex (ind);
   workCopiesLater (slist, [this, stacks, type, clipboard, done] (
                       err_info *err, const QHash<QString, QString> &copies) {
      QModelIndexList list;

      for (const QPersistentModelIndex &ind : stacks)
         if (ind.isValid ())
            list << ind;
      if (err || list.isEmpty ())
         {
         for (const QString &copy : copies)
            QFile::remove (copy);
         done (err);
         return;
         }
      done (sendFiles (list, type, copies, clipboard));
   });
   }


err_info *Desktopmodel::opEmailFiles (QModelIndex parent, QModelIndexList &slist,
      File::e_type type)
{
   UNUSED (parent);
   return waitForOp ("email", [&] (OpDone done) {
      sendFilesLater (slist, type, false, done);
   });
}


err_info *Desktopmodel::opCopyFiles (QModelIndex parent, QModelIndexList &slist,
      File::e_type type)
{
   UNUSED (parent);
   return waitForOp ("copy", [&] (OpDone done) {
      sendFilesLater (slist, type, true, done);
   });
}


void Desktopmodel::opUpdateRepositoryList (QString &dirpath, bool add_not_delete)
   {
   // Tell our controller (Desktopwidget) what to do
   emit updateRepositoryList (dirpath, add_not_delete);
   }


void Desktopmodel::queueOp (std::function<void (OpDone done)> op,
                            OpDone report, bool remote)
   {
   if (!remote)
      {
      op (report);
      return;
      }
   _ops.enqueue ([this, op, report] () {
      op ([this, report] (err_info *err) {
         // a window closing with an operation out has no one to tell
         if (!_closing)
            report (err);
         _opBusy = false;

         // on to the next, though not from inside this one
         QMetaObject::invokeMethod (this, [this] () { runOps (); },
                                    Qt::QueuedConnection);
      });
   });
   runOps ();
   }


void Desktopmodel::runOps (void)
   {
   if (_opBusy || _ops.isEmpty () || _closing)
      return;
   _opBusy = true;
   _ops.dequeue () ();
   }


err_info *Desktopmodel::waitForOp (const QString &what,
                                   std::function<void (OpDone done)> op)
   {
   err_info *err = NULL;
   bool finished = false;
   QEventLoop loop;

   op ([&] (err_info *e) {
      err = e;
      finished = true;
      loop.quit ();
   });
   if (!finished)
      {
      BlockWatch::note (what);
      loop.exec ();
      }
   return err;
   }


bool Desktopmodel::isRemoteDir (const QString &dir)
   {
   QString path = dir;
   QModelIndex parent = deskFromDirname (path);
   Desk *desk = parent.isValid () ? getDesk (parent) : nullptr;

   return desk && dynamic_cast<RemoteBackend *> (desk->backend ());
   }


QModelIndex Desktopmodel::stackFor (const QString &dir, const QString &fname)
   {
   QString path = dir;
   QModelIndex parent = deskFromDirname (path);

   return parent.isValid () ? index (fname, parent) : QModelIndex ();
   }


void Desktopmodel::stackChangedLater (const QModelIndex &ind,
                                      const QList<int> &pages)
   {
   File *f = getFile (ind);
   RemoteBackend *remote = f ? remoteForFile (f) : nullptr;
   Filemax *max = remote ? sparseFor (f, remote, f->desk ()) : nullptr;

   if (!max)
      return;

   /* the version the folder's listing gave is out of date, so images of
      its pages in the cache are too, until the folder is listed again */
   f->setListedVersion (QString ());

   Desk *desk = f->desk ();
   QString path = remoteStackPath (desk, f);

   if (pages.isEmpty ())
      remote->invalidatePageDir (desk->repoName (), path);
   else
      for (int page : pages)
         if (page >= 0)
            remote->invalidatePage (desk->repoName (), path, page + 1);

   /* the structure comes again, and onStackInfoReady() reads it and
      has the stack shown afresh */
   f->setRemoteChecked (false);
   requestContent (ind);
   }


/* A move of stacks on a server to or from the trash (or another folder),
   one stack at a time, keeping what it needs to finish once the last
   has gone, since the desks may have changed by then */
struct Desktopmodel::TrashJob
   {
   bool back = false;      //!< moving back from the trash
   QPointer<RemoteBackend> remote;
   QString repo;           //!< the repository
   QString dir;            //!< the desk the stacks are in, or go back to
   QString src;            //!< the folder they come back from (back)
   QString dest;           //!< the folder they go to, as a desk path
   QString destRel;        //!< and relative to the repository
   QStringList names;      //!< the stacks, by name in their folder
   QStringList paths;      //!< and by path in the repository
   bool copy = false;      //!< copy rather than move
   std::shared_ptr<QStringList> got;  //!< the names each gets
   QList<pagepos_info> pagepos;       //!< where each goes back to (back)
   bool havePagepos = false;
   QStringList done;       //!< the stacks moved so far, by name
   int next = 0;           //!< the next stack to move
   OpDone report;          //!< called once all have gone, or one fails
   };


void Desktopmodel::opTrashStacks (const QModelIndexList &list,
                                  const QModelIndex &parent,
                                  std::shared_ptr<QStringList> trashlist,
                                  const QString &dest, bool copy, OpDone done)
   {
   Desk *desk = getDesk (parent);
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   if (!remote)
      {
      QModelIndexList stacks = list;
      QString to = dest;

      done (opTrashStacks (stacks, parent, *trashlist, to, copy));
      return;
      }

   /* Trash and move-to-dir both become a server-side move; the shared
      trash is a directory at the repository root */
   auto job = std::make_shared<TrashJob> ();

   job->dest = dest.isEmpty () ? desk->rootDir () + RemoteBackend::trashDirName ()
                               : dest;
   if (!job->dest.startsWith (desk->rootDir ()))
      {
      done (err_make (ERRFN, ERR_cannot_move_between_repositories));
      return;
      }
   job->remote = remote;
   job->repo = desk->repoName ();
   job->dir = desk->dir ();
   job->destRel = remoteRelDir (desk, job->dest);
   job->copy = copy;
   job->got = trashlist;
   job->report = done;
   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);

      if (!f)
         continue;
      job->names << f->filename ();
      job->paths << remoteStackPath (desk, f);
      }
   trashNext (job);
   }


void Desktopmodel::opUntrashStacks (const QStringList &trashlist,
                                    const QModelIndex &parent,
                                    std::shared_ptr<QStringList> filenames,
                                    const QList<pagepos_info> &pagepos,
                                    bool havePagepos, const QString &src,
                                    bool copy, OpDone done)
   {
   Desk *desk = getDesk (parent);
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   if (!remote)
      {
      QStringList trash = trashlist;
      QList<pagepos_info> pos = pagepos;
      QString from = src;

      done (opUntrashStacks (trash, parent, *filenames,
                             havePagepos ? &pos : nullptr, from, copy));
      return;
      }

   /* Reverse of opTrashStacks(): move each file back from the source
      directory (the trash, unless this was a move-to-dir) into this
      desk, or for a copy just delete the copy again */
   auto job = std::make_shared<TrashJob> ();
   QString srcRel = src.isEmpty () ? QString (RemoteBackend::trashDirName ())
                                   : remoteRelDir (desk, src);

   job->back = true;
   job->remote = remote;
   job->repo = desk->repoName ();
   job->dir = desk->dir ();
   job->src = src;
   job->destRel = remoteRelDir (desk, desk->dir ());
   job->copy = copy;
   job->got = filenames;
   job->pagepos = pagepos;
   job->havePagepos = havePagepos;
   job->report = done;
   job->names = trashlist;
   for (const QString &name : trashlist)
      job->paths << (srcRel.isEmpty () ? name : srcRel + "/" + name);
   trashNext (job);
   }


void Desktopmodel::trashNext (std::shared_ptr<TrashJob> job)
   {
   if (!job->remote)
      return;      // the server has gone, and the desks with it
   if (job->next == job->paths.size ())
      {
      finishTrash (job, NULL);
      return;
      }

   int i = job->next++;
   RemoteBackend *remote = job->remote;
   QString path = job->paths [i];

   // taking a copy back is just deleting it
   if (job->back && job->copy)
      {
      remote->deleteStack (job->repo, path, this,
                           [this, job, path] (
                              const RemoteBackend::Result &result) {
         if (_closing)
            return;      // the model is going, with what it showed
         if (!result.ok)
            {
            finishTrash (job, err_make (ERRFN, ERR_remote_op_failed2,
                                        "delete", qPrintable (result.error)));
            return;
            }
         if (job->remote)
            job->remote->invalidateCachedFile (job->repo, path);
         job->done << job->names [job->next - 1];
         trashNext (job);
      });
      return;
      }

   remote->moveStack (job->repo, path, job->destRel, !job->back && job->copy,
                      this, [this, job, i, path] (
                         const RemoteBackend::Result &result) {
      if (_closing)
         return;      // the model is going, with what it showed
      if (!result.ok)
         {
         finishTrash (job, err_make (ERRFN, ERR_remote_op_failed2, "move",
                                     qPrintable (result.error)));
         return;
         }
      QString name = result.json ().value ("name").toString ();

      if (job->remote && (job->back || !job->copy))
         job->remote->invalidateCachedFile (job->repo, path);
      if (job->back)
         {
         if (i < job->got->size ())
            (*job->got) [i] = name;
         }
      else
         *job->got << name;
      job->done << job->names [i];
      trashNext (job);
   });
   }


void Desktopmodel::finishTrash (std::shared_ptr<TrashJob> job, err_info *err)
   {
   QString dir = job->dir;
   QModelIndex parent = deskFromDirname (dir);
   Desk *desk = parent.isValid () ? getDesk (parent) : nullptr;

   if (!job->back)
      {
      /* take the stacks which went out of the desk, and show them where
         they went, if that desk is loaded */
      if (desk && !job->copy)
         {
         QModelIndexList del_list;

         for (const QString &name : job->done)
            {
            QModelIndex ind = index (name, parent);

            if (ind.isValid ())
               del_list << ind;
            }
         sortForDelete (del_list);
         for (const QModelIndex &ind : del_list)
            removeRows (ind.row (), 1, parent);
         }
      QStringList trashlist = *job->got;
      addFilesToDesk (job->dest, trashlist);
      job->report (err);
      return;
      }

   /* put back each stack which came back, where it was and at the page
      it was turned to */
   QList<File *> flist;

   if (desk && !job->copy)
      for (int i = 0; i < job->done.size (); i++)
         {
         File *fnew = desk->createFile (desk->dir (), (*job->got) [i]);

         desk->newFile (fnew);
         if (job->havePagepos && i < job->pagepos.size ())
            {
            fnew->setPos (job->pagepos [i].pos);
            fnew->setPagenum (job->pagepos [i].pagenum);
            }
         flist << fnew;
         if (job->remote)
            refreshRemoteThumbnail (desk, job->remote, fnew);
         }
   if (!flist.isEmpty ())
      insertRows (flist, parent);
   QString src = job->src;
   QStringList moved = job->done;
   removeFilesFromDesk (src, moved);
   job->report (err);
   }


/* Requests to a server for one operation, made one after another, each
   once the one before has been answered */
struct Desktopmodel::Steps
   {
   const char *what = "";   //!< the operation, for an error
   //! each request, which calls the function it is given with its result
   QList<std::function<void (RemoteBackend::Done)> > requests;
   //! takes each result in turn, by the request's position
   std::function<void (int i, const RemoteBackend::Result &)> each;
   //! called once all are done, or one fails, with how many succeeded
   std::function<void (int done, err_info *err)> finish;
   int next = 0;            //!< the next request to make
   };


void Desktopmodel::runSteps (std::shared_ptr<Steps> steps)
   {
   if (_closing)
      return;      // the model is going, with what it showed
   if (steps->next == steps->requests.size ())
      {
      steps->finish (steps->next, NULL);
      return;
      }

   int i = steps->next++;
   steps->requests [i] ([this, steps, i] (const RemoteBackend::Result &result) {
      if (_closing)
         return;
      if (!result.ok)
         {
         steps->finish (i, err_make (ERRFN, ERR_remote_op_failed2,
                                     steps->what, qPrintable (result.error)));
         return;
         }
      if (steps->each)
         steps->each (i, result);
      runSteps (steps);
   });
   }


void Desktopmodel::stackChangedOnServer (const QModelIndex &ind)
   {
   File *f = getFile (ind);
   RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

   if (!remote)
      return;

   // as in stackChangedLater()
   f->setListedVersion (QString ());
   Desk *desk = f->desk ();

   if (sparseFor (f, remote, desk))
      stackChangedLater (ind, QList<int> ());
   else
      {
      remote->invalidateCachedFile (desk->repoName (),
                                    remoteStackPath (desk, f));
      f->setRemoteChecked (false);
      requestContent (ind);
      }
   refreshRemoteThumbnail (desk, remote, f);
   }


void Desktopmodel::opDeleteStacks (const QModelIndexList &list,
                                   const QModelIndex &parent, OpDone done)
   {
   File *first = list.isEmpty () ? nullptr : getFile (list.first ());
   RemoteBackend *remote = first ? remoteForFile (first) : nullptr;

   if (!remote)
      {
      QModelIndexList stacks = list;

      done (opDeleteStacks (stacks, parent));
      return;
      }

   Desk *desk = first->desk ();
   QPointer<RemoteBackend> server (remote);
   QString repo = desk->repoName ();
   QString dir = desk->dir ();
   QStringList names, paths;
   auto steps = std::make_shared<Steps> ();

   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);

      if (!f)
         continue;
      names << f->filename ();
      paths << remoteStackPath (desk, f);
      }
   steps->what = "delete";
   for (const QString &path : paths)
      steps->requests << [this, server, repo, path] (RemoteBackend::Done d) {
         if (server)
            server->deleteStack (repo, path, this, d);
      };
   steps->finish = [this, server, repo, dir, names, paths, done] (int count,
                                                                  err_info *err) {
      QString path = dir;
      QModelIndex parent = deskFromDirname (path);
      QModelIndexList del_list;

      for (int i = 0; i < count; i++)
         {
         QModelIndex ind = parent.isValid () ? index (names [i], parent)
                                             : QModelIndex ();

         if (server)
            server->invalidateCachedFile (repo, paths [i]);
         if (ind.isValid ())
            del_list << ind;
         }
      sortForDelete (del_list);
      for (const QModelIndex &ind : del_list)
         removeRow (ind.row (), ind.parent ());
      done (err);
   };
   runSteps (steps);
   }


void Desktopmodel::opRestackStacks (const QModelIndexList &list,
                                    const QModelIndex &parent,
                                    const QList<QModelIndexList> &pages_list,
                                    std::shared_ptr<int> destpage, OpDone done)
   {
   Desk *desk = getDesk (parent);
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   if (!remote)
      {
      QModelIndexList dests = list;
      QList<QModelIndexList> pages = pages_list;

      done (opRestackStacks (dests, parent, pages, *destpage));
      return;
      }

   /* Stack on the server, which deletes the source files itself.  The
      destination is fetched afresh afterwards rather than mirrored,
      since the sources may not all be in the local cache */
   QPointer<RemoteBackend> server (remote);
   QString repo = desk->repoName ();
   QString dir = desk->dir ();
   QStringList dests;
   QList<QStringList> sourceNames;
   QList<QStringList> sourcePaths;
   QList<int> pages;
   int in_destpage = *destpage;
   int out_destpage = in_destpage;
   int upto = 0;

   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);
      QStringList names, paths;

      if (!f)
         {
         upto++;
         continue;
         }
      int page = in_destpage == -1 ? f->pagenum () : in_destpage;

      if (out_destpage == -1)
         out_destpage = page;
      for (const QModelIndex &pind : pages_list.value (upto++))
         if (pind.isValid ())
            {
            names << getFile (pind)->filename ();
            paths << remoteStackPath (desk, getFile (pind));
            }
      if (paths.isEmpty ())
         continue;
      dests << f->filename ();
      sourceNames << names;
      sourcePaths << paths;
      pages << page;
      }
   *destpage = out_destpage;

   auto steps = std::make_shared<Steps> ();

   steps->what = "stack";
   for (int i = 0; i < dests.size (); i++)
      {
      QString destPath = remoteStackPath (desk, getFile (index (dests [i],
                                                                parent)));
      QStringList paths = sourcePaths [i];
      int page = pages [i] + 1;

      steps->requests << [this, server, repo, destPath, paths, page] (
                            RemoteBackend::Done d) {
         if (server)
            server->stackStacks (repo, destPath, paths, page, this, d);
      };
      }
   steps->finish = [this, server, repo, dir, dests, sourceNames, sourcePaths,
                    done] (int count, err_info *err) {
      QString path = dir;
      QModelIndex parent = deskFromDirname (path);
      QModelIndexList delete_list;

      for (int i = 0; parent.isValid () && i < count; i++)
         {
         stackChangedOnServer (index (dests [i], parent));

         /* the server has already deleted the source files; drop their
            cached copies and rows */
         for (int j = 0; j < sourceNames [i].size (); j++)
            {
            QModelIndex pind = index (sourceNames [i][j], parent);

            if (server)
               server->invalidateCachedFile (repo, sourcePaths [i][j]);
            if (pind.isValid ())
               delete_list << pind;
            }
         }
      sortForDelete (delete_list);
      for (const QModelIndex &pind : delete_list)
         removeRow (pind.row (), pind.parent ());
      done (err);
   };
   runSteps (steps);
   }


void Desktopmodel::opUnstackPage (const QModelIndex &ind,
                                  std::shared_ptr<int> pagenum, bool remove,
                                  std::shared_ptr<QString> newname,
                                  OpDone done)
   {
   File *f = getFile (ind);
   RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

   if (!remote)
      {
      QModelIndex stack = ind;
      int page = *pagenum;
      QString name = *newname;
      err_info *e = opUnstackPage (stack, page, remove, name);

      *pagenum = page;
      *newname = name;
      done (e);
      return;
      }

   if (*pagenum == -1)
      *pagenum = f->pagenum ();
   /* the last page stays, as for a local stack; what is shown may lag an
      earlier change, though, in which case the server refuses it */
   if (remove && f->remoteChecked () && f->pagecount () <= 1)
      {
      done (NULL);
      return;
      }

   Desk *desk = f->desk ();
   QString dir = desk->dir ();
   QString fname = f->filename ();

   /* the server picks the new stack's name, and the new stack is a shell
      which fetches on its first read; the source is fetched again if a
      page left it */
   remote->unstackStack (desk->repoName (), remoteStackPath (desk, f),
                         *pagenum + 1, 1, remove, "", this,
                         [this, dir, fname, remove, newname, done] (
                            const RemoteBackend::Result &result) {
      if (_closing)
         return;
      if (!result.ok)
         {
         done (err_make (ERRFN, ERR_remote_op_failed2, "unstack",
                         qPrintable (result.error)));
         return;
         }
      QString name = result.json ().value ("name").toString ();
      QModelIndex ind = stackFor (dir, fname);
      File *f = getFile (ind);

      *newname = name;
      if (f)
         {
         Desk *desk = f->desk ();
         File *fnew = desk->createFile (desk->dir (), name);
         int row = desk->newFile (fnew, f, 1);
         QModelIndexList list;

         if (remove)
            stackChangedOnServer (ind);
         newItem (row, ind.parent (), list);
         refreshRemoteThumbnail (desk, remoteForFile (f), fnew);
         }
      done (NULL);
   });
   }


void Desktopmodel::opUnstackStacks (const QModelIndexList &list,
                                    const QModelIndex &parent,
                                    std::shared_ptr<QList<QStringList> > newnames,
                                    OpDone done)
   {
   Desk *desk = getDesk (parent);
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   if (!remote)
      {
      QModelIndexList stacks = list;
      QList<QStringList> names;
      err_info *e = opUnstackStacks (stacks, parent, names);

      *newnames << names;
      done (e);
      return;
      }

   /* Unstack all but one of each stack's pages on the server, one at a
      time, always taking the current first page, as the local path does.
      Ask the server how many pages each has first, since what is shown
      may not yet have caught up with an earlier change */
   QPointer<RemoteBackend> server (remote);
   QString repo = desk->repoName ();
   QString dir = desk->dir ();
   QStringList stacks, paths;
   auto steps = std::make_shared<Steps> ();
   auto made = std::make_shared<QList<QStringList> > ();
   auto owner = std::make_shared<QList<int> > ();  // the stack of each request

   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);

      if (!f)
         continue;
      QString path = remoteStackPath (desk, f);

      stacks << f->filename ();
      paths << path;
      *made << QStringList ();
      *owner << -1;
      steps->requests << [this, server, repo, path] (RemoteBackend::Done d) {
         if (server)
            server->stackStructure (repo, path, this, d);
      };
      }
   steps->what = "unstack";
   Steps *run = steps.get ();
   steps->each = [this, run, server, repo, paths, made, owner] (
                    int i, const RemoteBackend::Result &result) {
      int k = (*owner) [i];

      if (k != -1)
         {
         (*made) [k] << result.json ().value ("name").toString ();
         return;
         }

      // a stack's page count has come: unstack all its pages but one
      k = 0;
      for (int j = 0; j < i; j++)
         if ((*owner) [j] == -1)
            k++;
      QString path = paths [k];
      int count = result.json ().value ("pagecount").toInt ();

      for (int p = 1; p < count; p++)
         {
         run->requests.insert (i + p, [this, server, repo, path] (
                                         RemoteBackend::Done d) {
            if (server)
               server->unstackStack (repo, path, 1, 1, true, "", this, d);
         });
         owner->insert (i + p, k);
         }
   };
   steps->finish = [this, dir, stacks, made, newnames, done] (int,
                                                             err_info *err) {
      QString path = dir;
      QModelIndex parent = deskFromDirname (path);
      Desk *desk = parent.isValid () ? getDesk (parent) : nullptr;

      for (int k = 0; k < stacks.size (); k++)
         {
         QModelIndex ind = parent.isValid () ? index (stacks [k], parent)
                                             : QModelIndex ();
         File *f = getFile (ind);
         QList<File *> flist;
         int pagenum = 1;

         *newnames << (*made) [k];
         if (!f || !desk || (*made) [k].isEmpty ())
            continue;
         for (const QString &name : (*made) [k])
            {
            File *fnew = desk->createFile (desk->dir (), name);

            desk->newFile (fnew, f, pagenum++);
            refreshRemoteThumbnail (desk, remoteForFile (f), fnew);
            flist << fnew;
            }
         stackChangedOnServer (ind);
         insertRows (flist, parent);
         }
      done (err);
   };
   runSteps (steps);
   }


/* Makes a stack here from a copy of a remote one and uploads it, for a
   server which cannot convert stacks itself, as one step of an
   operation. It downloads the stack at @path, has @make turn the copy
   into a new stack, and uploads that as @dest, relative to the
   repository, or under the name the server chooses if that is taken,
   which the reply's JSON gives as "name", as a conversion's does. @leaf
   and @ext are the stack's name without its extension, and that, for the
   copy. @done is called with the upload's reply, or why it failed */
static void convertHere (QObject *context, QPointer<RemoteBackend> server,
      const QString &repo, const QString &path, const QString &leaf,
      const QString &ext, const QString &dest,
      std::function<err_info *(File *copy, File *&made)> make,
      RemoteBackend::Done done)
   {
   QString dir = QDir::tempPath () + "/";
   QString name = util_findNextFilename (leaf, dir, ext) + ext;

   if (!server)
      return;
   server->downloadFile (repo, path, dir + name, context,
                         [context, server, repo, dir, name, dest, make, done] (
                            const RemoteBackend::Result &result) {
      if (!result.ok)
         {
         QFile::remove (dir + name);
         done (result);
         return;
         }

      File *copy = File::createFile (dir, name, nullptr,
                                     File::typeFromName (name));
      File *made = nullptr;
      err_info *err = copy ? copy->load ()
            : err_make (ERRFN, ERR_cannot_open_file1, qPrintable (dir + name));
      QByteArray bytes;

      if (!err)
         err = make (copy, made);
      delete copy;
      QFile::remove (dir + name);
      if (!err && !made)
         err = err_make (ERRFN, ERR_cannot_find_any_pages_in_source_document1,
                         qPrintable (name));
      if (made)
         {
         QFile in (made->pathname ());

         if (!err && in.open (QIODevice::ReadOnly))
            bytes = in.readAll ();
         else if (!err)
            err = err_make (ERRFN, ERR_cannot_open_file1,
                            qPrintable (made->pathname ()));
         QString madePath = made->pathname ();

         delete made;
         QFile::remove (madePath);
         }
      if (err || !server)
         {
         RemoteBackend::Result out;

         out.error = err ? QString (err->errstr)
                         : Desktopmodel::tr ("the server has gone");
         done (out);
         return;
         }

      // the reply gives the name the server chose, as a conversion's does
      server->uploadFile (repo, dest, bytes, false, context, done);
   });
   }


void Desktopmodel::opDuplicateStacks (const QModelIndexList &list,
                                      const QModelIndex &parent,
                                      std::shared_ptr<QStringList> namelist,
                                      File::e_type type, int odd_even,
                                      OpDone done)
   {
   Desk *desk = getDesk (parent);
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;
   bool convert = type != File::Type_other || odd_even != 3;

   // a local desk does it at once
   if (!remote)
      {
      QModelIndexList stacks = list;
      QStringList names;
      err_info *e = opDuplicateStacks (stacks, parent, names, type, odd_even);

      *namelist << names;
      done (e);
      return;
      }

   QPointer<RemoteBackend> server (remote);
   QString repo = desk->repoName ();
   QString dir = desk->dir ();
   QString ext = type == File::Type_other ? QString ()
                 : File::typeExt (type).mid (1);
   QStringList sources;
   auto made = std::make_shared<QStringList> ();
   auto steps = std::make_shared<Steps> ();

   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);

      if (!f)
         continue;
      QString path = remoteStackPath (desk, f);

      sources << f->filename ();

      /* a server without conversion has a copy of the stack converted
         here, which goes back up beside it */
      if (convert && !remote->hasConvert ())
         {
         QString dest = remoteRelDir (desk, desk->dir ());
         QString typeExt = File::typeExt (type);

         dest += (dest.isEmpty () ? "" : "/") + f->leaf () + "_copy"
                 + typeExt;
         steps->requests << [this, server, repo, path, dest, type, typeExt,
                             odd_even, leaf = f->leaf (), fext = f->ext ()] (
                               RemoteBackend::Done d) {
            convertHere (this, server, repo, path, leaf, fext, dest,
                         [type, typeExt, odd_even, leaf] (File *copy,
                                                          File *&made) {
               QString dir = QDir::tempPath () + "/";
               QString uniq = util_findNextFilename (leaf + "_copy", dir,
                                                     typeExt);
               Operation op (tr ("Convert to %1").arg (File::typeName (type)),
                             copy->pagecount (), 0);

               return copy->duplicateToDesk (0, type, uniq, odd_even, op,
                                             made);
            }, d);
         };
         continue;
         }
      steps->requests << [this, server, repo, path, convert, ext, odd_even] (
                            RemoteBackend::Done d) {
         if (!server)
            return;
         if (convert)
            server->convertStack (repo, path, ext, odd_even, this, d);
         else
            server->duplicateStack (repo, path, this, d);
      };
      }
   steps->what = convert ? "convert" : "duplicate";
   steps->each = [made] (int, const RemoteBackend::Result &result) {
      *made << result.json ().value ("name").toString ();
   };
   steps->finish = [this, dir, sources, made, namelist, done] (int,
                                                              err_info *err) {
      addMadeStacks (dir, sources, *made, namelist, err, done);
   };
   runSteps (steps);
   }


void Desktopmodel::opUnfoldBooklets (const QModelIndexList &list,
                                     const QModelIndex &parent,
                                     std::shared_ptr<QStringList> namelist,
                                     OpDone done)
   {
   Desk *desk = getDesk (parent);
   RemoteBackend *remote = desk
         ? dynamic_cast<RemoteBackend *> (desk->backend ()) : nullptr;

   // a local desk does it at once
   if (!remote)
      {
      QModelIndexList stacks = list;
      QStringList names;
      err_info *e = opUnfoldBooklets (stacks, parent, names);

      *namelist << names;
      done (e);
      return;
      }

   QPointer<RemoteBackend> server (remote);
   QString repo = desk->repoName ();
   QString dir = desk->dir ();
   QStringList sources;
   auto made = std::make_shared<QStringList> ();
   auto steps = std::make_shared<Steps> ();

   for (const QModelIndex &ind : list)
      {
      File *f = getFile (ind);

      if (!f)
         continue;
      QString path = remoteStackPath (desk, f);

      sources << f->filename ();

      /* a server without conversion has a copy of the booklet unfolded
         here, which goes back up beside it */
      if (!remote->hasConvert ())
         {
         QString dest = remoteRelDir (desk, desk->dir ());

         dest += (dest.isEmpty () ? "" : "/") + f->leaf () + "_unfold.max";
         steps->requests << [this, server, repo, path, dest,
                             leaf = f->leaf (), fext = f->ext ()] (
                               RemoteBackend::Done d) {
            convertHere (this, server, repo, path, leaf, fext, dest,
                         [leaf] (File *copy, File *&made) {
               QString dir = QDir::tempPath () + "/";
               QString uniq = util_findNextFilename (leaf + "_unfold", dir,
                                                     ".max");
               // each page splits into two, so the work is twice the pages
               Operation op (tr ("Unfold booklet"), 2 * copy->pagecount (),
                             0);

               return copy->unfoldBookletTo (dir + uniq + ".max", op, made);
            }, d);
         };
         continue;
         }
      steps->requests << [this, server, repo, path] (RemoteBackend::Done d) {
         if (server)
            server->unfoldStack (repo, path, this, d);
      };
      }
   steps->what = "unfold";
   steps->each = [made] (int, const RemoteBackend::Result &result) {
      *made << result.json ().value ("name").toString ();
   };
   steps->finish = [this, dir, sources, made, namelist, done] (int,
                                                              err_info *err) {
      addMadeStacks (dir, sources, *made, namelist, err, done);
   };
   runSteps (steps);
   }


void Desktopmodel::addMadeStacks (const QString &dir,
                                  const QStringList &sources,
                                  const QStringList &made,
                                  std::shared_ptr<QStringList> namelist,
                                  err_info *err, OpDone done)
   {
   QString path = dir;
   QModelIndex parent = deskFromDirname (path);
   Desk *desk = parent.isValid () ? getDesk (parent) : nullptr;
   QList<File *> flist;

   for (int i = 0; i < made.size (); i++)
      {
      QModelIndex ind = parent.isValid () ? index (sources [i], parent)
                                          : QModelIndex ();
      File *f = getFile (ind);

      *namelist << made [i];
      if (!desk || !f)
         continue;
      File *fnew = desk->createFile (desk->dir (), made [i]);

      desk->newFile (fnew, f);
      refreshRemoteThumbnail (desk, remoteForFile (f), fnew);
      flist << fnew;
      }
   if (!flist.isEmpty ())
      insertRows (flist, parent);
   done (err);
   }


void Desktopmodel::ocrPageLater (const QModelIndex &ind, int pagenum,
                                 std::function<void (err_info *err,
                                                     const QString &text)> done)
   {
   File *f = getFile (ind);
   RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

   if (!remote)
      {
      QString text;
      err_info *err = ocrLocalPage (ind, pagenum, text);

      done (err, text);
      return;
      }

   QString dir = f->desk ()->dir ();
   QString fname = f->filename ();

   queueOp ([this, dir, fname, pagenum, done] (OpDone opDone) {
      QModelIndex ind = stackFor (dir, fname);
      File *f = getFile (ind);
      RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

      if (!remote)
         {
         opDone (NULL);
         return;
         }
      Desk *desk = f->desk ();

      remote->ocrPage (desk->repoName (), remoteStackPath (desk, f),
                       pagenum + 1, this,
                       [this, dir, fname, pagenum, done, opDone] (
                          const RemoteBackend::Result &result) {
         if (_closing)
            return;      // the model is going, with what it showed
         if (!result.ok)
            {
            done (err_make (ERRFN, ERR_remote_op_failed2, "ocr",
                            qPrintable (result.error)), QString ());
            opDone (NULL);
            return;
            }

         QJsonObject reply = result.json ();
         QString text = reply.value ("text").toString ();
         QJsonObject obj = reply.value ("words").toObject ();
         QByteArray words = obj.isEmpty () ? QByteArray ()
               : QJsonDocument (obj).toJson (QJsonDocument::Compact);
         QModelIndex ind = stackFor (dir, fname);
         File *f = getFile (ind);
         RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

         /* a stack held a page at a time fetches the page again, which
            now carries what was read; otherwise the server kept the
            page's words with it, or else stored the text in the stack's
            ocr annotation, so mirror it onto the parsed cached copy */
         if (remote && sparseFor (f, remote, f->desk ()))
            stackChangedLater (ind, QList<int> () << pagenum);
         else if (f && f->remoteChecked ())
            {
            OcrPage page;

            if (!words.isEmpty () && OcrPage::fromBytes (words, page))
               f->putPageOcr (pagenum, page);
            else
               {
               QHash<int, QString> updates;

               updates [File::Annot_ocr] = text;
               f->putAnnot (updates);
               f->flush ();
               }
            }
         done (NULL, text);
         opDone (NULL);
      });
   }, [] (err_info *) {}, true);
   }


void Desktopmodel::askServerToReadLater (const QModelIndex &ind,
                                         OpDone report)
   {
   File *f = getFile (ind);
   RemoteBackend *remote = f ? remoteForFile (f) : nullptr;

   if (!remote)
      {
      report (err_make (ERRFN, ERR_remote_op_failed2, "read",
                        "the stack is not on a server"));
      return;
      }
   Desk *desk = f->desk ();

   remote->readStack (desk->repoName (), remoteStackPath (desk, f), this,
                      [this, report] (const RemoteBackend::Result &result) {
      if (_closing)
         return;
      report (result.ok ? NULL
                        : err_make (ERRFN, ERR_remote_op_failed2, "read",
                                    qPrintable (result.error)));
   });
   }


/* Brings the pages of some stacks here, a stack at a time: first its
   structure, then each page not here yet. Each time something comes, or
   fails to, it looks again at what the stack still lacks */
struct Desktopmodel::FetchJob
   {
   QList<QPersistentModelIndex> stacks;
   int next = 0;              //!< the stack being fetched
   bool askedInfo = false;    //!< its structure has been asked for
   bool askedPages = false;   //!< and the pages it lacks
   QMetaObject::Connection conn;
   OpDone done;
   };


void Desktopmodel::fetchStacksLater (const QModelIndexList &list,
                                     OpDone done)
   {
   auto job = std::make_shared<FetchJob> ();

   for (const QModelIndex &ind : list)
      job->stacks << QPersistentModelIndex (ind);
   job->done = done;
   job->conn = connect (this, &Desktopmodel::contentFetched, this,
                        [this, job] () { fetchNext (job); });
   fetchNext (job);
   }


void Desktopmodel::fetchNext (std::shared_ptr<FetchJob> job)
   {
   for (; job->next < job->stacks.size ();
        job->next++, job->askedInfo = false, job->askedPages = false)
      {
      QModelIndex ind (job->stacks [job->next]);
      File *f = ind.isValid () ? getFile (ind) : nullptr;

      if (!f || !remoteForFile (f))
         continue;               // gone, or local

      // wait for anything on its way for this stack
      for (const QPersistentModelIndex &other : _pendingContent)
         if (other.isValid () && getFile (QModelIndex (other)) == f)
            return;
      for (const auto &other : _pendingPages)
         if (other.first.isValid () && getFile (QModelIndex (other.first)) == f)
            return;

      err_info *err = NULL;

      if (!f->remoteChecked ())
         {
         if (!job->askedInfo)
            {
            job->askedInfo = true;
            if (requestContent (ind))
               return;
            }
         err = err_make (ERRFN, ERR_remote_fetch_failed1,
               qPrintable (tr ("cannot fetch %1").arg (f->filename ())));
         }
      else
         {
         QList<int> lack;

         for (int pagenum = 0; pagenum < f->pagecount (); pagenum++)
            if (!pageHere (ind, pagenum))
               lack << pagenum;
         if (lack.isEmpty ())
            continue;
         if (!job->askedPages)
            {
            job->askedPages = true;
            for (int pagenum : lack)
               requestPage (ind, pagenum);
            return;
            }
         err = err_make (ERRFN, ERR_remote_fetch_failed1,
               qPrintable (tr ("cannot fetch page %1 of %2")
                           .arg (lack.first () + 1).arg (f->filename ())));
         }
      disconnect (job->conn);
      job->done (err);
      return;
      }
   disconnect (job->conn);
   job->done (NULL);
   }
