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

#include <assert.h>

#include "config.h"

#include "qapplication.h"
#include "qcursor.h"
#include "qmessagebox.h"
#include "qtimer.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QMimeData>
#include <QPixmap>
#include <QMouseEvent>
#include <QUndoStack>

#include "err.h"
#include "mem.h"

#include "desktopmodel.h"
#include "desktopundo.h"
#include "desk.h"
#include "file.h"
#include "paperstack.h"
#include "utils.h"


// time to delay between scanning each stack, should be 0 unless testing
#define DELAY_TIME 0  //1000


Desktopmodel::Desktopmodel(QObject *parent)
      : QAbstractItemModel (parent)
   {
//    _desk = 0;
   _updateTimer = new QTimer (this);
   _updateTimer->setSingleShot (true);
   _debug_level = 0;
   _op = 0;
   _subdirs = false;
   _forceVisible = QString();
   _drop_target = 0;
   QFont font;
   _fm = new QFontMetrics (font);
   _scan_desk = 0;
   _scan_file = 0;
   _model_invalid = false;
   _minor_change = false;
   _need_scaled_image = false;
   _cloned = false;

   _flushTimer = new QTimer (this);
   connect(_flushTimer, SIGNAL(timeout()), this, SLOT(flushAllDesks()));
   _flushTimer->start(1000 * 60);  // flush every minute

   _undo = new Desktopundostack (this);
   connect (_undo, SIGNAL (undoTextChanged (const QString &)),
      this, SIGNAL (undoTextChanged (const QString &)));
   connect (_undo, SIGNAL (redoTextChanged (const QString &)),
      this, SIGNAL (redoTextChanged (const QString &)));
   connect (_undo, SIGNAL (canUndoChanged (bool)),
      this, SIGNAL (canUndoChanged (bool)));
   connect (_undo, SIGNAL (canRedoChanged (bool)),
      this, SIGNAL (canRedoChanged (bool)));

   connect (_undo, SIGNAL (undoTextChanged (const QString &)),
      this, SIGNAL (undoChanged ()));
   connect (_undo, SIGNAL (redoTextChanged (const QString &)),
      this, SIGNAL (undoChanged ()));
   connect (_undo, SIGNAL (canUndoChanged (bool)),
      this, SIGNAL (undoChanged ()));
   connect (_undo, SIGNAL (canRedoChanged (bool)),
      this, SIGNAL (undoChanged ()));

   connect (_updateTimer, SIGNAL (timeout()), this, SLOT (nextUpdate ()));
   connect (qApp, SIGNAL (aboutToQuit ()),
               this, SLOT (aboutToQuit ()));
   if (!_unknown)
      {
      _no_access = QPixmap (":images/images/no_access.xpm");
      _unknown = QPixmap (":images/images/unknown.xpm");
      Q_ASSERT (!_no_access.isNull ());
      Q_ASSERT (!_unknown.isNull ());
      }
   }


Desktopmodel::~Desktopmodel()
   {
   delete _fm;
   if (!_cloned)
      while (!_desks.isEmpty ())
         delete _desks.takeFirst ();

   // ensure the maxdesk is correctly closed (writes the maxdesk.ini file)
//    if (_desk)
//       delete _desk;
   }


#define MAX_ID 10240  // maximum ID we can assign to a 'desk' node

/** index points to a Desk */
#define IS_DESK(ind) ((unsigned)(ind).internalId () < MAX_ID)

/** index points to a File */
#define IS_FILE(ind) ((unsigned)(ind).internalId () >= MAX_ID)

#define DESK_INDEX(row) createIndex (row, 0, row + 10)

#define FILE_INDEX(row,f) createIndex (row, 0, (void *)f)


QVariant Desktopmodel::data(const QModelIndex &index, int role) const
   {
//    qDebug () << "data" << index << role;
   if (!index.isValid() || _model_invalid)
      return QVariant();

   if (!IS_FILE (index))
      return QVariant ();

   Q_ASSERT (IS_FILE (index));
   File *f = (File *)index.internalPointer ();

   // if we don't have valid information for this stack, make a note to get it
   if (!f->valid () && role == Role_pixmap)
      {
      Desktopmodel *model = (Desktopmodel *)this;

      // don't add if already there (can happen with multiple redraws of an item)
      if (!model->_pending_scan_list.contains (index))
         {
         // restart the timer if not running
         if (_pending_scan_list.isEmpty ())
            _updateTimer->start (DELAY_TIME);
         model->_pending_scan_list << index;
         }
      }

   switch (role)
      {
      case Qt::DecorationRole :
         return QIcon (f->pixmap ());

      case Qt::DisplayRole :
      case Qt::EditRole :
         return f->basename ();

      case Role_position :
         return f->pos ();

      case Role_preview_maxsize :
         return f->previewMaxsize ();

      case Role_pixmap :
         return f->pixmap ();

      case Role_pagenum :
         return f->pagenum ();

      case Role_pagename :
         return f->pageTitle (-1);

      case Role_pagecount :
         return f->pagecount ();

      case Role_title_maxsize :
         return f->titleMaxsize ();

      case Role_pagename_maxsize :
         return f->pagenameMaxsize ();

      // this is only a guess, since we don't know what font will be used
      case Role_maxsize :
         {
         QSize size;

         // use the maximum width
         // for height, use the pixmap preview plus 3 lines of text
         size = f->previewMaxsize ();
         size = size.expandedTo (f->titleMaxsize ());
         size = size.expandedTo (f->pagenameMaxsize ());
         size.setHeight (f->previewMaxsize ().height ()
            + 3 * _fm->height ());
         return size;
         }

      // returns a string list of all the page names
      case Role_pagename_list :
         {
         QStringList name;
         int i;

         for (i = 0; i < f->pagecount (); i++)
            name << f->pageTitle (i);
         return name;
         }

      case Role_valid :
         return f->valid ();

      case Role_droptarget :
         return _drop_target && index == *_drop_target;

      case Role_message :
         if (f->err ())
            return f->err ()->errstr;
         else
            {
            QString str;

            str = imageInfo (index, f->pagenum (), true);
/*
            util_bytes_to_user (numstr, f->size);
            str = QString ("%1 page%2, %3").arg (f->pagecount).arg (f->pagecount > 1 ? "s" : "").arg (numstr);
*/
            if (_subdirs)
               str += ", at " + f->pathname ();
            return str;
            }

      case Role_pathname :
         return f->pathname ();

      case Role_filename :
         return f->filename ();

      case Role_author :
         return getAnnot (index, File::Annot_author);

      case Role_title :
         return getAnnot (index, File::Annot_title);

      case Role_keywords :
         return getAnnot (index, File::Annot_keywords);

      case Role_notes :
         return getAnnot (index, File::Annot_notes);

      case Role_ocr :
         return getAnnot (index, File::Annot_ocr);

      case Role_error :
         if (f->err ())
            return f->err ()->errstr;
         break;
      }

   return QVariant();
   }


