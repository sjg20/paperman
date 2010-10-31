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
//Added by qt3to4:
// #include <QDropEvent>

class QCheckBox;
class QDirModel;
class QLineEdit;
class QSplitter;
class QTimer;
class QToolBar;
class QToolButton;
class QTreeView;

class Q3IconView;
class Q3IconViewItem;
class Q3ListViewItem;

class Desktopdelegate;
class Desktopitem;
class Desktopmodel;
class Desktopmodelconv;
class Desktopproxy;
class Desktopview;
class QListView;
class Dirmodel;
class Dirview;
class Paperstack;
class Pagewidget;
struct file_info;
class Desk;
class Operation;

struct err_info;
struct file_info;


#include <QAbstractItemModel>

#include "q3ptrlist.h"
#include "qsplitter.h"
#include "qstring.h"
#include "qwidget.h"
#include "desk.h"


/** a DesktopWidget is a splitter with a directory tree on the left and a
Desktopviewer on the right (containing thumbnails). Users can navigate the
directory tree, and click on a directory, which then becomes the current
directory. This class will then display that directory and allow the user
to work with the thumbnails in it. The parent is a Mainwidget */

class Desktopwidget : public QSplitter //QWidget
   {
   Q_OBJECT
public:
   /** construct a desktop widget */
   Desktopwidget (QWidget *parent = 0);

   /** destroy a desktop widget */
   ~Desktopwidget ();

   /** add a new 'root' directory to the tree of directories */
   void setDir (QString dirname);

   /** returns a pointer to the model, which contains the items being displayed */
   Desktopmodel *getModel (void) { return _contents; }

   /** returns a pointer to the view,, which contains the view of the items */
   Desktopview *getView (void) { return _view; }

   /** returns a pointer to the model converter, which allows access to the
       proxy <-> source model conversion features */
   Desktopmodelconv *getModelconv (void) { return _modelconv; }

   /** gets the index of the current file (the one selected by the user).

      \param index     returns index of file

      \returns true if there is a current file, false otherwise. If false,
      then index is invalid */
   bool getCurrentFile (QModelIndex &index);
//    bool getCurrentFile (Desk *&maxdesk, file_info *&file);

   /** update the match string and perform a new search

      \param match   string to match
      \param subdirs true to check subdirectories, else just filter current one
      \param reset   true to reset and redisplay current directory */
   void matchUpdate (QString match, bool subdirs, bool reset = false);

   /** sets the selected file in the viewer. All other items are deselected

      \param file   file to search for
      \returns true if found, false if not */
   bool setCurrentFile (file_info *file);

   void closing (void);

   /** select a directory in the view

      \param index   index of directory to select. If this is QModelIndex()
                     then select the first directory */
   void selectDir (QModelIndex &index);

   /** a convenience function to add a new action */
   void addAction (QAction *&_act, const char *text, const char *slot,
         const QString &shortcut, QWidget *parent = 0, const char *icon = 0);

protected:

signals:
   void newContents (QString str);

   /** emitted when we have completed displaying a new directory */
   void updateDone (void);

   /** emitted when the undo stack changes */
   void undoChanged (void);

   /** switch views and show the current page from the selected stack */
   void showPage (const QModelIndex &index);

   /** indicate that a new item has been selected */
   void itemSelected (const QModelIndex &);

public slots:
   void slotPopupMenu (QModelIndex &index);

   /** handle an item being clicked

      \param index      item clicked
      \param which      which part of it was clicked */
   void slotItemClicked (const QModelIndex &index, int which);

   /** handle a preview request for an item. This updates the preview
       window with this item's current page image

      \param index      item clicked
      \param which      which part of it was clicked
      \param now        true to display preview now, else wait a bit for user */
   void slotItemPreview (const QModelIndex &index, int which, bool now);

   void stackRight (void);
   void stackLeft (void);
   void pageLeft (void);
   void pageRight (void);

   // open a stack (view the current page and swap views)
   void openStack (const QModelIndex &index);

   // indicate that a scan is about to begin
   void slotBeginningScan (const QModelIndex &sind);

   /** indicate that a scan is about to end

      \param cancel  true if the scan was cancelled (rather than
               completing normally) and the stack will be deleted */
   void slotEndingScan (bool cancel);

   /** indicate that a scan has ended */
   void scanComplete (void);

   /** handle a change in the filter string - we adjust the filter */
   void matchChange (const QString &);

   /** handle pressing return in the filter string - we do a search */
   void matchUpdate (void);

   /** handle a click on the 'find' button - we do a search */
   void findClicked (void);

   /** handle the reset filter button */
   void resetFilter (void);

private slots:
   /** called when the directory is changed. We refresh the view and move
       down to display the bottom of the directory

      \param dirPath    pathname of directory the user has changed to
      \param deskind    source model index of that directory path */
   void slotDirChanged (QString &dirPath, QModelIndex &deskind);

   /** select a directory and display its contents

      \param index      index of directory to select
      \param allow_undo true to allow user to undo this change */
   void dirSelected (const QModelIndex &index, bool allow_undo = true);

   /** handle a number of objects being dropped onto a folder. This moves
       the corresponding files into the new folder, removing them from their
       old location */
   void slotDroppedOnFolder(const QMimeData *event, QString &dir);

#ifdef USE_CTL
   /** handle a number of objects being dropped onto a folder. This moves
       the corresponding files into the new folder, removing them from their
       old location */
   void slotDroppedOnFolder(const QMimeData *event, QString &dir);

   /** handle the selection of a folder. This displays the thumbnails in the
       new folder */
   void slotTreeFolderSelected(const QString &path);

   /** handle a context menu select on a folder - this brings up a menu to
       allow the user to modify a folder (e.g. rename it) */
   void slotTreeContextMenuRequested(Q3ListViewItem *item, const QPoint &pos ,int col);

   /** handle a folder being dropped onto another folder. In this case we
       move the source into the target, so that it becomes a subdirectory
       of the new parent */
   void slotMoveFolder(QDropEvent *event, QString &src, QString &dst);

   void slotSelectFolder(const QString &dir);

#endif

   /** the viewer has finished updating */
   void slotUpdateDone();

   //! rename a directory
   void renameDir ();

   //! create a new subdirectory
   void newDir ();

   //! delete the current directory
   void deleteDir ();

   //! refresh the current directory
   void refreshDir ();

   void updatePreview (void);

   //! duplicate the selected items
   void duplicate (void);

   void duplicateMax (void);

   void duplicatePdf (void);

   void duplicateEven (void);

   void duplicateOdd (void);

   // email files
   void email (void);

   // email max files as attachments
   void emailMax (void);

   // convert files to pdf and email as attachments
   void emailPdf (void);

   // send files
   void send (void);

   // deliver outgoing files
   void deliverOut (void);

   //! locate and open the folder for the selected item
   void locateFolder (void);

   //! delete selected stacks
   void deleteStacks (void);

   //! unstack selected stacks
   void unstackStacks (void);

   //! unstack the current page from the selected stack
   void unstackPage (void);

   //! duplicate the current page from the selected stack
   void duplicatePage (void);

   //! stack the currently selected pages/stacks
   void stackPages (void);

   //! rename the current stack
   void renameStack (void);

   //! rename the current page
   void renamePage (void);

   /** adjust our splitter size according to the new mode */
   void slotModeChanging (int new_mode, int old_mode);

private:
   void emailFiles (QString &fname, QStringList &fnamelist);

   void pageLeft (const QModelIndex &index);

   void pageRight (const QModelIndex &index);

   /** complete an operation which creates items, and report any error. This
       function selects the newly created items in the view and report the
       error, if any

       \param parent    parent of items
       \param err       error to report, or NULL if no error */
   void complete (QModelIndex parent, err_info *err);

private:
#ifdef USE_CTL
   //! the directory tree viewer
   ctlTreeView *_dir;

   /** the directory tree item relating to the open context menu. If no
       context menu is open then this value is not valid. It is set up
       when a context menu is opened. When an action is chosen from the menu,
       this is the item which will be actioned (renamed, or whatever) */
   ctlTreeViewItem *_contextItem;
#endif
   /** this is the model for the directories tree */
   Dirmodel *_model;

   /** this is the view for the directories tree */
   Dirview *_dir;

   /** this is the model for the desktop viewer (the right pane) which contains the
       files in the current directory */
   Desktopmodel *_contents;

   /** this is the desktop viewer (the right pane) which contains
   thumbnails of the files in the current directory */
   Desktopview *_view;

   /** this is the item delegate for the view */
   Desktopdelegate *_delegate;

   /** this is the proxy model used for filtering */
   Desktopproxy *_proxy;

   /** this is the page viewer (to the right of desktop viewer which contains a preview of
       the current page */
   Pagewidget *_page;

   /** this is the parent widget, which will be a Mainwidget */
   QWidget *_parent;

   /** current path being displayed */
   QString _path;

   /** pending match, for when we are no longer busy */
   QString _pendingMatch;

   /** true if we are busy updating */
   bool _updating;

   /** timer to use for updating the preview page */
   QTimer *_timer;

//    Desk *_update_desk;
//    file_info *_update_f;
   QPersistentModelIndex _update_index;

   // actions
   QAction *_act_duplicate, *_act_locate, *_act_delete;
   QAction *_act_unstack_all, *_act_unstack_page, *_act_stack;
   QAction *_act_rename_stack, *_act_rename_page, *_act_duplicate_page;
   QAction *_act_duplicate_max, *_act_duplicate_pdf, *_act_duplicate_tiff;
   QAction *_act_duplicate_odd, *_act_duplicate_even;
   QAction *_act_email, *_act_email_max, *_act_email_pdf;
   QAction *_act_send, *_act_deliver_out;

   QToolBar *_toolbar;

   // more actions (toolbar)
   QAction *_actionPprev;

   Desktopmodelconv *_modelconv; //!< proxy <-> source model conversion
   Desktopmodelconv *_modelconv_assert;   //!< same, but only allows assertions

   QString _scroll_to;     //!< filename to scroll to when a new directory is opened

   QLineEdit *_match;      //!< line edit for the match
   QAction *_find, *_reset;
   QCheckBox *_global;
   };

