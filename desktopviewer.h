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


class Desk;
class QDragObject;
class Desktopitem;
struct err_info;

#include "qiconview.h"
#include "qstring.h"
#include "qwidget.h"

#include "desk.h"


/** implements a viewer, which contains a number of thumbnails representing
paper stacks */

class Desktopviewer : public QIconView
   {
   Q_OBJECT

   friend class Desktopitem;
public:
   Desktopviewer(QWidget *parent);
   ~Desktopviewer();
   Desktopitem *itemFind (const QPoint & opos, int &which);
   Desk *getDesk () { return _desk; }
   void repaintItem( Desktopitem *item );

   /** print some progress information in the status bar */
   void progress (const char *fmt, ...);

   /** add a new paper stack to the viewer */
   struct err_info *addPaperstack (Paperstack *stack, struct file_info **fp);

   QDragObject *dragObject(void);

   bool checkerr (struct err_info *err);

   /** confirm the addition of a paper stack to a maxdesk */
   struct err_info *confirmPaperstack (Desk *maxdesk, file_info *f);

   struct err_info *cancelPaperstack (Desk *maxdesk, file_info *f);

   err_info *operation (Desk::operation_t type, int ival);

   /** sets the debug level to use for newly created max files */
   void setDebugLevel (int level);

   /** cancel a background refresh operation */
   void stopUpdate (void);

signals:
   void newContents (const char *str);
   void signalStackItem (Desktopitem *dest, Desktopitem *src);
   void signalStackItems (Desktopitem *dest, QPtrList<Desktopitem> &items);
   void showPage (Desk *maxdesk, file_info *f);
   void signalItemMoved (Desktopitem *item);
   void selectFolder (const QString &dir);

   //! signal to emit when we have finished updating the viewer
   void updateDone ();

   //! signal emitted when a new item is selected
   void itemSelected (Desktopitem *item);

public slots:
   void stackItem (Desktopitem *dest, Desktopitem *src);
   void stackItems (Desktopitem *dest, QPtrList<Desktopitem> &items);

   /** refresh the viewer with files from the given directory.

      \param dirpath    directory to search
      \param do_readDesk  true to read the directory's maxdesk.ini file
      \param match      string to match against filenames (QString::null for all)
      \param subdirs    true to search subdirectories also */
   void refresh (QString dirpath, bool do_readDesk = true,
         const QString &match = QString::null, bool subdirs = false, Operation *op = 0);

   void doRename (QIconViewItem *item, const QString &name);
   void itemMoved (Desktopitem *item);
   void arrangeBy (int type);
   void moveSelectedToDir (QString &dir);
   void moveDir (QString &src, QString &dst);

   /** given a desktop item, returns the associated maxdesk and file

      \param item       item to check
      \param *maxdeskp  returns the maxdesk
      \param *filep     returns the file */
   void getItemData (Desktopitem *item, Desk **maxdeskp, file_info **filep);

protected:
   void contentsMousePressEvent( QMouseEvent *e );
   void contentsMouseReleaseEvent( QMouseEvent *e);
   void keyPressEvent (QKeyEvent * e);

protected slots:
   void open (QIconViewItem *item);
   void itemClicked (Desktopitem *item, int which);
   void autoRepeatTimeout (void);

   /** add the next item in the maxview to the viewer */
   void nextUpdate (void);

   void stackPages (void);
   void unstackPage (Desktopitem *item = 0);
   void unstack (Desktopitem *item = 0);
   void duplicate (Desktopitem *item = 0);
   void renameStack (Desktopitem *item = 0);
   void renamePage (Desktopitem *item = 0);
   void duplicatePdf (Desktopitem *item = 0);
   void duplicateMax (Desktopitem *item = 0);
   void duplicateTiff (Desktopitem *item = 0);
   void deleteStack (void);
   void aboutToQuit (void);
   void locateFolder (Desktopitem *item = 0);
   void duplicatePage (Desktopitem *item = 0);

private:
   void doUnstackPage (Desktopitem *item, bool remove);
   void contentsContextMenuEvent ( QContextMenuEvent * );

   Desktopitem *buildItem (Desktopitem *item, struct file_info *f);

   Desktopitem *itemFind (struct file_info *file);

   QTimer *_updateTimer;
   struct file_info *_upto;
   QTimer *_timer;
   Desk *_desk;
   Desktopitem *_hit;    //!< desktop item which was clicked on
   int _hitwhich;        //!< which part of desktop item was clicked
   Desktopitem *_contextItem;
   int _addCount;        //!< number of things added to the maxdesk so far
   Operation *_op;       //!< operation being performed
   int _debug_level;     //!< the debug level to use for max debugging
   bool _subdirs;        //!< true if searched subdirectories
   QString _forceVisible;  //!< pathname of item to force to be visible when updating is completed
   bool _stopUpdate;     //!< signal to stop an update which is in progress
   };