bool Desktopmodel::setData(const QModelIndex &index, const QVariant &value, int role)
   {
   bool changed = false;

   if (!index.isValid() || !IS_FILE (index))
      return false;

   Q_ASSERT (IS_FILE (index));
   File *f = (File *)index.internalPointer ();

   switch (role)
      {
      case Qt::EditRole :
         renameStack (index, value.toString ());
         break;

      case Qt::DecorationRole :
      case Qt::DisplayRole :
      case Role_pixmap :
      case Role_pagecount :
         break;

      case Role_pagename :
         renamePage (index, value.toString ());
         break;

      case Role_preview_maxsize :
         f->setPreviewMaxsize (value.toSize ());
         break;

      case Role_title_maxsize :
         f->setTitleMaxsize (value.toSize ());
         break;

      case Role_pagename_maxsize :
         f->setPagenameMaxsize (value.toSize ());
         break;

      case Role_position :
         f->setPos (value.toPoint ());
         changed = true;
         break;

      case Role_pagenum :
         {
         int newpage = value.toInt ();

         if (newpage < 0)
            newpage = 0;
         if (newpage >= f->pagecount ())
            newpage = f->pagecount () - 1;
         if (newpage != f->pagenum ())
            {
            f->setPagenum (newpage);
            changed = true;
            }
         break;
         }
      }

   if (changed)
      {
      QString pagename;

      f->pixmap (true);
      //FIXME: better to emit our own signal which tells Desktopdelegate to just update the pixmap
      _minor_change = true;
      emit dataChanged (index, index);
      _minor_change = false;
      }
   return true;
   }


Qt::ItemFlags Desktopmodel::flags(const QModelIndex &index) const
   {
   if (index.isValid())
      return (Qt::ItemIsEnabled | Qt::ItemIsSelectable
         | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);

   return Qt::ItemIsDropEnabled;
   }


int Desktopmodel::rowCount(const QModelIndex &parent) const
   {
   int count;

   if (!parent.isValid())
      count = _desks.size ();
   else if (IS_DESK (parent))
      count = getDesk (parent)->rowCount (); //fileCount ();
   else
      count = 0;
//    qDebug () << "rowCount" << parent << count;
   return count;
   }


int Desktopmodel::columnCount(const QModelIndex &parent) const
   {
   if (IS_DESK (parent))
      return 1;
   return 0;
   }


Qt::DropActions Desktopmodel::supportedDropActions() const
   {
   return Qt::CopyAction | Qt::MoveAction;
   }


QModelIndex Desktopmodel::index (int row, int column, const QModelIndex &parent) const
   {
   QModelIndex ind;

//    debug () << "index row =" << row;
//    if (row == -1)
//       {
//       debug () << "bad row =" << row;
//       }
   if (parent == QModelIndex ())
      {
      if (column == 0 && row >= 0 && row < _desks.size ())
   //       printf ("%d, pixmap = %p\n", row, di->pixmap);
         ind = DESK_INDEX (row);
      }
   else if (IS_DESK (parent))
      {
      // the parent is a desk node
      Desk *desk = _desks [parent.internalId () - 10];

      if (column == 0 && row >= 0 && row < desk->fileCount ())
         ind = FILE_INDEX (row, desk->getFile (row));
      }
//    qDebug () << "index" << parent << "row" << row << "ind" << ind;
   return ind;
   }


QModelIndex Desktopmodel::index (const QString &fname, QModelIndex parent) const
   {
   if (parent == QModelIndex ())
      {
      for (int row = 0; row < _desks.size (); row++)
         {
         Desk *desk = _desks [row];

         if (desk->dir () == fname)
            return DESK_INDEX (row);
         }
      }
   else  // must be a desk
      {
      Desk *desk = getDesk (parent);
      int row;
      File *f = desk->findFile (fname, row);

      if (f)
         return FILE_INDEX (row, f);
      }
   return QModelIndex ();
   }


QModelIndex Desktopmodel::parent(const QModelIndex &item) const
   {
   if (IS_FILE (item))
      {
      // this is a file node, so the parent will be a desk
      File *file = (File *)item.internalPointer ();
      Desk *desk = file->desk ();
      int row = _desks.indexOf (desk);
      Q_ASSERT (row != -1);
//       qDebug () << "parent" << item;
      return DESK_INDEX (row);
      }

   // otherwise parent is the root index
   return QModelIndex ();
   }


bool Desktopmodel::removeDesk (const QString &pathname)
   {
   QModelIndex ind = index (pathname + "/", QModelIndex ());

   if (ind == QModelIndex ())
      {
      qDebug () << "Cannot find desk for" << pathname;
      return false;
      }

   // TODO: check that when re-adding it reuses the same Desk
   if (!IS_DESK (ind))
      {
      qDebug() << "Cannot removeDesk() on a non-Desk";
      return false;
      }

   Desk *desk = getDesk (ind);

   Q_ASSERT (desk);

   int item = ind.row ();

   beginRemoveRows (ind.parent (), ind.row (), ind.row ());
   delete desk;
   _desks.removeAt(item);
   endRemoveRows ();
   return true;
   }

void Desktopmodel::setModelConv (Desktopmodelconv *modelconv)
   {
   _modelconv = modelconv;
   }


QDebug Desktopmodel::debug (void) const
   {
   return qDebug () << "Desktopmodel";
   }


Desktopundostack *Desktopmodel::getUndoStack (void)
   {
   return _undo;
   }


File *Desktopmodel::getFile (const QModelIndex &index) const
   {
   _modelconv->assertIsSource (0, &index, 0);

   if (!index.isValid ())
      return NULL;
   if (!IS_FILE (index))
      {
      qDebug () << "bad getFile" << index << index.internalId ();
      }
   Q_ASSERT (IS_FILE (index));
   File *f = (File *)index.internalPointer ();
//    if (!f->max && di->valid)
//       qDebug () << "max is null, di->valid=" << di->valid;
//    Q_ASSERT (!di->valid || f->max);
   return f;
   }


Desk *Desktopmodel::getDesk (const QModelIndex &index) const
   {
   _modelconv->assertIsSource (0, &index, 0);

   if (!index.isValid ())
      return NULL;
   Q_ASSERT (IS_DESK (index));

//       debug () << "rowCount" << _items.size ();
   Desk *desk = _desks [index.internalId () - 10];
//    if (!f->max && di->valid)
//       qDebug () << "max is null, di->valid=" << di->valid;
//    Q_ASSERT (!di->valid || f->max);
   return desk;
   }


QString &Desktopmodel::deskRootDir (QModelIndex ind)
   {
   return _desks [ind.row ()]->rootDir ();
   }


