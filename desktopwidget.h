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

class QCheckBox;
class QDirModel;
class QKeyEventTransition;
class QLineEdit;
class QSplitter;
class QTimer;
class QToolBar;
class QToolButton;
class QTreeView;

class QListWidgetItem;

class Desktopdelegate;
class Desktopitem;
class Desktopmodel;
class Desktopmodelconv;
class Desktopproxy;
class Desktopview;
class QListView;
class Dirmodel;
class Dirproxy;
class Dirview;
class Folderlist;
class Mainwidget;
class Paperstack;
class Pagewidget;
struct file_info;
class Desk;
class Operation;
class Toolbar;
class TreeItem;

struct err_info;
struct file_info;


#include <QAbstractItemModel>
#include <QDialog>

#include "qsplitter.h"
#include "qstring.h"
#include "qwidget.h"
#include "desk.h"
#include "ui_toolbar.h"
#include "ui_move.h"


/** a DesktopWidget is a splitter with a directory tree on the left and a
Desktopview on the right (containing thumbnails). Users can navigate the
directory tree, and click on a directory, which then becomes the current
directory. This class will then display that directory and allow the user
to work with the thumbnails in it. The parent is a Mainwidget

All QModelIndex parameters here refer to the proxy model _dir_proxy
*/

class Desktopwidget : public QSplitter //QWidget
   {
   Q_OBJECT
public:
   /** construct a desktop widget */
   Desktopwidget (QWidget *parent = 0);

   /** destroy a desktop widget */
   ~Desktopwidget ();

   /** constructor helper functions */
   QWidget *createToolbar(void);
   void createPage(void);
   void addActions(void);

   /** add a new 'root' directory to the tree of directories

     \param dirname        Full path of directory to add
     \param ignore_error   true to add it anyway, even on error
     \returns NULL if ok, else error */
   err_info *addDir (QString dirname, bool ignore_error = false);

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

   void closing (void);

   /** select a directory in the view

      \param index   index of directory to select. If this is QModelIndex()
                     then select the first directory */
   void selectDir(const QModelIndex &target, bool forceChange = false);

   /** a convenience function to add a new action */
   void addAction (QAction *&_act, const char *text, const char *slot,
         const QString &shortcut, QWidget *parent = 0, const char *icon = 0);

   /** activate the search feature, which allows looking for stacks by name */
   void activateSearch();

   /**
    * @brief Get the root directory of the currentl selected paper repository
    * @return Directory path
    *
    * The top level of the the Dirmodel contains a number of Diritem objects,
    * each of which is a separate directory in the file system, with no
    * relationship between them. There is always a highlighted directory in the
    * Dirview _dir so this function finds the filesystem path associated with
    * that _dir
    */
   const QString getRootDirectory();

   /** convert a full directory path into a model index in _dir_proxy */
   QModelIndex getDirIndex(const QString dirname);

   /**
    * @brief Create a new directory
    * @param dir_path   Full path to new directory
    * @param index      Returns the Dirmodel index of the created directory
    * @return true if OK, false on error
    */
   bool newDir(const QString& dir_path, QModelIndex& index);

   /**
    * @brief Find the Dirmodel index for a directory path
    * @param dir_path   Full path to new directory
    * @return Dirmodel index of the directory
    */
   QModelIndex findDir(const QString& dir_path);

   // Set whether the dir filter is active or not
   void setDirFilter(bool active);

   /**
    * @brief  Find folders in the current repo which match a text string
    * @param  text    Text to match
    * @param  dirPath Returns the path to the root directory
    * @param  missing Suggestions for directories to create
    * @return list of matching paths
    *
    * This looks for 4-digit years and 3-character months to try to guess
    * which folders to put at the top of the list
    */
   QStringList findFolders(const QString& text, QString& dirPath,
                           QStringList& missing);

   void showImports(const QString& path);

protected:
   //bool eventFilter (QObject *watched_object, QEvent *e);

   // Return the cache
   TreeItem *ensureCache();

   // Get the model index of the current repository (top-level directory)
   const QModelIndex getRootIndex();

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

   /** handle the reset filter button */
   void resetFilter();

private slots:
   /** called when the directory is changed. We refresh the view and move
       down to display the bottom of the directory

      \param dirPath    pathname of directory the user has changed to
      \param deskind    source model index of that directory path */
   void slotDirChanged (QString &dirPath, QModelIndex &deskind);

   /** select a directory and display its contents

      \param index      index of directory to select
      \param allow_undo true to allow user to undo this change */
   void dirSelected(const QModelIndex &index, bool allow_undo = true,
                    bool force_change = false);

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
   void slotTreeContextMenuRequested(QListWidgetItem *item, const QPoint &pos ,int col);

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

   //! add to the list of recent directories
   void addToRecent (void);

   /** Ask the user for a directory and add it to the list of repositories.
      Supports undo */
   void slotAddRepository ();

   //! Remove the selected respository. Supported undo.
   void slotRemoveRepository ();

   //! Refresh the cache
   void slotRefreshCache();

   void updatePreview (void);

   //! duplicate the selected items
   void duplicate (void);

   void duplicateMax (void);

   void duplicatePdf (void);

   void duplicateJpeg (void);

   void duplicateEven (void);

   void duplicateOdd (void);

   // email files
   void email (void);

   // email max files as attachments
   void emailMax (void);

   // Move a stack to a folder
   void moveToFolder();

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

   /** update the list of repositories by adding/removing a dir

     \param dirname        Directory to add / delete
     \param add_not_delete true to add, false to delete */
   void slotUpdateRepositoryList (QString &dirname, bool add_not_delete);

   //! Open the search window to
   void searchInFolders();

   // Exit the folder search and return to the normal view
   void exitSearch();

   //! Update actions to set whether they are enabled/disabled
   void updateActions();

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

   /** Update the respository list in settings */
   void updateSettings ();

   /**
    * @brief start a search for a stack through dirs and subdirs
    * @param path    Full path to directory to search
    * @param match   Match string to use
    */
   void startSearch(const QString& path, const QString& match);

   //! Update the view to indicate that it is in a search/import mode
   void specialView(const QString& prompt);

   //! Drop the blue colour and other features of the search/import view
   void normalView();

private:
   /** this is the model for the directories tree */
   Dirmodel *_model;

   /** proxy model used for the filtering the directory tree */
   Dirproxy *_dir_proxy;

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
   Desktopproxy *_contents_proxy;

   /** this is the page viewer (to the right of desktop viewer which contains a preview of
       the current page */
   Pagewidget *_page;

   /** this is the parent widget, which will be a Mainwidget */
   Mainwidget *_main;

   /** full path of directory being displayed */
   QString _path;

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
   QAction *_act_duplicate_jpeg, *_act_move;
   QAction *_act_email, *_act_email_max, *_act_email_pdf;
//   QAction *_act_send, *_act_deliver_out;

   Toolbar *_toolbar;
   //QWidget *_toolbar;

   // more actions (toolbar)
   QAction *_actionPprev;

   Desktopmodelconv *_modelconv; //!< proxy <-> source model conversion
   Desktopmodelconv *_modelconv_assert;   //!< same, but only allows assertions

   QString _scroll_to;     //!< filename to scroll to when a new directory is opened

   // Text used for a recursive folder search
   QString _search_text;

   bool _showing_imports;

   Folderlist *_folders;
   };


class Toolbar : public QFrame, public Ui::Toolbar
{
    Q_OBJECT

public:
    Toolbar(QWidget* parent = 0, Qt::WindowFlags fl = Qt::WindowFlags());
    ~Toolbar();

    // Set whether the search butter-bar is shown or not
    void setSearchEnabled(bool enable);

    // Check whether the search butter-bar is shown or not
    bool searchEnabled();

    // Set whether the filter option is available or not
    void setFilterEnabled(bool enable);

public:
    QKeyEventTransition *pressed_esc;
};

class Move : public QDialog, public Ui::Move
{
   Q_OBJECT
};
