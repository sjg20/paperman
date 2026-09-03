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
#include <QJsonObject>
#include <QMimeData>
#include <QProcess>
#include <QUrl>

#include "desktopmodel.h"
#include "desk.h"
#include "file.h"
#include "ocr.h"
#include <QDir>

#include "op.h"
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

      RemoteBackend *remote = remoteForFile (f);
      if (remote)
         {
         /* Unstack on the server page by page, mirroring each step
            onto the cached copy under the server's chosen name.  Both
            sides always remove the current first page, so the page
            numbering stays aligned. */
         Desk *desk = f->desk ();

         CALL (ensureContent (ind));
         while (!err && f->pagecount () > 1)
            {
            QString serverName;
            if (!remote->unstackStack (desk->repoName (),
                                       remoteStackPath (desk, f), 1, 1,
                                       true, "", &serverName))
               {
               err = err_make (ERRFN, ERR_remote_op_failed2, "unstack",
                               qPrintable (remote->lastError ()));
               break;
               }
            fnew = desk->createFile (desk->dir (), serverName);
            desk->newFile (fnew, f, pagenum++);
            err = fnew->create ();
            if (!err)
               err = f->unstackPages (0, 1, true, fnew);
            fnew->setRemoteChecked (true);
            stack_names << serverName;
            flist << fnew;
            }
         if (flist.size ())
            {
            buildItem (ind);
            insertRows (flist, parent);
            }
         _newnames << stack_names;
         if (err)
            break;
         continue;
         }

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
      /* Stack on the server; it deletes the source files itself.  The
         destination is refetched afterwards rather than mirrored,
         since the sources may not all be in the local cache. */
      foreach (ind, list)
         {
         File *f = getFile (ind);
         int destpage = in_destpage == -1 ? f->pagenum () : in_destpage;

         if (out_destpage == -1)
            out_destpage = destpage;

         QStringList sources;
         foreach (pind, pages_list [upto])
            if (pind != QModelIndex ())
               {
               File *pdel = getFile (pind);

               sources << remoteStackPath (rdesk, pdel);
               delete_list << pind;
               }
         upto++;
         if (sources.isEmpty ())
            continue;

         QString destPath = remoteStackPath (rdesk, f);
         if (!remote->stackStacks (rdesk->repoName (), destPath, sources,
                                   destpage + 1))
            {
            e = err_make (ERRFN, ERR_remote_op_failed2, "stack",
                          qPrintable (remote->lastError ()));
            break;
            }

         remote->invalidateCachedFile (rdesk->repoName (), destPath);
         f->setRemoteChecked (false);
         err_info *e2 = ensureContent (ind);
         if (!e)
            e = e2;
         }
      in_destpage = out_destpage;

      /* the server has already deleted the source files; drop their
         cached copies and rows */
      foreach (pind, delete_list)
         remote->invalidateCachedFile (rdesk->repoName (),
                                       remoteStackPath (rdesk,
                                                        getFile (pind)));
      sortForDelete (delete_list);
      foreach (pind, delete_list)
         removeRow (pind.row (), pind.parent ());
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

   if (remote)
      {
      /* only a plain copy is available remotely; conversions need
         the pages, which live on the server */
      if (type != File::Type_other)
         return err_make (ERRFN, ERR_remote_op_failed2, "convert",
                          "not supported for a remote stack");

      foreach (ind, list)
         {
         f = getFile (ind);
         QString name;

         if (!remote->duplicateStack (rdesk->repoName (),
                                      remoteStackPath (rdesk, f), &name))
            {
            err = err_make (ERRFN, ERR_remote_op_failed2, "duplicate",
                            qPrintable (remote->lastError ()));
            break;
            }
         fnew = rdesk->createFile (rdesk->dir (), name);
         rdesk->newFile (fnew, f);
         refreshRemoteThumbnail (rdesk, remote, fnew);
         flist << fnew;
         namelist << name;
         }
      insertRows (flist, parent);
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

   UNUSED (parent);
//    _model_invalid = true;
   foreach (ind, list) if (getFile (ind))
      {
      File *f = getFile (ind);
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *fdesk = f->desk ();
         QString stackPath = remoteStackPath (fdesk, f);

         if (!remote->deleteStack (fdesk->repoName (), stackPath))
            {
            e = err_make (ERRFN, ERR_remote_op_failed2, "delete",
                          qPrintable (remote->lastError ()));
            break;
            }
         remote->invalidateCachedFile (fdesk->repoName (), stackPath);
         }
      else
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
      /* Trash and move-to-dir both become a server-side move; the
         shared trash is a directory at the repository root.  The undo
         record keeps the synthetic absolute path so the round-trip
         through opUntrashStacks() can find it again. */
      if (dest.isEmpty ())
         dest = desk->rootDir () + RemoteBackend::trashDirName ();
      if (!dest.startsWith (desk->rootDir ()))
         return err_make (ERRFN, ERR_cannot_move_between_repositories);
      QString destRel = remoteRelDir (desk, dest);

      foreach (ind, list)
         {
         if (!ind.isValid ())
            continue;
         File *f = getFile (ind);
         QString stackPath = remoteStackPath (desk, f);
         QString finalName;

         if (!remote->moveStack (desk->repoName (), stackPath, destRel,
                                 copy, &finalName))
            {
            e = err_make (ERRFN, ERR_remote_op_failed2, "move",
                          qPrintable (remote->lastError ()));
            break;
            }
         if (!copy)
            remote->invalidateCachedFile (desk->repoName (), stackPath);
         del_list << ind;
         trashlist << finalName;
         }
      if (!copy)
         {
         sortForDelete (del_list);
         foreach (ind, del_list)
            removeRows (ind.row (), 1, parent);
         }
      addFilesToDesk (dest, trashlist);
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
      /* Reverse of the remote branch in opTrashStacks(): move each
         file back from the source directory (the trash, unless this
         was a move-to-dir) into this desk, or for a copy just delete
         the copy again. */
      QString srcRel = src.isEmpty ()
            ? QString (RemoteBackend::trashDirName ())
            : remoteRelDir (desk, src);
      QString destRel = remoteRelDir (desk, desk->dir ());

      for (i = 0; i < trashlist.size (); i++)
         {
         QString srcPath = srcRel.isEmpty ()
               ? trashlist [i] : srcRel + "/" + trashlist [i];

         if (copy)
            {
            if (!remote->deleteStack (desk->repoName (), srcPath))
               {
               e = err_make (ERRFN, ERR_remote_op_failed2, "delete",
                             qPrintable (remote->lastError ()));
               break;
               }
            remote->invalidateCachedFile (desk->repoName (), srcPath);
            continue;
            }

         QString finalName;
         if (!remote->moveStack (desk->repoName (), srcPath, destRel,
                                 false, &finalName))
            {
            e = err_make (ERRFN, ERR_remote_op_failed2, "move",
                          qPrintable (remote->lastError ()));
            break;
            }
         remote->invalidateCachedFile (desk->repoName (), srcPath);

         fnew = desk->createFile (desk->dir (), finalName);
         desk->newFile (fnew);
         if (pagepos)
            {
            fnew->setPos ((*pagepos) [i].pos);
            fnew->setPagenum ((*pagepos) [i].pagenum);
            }
         filenames [i] = finalName;
         flist << fnew;
         refreshRemoteThumbnail (desk, remote, fnew);
         }

      if (!copy)
         insertRows (flist, parent);
      removeFilesFromDesk (src, trashlist);
      return e;
      }

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

   RemoteBackend *remote = remoteForFile (f);
   if (remote)
      {
      Desk *desk = f->desk ();

      CALL (ensureContent (ind));
      if (remove && f->pagecount () <= 1)
         return NULL;

      /* the server picks the new stack's name; mirror the unstack
         onto the cached copy under that name so both stay in step */
      QString serverName;
      if (!remote->unstackStack (desk->repoName (),
                                 remoteStackPath (desk, f), pagenum + 1, 1,
                                 remove, "", &serverName))
         return err_make (ERRFN, ERR_remote_op_failed2, "unstack",
                          qPrintable (remote->lastError ()));

      fnew = desk->createFile (desk->dir (), serverName);
      row = desk->newFile (fnew, f, 1);
      e = fnew->create ();
      if (!e)
         e = f->unstackPages (pagenum, 1, remove, fnew);
      fnew->setRemoteChecked (true);

      newname = serverName;
      if (remove)
         buildItem (ind);
      QModelIndexList list;
      newItem (row, ind.parent (), list);
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
   _modelconv->assertIsSource (ind.model (), &ind, 0);
   File *f = getFile (ind);

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QJsonObject wire;

         for (auto it = updates.constBegin (); it != updates.constEnd ();
              ++it)
            wire [File::annotWireName ((File::e_annot)it.key ())]
                  = it.value ();
         if (!remote->updateAnnotations (desk->repoName (),
                                         remoteStackPath (desk, f), wire))
            return err_make (ERRFN, ERR_remote_op_failed2, "annotation",
                             qPrintable (remote->lastError ()));

         /* mirror onto the parsed cached copy so the annotations pane
            shows the new values without a refetch */
         if (f->remoteChecked ())
            {
            f->putAnnot (updates);
            f->flush ();
            }
         return NULL;
         }

      f->putAnnot (updates);
      f->flush ();
      }
   return NULL;
   }