pagepos_info Desktopmodel::getPosData (const QModelIndex &index) const
   {
   pagepos_info pos;

   pos.pos = QPoint (-1, -1);
   pos.pagenum = -1;
   if (index.isValid ())
      {
      File *f = getFile (index);

      pos.pos = f->pos ();
      pos.pagenum = f->pagenum ();
      pos.pagecount = f->pagecount ();
      }
   return pos;
   }


#if 0// these functions not needed?
void Desktopmodel::savePersistentIndexes (void)
   {
   QModelIndexList list = persistentIndexList ();

   _persistent_filenames = listToFilenames (list);
   qDebug () << "savePersistentIndexes" << _persistent_filenames.size ();
   foreach (QString str, _persistent_filenames)
      qDebug () << "   " << str;
   }


void Desktopmodel::restorePersistentIndexes (void)
   {
   qDebug () << "restorePersistentIndexes" << _persistent_filenames.size ();
   foreach (QString str, _persistent_filenames)
      qDebug () << "   " << str;
   QModelIndexList list = listFromFilenames (_persistent_filenames);
   changePersistentIndexList (persistentIndexList (), list);
   }
#endif


QString Desktopmodel::deskToDirname (QModelIndex parent)
   {
   Desk *desk = getDesk (parent);

   return desk->dir ();
   }


QModelIndex Desktopmodel::deskFromDirname (QString &dir)
   {
   return index (dir, QModelIndex ());
   }


QStringList Desktopmodel::listToFilenames (const QModelIndexList &list)
   {
   QModelIndex ind;
   QStringList slist;

   // add the filename for each index to the list
   foreach (ind, list)
      if (ind.isValid ())
         slist << getFile (ind)->filename ();
   return slist;
   }


QModelIndexList Desktopmodel::listFromFilenames (const QStringList &slist,
      QModelIndex parent)
   {
   QString fname;
   QModelIndex ind;
   QModelIndexList list;

   // work through each filename in turn
   foreach (fname, slist)
      {
      // search through all files looking for a match
      ind = index (fname, parent);

      // did the file disappear since we added it to the undo list?
      if (ind == QModelIndex ())
         printf ("Warning: filename '%s' is not present, but should be\n", qPrintable (fname));

      // add the model index (found or not) to the list
      list << ind;
      }
   return list;
   }


void Desktopmodel::sortForDelete (QModelIndexList &list)
   {
   if (list.size () < 2)
      return;

   // get a list of the rows
   QList<int> ilist;
   QModelIndex ind, parent = list [0].parent ();
   foreach (ind, list)
      ilist << ind.row ();

   // sort the list
   std::sort (ilist.begin (), ilist.end (), std::greater<int> ());

   // and create a sorted index list
   list.clear ();
   int i;
   foreach (i, ilist)
      {
      list << index (i, 0, parent);
      // printf ("   %d\n", i);
      }
   }


typedef struct sort_info
   {
   QModelIndex ind;
   QPoint pos;
   } sort_info;


static bool positionLessThan (const sort_info &s1, const sort_info &s2)
   {
   int diff;

   diff = s2.pos.y () - s1.pos.y ();
   if (diff > 80)
      return true;
   else if (diff < -80)
      return false;
   return s1.pos.x () < s2.pos.x ();
   }


void Desktopmodel::sortByPosition (QModelIndexList &list)
   {
   if (list.size () < 2)
      return;

   // get a list of the positions
   QList<sort_info> ilist;
   QModelIndex ind, parent = list [0].parent ();
   foreach (ind, list)
      {
      sort_info s;

      s.ind = ind;
      s.pos = getFile (ind)->pos ();
      ilist << s;
      }

   // sort the list
   std::stable_sort (ilist.begin (), ilist.end (), positionLessThan);

   // and create a sorted index list
   list.clear ();
   sort_info s;
   foreach (s, ilist)
      list << index (s.ind.row (), 0, parent);
   }


void Desktopmodel::buildItem (QModelIndex index)
   {
//    qDebug () << "buildItem" << index;
   File *f = getFile (index);

   // if we can't load it, still mark it as valid otherwise we will keep loading it
   if (f->load ())
      f->setValid (true);

   /* if we are asking for a page that has not been added yet, do nothing.
      This happens during scanning */
   if (f->pagenum () < f->pagecount ())
      f->pixmap (true);   // regenerate the pixmap

   /* tell the view that the data has changed. The view will request the
      new data. Note that if we don't have information about the item
      size or titlesize yet, this will be calculated by the view (actually
      delegate) and then stored back in the model */
   emit dataChanged (index, index);
   }


void Desktopmodel::aboutToQuit (void)
   {
   // we need to make sure that the maxdesk.ini file is saved
/*FIXME: port this
   if (_desk)
      {
      delete _desk;
      _desk = 0;
      }
*/
   }


QString Desktopmodel::getAnnot (QModelIndex ind, File::e_annot type) const
   {
   File *f = getFile (ind);
   QString text;

   text.clear ();
   if (!f->valid()) {
      f->setErr(err_make(ERRFN, ERR_file_not_loaded_yet1,
                         qPrintable(f->filename())));
      return text;
   }
   if (f)
      f->setErr (f->getAnnot (type, text));
   return text;
   }


/*************************** scanning support functions *****************************/

err_info *Desktopmodel::beginScan (QModelIndex parent, const QString &stack_name)
   {
   Desk *desk;
   QModelIndexList list;
   int row;
   err_info *err = NULL;

   // work out which desk to scan into
   desk = getDesk (parent);
   if (!desk)
      err = err_make (ERRFN, ERR_do_not_have_a_valid_directory_to_scan_into);

   qDebug () << "scan parent" << parent << desk;
   if (!err)
      // create the new stack
      err = desk->addPaperstack (stack_name, _scan_file, row);

   if (err)
   {
       // give up, and make a note to ignore future signals
       _scan_err = true;
       return err;
   }
   
   // stop this maxdesk from being disposed while we are scanning into it
   desk->setAllowDispose (false);

   // add to the model
   qDebug () << "scan_file" << _scan_file;
   newItem (row, parent, list);

   emit beginningScan (list [0]);

   /** set this last, since emitting beginningScan() may call checkScanStack()
       which will get confused if we have already switch to the new stack */
   _scan_desk = desk;
   _scan_parent = parent;
   _scan_err = false;
   return NULL;
   }


err_info *Desktopmodel::cancelScan (void)
   {
   if (_scan_err)
      return NULL;
   Q_ASSERT (_scan_desk && _scan_file);

   QModelIndex ind = index (_scan_file->filename (), _scan_parent);

   // emit a signal for the benefit of pagewidget - it will remove its pages
   emit endingScan (true);

   // allow the scanning maxdesk to be disposed (we have finished scanning into it)
   _scan_desk->setAllowDispose (true);

//   QModelIndex parent = _scan_parent;
   _scan_parent = QModelIndex ();
   _scan_desk = 0;
   _scan_file = 0;

   // remove the stack in the maxdesk if it is there
   return opDeleteStack (ind);
   }


