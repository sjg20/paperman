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

#include <QTreeView>


class Dirview : public QTreeView
   {
   Q_OBJECT
public:
   Dirview (QWidget *parent = 0);
   ~Dirview ();

   /** returns the full path referred to by a context menu click */
   QString menuGetPath (void);

   /** returns the filename leaf referred to by a context menu click */
   QString menuGetName (void);

   /** returns the context menu model index */
   QModelIndex menuGetModelIndex (void) { return _context; }

   /** select a particular item as the current one to be operated on by keypresses/menus */
   void selectContextItem (const QModelIndex &index);

//    void dragEnterEvent(QDragEnterEvent *event);

   void dropEvent (QDropEvent *event);

protected:
   void contextMenuEvent (QContextMenuEvent * e);

public slots:
   void selectionChanged (const QItemSelection &selected, const QItemSelection &deselected);

public:
   // the menu actions which the user can select
   QAction *_new, *_delete, *_rename, *_refresh, *_add_recent;

private:
   QPersistentModelIndex _context;
   };

