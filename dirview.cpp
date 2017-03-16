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

   // We can't use shortcuts here as they conflict with main view
   _new = new QAction ("&New subdirectory", this);
   _rename = new QAction ("&Rename", this);
   _delete = new QAction ("&Delete", this);
   _refresh = new QAction ("Re&fresh", this);
   _add_recent = new QAction ("&Add to recent", this);
   _add_repository = new QAction ("Add repository", this);
   _remove_repository = new QAction ("Remove repository", this);

   addAction (_new);
   addAction (_rename);
   addAction (_delete);
   addAction (_refresh);
   addAction (_add_recent);
   addAction (_add_repository);
   addAction (_remove_repository);
   }


Dirview::~Dirview ()
   {
   }


void Dirview::selectionChanged (const QItemSelection &selected, const QItemSelection &deselected)
   {
   QModelIndexList list = selected.indexes ();

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
   menu->addAction (_add_recent);
   menu->addSeparator ();
   menu->addAction (_add_repository);
   menu->addAction (_remove_repository);
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