err_info *Desktopmodel::confirmScan (void)
   {
//    qDebug () << "Desktopmodel::confirmScan";
   Q_ASSERT (_scan_desk && _scan_file);

   QModelIndex ind = index (_scan_file->filename (), _scan_parent);

   // flush the item
   CALL (_scan_file->flush ());

// this comment might not be relevant since Desktopmodel was enhanced to have multiple maxdesks:
   /* at this point, although we have just flushed the item, it is possible
      that the model does not have a valid max pointer (getFile(ind)->max).
      This can happen when the user starts a scan, then clicks to another
      folder then clicks back. Since the scan has not been flushed to the
      filesystem yet, its stack might be blank and will have given an error
      when loaded.

      We are about to commit the scan, which will rely on the max pointer
      being correct. The fix for this is in scanCommitPages() where we
      re-read the stack */

   // emit a signal for the benefit of pagewidget
   // this may call scanCommitPages()
   emit endingScan (false);

   _scan_desk = 0;
   _scan_file = 0;

   if (ind != QModelIndex ())
      {
      //desk->rescanFile (getFile (ind));  // shouldn't need to do this now
      buildItem (ind);
      }
   return NULL;
   }


err_info *Desktopmodel::addPageToScan (const Filepage *mp, const QString &coverageStr)
   {
   if (_scan_err)
      return NULL;
   CALL (_scan_file->addPage (mp, false));
//    qDebug () << "Desktopmodel::addPageToScan, page count" << _scan_file->pagecount;
   emit newScannedPage (coverageStr, mp->markBlank ());
   return NULL;
   }


void Desktopmodel::pageStarting (Paperscan &scan, const PPage *page)
   {
   UNUSED (scan);
   UNUSED (page);
   if (_scan_err)
      return;
//    qDebug () << "pageStarting" << scan.getPagenum (page);

   // no scaled image size registered as yet
   _need_scaled_image = false;
   _scaled_image_size = QSize ();
   _scaled_linenum = 0;
   emit beginningPage ();
   }


void Desktopmodel::pageProgress (Paperscan &scan, const PPage *page)
   {
   QImage image;
   const char *data = 0;
   int scaled_linenum;
   int size = 0;
   bool ok;

   if (_scan_err)
      return;

   ok = scan.getData (page, data, size);
//    qDebug () << "pageProgress" << scan.getPagenum (page) << ok << size;
//    emit dataAddedToPage (page, data, size);
   if (ok && getNewScaledImage (scan, page, data, size, image, scaled_linenum))
      {
//       qDebug () << "   newScaledImage";
      emit newScaledImage (image, scaled_linenum);
      }
   }


#if QT_VERSION >= 0x040400
#define USE_24BPP
#endif


bool Desktopmodel::getNewScaledImage (Paperscan &scan, const PPage *page,
      const char *data, int nbytes, QImage &image, int &scaled_linenum)
   {
   int width, height;
   int depth, stride;

   if (_need_scaled_image && scan.getPageDetails (page, width, height, depth, stride) && stride)
      {
      int linenum, lines;

      // how many scan lines worth of data do we have?
      lines = nbytes / stride;

      // what scaled line number are we up to now?
      linenum = lines * _scaled_image_size.height () / height;

      // if no new lines, exit
//       if (linenum == _scaled_linenum)

      /** we must have at least 2 lines to work with to be sure of getting a
          single line result */
      if (linenum - _scaled_linenum < 2)
         return false;

      // start from the previous scaled line, to give us a a of margin
      // otherwise we might get gaps in the final image
      int scaled_from = _scaled_linenum;
      if (scaled_from > 0)
         scaled_from--;

      /* we now need to generate an image from scaled lines _scaled_linenum
         to linenum. First work out the input (unscaled) line numbers */
      int from_linenum = scaled_from * height / _scaled_image_size.height ();
      int to_linenum = linenum * height / _scaled_image_size.height ();

//       qDebug () << "getNewScaledImage bytes " << nbytes << " lines from" << from_linenum << to_linenum;
      Filepage::getImageFromLines (data + stride * from_linenum, width,
         to_linenum - from_linenum, depth, stride, image);
      if (image.format () == QImage::Format_Indexed8)
#ifdef USE_24BPP
         image = image.convertToFormat (QImage::Format_RGB888);
#else
         image = image.convertToFormat (QImage::Format_RGB32);
#endif
         
      image = image.scaled (_scaled_image_size, Qt::KeepAspectRatio);
      if (!image.height ())
         return false;
//       qDebug () << "desktopmodel image" << image.width () << image.height ()<< image.format ();
      scaled_linenum = scaled_from;
      _scaled_linenum = linenum;
      return true;
      }
   return false;
   }


void Desktopmodel::registerScaledImageSize (const QSize &size)
   {
   /* note that we need to generate scaled images (via emit newScaledImage()
      whenever we receive new data for this page */
   _need_scaled_image = true;
   if (_scaled_image_size != size)
      {
      // if the size has changed, start the image again
      _scaled_image_size = size;
      _scaled_linenum = 0;
      }
   }


int Desktopmodel::getScanSize (void)
   {
   return _scan_file ? _scan_file->getSize () : 0;
   }


err_info *Desktopmodel::scanCommitPages (QModelIndex &ind, int del_count,
      QBitArray &pages, bool allow_undo)
   {
   err_info *err = NULL;

   if (_scan_err)
      return NULL;

    // do nothing - nothing to delete
   if (!del_count)
      ;

   // delete whole stack
   else if (del_count == pages.size ())
      {
      /*if (use_desk)
         err = _scan_desk->remove (_scan_file);
      else*/ if (allow_undo)
         trashStack (ind);
      else
         err = opDeleteStack (ind);
      }

   // delete a selection of pages
   else
      {
      QByteArray ba;

      /*if (use_desk)
         err = _scan_desk->removePages (_scan_file, pages, ba, count);
      else*/ if (allow_undo)
         deletePages (ind, pages);
      else
         err = opDeletePages (ind, pages, ba, del_count);
      }
   return err;
   }


/***************************************************************************/

bool Desktopmodel::checkerr (err_info *err)
   {
   if (err)
      QMessageBox::warning (0, "Maxview", err->errstr);
   return err != NULL;
   }


#if 0 //p

void Desktopmodel::stackItem (Desktopitem *dest, Desktopitem *src)
   {
   file_info *fdest, *fsrc;

   fdest = (file_info *)dest->userData ();
   fsrc = (file_info *)src->userData ();

   if (dest != src && !checkerr (_desk->stackItem (fdest, fsrc)))
      {
      // delete the source page
      if (!checkerr (_desk->remove (fsrc)))
         delete src;
      buildItem (dest, fdest);
      }
   }

#endif