err_info *Desktopmodel::opDeletePages (QModelIndex &ind, QBitArray &pages,
      QByteArray &del_info, int &count)
   {
   File *f = getFile (ind);
   err_info *e = NULL;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();

         CALL (ensureContent (ind));

         QList<int> pageList;
         for (int i = 0; i < pages.size (); i++)
            if (pages.testBit (i))
               pageList << i + 1;

         QString undoId;
         if (!remote->deletePages (desk->repoName (),
                                   remoteStackPath (desk, f), pageList,
                                   &undoId))
            return err_make (ERRFN, ERR_remote_op_failed2, "page delete",
                             qPrintable (remote->lastError ()));

         /* Mirror the removal onto the cached copy so the view stays
            in step without a refetch.  The undo record's blob carries
            the server's token alongside the local recovery data, so
            opUndeletePages() can restore both sides. */
         QByteArray localInfo;
         e = f->removePages (pages, localInfo, count);
         del_info.clear ();
         QDataStream ds (&del_info, QIODevice::WriteOnly);
         ds << undoId << localInfo;
         buildItem (ind);
         return e;
         }

      e = f->removePages (pages, del_info, count);
      buildItem (ind);
      getDesk (ind.parent ())->dirty ();
      }
   return e;
   }


err_info *Desktopmodel::opUndeletePages (QModelIndex &ind, QBitArray &pages,
      QByteArray &del_info, int count)
   {
   File *f = getFile (ind);
   err_info *e = NULL;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString undoId;
         QByteArray localInfo;
         QDataStream ds (&del_info, QIODevice::ReadOnly);

         ds >> undoId >> localInfo;
         if (!remote->undeletePages (desk->repoName (),
                                     remoteStackPath (desk, f), undoId))
            return err_make (ERRFN, ERR_remote_op_failed2,
                             "page undelete",
                             qPrintable (remote->lastError ()));

         e = f->restorePages (pages, localInfo, count);
         buildItem (ind);
         return e;
         }

      e = f->restorePages (pages, del_info, count);
      buildItem (ind);
      }
   return e;
   }


