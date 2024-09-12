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
#include <QContextMenuEvent>

#include <QMouseEvent>
#include <QKeyEvent>


class QFontMetrics;

class Desk;
class Desktopitem;
class Desktopmodelconv;
class Desktopundostack;
class Filepage;
class PPage;
class Paperscan;
class QImage;

struct err_info;
struct max_page_info;

#include "qstring.h"
#include "qwidget.h"

#include "desk.h"
#include "file.h"

#include "qabstractitemmodel.h"

#include <QSortFilterProxyModel>


class QUndoStack;

#if 0
typedef struct ditem_info
   {
   file_info *file;     //! the file this item refers to
   QPixmap pixmap;     //! the pixmap for this item
   bool valid;          //! true if we have valid info for this item
   } ditem_info;
#endif


/* a directory containing a list of files

typedef struct diritem_info
   {
   QList<File *> _file;
   } diritem_info;
*/


typedef struct pagepos_info
   {
   QPoint pos;
   int pagenum;
   int pagecount;
   } pagepos_info;


/** implements an item model, which contains a number of thumbnails representing
paper stacks */

class Desktopmodel : public QAbstractItemModel
   {
   Q_OBJECT

   // allow the undo classes to access our state and in particular the opXXX functions
   friend class UCTrashStacks;
   friend class UCMove;
   friend class UCDuplicate;
   friend class UCUnstackStacks;
   friend class UCUnstackPage;
   friend class UCStackStacks;
   friend class UCChangeDir;
   friend class UCRenameStack;
   friend class UCRenamePage;
   friend class UCDeletePages;
   friend class UCUpdateAnnot;
   friend class UCAddRepository;
   friend class UCRemoveRepository;

public:
   Desktopmodel(QObject *parent);
   ~Desktopmodel();

   // debug output
   QDebug debug (void) const;

   enum e_role
      {
/* roles available in this model:

   Qt::DisplayRole         QString  stack name
   Qt::EditRole            QString  stack name (same as display)
   Qt::DecorationRole      QIcon    a QIcon of the pixmap

*/
      Role_position   = Qt::UserRole,  // QPoint   position
      Role_pixmap,            // QPixmap  pixmap
      Role_pagenum,           // Int      page number
      Role_pagecount,         // Int      page count
      Role_pagename,          // QString  page name

      Role_preview_maxsize,   // QSize    max preview size for file (this is the preview pixmap)
      Role_title_maxsize,     // QSize    title size
      Role_pagename_maxsize,  // QSize    max page name size
      Role_maxsize,           // QSize    max size including all features

      Role_pagename_list,     // QStringList list of page names
      Role_valid,             // bool     item has been built
      Role_droptarget,        // bool     true if this item is the drop target
      Role_message,           // QString  error or descriptive message
      Role_filename,          // QString  the file's filename (excluding directory)
      Role_pathname,          // QString  the file's full path

      Role_author,            // QString  the author
      Role_title,             // QString  the title (filename with perhaps some special characters like /)
      Role_keywords,          // QString  the keywords
      Role_notes,             // QString  the notes
      Role_error,             // QString  an error message, if available

      Role_count
      };

   /** returns a pointer to the undo stack for this model

      \return pointer to model's undo stack */
   Desktopundostack *getUndoStack (void);

   /** sets the model converter to use when needed for accessing the
       source model (as opposed to any proxy which might be in the way

       This class should only use this for assertion purposes

      \param modelconv  new model converter to use */
   void setModelConv (Desktopmodelconv *modelconv);

   /** return the total number of pages of all stacks in the list */
   int listPagecount (const QModelIndexList &list);

   /************************ MODEL ACCESS FUNCTIONS *******************/
   /** retrieve data from the model

      \param index   index to retrieve
      \param role    role required

      \returns data from the model */
   QVariant data (const QModelIndex &index, int role = Qt::DisplayRole) const;

   /** set data in the model and update the view

      \param index   index to update
      \param value   data value
      \param role    role required */
   bool setData (const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

   /** returns flags for a given model index

      \param index   index to check

      \returns flags for the given index */
   Qt::ItemFlags flags (const QModelIndex &index) const;

   /** returns the number of rows under a given parent

      \param parent     the parent index to check

      \returns number of rows under the parent */
   int rowCount (const QModelIndex &parent) const;

   /** returns the number of columns under a given parent

      \param parent     the parent index to check

      \returns number of columns under the parent */
   int columnCount (const QModelIndex &parent) const;

   /** returns the drop actions supported by this model

      \returns drop actions */
   Qt::DropActions supportedDropActions() const;

   /** returns an index given the parent, row and column

      \param row     row to retrieve
      \param column  column to retrieve
      \param parent  parent index

      \returns the index of the item, or QModelIndex() if none was found */
   QModelIndex index (int row, int col, const QModelIndex& parent) const;

   /** looks up a filename and returns its index

      \param fname   filename to look up
      \param parent  parent index (must be an index of a desk)
      \returns the model index for that filename, which is null if not found */
   QModelIndex index (const QString &fname, QModelIndex parent) const;

   /** returns the parent of a given index

      \param index   index to check

      \returns the parent of the item */
   QModelIndex parent (const QModelIndex& index) const;

   /** remove a Desk and all its children and File objects

     This only updates our internal data - it does not delete any files on disk.

     \param index    Index of Desk to delete (must be a top-level item)
      \returns true on success, false on failure (which would be bad) */
   bool removeDesk (const QString &pathname);

   //* returns a list of mime types that we can generate */
   QStringList mimeTypes() const;

   QMimeData *mimeData (const QModelIndexList &list) const;

   /** given a parent index which points to a desk, get the desk's directory
       name. It is safe to store this (instead of a model index) as it
       shouldn't change

       \param parent    parent to look up
       \returns full directory path for parent */
   QString deskToDirname (QModelIndex parent);

   /** given a directory name, return an index to the desk for this directory.
       If not found then an invalid model index will be returned

       \param dir    directory path to look up
       \returns the model index for that directory */
   QModelIndex deskFromDirname (QString &dir);

   /** builds a filename list from the given model index list. This list
       is a string containing each filename (not full pathname) separated
       by a space. Where the filename includes a space, the filename is
       enclosed in double quotes

      \param list    list of model indexes to include in the list
      \returns the list of filenames */
   QStringList listToFilenames (const QModelIndexList &list);

   /** builds a list of model indexes given a list of filenames. If any one
       filename is not found, then a null model index will be inserted in
       that position

       \param slist     list of filenames to look up
       \returns a list of model indexes, one for each filename */
   QModelIndexList listFromFilenames (const QStringList &slist,
         QModelIndex parent);

   /** gets the file info for a given model index

      \param index   the model index to look up
      \returns a pointer to the file information */
   File *getFile (const QModelIndex &index) const;

   /** gets the desk info for a given model index

      \param index   the model index to look up
      \returns a pointer to the file information, or 0 if not valid */
   Desk *getDesk (const QModelIndex &index) const;

   QString &deskRootDir (QModelIndex index);

   pagepos_info getPosData (const QModelIndex &index) const;

   /************************ OTHER FUNCTIONS *******************/

   /** sort a model index list in the way that delete operations require
       it. This is by order of row number, highest first. This allows
       a delete to proceed through the list from the start, deleting
       as it goes, without having to worry about row numbers later in
       the list changing underneath it.

       \param list      list to sort */
   void sortForDelete (QModelIndexList &list);

   /** sort a model index list so that items appear from top to bottom,
       left to right */
   void sortByPosition (QModelIndexList &list);

   /** print some progress information in the status bar */
   void progress (const char *fmt, ...);

   /** cancel a background refresh operation */
   void stopUpdate (void);

#if 0
   /** update the pixmap of an item item (which depends on its page number)

      \param di         item information
      \param pagename   returns page name of current page */
   void updatePixmap (ditem_info &di, QString &pagename);
#endif

   /** resize all items in the model by measuring them. This ignores all
       previously-held information about size and starts again. It scans
       each page of each stack to obtain the maximum preview size and
       pagename size. Then it updates the model, which causes the view
       to update

       \param parent    parent desk */
   void resizeAll (QModelIndex parent);

   void setDropTarget (const QModelIndex &index);
   const QPersistentModelIndex *dropTarget (void);

   bool isMinorChange (void) const { return _minor_change; }

   /* a list of filenames has been added to a directory. If the directory
      has been loaded into a desk, this desk needs to be notified. This
      function handle that. It simply adds the files to the desk

      \param dir        directory which now contains the new files
      \param fnamelist  list of filenames added */
   void addFilesToDesk (QString dir, QStringList &fnamelist);

   /* a list of filenames has been removed from a directory. If the directory
      has been loaded into a desk, this desk needs to be notified. This
      function handle that. It simply removes the files from the desk */
   void removeFilesFromDesk (QString dir, QStringList &fnamelist);

   /** clear all files in a desk */
   void clearAll (QModelIndex &parent);

   /****************** operations which support undo / redo ***************/
   /** add a number of stacks to a given stack. Stacks are added in order
       and are inserted before the destination stack's current page.
       Supports undo

       Note: src MUST BE SORTED from highest row number to lowest, otherwise
       this algorithm will crash!

      \param dest    destination stack to add to
      \param src     list of source stacks, ordered from high row number to low
      \returns       error, or NULL if none */
   err_info *stackItems (QModelIndex dest, QModelIndexList &src,
         QList<QPoint> *poslist);

   bool checkerr (struct err_info *err);

   /** duplicate a list of stacks. The new stacks will be added to the end
       of the model. Supports undo

       Commits any pending scan.

      \param list    the list of stacks to duplicate
      \param parent  parent stack
      \returns       error, or NULL if none */
   err_info *duplicate (QModelIndexList &list, QModelIndex parent);

   void duplicatePdf (QModelIndexList &list, QModelIndex parent);

   void duplicateJpeg (QModelIndexList &list, QModelIndex parent);

   /** duplicate a file as a .max file

      \param item       item to duplicate (NULL means the current item in the viewer)
      \param parent  parent stack
      \param odd_even   which pages to duplicate: 1 = even, 2 = odd, 3 = both */
   void duplicateMax (QModelIndexList &list, QModelIndex parent, int odd_even = 3);

   /** delete a list of stacks by moving them to the trash. The list may
       contain null model indexes, which are ignored. Supports undo

       Commits any pending scan

      \param list    list of stacks to delete, ordered from high row number to low
      \param parent  parent desk
      \returns      error, or NULL if none */
   void trashStacks (QModelIndexList &list, QModelIndex parent);

   /** conveniece function similar to the above

      Commits any pending scan

      \param index  stack to delete */
   void trashStack (QModelIndex &ind);

   /** unstack a list of stacks. Every page in the stack will get its own
       separate stack containing a single page. Supports undo

      Commits any pending scan

      \param list    list of stacks to unstack
      \param parent  parent desk
      \returns      error, or NULL if none */
   err_info *unstackStacks (QModelIndexList &list, QModelIndex parent);

   /** unstack a single page from a stack, to create a new stack containing
       only that page. Supports undo

      Commits any pending scan

      \param ind     the index of the stack to process
      \param pagenum the page number to unstack (-1 for current page)
      \param remove  true to remove the page from the stack also */
   void unstackPage (QModelIndex &ind, int pagenum, bool remove);

#if 0
// maybe implement this later
   /** unstack a list of pages from a stack, to create a new stack containing
       those pages. Supports undo.

       It is also possible to just mark the pages as deleted. They will still
       disappear but in this case a new stack is not created. A markOnly
       operation can be undone simply by reversing the marking, whereas
       otherwise the pages must be restacked into the original stack (assuming
       they were removed).

       Commits any pending scan

      \param ind     the index of the stack to process
      \param pages   the page numbers to unstack
      \param remove  true to remove the pages from the stack also
      \param markOnly    true to just mark them deleted and not create the new stack
                     false to create the new stack */
   void unstackPages (QModelIndex &ind, QBitArray &pages, bool remove, bool markOnly);
#endif
   /** delete a list of pages from a stack. Supports undo.

      Commits any pending scan

      \param ind     the index of the stack to process
      \param pages   the page numbers to unstack, bit n is true to unstack page n */
   void deletePages (QModelIndex &ind, QBitArray &pages);

   /** create and execute a new undo record to move stacks to a new position.
       Supports undo

      \param list       list of stacks to move
      \param parent     parent desk
      \param oldplist   oldp osition for each stack
      \param plist      new position for each stack */
   void move (QModelIndexList &list, QModelIndex parent, QList<QPoint> &oldplist, QList<QPoint> &plist);

   /** move a list of files to the given directory. Supports undo

      Commits any pending scan

      \param list    list of stacks to move
      \param parent  desk containing stacks
      \param dir     destination directory
      \param translist  returns list of resulting filenames
      \param copy    true to copy, false to move */
   void moveToDir (QModelIndexList &list, QModelIndex parent, QString &dir, 
         QStringList &trashlist, bool copy = false);

   /** change directory. Supports undo

      The root directory is used to set up a trash directory for the model.

      We allow an option to not use the undo stack - this is for the first
      first directory change on startup, since undoing that would put us back
      into not having a current directory. We don't want to user to be able
      to undo this first directory change.

      Commits any pending scan

      \param dirpath    directory to search
      \param rootpath   root directory for this branch of the model
      \param allow_undo use the undo else (else we just do the operation now) */
   void changeDir (QString dirPath, QString rootPath, bool allow_undo);

   /** rename a stack. Supports undo.

      \param index   stack to rename
      \param newname new name for stack, excluding .max extension */
   void renameStack (const QModelIndex &index, QString newname);

   /** rename a stack's current page. Supports undo.

      \param index   stack containing page to rename
      \param newname new name for page */
   void renamePage (const QModelIndex &index, QString newname);

   /** add a new repository to the list. Supports undo.

     \param dirPath  path to repository */
   void addRepository (QString dir_path);

   /** remove a repository from the list. Supports undo.

     \param dirPath  path to repository */
   void removeRepository (QString dir_path);

   /** arrange the selected items in the given order. Supports undo

      Commits any pending scan

      \param type    order type (t_arrangeBy)
      \param list    list of items to arrange
      \param parent  parent desk
      \param oldplist   current (original) position of items */
   void arrangeBy (int type, QModelIndexList &list, QModelIndex parent, QList<QPoint> &oldplist);

   /** update the annotation text on a stack. Supports undo

      Does not affect a pending scan

      \param ind     index to update
      \param updates updates to apply, indexed by MAXA_... */
   void updateAnnot (QModelIndex &ind, QHash<int, QString> &updates);

   /** returns an annotation text item on a stack

     If an error occurs then this will be stored with the file and available
     through the Role_error role.

      \param ind     index to check
      \param type    annotation type to return (File::e_annot)
      \returns returns text for annotation, if no error and it is available
   */
   QString getAnnot (QModelIndex ind, File::e_annot type) const;

   /*********************** UNDO / REDO operations **********************/
protected:
   /* these 'op' functions are used by the undo / redo stack to actually
      perform operations on the model */


   /** delete a list of stacks by moving them to the trash or another
       directory. The list may contain null model indexes, which are ignored

      \param list    list of stacks to delete, ordered from high row number to low
      \param trashlist  returns a list of filenames of the files that were placed in
                        the trash. These will normally be the same as the original
                        filenames, except when a file of the same name already
                        exists in the trash, in which case we rename this new one
      \param dest   use this directory instead of the trash
      \param copy   true to copy instead of move
      \returns      error, or NULL if none */
   err_info *opTrashStacks (QModelIndexList &list, QModelIndex parent,
         QStringList &trashlist, QString &dest, bool copy);

   /** undelete a list of files by moving them from the trash or another
       directory. Each file is renamed as it is moved according to the
       supplied name in filenames. You must have one 'filenames' entry for
       each 'trashlist' entry.

      \param trashlist  list of stacks to undelete (filename only, no path)
      \param filenames  list of filenames to use, one for each trash file
                        (note this may be updated if the filenames have to change)
      \param pagepos    if non-NULL, this is a list of pagepos_info records,
                        one for each of the files to be undelete. This provides
                        the position and page number for each undeleted file
      \param src    use this directory instead of the trash
      \param copy   true to copy instead of move
      \returns      error, or NULL if none */
   err_info *opUntrashStacks (QStringList &trashlist, QModelIndex parent, QStringList &filenames,
         QList<pagepos_info> *pagepos, QString &src, bool copy);

   /** move a list of stacks to the given positions

      \param list    list of stacks to move
      \param newpos  new position for each stack */
   void opMoveStacks (QModelIndexList &list, QModelIndex parent, QList<QPoint> &newpos);

   /** duplicate a list of stacks, recording its name in namelist (which
       should be empty when this function is called.

      \param list       list of stacks to duplicate
      \param parent     parent desk
      \param namelist   returns list of filenames used
      \param type       type to duplicate as e_type (e.g. max, pdf)
      \param odd_even   only supported for type = Type_max
                        normally 3
                           - bit 0 set means duplicate even pages
                           - bit 1 set means duplicate odd pages
      \returns      error, or NULL if none */
   err_info *opDuplicateStacks (QModelIndexList &list, QModelIndex parent, QStringList &namelist,
         File::e_type type, int odd_even = 3);

   /** delete a single stack without moving it to the trash

      \param index   stack to delete
      \returns      error, or NULL if none */
   err_info *opDeleteStack (QModelIndex &index);

   /** delete a list of stacks without moving them to the trash. The list may
       contain null model indexes, which are ignored

      \param list    list of stacks to delete, ordered from high row number to low
      \returns      error, or NULL if none */
   err_info *opDeleteStacks (QModelIndexList &list, QModelIndex parent);

   /** unstack a list of stacks. For each stackm this produces a number of new
       separate stacks. We record the names of these in _newnames. Each item
       of _newnames is a string list, containing one filename for each page
       of that stack that was unstacked

       \param list      list of stacks to unstack
       \param newnames  list of name lists for each stack
       \returns      error, or NULL if none */
   err_info *opUnstackStacks (QModelIndexList &list, QModelIndex parent,
         QList<QStringList> &newnames);

   /** given a list of stacks, this function restacks pages onto them. The
       pages to be restacked are in pagenames which is a list of stringlists,
       one for each stack. The contents of each stringlist are scanned and
       each filename is stacked back onto the original stack, in order

       \param list      list of stacks to replenish
       \param pagenames list of stringlists, each containing a list of page
                        names to restack
       \param destpage  destination page in each destination stack to insert
                        the new stacks into (-1 for the current page in each
                        stack, in which case it returns the actual dest page
                        of the first stack)
       \returns      error, or NULL if none */
   err_info *opRestackStacks (QModelIndexList &list, QModelIndex parent,
         QList<QModelIndexList> &pages_list, int &destpage);

   /** unstack a single page from a stack, creating a new stack for it
       containing just that page

       \param index     index of stack to find page
       \param pagenum   page number to unstack (-1 for current). Returns page
                        actually unstacked
       \param remove    true to remove the page from the source stack, false
                        to just copy it (leaving the original page intact)
       \param newname   returns the name given to the new single-page stack
       \returns         error, or NULL if none */
   err_info *opUnstackPage (QModelIndex &index, int &pagenum,
      bool remove, QString &newname);

   /** stack a list of stacks into a stack at page 'pagenum'

      \param index      index of destination stack
      \param list       list of stacks to stack
      \param pagenum    destination page number (stacks will be inserted here).
                        If this is -1, the current page in the destination
                        stack is used (and returns this number)
      \returns      error, or NULL if none */
   err_info *opStackStacks (QModelIndex &index, QModelIndexList &list, int &pagenum);

   /** unstack a number of groups of pages from a source stack, putting each
      group into its own new stack. The pages are removed from the source
      stack.

      The first page to be unstacked is srcpagenum. The files list gives
      information about each file to be unstacked - in particular it gives
      the position and the number of pages in each new stack

      The total number of pages, n, to be unstacked is therefore the number of
      items in the files list (and also namelist). After this operation, pages
      'srcpagenum' to 'srcpagenum + n - 1' will have been removed from the stack

      This operation is really used for undoing a stack operation, where a
      number of stacks were stacked into another stack.

      \param src    source stack from which to unstack
      \param newnames   the name of each new stack
      \param srcpagenum the first source stack page number to unstack
      \param pagepos    information about each new stack
                           ->pagecount   number of pages to unstack
                           ->pos         position to unstack to
      \returns      error, or NULL if none */
   err_info *opUnstackFromStack (QModelIndex &pagepos, QStringList &_newnames,
      int srcpagenum, QList<pagepos_info> &files);

   /** Change directory operation

      The root directory is used to set up a trash directory for the model.

      \param dirpath    directory to search
      \param rootpath   root directory for this branch of the model */
   void opChangeDir (QString &dirPath, QString &rootPath);

   /** rename a stack

      \param index   the stack to rename
      \param newname the new name to be given (this will auto-rename and
                     return what is actually assigned
      \returns       error, or NULL if none */
   err_info *opRenameStack (const QModelIndex &index, QString &newname);

   /** rename a page and move to show that page

      \param index   the stack containing the page to rename
      \param pagenum the page number to rename
      \param newname the new name to be given
      \returns       error, or NULL if none */
   err_info *opRenamePage (const QModelIndex &index, int pagenum, QString &newname);

   /** delete a list of pages from a stack.

      \param ind     the index of the stack to process
      \param pages   the page numbers to delete, bit n is true to delete page n
      \param del_info information about deleted pages, used for undo
      \param count    returns number of pages deleted */
   err_info *opDeletePages (QModelIndex &ind, QBitArray &pages,
         QByteArray &del_info, int &count);

   /** undelete a list of pages from a stack.

      This works by restoring the supplied page info into the stack and marking
      those pages as undeleted again.

      The pages array is the one passed to opDeletePages() so needs to be
      processed in forward order. For example, if in a delete bits 2 and 4 were
      set we might have:

         0  fred
         1  john
         2  mary   x
         3  bert
         4  mark   x
         5  anne
         6  joan

      where x marks a page to be deleted. Afterwards we get:

         0  fred
         1  john
         2  bert
         3  anne
         4  joan

      For the undelete, we also get bits 2 and 4 set. These bits mark where
      pages should be inserted. The first is before page 2:

         0  fred
         1  john
         2  mary   x
         3  bert
         4  anne
         5  joan

      and the second before page 4 to give the original stack

         0  fred
         1  john
         2  mary   x
         3  bert
         4  mark   x
         5  anne
         6  joan

      \param ind     the index of the stack to process
      \param pages   the page numbers to undelete, bit n is true to undelete page n
      \param del_info information about deleted pages, previously stored
      \param count   number of pages to restore */
   err_info *opUndeletePages (QModelIndex &ind, QBitArray &pages,
         QByteArray &del_info, int count);

   /** update the annotation strings of a stack

      \param ind     index of stack to update
      \param updates updates to make, indexed by type (MAXA_...) */
   err_info *opUpdateAnnot (QModelIndex &ind, QHash<int, QString> &updates);

   /** update the list of repositories by adding/removing a dir

     \param dirpath        Directory to add / delete
     \param add_not_delete true to add, false to delete */
   void opUpdateRepositoryList (QString &dirpath, bool add_not_delete);

public:
   // this is public since it is called from outside the undo system
   
   /** email a set of files as attachments, optionally converting them first. 
       If more than one file is sent, then they are packed into a zip archive
       as it seems that Thunderbird can't cope with more than one attachment
       through its '-compose- interface
   
      \param parent  desk containing the files 
      \param slist   list of files to send
      \param type    type to convert to (or Type_other to leave as is)
      \returns error or NULL if ok */
   err_info *opEmailFiles (QModelIndex parent, QModelIndexList &slist,
         File::e_type type);

#if 0 //p
   Desktopitem *itemFind (const QPoint & opos, int &which);
   Desk *getDesk () { return _desk; }
   void repaintItem( Desktopitem *item );
#endif

   /****************************** scanning operations **********************/

   /*
       beginScan() is called when we start scanning into a new stack
         - it creates the stack and emits beginningScan() for the benefit
             of Pagewidget

       pageStarting() is called when a new page is starting to be scanned.
          - we simply remember this in case we are asked to create a preview
               for Pagewidget
          - we also emit beginningPage() for the benefit of Pagewidget
               (this will likely call Desktopmodel::registerScaledImageSize() to
                 tell Desktopmodel what scale is required for previews)

       pageProgress() is called regularly as the page is scanned in
          - we record the data that we are given in case we are asked to
               create a preview later on
          - we also emit dataAddedToPage() for the benefit of Pagewidget
               (which might call Desktopmodel::getScaledImage())
          - if we were told about a required scaled image size
               by a call to Desktopmodel::setScaledImageSize (see above)
               then we emit newScaledImage() for the benefit of Pagewidget.
               It will draw it on the screen

       addPageToScan() is called after every page is scanned to add a new
         page to the stack. It adds the page and emits newScannedPage()
         for the benefit of Pagewidget. At this point all page progress
         is irrelevant as the page has been committed

       confirmScan() is called at the end to finalise the stack

       or:

       cancelScan() is called at the end to cancel the stack
            - it deleted it as if the scan had never taken place
    */


   /** add a new paper stack to the model for scanning into. Later, you
       must call either confirmScan() to save it or cancelScan() to
       remove it.

      \param parent  parent desk to scan into
      \param stack   stack to add
      \returns       error, or NULL if none */
   err_info *beginScan (QModelIndex parent, const QString &stack_name);

   /** confirm and save the pending scan */
   err_info *confirmScan (void);

   /** cancel and remove the pending scan */
   err_info *cancelScan (void);

   /** add a newly scanned page to the current scan file

      \param max_page_info    info about the page
      \param coverageStr      page coverage info */
   err_info *addPageToScan (const Filepage *mp, const QString &coverageStr);

   /** returns the size of the stack currently being scanned. It will grow
       as more pages are added */
   int getScanSize (void);

   /** a new page is starting to be scanned */
   void pageStarting (Paperscan &scan, const PPage *page);

   /** new data has arrived for the current page */
   void pageProgress (Paperscan &scan, const PPage *page);

   /** tell Desktopmodel what size we want the scaled images to be */
   void registerScaledImageSize (const QSize &size);

   /** this is a bit of a monster... it commits changes to a stack, but this
       is not always simple. There are three cases:

          1 we are doing it from a pagemodel, where we want to support undo
          2 we are doing it from a pagemodel, but undo cannot be supported
               because the user has clicked elsewhere - conceptually we can
               imagine that the scan can commit become a single operation
          3 the pagemodel has lost everything, because another directory
               has been selected

       For 1 we use the normal 'undo' functions
       For 2 we use the direct 'op' functions
       For 3 we use maxdesk directly (yukk!)

       In order to simplify this we would need Desktopmodel to be more
       aware of directories, perhaps unifying Desktopmodel and Dirmodel.
       But for the moment that seems a bridge to far, with other uncertain
       implications, so we are left with this function

       \param ind    index of stack to commit (will be ignored if the directory
                     has changed
       \param del_count number of pages to delete
       \param page   bit n means delete page n
       \param allow_undo  true to allow the user to undo this */
   err_info *scanCommitPages (QModelIndex &ind, int del_count,
         QBitArray &page, bool allow_undo);

signals:
   /** indicate that we are about to start scanning into an item

      \index   item we are scanning into */
   void beginningScan (const QModelIndex &index);

   /** indicate that we are about to scan a new page */
   void beginningPage (void);

   /** indicate that a scan is about to end

      \param cancel true if scan was cancelled (so stack is about to be deleted) */
   void endingScan (bool cancel);

   /** indicate that a new scaled image is available for the page currently
       being scanned

      \param image            the new part of the image
      \param scaled_linenum   destination start line number for this image */
   void newScaledImage (const QImage &image, int scaled_linenum);

   /** request that the scan stack be commited, because we are about to
       operate on it */
   void commitScanStack (void);

   /** request an update to the repository list

     \param dirpath        Directory to add to / delete from list
     \param add_not_delete true to add, false to delete
     */
   void updateRepositoryList (QString &dirpath, bool add_not_delete);

private:
   bool getNewScaledImage (Paperscan &scan, const PPage *page, const char *data,
         int nbytes, QImage &image, int &scaled_linenum);

   /** check if the list contains the scan stack - if so commit it

      returns false if we are scanning and so the operation is not permitted */
   bool checkScanStack (QModelIndexList &list, QModelIndex parent);

   /***********************************************************************/
public:
#if 0 //p
   Q3DragObject *dragObject(void);

   /** confirm the addition of a paper stack to a maxdesk */
   struct err_info *confirmPaperstack (Desk *maxdesk, file_info *f);

   struct err_info *cancelPaperstack (Desk *maxdesk, file_info *f);

   err_info *operation (Desk::operation_t type, int ival);

   /** sets the debug level to use for newly created max files */
   void setDebugLevel (int level);

   /** returns true if the user is busy scrolling through pages */
   bool userBusy (void);

   void updatePage (Desktopitem *item);
   void pageLeft (Desktopitem *item = 0);
   void pageRight (Desktopitem *item = 0);
#endif

   //! record that no items have been added
   void resetAdded (void);

   /** record that items are about to be added

      \param parent  parent (desk) which will contain new items */
   void aboutToAdd (QModelIndex parent);

   /** request information on items added by a recent operation

      \param parent  parent containing new items
      \param start   returns row of first item added
      \param count   returns number of items added
      \returns true if any items added */
   bool itemsAdded (QModelIndex parent, int &start, int &count);

   //! returns the current directory path being displayed
   QString &getDirPath (void);

   /** clear the current directory path */
   void resetDirPath (void);

   /** read an image for page 'pagenum' of the file

      \param ind      model index to read
      \param pagenum  page number to read
      \param do_scale always false, not used (TODO: remove?)
      \param *imagep  returns pointer to allocated image (must be deleted by caller)
      \param Size     returns original size of image (image may be slightly
                           larger to accomodate padding margins)
      \param trueSize returns actual size of image (including padding margins)
      \param bpp      returns number of bits per pixel in orginal image

      \returns error, or NULL if none */
//    err_info *getImage (const QModelIndex &ind, int pagenum, bool do_scale,
//                   QImage **imagep, QSize &Size, QSize &trueSize, int &bpp);

   /** get image, updating the existing reference */
   err_info *getImage (const QModelIndex &ind, int pnum, bool do_scale,
                  QImage &imagep, QSize &Size, QSize &trueSize, int &bpp, bool blank = false) const;

   /** gets an image of a page scaled to the exact requested size, using
       either the preview or full image. Returns a pixmap which is first freed
       if non-null.

       In the case of an error, this function returns an error pixmap, and
       does not report the error. It can presumably be recreated and is
       probably logged against the file anyone, so the the user can later
       click on the file to obtain the error message.

       \param ind       stack index
       \param pagenum   page number
       \param size      desired size in pixels
       \param pixmap    on entry, the old pixmap (or 0), on exit the new pixmap
       \param blank     modified the pixmap to indicate that this page is blank */
   void getScaledImage (const QModelIndex &ind, int pagenum, QSize &size,
         QPixmap &pixmap, bool blank) const;

   /** reads the OCR text from a page

      \param ind     index of stack to read
      \param pagenum page number to read
      \param str     returns string containing OCR text */
   err_info *getPageText (const QModelIndex &ind, int pagenum, QString &str);

   QString getPageName (const QModelIndex &ind, int pnum) const;

   /** works through a list of indexes, counting the pages in each stack.
       Returns the total number of pages

      \param list    list to count
      \param sepSheets true to put each stack on a separate sheet (i.e. with
                       a blank page afterwards if it is an odd number of pages.
                       This helps with duplex printers.
      \returns total number of pages */
   int countPages (QModelIndexList &list, bool sepSheets);

   /** returns a string with information on the given page in the format

      aMP, bxc d ebpp, fMB of gMB

      where:
         a is Megapixels
         b is width in pixels
         c is height in pixels
         d is 'Mono/Grey/Colour'
         e is size in bytes
         f is size of stack in bytes

     \returns image information string */
   QString imageInfo (const QModelIndex &ind, int pagenum, bool extra) const;

   /** returns the stack size in bytes

      \param ind           model index of stack */
   int imageSize (const QModelIndex &ind) const;

   /** returns information about the image and preview sizes

      \param ind           model index of stack
      \param pagenum       page number within stack
      \param preview_size  returns pixel size of preview
      \param image_size    returns true pixel size of image (including padding margins) */
   err_info *getImagePreviewSizes (const QModelIndex &ind, int pagenum,
      QSize &preview_size, QSize &image_size) const;

   /** returns the preview image for a particular page. If there is an
       error it returns an error pixmap

      \param ind        model index of stack
      \param pagenum    page number within stack
      \param *pixmapp   returns pixmap
      \param blank         true to show image as 'blank'
      \returns error, or NULL if none */
   err_info *getImagePreview (const QModelIndex &ind, int pagenum,
         QPixmap &pixmap, bool blank = false) const;

   /** deletes a pixmap if non-null, and not one of our internal ones. Sets
       *pixmapp to 0

       \param *pixmapp  pixmap pointer to delete */
   void deletePixmap (QPixmap **pixmapp) const;

   /** returns the timestamp for the given image

      \returns string containing timestamp */
   QString imageTimestamp (const QModelIndex &ind, int pagenum);

   /** \returns true if we created this model by searching multiple
       subdirectories, false if it was just a single directory */
   bool searchedSubdirs (void) { return _searched_subdirs; }

signals:
   void newContents (const char *str);

   /** emitted when the current directory has changed

      \param dirPath    the directory we changed to
      \param index      index of the new desk in Desktopmodel */
   void dirChanged (QString &dirPath, QModelIndex &index);

   //! signal emitted when we have finished updating the viewer
   void updateDone ();

   //! signal emitted when we have changed the model. We recheck item positions
   void modelChanged ();

   /** signal emitted when we have changed directory and finished adding
       to the model */
   void viewRefreshed ();

   void canRedoChanged (bool canUndo);
   void canUndoChanged (bool canUndo);
   void redoTextChanged (const QString &redoText);
   void undoTextChanged (const QString &undoText);
   void undoChanged (void);

#if 0//p
   void signalStackItem (Desktopitem *dest, Desktopitem *src);
   void signalStackItems (Desktopitem *dest, Q3PtrList<Desktopitem> &items);
   void showPage (Desk *maxdesk, file_info *f);
   void signalItemMoved (Desktopitem *item);
   void selectFolder (const QString &dir);

   /** signal emitted when a new item is selected

      \param item    item selected
      \param user_busy  true if the user is busy doing things, so we should hurry */
   void itemSelected (Desktopitem *item, bool user_busy);
#endif
   /** emitted when a new page is scanned

      \param coverageStr      coverage string for user
      \param mark_blank       true if page should be marked blank */
   void newScannedPage (const QString &coverageStr, bool mark_blank);

public:
   /** clone a model

      \param contents      parent model
      \param slist         list of indexes to add from parent model */
   void cloneModel (Desktopmodel *contents, QModelIndex parent);

   /**
    * @brief Select a new directory to display
      @param dirpath    directory to show
      @param rootpath   root directory for this branch of the model
      @returns parent model-index for the items
    */
   QModelIndex showDir(QString dirPath, QString rootPath);

   /**
    * @brief Do a folder search and show the results
    * @param dirpath    directory to search (recursively)
    * @param rootpath   root directory for this branch of the model
    * @param match      string to match against filenames (QString() for all)
    * @param op         operation to update as things progress
    * @return parent index for the added items
    */
   QModelIndex folderSearch(QString dirPath, QString rootPath,
                            const QString &match, Operation *op);

   /**
    * @brief Refresh the current directory contents
      @param dirpath    directory to refresh
      @returns parent model-index for the items
    */
   QModelIndex refresh(QString dirPath);

public slots:
   /** advises the model of the current context index (default item for
       operations */
//    void slotNewContextEvent (QModelIndex &index);

#if 0//p
//    void moveDir (QString &src, QString &dst);

#endif //p

protected slots:
   /** add the next item in the maxview to the viewer */
   void nextUpdate (void);

   /** save the maxdesk file */
   void aboutToQuit (void);

#if 0 //p

   void duplicateTiff (Desktopitem *item = 0);
   void deleteStack (void);
   void locateFolder (Desktopitem *item = 0);
   void duplicatePage (Desktopitem *item = 0);

private:
   void doUnstackPage (Desktopitem *item, bool remove);
   void contentsContextMenuEvent ( QContextMenuEvent * );
   Desktopitem *itemFind (struct file_info *file);

#endif // 0
   /** open the file at the given row in the list, get its pixmap and update
       the view */
   void buildItem (QModelIndex index);

   /** flush all desks */
   void flushAllDesks (void);

protected:
   /** remove rows from the model. Note this does not change the underlying
      maxdesk information, which is assumed to be done already */
   void internalRemoveRows (int row, int count, const QModelIndex &parent);

   /* remove rows from the model. Note this does not change the underlying
      maxdesk information, which is assumed to be done already.

      \param row     the row number to remove
      \param count   number of rows to remove
      \param parent  the parent model index */
   bool removeRows (int row, int count, const QModelIndex &parent);

   /* remove rows from the model. Note this does not change the underlying
      maxdesk information, which is assumed to be done already.

      \param list    list of rows to remove */
   void removeRows (QModelIndexList &list);

   /** insert a new row into the list

      \param row     the row number to insert before
      \param parent  parent model index
      \param di      the item to insert */
   bool insertRow (int row, const QModelIndex &parent);

   /** handles a new item being created in the model. The item must have been
       already added to the Desk

      This function does these things:
         1. Tells the model about the new row
         2. Emits datachanged() so that its pixmap will appear, etc.

      \param row     row number of the item
      \param parent  parent desk
      \param list    a model index list to which the new item is added */
   void newItem (int row, QModelIndex parent, QModelIndexList &list);

   /** adds a number of new items to the model - they will added after all
       other rows

       \param flist  list of files to add */
   void insertRows (QList<File *> &flist, QModelIndex parent);

   /** save persistent model indexes in preparation for a change to the model.
       This function isn't useful yet... */
   void savePersistentIndexes (void);

   /** restore persistent model indexes (some may become invalid) */
   void restorePersistentIndexes (void);

   /** email a set of files
   
      \param fname   filename to use for zip file (if required). The extension 
                     of this is ignored and replaced with .zip
      \param fnameList list of filenames to send (each a full path)
      \param can_delete returns true if the original files can be deleted, 
                     false if the email program wants them to stay around
      \returns error or NULL if ok */
   err_info *emailFiles (QString &fname, QStringList &fnamelist, bool &can_delete);

private:
   QList<Desk *> _desks;  //!< the model directories
   QTimer *_updateTimer;  //!< each time this times out we update another item

   //! we update items in the background, this is the next one to be updated
   int _upto;
//    Desk *_desk;
//   Desktopitem *_hit;    //!< desktop item which was clicked on
   int _hitwhich;        //!< which part of desktop item was clicked
//    Desktopitem *_contextItem;
   int _addCount;        //!< number of things added to the maxdesk so far
   Operation *_op;       //!< operation being performed
   int _debug_level;     //!< the debug level to use for max debugging
   bool _subdirs;        //!< true if searched subdirectories
   QString _forceVisible;  //!< pathname of item to force to be visible when updating is completed
   bool _stopUpdate;     //!< signal to stop an update which is in progress
   QModelIndex _update_parent;   //!< parent desk for current update operation

   // note that these two change all the time
   QString _dirPath;    //!< current directory path
   QString _rootPath;   //!< root path for current directory

   QPersistentModelIndex *_drop_target;
   QPersistentModelIndex _context_index;
   Desktopundostack *_undo;   //!< the undo stack
   QModelIndex _scan_parent;  //!< index of desk that we are scanning into
   Desk *_scan_desk;          //!< desk that we are scanning into
   File *_scan_file;     //!< file that we are scanning into
   bool _scan_err;          //!< true if we hit an error while scanning and should stop
   bool _about_to_add;        //!< true if about to add some items
   int _add_start;            //!< first row of added item
   Desktopmodelconv *_modelconv;  //!< model converter
   bool _searched_subdirs;    //!< true if this model may contain files from many directories
   QFontMetrics *_fm;         //!< metrics for the standard font (used for guessing sizes)

   /** true if we should generate a scaled image when new scan data is
       received */
   bool _need_scaled_image;
   QSize _scaled_image_size;  //!< size of scaled image to generate
   int _scaled_linenum;       //!< the scaled line number we are up to in the scan
   QPixmap _unknown;
   QPixmap _no_access;
   QStringList _persistent_filenames;  //!< list of filename for each persistent model index (used when saving)
   QList<QPersistentModelIndex> _pending_scan_list; //!< list of stacks which need to be scanned

   /** this field should not be required any more. With the previous model
       revision, files were deleted in opDeleteStacks() before they were
       removed from the model. Now, we call File::remove() which only
       deletes the file. The actually delete of the File object happens
       later, properly bracketed by beginRemoveRows() and endRemoveRows() */
   bool _model_invalid;  //!< model is temporarily invalid and cannot be accessed

   bool _minor_change;   //!< true if a dataChanged() signal is only for a minor change (no image data)

   QTimer *_flushTimer;    //!< time to indicate when we need to flush the desks
   QModelIndex _subdirs_index;      //!< model index for our subdirectory search desk
   bool _cloned;  //!< true if this model is cloned from another
   };



/* we use a proxy class to filter the source model. This allows us to easily
implement the filter feature, where the user can type a few characters and
see only those files which match.

The view uses the proxy model, but most of the underlying operations use
the source model, typically referred to as _content. It is possible to
convert indexes between the two using the class Desktopmodelconv */


class Desktopproxy : public QSortFilterProxyModel
   {
   Q_OBJECT
public:
   Desktopproxy (QObject *parent);
   ~Desktopproxy ();

   // set up the proxy to include only the given rows from the parent
   void setRows (int parent_row, int child_count, QList<int> &rows);

protected:
   bool filterAcceptsRow (int source_row, const QModelIndex& source_parent) const;

private:
   QBitArray _rows;
   int _parent_row;
   bool _subset;
   };


/** a class for converting things between the proxy model and the source
model. This class understands how to do these operations, and can be
provided to other classes that need it */

class Desktopmodelconv : public QObject
   {
   Q_OBJECT
public:
   Desktopmodelconv (Desktopmodel *source, Desktopproxy *proxy, bool canConvert = true);

   /** constructor for a null converter which does nothing */
   Desktopmodelconv (Desktopmodel *source, bool canConvert = true);
   ~Desktopmodelconv ();

   /** given a model, this returns the Desktopmodel equivalent. This might
       normally be the same thing (with just a cast), but where a proxy
       model is in use (for sorting or filtering) then they are completely
       different.

       \param model  base model which the indexes relate to
       \returns Desktopmodel pointer */
   Desktopmodel *getDesktopmodel (const QAbstractItemModel *model);

   /** converts a list of model indexes to those required by Desktopmodel

       \param model  base model which the indexes relate to
       \param list   list to convert */
   void listToSource (const QAbstractItemModel *model, QModelIndexList &list);

   /** converts a model index to one required by Desktopmodel

       \param model  base model which the index relate to
       \param index  index to convert */
   void indexToSource (const QAbstractItemModel *model, QModelIndex &index);

   /** converts a list of model indexes to those required by the proxy

       \param model  base model which the indexes relate to
       \param list   list to convert */
   void listToProxy (const QAbstractItemModel *model, QModelIndexList &list);

   /** converts a model index to one required by the proxy

       \param model  base model which the index relate to
       \param index  index to convert */
   void indexToProxy (const QAbstractItemModel *model, QModelIndex &index);

   void debugList (QString name, QModelIndexList &list);

   /** asserts that everything passed in relates to the source model rather
       than the proxy model

       \param model  base model which the indexes relate to
       \param index  index to check, or 0
       \param list   list to check, or 0 */
   void assertIsSource (const QAbstractItemModel *model,
      const QModelIndex *index, const QModelIndexList *list);

private:
   Desktopmodel *_source;
   Desktopproxy *_proxy;
   bool _can_convert;   //!< true if conversions are allowed
   };