void Desktopmodel::newItem (int row, QModelIndex parent, QModelIndexList &list)
   {
   Desk *desk = getDesk (parent);
   QModelIndex ind = FILE_INDEX (row, desk->getFile (row));

   // qDebug () << "newItem index" << ind;
   insertRow (row, parent);
   buildItem (ind);
   list << ind;
//    printf ("   newItem row %d\n", row);
   }


bool Desktopmodel::removeRows (int row, int count, const QModelIndex &parent)
   {
   Desk *desk = getDesk (parent);

   beginRemoveRows (parent, row, row + count - 1);
   internalRemoveRows (row, count, parent);
   desk->updateRowCount ();
   endRemoveRows ();
   return true;
   }


void Desktopmodel::internalRemoveRows (int row, int count, const QModelIndex &parent)
   {
   getDesk (parent)->removeRows (row, count);
   }


bool Desktopmodel::insertRow (int row, const QModelIndex &parent)
   {
   Desk *desk = getDesk (parent);

// //    printf ("Desktopmodel::neinsertRow\n");
   beginInsertRows (parent, row, row);
//    _items.insert (row, di);
   desk->updateRowCount ();
   endInsertRows ();
   return true;
   }


void Desktopmodel::insertRows (QList<File *> &flist, QModelIndex parent)
   {
   Desk *desk = getDesk (parent);
   File *fnew;

   if (!flist.size ())
      return;
   // now insert all the new file_info records into the model
   int row = desk->fileCount () - flist.size ();
   // qDebug () << "beginInsertRows" << row << desk->fileCount () - 1;
   beginInsertRows (parent, row, desk->fileCount () - 1);
   foreach (fnew, flist)
      {
      UNUSED (fnew);
      // qDebug () << "build item" << row;
      buildItem (index (row++, 0, parent));
//       list << createIndex (row, 0, (void *)&_items [row]);
      }
   desk->updateRowCount ();
   endInsertRows ();
   }


void Desktopmodel::removeRows (QModelIndexList &list)
   {
   QModelIndex ind;

   // work through each stack in the list
   foreach (ind, list)
      removeRow (ind.row ());
   }


void Desktopmodel::resetAdded (void)
   {
   // no items have been added
   _about_to_add = false;
   }


void Desktopmodel::aboutToAdd (QModelIndex parent)
   {
   // we are about to add some items
   _about_to_add = true;
   _add_start = rowCount (parent);
   }


bool Desktopmodel::itemsAdded (QModelIndex parent, int &start, int &count)
   {
   if (_about_to_add)
      {
      _about_to_add = false;
      start = _add_start;
      count = rowCount (parent) - start;
      return count > 0;
      }
   return false;
   }


int Desktopmodel::listPagecount (const QModelIndexList &list)
   {
   QModelIndex ind;
   File *f;
   int count = 0;

   foreach (ind, list)
      {
      f = getFile (ind);
      if (f)
         count += f->pagecount ();
      }

   return count;
   }


#if 0
void Desktopmodel::duplicateTiff (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();
   file_info *fnew;
   Operation op ("Convert to .tiff", f->pagecount, this);

   if (!checkerr (_desk->duplicateTiff (f, &fnew, op)))
      buildItem (0, fnew);
   }
#endif


bool Desktopmodel::checkScanStack (QModelIndexList &list, QModelIndex parent)
   {
   Desk *desk = getDesk (parent);

   UNUSED (list);
   /* if the proposed operation is happening on the same desk as we are 
      scanning to, stop it */
   if (_scan_desk == desk)
      {
      QMessageBox::warning (0, "Maxview", tr ("Sorry, you cannot do this while scanning"));
      return false;
      }
/*
   QModelIndex ind;
   if (_desk != _scan_desk)
      return true;
   foreach (ind, list)
      if (getFile (ind) == _scan_file)
         emit commitScanStack ();
*/
   emit commitScanStack ();
   return true;
   }


QString Desktopmodel::getPageName (const QModelIndex &ind, int pnum) const
   {
   File *f = getFile (ind);

   return f->pageTitle (pnum);
   }


#if 0

// void Desktopmodel::moveDir (QString &src, QString &dst)
// {
//    err_complain (_desk->moveDir (src, dst));
// }


Q3DragObject *Desktopmodel::dragObject()
{
    if ( !currentItem() )
        return 0;

    QPoint orig = viewportToContents( viewport()->mapFromGlobal( QCursor::pos() ) );
    DesktopitemDrag *drag = new DesktopitemDrag( viewport() );
//    drag->setPixmap( *currentItem()->pixmap(),
//                     QPoint( currentItem()->pixmapRect().width() / 2, currentItem()->pixmapRect().height() / 2 ) );
    for ( Desktopitem *item = (Desktopitem*)firstItem(); item;
          item = (Desktopitem*)item->nextItem() ) {
        if ( item->isSelected() ) {
            Q3IconDragItem id;
            char str [20];

            sprintf (str, "%p", item);
//            printf ("str=%s\n", str);
//            str.setNum ((int)item);

//    QByteArray ba( str.length() + 1 );
//    memcpy( ba.data(), str.latin1(), ba.size() );
    QByteArray ba( strlen (str) + 1 );
    memcpy( ba.data(), str, ba.size() );

            id.setData( ba );
            drag->append( id,
                          QRect( item->pixmapRect( false ).x() - orig.x(),
                                 item->pixmapRect( false ).y() - orig.y(),
                                 item->pixmapRect().width(), item->pixmapRect().height() ),
                          QRect( item->textRect( false ).x() - orig.x(),
                                 item->textRect( false ).y() - orig.y(),
                                 item->textRect().width(), item->textRect().height() ),
                          item );
        }
    }

    return drag;
}

#endif


void Desktopmodel::progress (const char *fmt, ...)
   {
   char str [256];
   va_list ptr;

   va_start (ptr, fmt);
   vsprintf (str, fmt, ptr);
   va_end (ptr);
   emit newContents (str);
   }


void Desktopmodel::nextUpdate (void)
   {
   if (_pending_scan_list.isEmpty ())
      {
      // nothing to do, so finish
      _updateTimer->stop ();
      emit updateDone ();
      }
   else
      {
      // build this one
      QModelIndex ind = _pending_scan_list.takeFirst ();

      if (ind.isValid ())
         buildItem (ind);
      _updateTimer->start (DELAY_TIME);
      }
   }

void Desktopmodel::stopUpdate (void)
   {
   // cancel an update
   _stopUpdate = true;
   }


QString &Desktopmodel::getDirPath (void)
   {
   return _dirPath;
   }


void Desktopmodel::resetDirPath (void)
   {
   _dirPath.clear ();
   }

void Desktopmodel::flushAllDesks (void)
   {
   foreach (Desk *desk, _desks)
      desk->flush ();
   }