void Desktopmodel::opChangeDir (QString &dirPath, QString &rootPath)
   {
   QModelIndex ind = showDir(dirPath, rootPath, nullptr);
   emit dirChanged (dirPath, ind);
   }


err_info *Desktopmodel::opRenameStack (const QModelIndex &index, QString &newname)
   {
   File *f = getFile (index);
   err_info *e = NULL;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();
         QString oldCache = f->pathname ();

         if (!remote->renameStack (desk->repoName (),
                                   remoteStackPath (desk, f), newname))
            return err_make (ERRFN, ERR_remote_op_failed2, "rename",
                             qPrintable (remote->lastError ()));

         /* carry the cached copy (and its validator) over to the new
            name, then update the file object to match the server */
         f->updateFilename (newname);
         QFile::rename (oldCache, f->pathname ());
         QFile::rename (oldCache + ".etag", f->pathname () + ".etag");
         buildItem (index);
         return NULL;
         }

      e = f->rename (newname, true);
      buildItem (index);
      getDesk (index.parent ())->dirty ();
      }
   return e;
   }


err_info *Desktopmodel::opRenamePage (const QModelIndex &index, int pagenum, QString &newname)
   {
   _modelconv->assertIsSource (0, &index, 0);
   File *f = getFile (index);
   err_info *e = NULL;

   if (f)
      {
      RemoteBackend *remote = remoteForFile (f);

      if (remote)
         {
         Desk *desk = f->desk ();

         if (!remote->renamePage (desk->repoName (),
                                  remoteStackPath (desk, f), pagenum + 1,
                                  newname))
            return err_make (ERRFN, ERR_remote_op_failed2, "page rename",
                             qPrintable (remote->lastError ()));

         /* mirror the change onto the cached copy so the page view
            stays in step without a refetch */
         if (pagenum >= 0 && pagenum < f->pagecount ())
            {
            f->renamePage (pagenum, newname);
            f->setPagenum (pagenum);
            }
         buildItem (index);
         return NULL;
         }

      if (pagenum >= 0 && pagenum < f->pagecount ()) // which it should be!
         {
         e = f->renamePage (pagenum, newname);
         f->setPagenum (pagenum);
         }
      buildItem (index);
      }
   return e;
   }


