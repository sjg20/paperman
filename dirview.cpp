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
   dirview - directory viewer
*/


#include <QAction>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDirModel>
#include <QHeaderView>
#include <QMenu>

#include "dirview.h"


Dirview::Dirview (QWidget *parent)
   : QTreeView (parent)
   {
//    _dir->setRootIsDecorated (false);
   setColumnWidth (0, 400);
   hideColumn (1);
   hideColumn (2);
   hideColumn (3);
   header ()->hide ();
   setDragDropMode (QAbstractItemView::DragDrop);
//    _dir->setDropIndicatorShown (true); - defaults to this
   setDragEnabled (true);
   setAcceptDrops (true);

   _new = new QAction ("&New subdirectory", this);
//    _new->setShortcut (Qt::CTRL+Qt::Key_N);

   _rename = new QAction ("&Rename", this);
//    _rename->setShortcut (Qt::CTRL+Qt::Key_R);

   _delete = new QAction ("&Delete", this);
   // conflicts with main view
//    _delete->setShortcut (Qt::Key_Delete);
//    _delete->setShortcut (Qt::CTRL+Qt::Key_D);

   _refresh = new QAction ("Re&fresh", this);
//    _refresh->setShortcut (Qt::Key_F3);

   addAction (_new);
   addAction (_rename);
   addAction (_delete);
   addAction (_refresh);
   }


Dirview::~Dirview ()
   {
   }


void Dirview::selectionChanged (const QItemSelection &selected, const QItemSelection &deselected)
   {
   QModelIndexList list = selected.indexes ();
   //qDebug () << "Dirview::selectionChanged" << list.size ();
   if (list.size () == 0)
      qDebug () << "lost selection";
   QTreeView::selectionChanged (selected, deselected);

   if (list.size () == 1)
      selectContextItem (list [0]);
   }


void Dirview::selectContextItem (const QModelIndex &index)
   {
   _context = QPersistentModelIndex (index);
   }


void Dirview::contextMenuEvent (QContextMenuEvent * e)
   {
   QMenu *menu = new QMenu ();

   selectContextItem (indexAt (e->pos ()));
   menu->addAction (_new);
   menu->addAction (_rename);
   menu->addAction (_delete);
   menu->addAction (_refresh);
   menu->exec (e->globalPos());
   }


QString Dirview::menuGetPath (void)
   {
   return model ()->data (_context, QDirModel::FilePathRole).toString ();
   }


QString Dirview::menuGetName (void)
   {
   return model ()->data (_context, QDirModel::FileNameRole).toString ();
   }


// void Dirview::dragEnterEvent(QDragEnterEvent *event)
//    {
// //      if (event->mimeData()->hasFormat("text/plain"))
//          event->acceptProposedAction();
//    }


void Dirview::dropEvent (QDropEvent *event)
   {
   QTreeView::dropEvent (event);

   /*FIXME: drop events seem to close up the list, so we open it again */
   QModelIndex index = model ()->index (0, 0, QModelIndex ());
   setExpanded (index, true);
   }