void Desktopmodel::cloneModel (Desktopmodel *contents, QModelIndex parent)
{
   QModelIndex ind;

   UNUSED (parent);
   _cloned = true;
   beginInsertRows (ind, 0, contents->_desks.size () - 1);
   _desks = contents->_desks;
   endInsertRows ();
}

void Desktopmodel::refresh(QModelIndex ind)
{
   Q_ASSERT(ind.isValid());
   Desk *desk = getDesk(ind);
   beginResetModel();
   desk->refresh();
   endResetModel();

   // Insert the new items
   beginInsertRows(ind, 0, desk->fileCount() - 1);
   endInsertRows();
}

QModelIndex Desktopmodel::refresh(QString dirPath)
{
   dirPath += "/";
   QModelIndex ind = index(dirPath, QModelIndex ());

   refresh(ind);

   return ind;
}

QModelIndex Desktopmodel::showDir(QString dirPath, QString rootPath,
                                  Measure *meas)
   {
   QModelIndex ind;

   // move into the new directory
   _dirPath = dirPath;
   _rootPath = rootPath;

   rootPath += "/";
   dirPath += "/";
   //qDebug () << "refresh" << dirPath;
   Desk *desk;

   flushAllDesks ();

   ind = index(dirPath, QModelIndex ());
   if (ind.isValid()) {
      desk = getDesk(ind);
      ind = index(dirPath, QModelIndex());
   } else {
      beginInsertRows (ind, _desks.size (), _desks.size ());
      desk = new Desk(dirPath, rootPath);
      _desks << desk;
      endInsertRows ();
      ind = index (_desks.size () - 1, 0, QModelIndex ());
   }

   desk->setDebugLevel (_debug_level);

   // add files that are not in the maxdesk.ini file
   desk->addFiles(dirPath, meas);

   // no pending list at present
   _pending_scan_list.clear ();

   return ind;
   }

Desk *Desktopmodel::prepareSearchDesk(QString& dirPath, QString& rootPath,
                                      QModelIndex& ind, bool& add_items)
{
   Desk *desk;

   // move into the new directory
   _dirPath = dirPath;
   _rootPath = rootPath;

   rootPath += "/";
   dirPath += "/";

   flushAllDesks ();

   /*
    * We only have one desk for subdirectory searches and we reuse it
      each time
   */
   if (ind.isValid()) {
      desk = getDesk(ind);
      add_items = true;
   } else {
      beginInsertRows (ind, _desks.size (), _desks.size ());
      desk = new Desk("" , rootPath, false);
      _desks << desk;
      endInsertRows ();
      ind = index (_desks.size () - 1, 0, QModelIndex ());
      add_items = false;
   }
   desk->setDebugLevel (_debug_level);

   clearAll (ind);
   desk->advance ();

   return desk;
}

QModelIndex Desktopmodel::finishFileSearch(QString dirPath, QString rootPath,
                                           const QStringList& matches,
                                           Measure *meas)
{
   bool add_items;
   Desk *desk;

   desk = prepareSearchDesk(dirPath, rootPath, _subdirs_index, add_items);

   desk->addMatches(dirPath, matches, meas);
   if (add_items)
      {
      /* we are re-using the same subdir desk for the new search. All the
         items have been cleared, so we need to add the new matches */
      beginInsertRows (_subdirs_index, 0, desk->fileCount () - 1);
      endInsertRows ();
      }
   _subdirs = true;

   // no pending list at present
   _pending_scan_list.clear ();

   return _subdirs_index;
}

QModelIndex Desktopmodel::showFilesIn(const QString& inPath,
                                      const QString &rootPath, Measure *meas)
{
   QString dirPath = inPath + "/", root = rootPath + "/";
   bool add_items = true;
   Desk *desk;

   flushAllDesks ();

   QModelIndex ind = index(dirPath, QModelIndex ());
   if (ind.isValid()) {
      desk = getDesk(ind);
      add_items = false;
   } else {
      beginInsertRows (ind, _desks.size (), _desks.size ());
      desk = new Desk(dirPath, rootPath);
      _desks << desk;
      endInsertRows ();
      ind = index(_desks.size () - 1, 0, QModelIndex ());
   }
   _otherdir_index = ind;

   desk->addFromDir(dirPath, meas);
   if (add_items)
   {
      /* we are re-using the same subdir desk for the new search. All the
         items have been cleared, so we need to add the new matches */
      beginInsertRows(_otherdir_index, 0, desk->fileCount() - 1);
      endInsertRows();
   } else {
      beginResetModel();
      endResetModel();
   }
   _subdirs = true;

   // no pending list at present
   _pending_scan_list.clear ();

   return _otherdir_index;
}

void Desktopmodel::clearAll (QModelIndex &parent)
   {
   Desk *desk = getDesk (parent);

   beginRemoveRows (parent, 0, desk->fileCount () - 1);
   desk->clear ();
   desk->updateRowCount ();
   endRemoveRows ();
   }


void Desktopmodel::arrangeBy (int type, QModelIndexList &list, QModelIndex parent,
      QList<QPoint> &oldplist)
   {
   Desk *desk = getDesk (parent);
   QList<QPoint> newplist;
   QModelIndex ind;

   _modelconv->assertIsSource (0, 0, &list);

   if (!checkScanStack (list, parent))
      return;

   // arrange the items
   desk->arrangeBy (type);

   // get a list of the new positions
   foreach (ind, list)
      newplist << getFile (ind)->pos ();

   // perform the move (support undo)
   move (list, parent, oldplist, newplist);
   }


#if 0 //p

err_info *Desktopmodel::operation (Desk::operation_t type, int ival)
   {
   file_info *f;
   Desktopitem *src, *next;
   err_info *err = NULL;

   for (src = (Desktopitem *)firstItem(); !err && src; src = next)
      {
      next = (Desktopitem *)src->nextItem();
      if (!src->isSelected ())
         continue;
      f = (file_info *)src->userData ();
      CALL (_desk->operation (f, -1, type, ival));
//      printf ("done operation\n");
      buildItem (src, f);
      }

   return NULL;
   }


bool Desktopmodel::userBusy (void)
   {
   return _userbusy;
   }

#endif //0


void Desktopmodel::resizeAll (QModelIndex parent)
   {
   for (int i = 0; i < rowCount (parent); i++)
      {
      QModelIndex ind = index (i, 0, parent);
      File *f = getFile (ind);

      f->setPreviewMaxsize (QSize ());
      f->setTitleMaxsize (QSize ());
      f->setPagenameMaxsize (QSize ());

      /* tell the view that the data has changed. The view will request the
         new data. Note that if we don't have information about the item
         size or titlesize yet, this will be calculated by the view (actually
         delegate) and then stored back in the model */
      emit dataChanged (ind, ind);
      }
   }