err_info *Desktopmodel::opTransformPage (const QModelIndex &index,
      int pagenum, File::e_transform op)
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
         return opTransformPageRemote (index, f, desk, remote, pagenum, op);
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
   return e;
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


QString Desktopmodel::remoteStackPath (Desk *desk, File *file) const
   {
   /* desk->dir() and desk->rootDir() are absolute and both end in '/';
      the stack's path within the repository is the directory below the
      root plus the filename. */
   QString dirInRepo = desk->dir ();
   QString root = desk->rootDir ();
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
   quint64 token = backend->fetchThumbnailAsync (desk->repoName (),
         remoteStackPath (desk, file), /*page=*/1, /*size=*/"small");
   _pendingThumbnails.insert (token, file);
   }


err_info *Desktopmodel::ocrPage (const QModelIndex &ind, int pagenum,
      QString &text)
   {
   File *f = getFile (ind);

   if (!f)
      return NULL;

   RemoteBackend *remote = remoteForFile (f);
   if (remote)
      {
      Desk *desk = f->desk ();

      if (!remote->ocrPage (desk->repoName (), remoteStackPath (desk, f),
                            pagenum + 1, &text))
         return err_make (ERRFN, ERR_remote_op_failed2, "ocr",
                          qPrintable (remote->lastError ()));

      /* the server stored the text in the stack's ocr annotation;
         mirror it onto the parsed cached copy */
      if (f->remoteChecked ())
         {
         QHash<int, QString> updates;

         updates [File::Annot_ocr] = text;
         f->putAnnot (updates);
         f->flush ();
         }
      return NULL;
      }

   CALL (ensureContent (ind));

   QImage image;
   QSize size, trueSize;
   int bpp;
   CALL (getImage (ind, pagenum, false, image, size, trueSize, bpp));

   err_info *err;
   Ocr *ocr = Ocr::getOcr (err);

   if (!ocr)
      return err;
   return ocr->imageToText (image, text);
   }


err_info *Desktopmodel::uploadScanStack (File *f)
   {
   Desk *desk = f->desk ();
   RemoteBackend *remote = dynamic_cast<RemoteBackend *> (desk->backend ());

   QFile in (f->pathname ());
   if (!in.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1,
                       qPrintable (f->pathname ()));
   QByteArray bytes = in.readAll ();
   in.close ();

   QString finalName, etag;
   if (!remote->uploadFile (desk->repoName (), remoteStackPath (desk, f),
                            bytes, &finalName, &etag))
      return err_make (ERRFN, ERR_remote_op_failed2, "upload",
                       qPrintable (remote->lastError ()));

   if (!finalName.isEmpty () && finalName != f->filename ())
      {
      QString oldCache = f->pathname ();

      f->updateFilename (finalName);
      QFile::rename (oldCache, f->pathname ());
      }

   /* record the server's validator so the next open costs a 304 */
   if (!etag.isEmpty ())
      {
      QFile ef (f->pathname () + ".etag");

      if (ef.open (QIODevice::WriteOnly))
         ef.write (etag.toUtf8 ());
      }
   f->setRemoteChecked (true);
   return NULL;
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


err_info *Desktopmodel::opTransformPageRemote (const QModelIndex &index,
      File *file, Desk *desk, RemoteBackend *backend, int pagenum,
      File::e_transform op)
   {
   /* The wire page number is 1-based; a value below 1 means every page,
      which is how the all-pages case (pagenum == -1) is expressed. */
   int wirePage = pagenum < 0 ? 0 : pagenum + 1;

   QString stackPath = remoteStackPath (desk, file);

   if (!backend->transformPage (desk->repoName (), stackPath, wirePage,
                                File::transformName (op)))
      return err_make (ERRFN, ERR_remote_transform_failed1,
                       qPrintable (backend->lastError ()));

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
   return NULL;
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
      File::e_type type, Operation &op, QString &fname,
      QStringList &fname_list, QStringList &tmp_list)
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

      // if just copying, then get the filename
      if (type == File::Type_other)
         fname_list << f->pathname ();

      // else convert this file into a temporary one of the right type
      else
      {
         QString uniq;

         QString ext = File::typeExt (type);
         uniq = util_findNextFilename (f->leaf (), dir, ext);
         CALL (f->duplicateToDesk (0, type, uniq, 3, op, fnew));
         fname_list << uniq;
         tmp_list << uniq;
      }
      op.setProgress (upto++);
   }
   return NULL;
}


err_info *Desktopmodel::opEmailFiles (QModelIndex parent, QModelIndexList &slist,
      File::e_type type)
{
   QString fname;
   QStringList fname_list, tmp_list;

   UNUSED (parent);
   Operation op ("Packaging", slist.size (), 0);

   err_info *e = packageFiles (slist, type, op, fname, fname_list, tmp_list);
   bool can_delete = true;
   if (!e)
      e = emailFiles (fname, fname_list, can_delete);

   // delete the files if we are allowed
   if (can_delete) {
      foreach (QString fname, tmp_list)
         {
         QFile f (fname);
         f.remove ();
         }
   }
   return e;
}


err_info *Desktopmodel::opCopyFiles (QModelIndex parent, QModelIndexList &slist,
      File::e_type type)
{
   QString fname;
   QStringList fname_list, tmp_list;

   UNUSED (parent);
   Operation op ("Packaging", slist.size (), 0);

   err_info *e = packageFiles (slist, type, op, fname, fname_list, tmp_list);
   bool can_delete = true;
   if (!e)
      e = copyFilesToClipboard (fname, fname_list, can_delete);

   // delete the converted files if they are not the one on the clipboard
   if (can_delete) {
      foreach (QString fname, tmp_list)
         {
         QFile f (fname);
         f.remove ();
         }
   }
   return e;
}


void Desktopmodel::opUpdateRepositoryList (QString &dirpath, bool add_not_delete)
   {
   // Tell our controller (Desktopwidget) what to do
   emit updateRepositoryList (dirpath, add_not_delete);
   }