void Desktopmodel::setDropTarget (const QModelIndex &index)
   {
   _modelconv->assertIsSource (0, &index, 0);
   if (_drop_target)
      {
      QPersistentModelIndex *old = _drop_target;

      _drop_target = 0;
      _minor_change = true;
      emit dataChanged (*old, *old);
      _minor_change = false;
      delete _drop_target;
      }
   if (index != QModelIndex ())
      _drop_target = new QPersistentModelIndex (index);
   }


const QPersistentModelIndex *Desktopmodel::dropTarget (void)
   {
   return _drop_target;
   }


// void Desktopmodel::slotNewContextEvent (QModelIndex &index)
//    {
//    _context_index = index;
//    }


QStringList Desktopmodel::mimeTypes() const
   {
   QStringList types;
   types << "text/uri-list";
   types << "application/vnd.text.list";
   return types;
   }


QMimeData *Desktopmodel::mimeData (const QModelIndexList &list) const
   {
   QMimeData *mimeData = new QMimeData();
   QByteArray encodedData;

   _modelconv->assertIsSource (0, 0, &list);
   QDataStream stream(&encodedData, QIODevice::WriteOnly);

   foreach (QModelIndex ind, list) {
      if (ind.isValid()) {
            QString text = data(ind, Desktopmodel::Role_filename).toString();
            stream << text;
      }
   }

   mimeData->setData("application/vnd.text.list", encodedData);
   return mimeData;
   }



// err_info *Desktopmodel::getImage (const QModelIndex &ind, int pnum, bool do_scale,
//                QImage **imagep, QSize &Size, QSize &trueSize, int &bpp)
//    {
//    _modelconv->assertIsSource (0, &ind, 0);
//    File *f = getFile (ind);
//
//    return f->getImageQImage (pnum, do_scale, imagep, Size, trueSize, bpp);
//    }


err_info *Desktopmodel::getImage (const QModelIndex &ind, int pnum, bool do_scale,
               QImage &imagep, QSize &Size, QSize &trueSize, int &bpp, bool blank) const
   {
   _modelconv->assertIsSource (0, &ind, 0);
   File *f = getFile (ind);

   if (!f->valid())
      return err_make(ERRFN, ERR_file_not_loaded_yet1,
                      qPrintable(f->filename()));

   return f->getImage (pnum, do_scale, imagep, Size, trueSize, bpp, blank);
   }


int Desktopmodel::countPages (QModelIndexList &list, bool sepSheets)
   {
   QModelIndex ind;
   int count = 0;

   _modelconv->assertIsSource (0, 0, &list);
   foreach (ind, list)
      {
      File *f = getFile (ind);
      count += f->pagecount ();

      // add a blank page if this stack has an odd number of pages
      if (sepSheets & (f->pagecount () & 1))
         count++;
      }
   return count;
   }


QString Desktopmodel::imageInfo (const QModelIndex &ind, int pagenum, bool extra) const
   {
   _modelconv->assertIsSource (0, &ind, 0);
   QSize size, trueSize;
   int bpp, csize, num_bytes;
   File *f = getFile (ind);
   QDateTime dt;
   err_info *e;

   e = f->getImageInfo (pagenum, size, trueSize, bpp, num_bytes, csize, dt);
   if (e)
      return QString (tr ("Error: %1")).arg (e->errstr);

   char stacksize [30], pagesize [30];

   util_bytes_to_user (stacksize, num_bytes);
   util_bytes_to_user (pagesize, csize);

   QString datestr = dt.toString (tr ("ddd dd-MMM-yy hh:mm:ss"));

   QString str = QString (tr ("%1MP, %2x%3, %4 %5bpp, %6 of %7, %8"))
      .arg (size.width () * size.height () / 1000000.0, 0, 'f', 1)
      .arg (size.width ()).arg (size.height ())
      .arg (bpp == 1 ? tr ("Mono") : bpp == 8 ? tr ("Grey") : tr ("Colour"))
      .arg (bpp).arg (QString (pagesize).trimmed ()).arg (QString (stacksize).trimmed ())
      .arg (datestr);

   if (extra)
      str = QString ("%1, Page %2 of %3   ")
          .arg (f->filename ()).arg (pagenum + 1).arg (f->pagecount ()) + str;
   return str;
   }


int Desktopmodel::imageSize (const QModelIndex &ind) const
   {
   File *f = getFile (ind);

   return f->size ();
   }


QString Desktopmodel::imageTimestamp (const QModelIndex &ind, int pagenum)
   {
   _modelconv->assertIsSource (0, &ind, 0);
   QSize size, trueSize;
   int bpp, csize, num_bytes;
   File *f = getFile (ind);
   err_info *e;
   QDateTime dt;

   e = f->getImageInfo (pagenum, size, trueSize, bpp, num_bytes, csize, dt);
   if (e)
      return QString (tr ("Error: %1")).arg (e->errstr);
   return dt.toString ("ddd dd-MM-yy hh:mm:ss");
   }


err_info *Desktopmodel::getImagePreviewSizes (const QModelIndex &ind, int pagenum,
      QSize &preview_size, QSize &image_size) const
   {
   _modelconv->assertIsSource (0, &ind, 0);
   File *f = getFile (ind);
   QSize dsize;
   int bpp, csize, num_bytes;
   QDateTime dt;

   if (!f->valid())
      return err_make(ERRFN, ERR_file_not_loaded_yet1,
                      qPrintable(f->filename()));
   CALL (f->getPreviewInfo (pagenum, preview_size, bpp));
   return f->getImageInfo (pagenum, dsize, image_size, bpp, num_bytes, csize, dt);
   }


void Desktopmodel::getScaledImage (const QModelIndex &ind, int pagenum,
      QSize &size, QPixmap &pixmap, bool blank) const
   {
   QSize preview_size, image_size;
   err_info *err;

   // work out the preview and image sizes
   err = getImagePreviewSizes (ind, pagenum, preview_size, image_size);

   // if the preview is big enough, use it
   if (size.width () <= preview_size.width () + 20
      && size.height () <= preview_size.height () + 20)
      {
      QPixmap pm;

      // we will use the preview
      getImagePreview (ind, pagenum, pm, blank);

      // do we need to scale?
      if (pm.width () != size.width ()
         && pm.height () != size.height ())
         pixmap = QPixmap (pm.scaled (size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      else
         pixmap = pm;
      }
   else
      {
      QImage image;
      QSize isize, tsize;
      int bpp;

      err = getImage (ind, pagenum, false, image, isize, tsize, bpp, blank);
      //qDebug () << "Desktopmodel::getScaledImage" << image.width () << image.height ()
      //        << image.bits () << image.size ();
      mem_check ();
      if (err)
         ;
      // do we need to scale?
      else
         {
         if (image.width () != size.width ()
            && image.height () != size.height ())
            image = util_smooth_scale_image (image, size);
         pixmap = QPixmap::fromImage (image);
         }
      }
   if (err)
      pixmap = _no_access;
   }


err_info *Desktopmodel::getImagePreview (const QModelIndex &ind, int pagenum,
         QPixmap &pixmap, bool blank) const
   {
   File *f = getFile (ind);
   err_info *err;

   err = f->getPreviewPixmap (pagenum == -1 ? f->pagenum () : pagenum,
      pixmap, blank);
   if (err)
      pixmap = err->errnum == ERR_cannot_open_file1 ? _no_access : _unknown;
   return err;
   }


err_info *Desktopmodel::getPageText (const QModelIndex &ind, int pagenum, QString &str)
   {
   File *f = getFile (ind);
   err_info *err;

   if (!f->valid())
      return err_make(ERRFN, ERR_file_not_loaded_yet1,
                      qPrintable(f->filename()));
   err = f->getPageText (pagenum, str);
   if (err)
      str = err->errstr;
   return err;
   }


#if 0
void Desktopmodel::updatePixmap (ditem_info &di, QString &pagename)
   {
   QPixmap *pixmap;
   err_info *err;
   int bpp;

   if (di.pixmap && di.pixmap != no_access && di.pixmap != unknown)
      {
      delete di.pixmap;
      di.pixmap = 0;
      }
   err = _desk->getPreviewPixmap (di.file, di.file->pagenum, pagename, &pixmap, bpp);
   QPixmap *pm = err == 0 ? pixmap : err->errnum == ERR_cannot_open_file1 ? no_access : unknown;
   di.pixmap = pm;
   }
#endif


void Desktopmodel::addFilesToDesk (QString dir, QStringList &fnamelist)
   {
   QModelIndex parent = index (dir, QModelIndex ());

   // qDebug () << "addFilesToDesk" << dir << parent;

   // if we haven't got a desk for this, don't worry
   if (!parent.isValid ())
      return;

   // add the files
   Desk *desk = getDesk (parent);

   QList <File *> flist;

   // qDebug () << "   - adding to desk" << desk->dir ();
   desk->advance ();
   foreach (QString fname, fnamelist)
      {
      File *fnew = desk->createFile (dir, fname);
      desk->newFile (fnew);
      flist << fnew;
      }
   insertRows (flist, parent);
   }


void Desktopmodel::removeFilesFromDesk (QString dir, QStringList &fnamelist)
   {
   QModelIndex parent = index (dir, QModelIndex ());

   // if we haven't got a desk for this, don't worry
   if (!parent.isValid ())
      return;

   QModelIndexList del_list = listFromFilenames (fnamelist, parent);
   sortForDelete (del_list);
   foreach (QModelIndex ind, del_list)
      {
      Q_ASSERT (ind.isValid ());
      removeRows (ind.row (), 1, parent);
//       delete desk->takeAt (ind.row ());
      }
   }


Desktopproxy::Desktopproxy (QObject *object)
      : QSortFilterProxyModel (object)
   {
   _subset = false;
   setFilterCaseSensitivity (Qt::CaseInsensitive);
   }


Desktopproxy::~Desktopproxy ()
   {
   }


bool Desktopproxy::filterAcceptsRow (int source_row, const QModelIndex& source_parent) const
   {
   if (!source_parent.isValid ())   // root node doesn't filter
      return true;
   if (_subset)
   {
      // use our own subsetting
      return _rows [source_row];
   }
   return QSortFilterProxyModel::filterAcceptsRow (source_row, source_parent);
   }


void Desktopproxy::setRows (int parent_row, int child_count, QList<int> &rows)
   {
   _subset = true;
   _parent_row = parent_row;
   _rows.fill (false, child_count);

   // make a list of rows to include
   foreach (int row, rows)
      _rows [row] = true;
   }


Desktopmodelconv::Desktopmodelconv (Desktopmodel *source, Desktopproxy *proxy, bool canConvert)
   {
   _source = source;
   _proxy = proxy;
   _can_convert = canConvert;
   }


Desktopmodelconv::Desktopmodelconv (Desktopmodel *source, bool canConvert)
   {
   _source = source;
   _proxy = (Desktopproxy *)source;
   _can_convert = canConvert;
   }


Desktopmodelconv::~Desktopmodelconv ()
   {
   }


void Desktopmodelconv::debugList (QString name, QModelIndexList &list)
   {
   QModelIndex ind;

   qDebug () << name;
   foreach (ind, list)
      qDebug () << "   row" << ind.row () << ":" << ind.internalPointer ();
   }


Desktopmodel *Desktopmodelconv::getDesktopmodel (const QAbstractItemModel *model)
   {
   Q_ASSERT (model == _source || model == _proxy);
   Q_ASSERT (_can_convert || model == _source);
   return _source;
   }

#define HAVE_PROXY ((QAbstractItemModel *)_proxy != (QAbstractItemModel *)_source)

void Desktopmodelconv::listToSource (const QAbstractItemModel *model, QModelIndexList &list)
   {
   Q_ASSERT (model == _source || model == _proxy);
   Q_ASSERT (_can_convert || model == _source);
//    debugList ("listToSource in", list);
   if (model == _proxy && HAVE_PROXY)
      {
      QModelIndexList old = list;
      QModelIndex ind;

      list.clear ();
      foreach (ind, old)
         list << _proxy->mapToSource (ind);
      }
//    debugList ("listToSource out", list);
   }


void Desktopmodelconv::indexToSource (const QAbstractItemModel *model, QModelIndex &index)
   {
   Q_ASSERT (!model || model == _source || model == _proxy);
   Q_ASSERT (_can_convert || model == _source);
   if (model == _proxy && HAVE_PROXY)
      index = _proxy->mapToSource (index);
   }


void Desktopmodelconv::listToProxy (const QAbstractItemModel *model, QModelIndexList &list)
   {
   Q_ASSERT (_can_convert || model == _source);
   Q_ASSERT (model == _source || model == _proxy);
//    debugList ("listToProxy in", list);
   if (model == _source && HAVE_PROXY)
      {
      QModelIndexList old = list;
      QModelIndex ind;

      list.clear ();
      foreach (ind, old)
         list << _proxy->mapFromSource (ind);
      }
//    debugList ("listToProxy out", list);
   }


void Desktopmodelconv::indexToProxy (const QAbstractItemModel *model, QModelIndex &index)
   {
   Q_ASSERT (!model || model == _source || model == _proxy);
   Q_ASSERT (_can_convert || HAVE_PROXY);
   if (model == _source && HAVE_PROXY)
      index = _proxy->mapFromSource (index);
   }


void Desktopmodelconv::assertIsSource (const QAbstractItemModel *model,
      const QModelIndex *index, const QModelIndexList *list)
   {
   Q_ASSERT (!model || model == _source);
   Q_ASSERT (!index || !index->isValid () || index->model () == _source);
#ifndef QT_NO_DEBUG
   if (list)
      {
      QModelIndex ind;

      foreach (ind, *list)
         Q_ASSERT (!ind.isValid () || ind.model () == _source);
      }
#endif
   }
