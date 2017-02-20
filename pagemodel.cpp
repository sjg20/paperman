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


#include <QBitArray>
#include <QDebug>
#include <QIcon>
#include <QMimeData>
#include <QPainter>
#include <QTimer>

#include "desktopmodel.h"
#include "pagemodel.h"

Q_DECLARE_METATYPE(QPixmap *)


Pagemodel::Pagemodel (QObject *parent)
      : QAbstractItemModel (parent)
   {
   _contents = 0;
   _start = 0;
   _count = 0;
   _column_count = 0;
   _scale_down = 24;
   _row_count = _count;
   _lost_contents = 0;
   _own_scan = _lost_scan = false;
   _rescaling = false;

   // create the pages
   _updateTimer = new QTimer (this);
   _updateTimer->setSingleShot(true);
   connect (_updateTimer, SIGNAL (timeout()), this, SLOT (nextUpdate ()));
   }


Pagemodel::~Pagemodel ()
   {
   }


void Pagemodel::clear (void)
   {
   disownScanning ();
   _contents = 0;
   _stackindex = QModelIndex ();
//    qDebug () << "Pagemodel::clear";
   _start = _count = _row_count = 0;
   _column_count = 1;
   _pages.resize (_count);
   _annot_updates.clear ();
   QAbstractItemModel::reset ();
   }


void Pagemodel::reset (const Desktopmodel *model, const QModelIndex &index,
      int start, int count)
   {
   _contents = model;
   _stackindex = index;
//    qDebug () << "Pagemodel::reset: _stackindex" << _stackindex << "count" << count;
   _start = start;
   _count = count;
   _column_count = 1;
   _scale_down = 24;
   _rescaling = false;
   Q_ASSERT (_contents);
   Q_ASSERT (index != QModelIndex ());
   Q_ASSERT (_column_count > 0);
   Q_ASSERT (_start >= 0);

//    _row_count = (_count + _column_count - 1) / _column_count;
   _row_count = count;
   Q_ASSERT (_row_count >= 0);

   // create the pages
   _pages.resize (_count);
   for (int i = 0; i < _count; i++)
      _pages [i].invalidate ();
   _annot_updates.clear ();
   QAbstractItemModel::reset ();
//    qDebug () << "Pagemodel::reset";
   }


void Pagemodel::keepAllPages (void)
   {
   for (int i = 0; i < _count; i++)
      if (_pages [i].toRemove ())
         {
         QModelIndex ind = index (i, 0, QModelIndex ());

         _pages [i].setRemove (false);
         emit dataChanged (ind, ind);
         }
   }


QVariant Pagemodel::data(const QModelIndex &index, int role) const
   {
   if (!index.isValid())
      return QVariant();

   if (_stackindex == QModelIndex ())
      {
//       qDebug ("Pagemodel::getPixmap: stack has gone");
      return QVariant ();
      }

   const Pageinfo *pi = &_pages [index.row ()];
   switch (role)
      {
      case Qt::DecorationRole :
         {
         QPixmap pm (_pagesize);

         pm.fill (Qt::red);
         return QIcon (pm);
         }

      case Qt::DisplayRole :
      case Qt::EditRole :
         if (_stackindex.isValid ())
            return _contents->getPageName (_stackindex, _start + index.row ());
         break;

      case Role_pageinfo :
         {
         Pageinfo *pi = ((Pagemodel *)this)->ensurePage (index.row ());

         return QVariant::fromValue (pi);
         }
#if 0
      // get the preview image
      case Role_pixmap :
         {
         QString pagename;
         QPixmap *pm = 0;

         _contents->getImagePreview (_stackindex, _start + index.row (),
            &pm, pagename);
         return QVariant::fromValue (pm);
         }
#endif
      case Role_pagenum :
         return _start + index.row () + 1;

      case Role_coverage :
         return pi->getCoverage ();

      case Role_blank :
         return pi->isBlank ();

      case Role_remove :
         return pi->toRemove ();

      case Role_scanning :
         return pi->scanning ();
      }

   return QVariant();
   }


bool Pagemodel::setData(const QModelIndex &index, const QVariant &value, int role)
   {
   if (!index.isValid())
      return false;
   Pageinfo *pi = &_pages [index.row ()];
   switch (role)
      {
      case Role_remove :
         pi->setRemove (value.toBool ());
         emit dataChanged (index, index);
         break;
      }
   return true;
   }


Qt::ItemFlags Pagemodel::flags(const QModelIndex &index) const
   {
   if (index.isValid())
      return (Qt::ItemIsEnabled | Qt::ItemIsSelectable
         | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);

   return Qt::ItemIsDropEnabled;
   }


int Pagemodel::rowCount(const QModelIndex &parent) const
   {
   // only the root note has any children/rows
   if (parent.isValid())
      return 0;
   else
      return _row_count;
   }


int Pagemodel::columnCount(const QModelIndex &parent) const
   {
   if (parent.isValid())
      return 0;
   else
      return _column_count;
   }


Qt::DropActions Pagemodel::supportedDropActions() const
   {
   return Qt::CopyAction | Qt::MoveAction;
   }


QModelIndex Pagemodel::index (int row, int column, const QModelIndex &parent) const
   {
   int item;

   if (column >= 0 && column < _column_count && row >= 0 && parent == QModelIndex ())
      {
      item = row;
//       item = row * _column_count + column;
      if (item < _count)
         return createIndex (row, column, (void *)&_pages [row]);
      }
   return QModelIndex ();
   }


QModelIndex Pagemodel::parent(const QModelIndex &item) const
   {
   UNUSED (item);
   // this is a single level model
   return QModelIndex ();
   }


QStringList Pagemodel::mimeTypes() const
   {
   QStringList types;
   types << "text/uri-list";
   types << "application/vnd.text.list";
   return types;
   }


QMimeData *Pagemodel::mimeData (const QModelIndexList &list) const
   {
   QMimeData *mimeData = new QMimeData();
   QByteArray encodedData;

   QDataStream stream(&encodedData, QIODevice::WriteOnly);

   foreach (QModelIndex ind, list) {
      if (ind.isValid()) {
            QString text = data(ind, Qt::DisplayRole).toString();
            stream << text;
      }
   }

   mimeData->setData("application/vnd.text.list", encodedData);
   return mimeData;
   }


/************************* other functions *******************/

void Pagemodel::setPagesize (QSize size)
   {
   _pagesize = size;
   }


void Pagemodel::setScale (int scale_down)
   {
   if (_scale_down != scale_down)
      {
      _scale_down = scale_down;

      // stop scaling all pages, since no point in any previous rescaling continuing
      for (int i = 0; i < _count; i++)
         _pages [i].haltRescale ();
      _updateTimer->stop ();
      _rescaling = false;
      }
   }


Pageinfo *Pagemodel::ensurePage (int item)
   {
   Pageinfo &pi = _pages [item];

   pi.setup (this, item);
   return &pi;
   }


void Pagemodel::getPixmap (const QModelIndex &ind, QSize &size, QPixmap &pixmap, bool blank) const
   {
   const Pageinfo &pi = _pages [ind.row ()];

   // if this is the image being scanned, use the scan image supplied by Desktopmodel
   if (pi.scanning ())
      pixmap = QPixmap::fromImage(_scan_image);
   else if (_stackindex.isValid ())
      _contents->getScaledImage (_stackindex, _start + ind.row (), size, pixmap, blank);
   size = _pagesize;
   }


void Pagemodel::scheduleRescale (void)
   {
   if (!_rescaling)
      {
      _rescaling = true;
      _update_upto = 0;
      }

   // hold off rescaling for a while in case the user wants to do some more rescaling
   _updateTimer->start (500);
   }


void Pagemodel::ensureRescale (void)
   {
   if (!_rescaling)
      scheduleRescale ();
   }


void Pagemodel::nextUpdate (void)
   {
   int start = _update_upto;
   Pageinfo *pi;

   // keep going until we find a page that needs updating, or get back to where we started
   for (; _update_upto < _count;)
      {
      pi = &_pages [_update_upto];
      if (++_update_upto == _count)
         _update_upto = 0;
      if (pi->updatePixmap ())
         {
         QModelIndex ind = index (pi->itemnum (), 0, QModelIndex ());
         emit dataChanged (ind, ind);
         _updateTimer->start (0);
         break;
         }
      if (_update_upto == start)
         {
         // nothing to do, so stop updating
         _updateTimer->stop ();
         _rescaling = false;
         break;
         }
      }
   }


/***************************** scanning *****************************/

void Pagemodel::beginningScan (void)
   {
   // we 'own' the scanning now, so will display previews as pages are scanning
   _own_scan = true;
   _lost_scan = false;
   _scan_pages.clear ();
   }


void Pagemodel::endingScan (void)
   {
//    qDebug () << "endingScan";
   _own_scan = false;
   _lost_scan = false;
   }


void Pagemodel::disownScanning (void)
   {
   if (_own_scan)
      {
//       qDebug () << "disownScanning, transferred" << _scan_pages.size ();

      // we no longer 'own' the scanning
      _own_scan = false;
      _lost_scan = true;
      _lost_contents = _contents;
      }
   }



void Pagemodel::slotNewScannedPage (const QString &coverageStr,
      bool mark_blank)
   {
   Pageinfo *page;
   int pagenum = -1;

   // if we own the scan, proceed normally
   if (_own_scan)
      {
      /* we have just finished scanning a page - so mark it as done */
      pagenum = _count - 1;
      page = &_pages [pagenum];
      }
   else
      page = new Pageinfo;

   // should invalidate pixmap so it is created fresh
   page->scanDone (coverageStr, mark_blank);

   _scan_pages << *page;
//       qDebug () << "Pagemodel::slotNewScannedPage, OWNED count now" << _scan_pages.size ();

   if (_own_scan)
      emit dataChanged (index (pagenum, 0, QModelIndex ()),
         index (pagenum, 0, QModelIndex ()));
   }


void Pagemodel::beginningPage (void)
   {
   Desktopmodel *contents =  (Desktopmodel *)_contents;
   int pagenum = _count++;
   QString str;

   Q_ASSERT (_own_scan);

//    qDebug () << "Pagemodel::beginningPage" << pagenum;

   /** tell Desktopmodel what image size we want from the scan that is
       about to start. It will send us an updated image every time it gets
       new data from the scanner */
   contents->registerScaledImageSize (_pagesize);

   _row_count = _count;

   beginInsertRows (QModelIndex (), pagenum, pagenum);
   _pages.resize (_count);

   Pageinfo &page = _pages [pagenum];
   page.setScanning (true, pagenum);
   _scan_image = QImage ();
   endInsertRows ();
//    emit dataChanged (index (pagenum, 0, QModelIndex ()),
//       index (pagenum, 0, QModelIndex ()));
   }


void Pagemodel::newScaledImage (const QImage &image, int scaled_linenum)
   {
   if (!_stackindex.isValid ())
      return;

   int pagenum = _contents->data (_stackindex, Desktopmodel::Role_pagecount).toInt ();
   Pageinfo &page = _pages [pagenum];
   bool ok;

//    qDebug () << "Pagemodel::newScaledImage page " << pagenum
//       << "width" << image.width () << "height" << image.height ()
//       << "scaled_linenum" << scaled_linenum;

   // update our scanned image
   QPainter p;

   if (_scan_image.isNull ())
      {
//       qDebug () << "pagesize" << _pagesize << image.format ();
      _scan_image = QImage (_pagesize.width (), _pagesize.height (), image.format ());
      _scan_image.fill (-1);
      ok = p.begin (&_scan_image);
//       qDebug () << "   starting, ok = " << ok << "image" << _scan_image.width () << _scan_image.height ()<< _scan_image.format ();
      if (ok)
         p.fillRect (QRect (QPoint (0, 0), _scan_image.size ()), QBrush (Qt::DiagCrossPattern));
      }
   else
      ok = p.begin (&_scan_image);

   if (ok)
      {
      p.drawImage (QPoint (0, scaled_linenum), image);
      p.end ();

      page.updateScanImage (_scan_image);

      // tell the view that part of an item has changed
      QModelIndex ind = index (pagenum, 0, QModelIndex ());

      emit pagePartChanged (ind, image, scaled_linenum);
      }
//    emit dataChanged (index (pagenum, 0, QModelIndex ()),
//       index (pagenum, 0, QModelIndex ()));
   }


err_info *Pagemodel::commit (void)
   {
   Desktopmodel *contents = (Desktopmodel *)_contents;
   QModelIndex index = _stackindex;   // may be invalid
   int del_count = 0;
   QVector<Pageinfo> *pages = &_pages;
   bool lost = _lost_scan;

   if (lost)
      {
      pages = &_scan_pages;
      _lost_scan = false;
      contents = (Desktopmodel *)_lost_contents;
      _lost_contents = 0;
      }
   if (!contents)
      return NULL;

   // delete any pages marked for deletion
   QBitArray ba (pages->size ());

   for (int i = 0; i < pages->size (); i++)
      if ((*pages) [i].toRemove ())
         {
         ba.setBit (i);
         del_count++;
         }

   // process on our local model (reverse order)
   if (!lost) for (int i = pages->size () - 1; i >= 0; i--)
      if (ba.testBit (i))
         removeRow (i, QModelIndex ());

   // update any annotation data
   if (!lost && index.isValid ())
      contents->updateAnnot (index, _annot_updates);

   //FIXME: need to write ocr text somewhere also, but needs to be page-based
   //FIXME: should only update annotations if they have changed
   //FIXME: what happens if they delete all pages? Should delete the stack

   /* now delete the pages we don't want, or trash the whole stack if
      we want none of them. This function also handles the case where
      contents points to another directory */
   // this may invalidate _stackIndex
   // also this function copes with an invalid index
   return contents->scanCommitPages (index, del_count, ba, !lost);
   }


/****************************************************************************/


void Pagemodel::updateAnnot (int type, QString str)
   {
   _annot_updates [type] = str;
   }


bool Pagemodel::removeRows (int row, int count, const QModelIndex &parent)
   {
   beginRemoveRows (parent, row, row + count - 1);
   for (int i = 0; i < count; i++)
      _pages.remove (row);
   _count -= count;
   _row_count -= count;
   endRemoveRows ();
   return true;
   }


/************************ Page info *******************/

Pageinfo::Pageinfo (void)
   {
   _valid = false;
   _pixmap = 0;
   _blank = false;
   _remove = false;
   _scanning = false;
   _pagenum = -1;
   _itemnum = -1;
   _model = 0;
   _size = QSize ();
   _rescale = false;
   }


Pageinfo::~Pageinfo ()
   {
   }


void Pageinfo::setup (const Pagemodel *model, int itemnum)
   {
   if (_valid)
      return;

   // set up the basic information
   _model = model;
   _itemnum = itemnum;
   QModelIndex ind = model->index (_itemnum, 0, QModelIndex ());
   _size = _model->pagesize ();
//    qDebug () << "setup";
   _pixmap = QPixmap ();
//    _model->getPixmap (ind, _size, _pixmap, _blank);
   if (_scanning)
      {
      _pagename.clear ();
//       _pagenum = model->data (ind, Pagemodel::Role_pagenum).toInt ();
      }
   else
      {
      _pagename = model->data (ind, Qt::DisplayRole).toString ();
      _pagenum = model->data (ind, Pagemodel::Role_pagenum).toInt ();
      }
   // we are now ready for action
   _valid = true;
   }


void Pageinfo::haltRescale ()
   {
   // don't bother with a previous rescaling, as we are about to get another!
   _rescale = false;
   }


void Pageinfo::invalidate ()
   {
   _valid = false;
   _coverage = "";
   _blank = _remove = _scanning = false;
   }


void Pageinfo::scanDone (QString coverage, bool mark_blank)
   {
   setCoverage (coverage);
   _rescale = true;  // force a regenerate of the pixmap
//    _valid = false;
   _coverage = coverage;
   _remove = _blank = mark_blank;
   _scanning = false;
   }


QPixmap Pageinfo::pixmap (bool &dodgy)
   {
   // if we have no pixmap yet, ask for one
   if (_pixmap.isNull ())
      {
      _rescale = true;
      Pagemodel *mod = (Pagemodel *)_model;
      mod->ensureRescale ();
      dodgy = true;
      return _pixmap;
      }
   else if (_model->pagesize () != _size)
      {
      // remember to rescale later
      _rescale = true;
      Pagemodel *mod = (Pagemodel *)_model;
      mod->ensureRescale ();

      // for now just rescale the pixmap we have
      const QPixmap &pm = QPixmap (_pixmap.scaled (_model->pagesize (), Qt::KeepAspectRatio, Qt::SmoothTransformation));
      dodgy = true;
      return pm;
      }
   dodgy = false;
   return _pixmap;
   }


bool Pageinfo::updatePixmap (void)
   {
   // any need for rescale?
   if (!_rescale)
      return false;

//    qDebug () << "updatePixmap";
   // do the rescale
   _size = _model->pagesize ();
//    qDebug () << "updatePixmap" << _itemnum << _size;
   QModelIndex ind = _model->index (_itemnum, 0, QModelIndex ());
   _model->getPixmap (ind, _size, _pixmap, _blank);

   // we rescaled
   _rescale = false;
   return true;
   }


void Pageinfo::updateScanImage (const QImage &image)
   {
   _pixmap = QPixmap::fromImage (image);
   }


void Pageinfo::setRemove (bool remove)
   {
   _remove = remove;
   }


void Pageinfo::setScanning (bool scanning, int pagenum)
   {
   if (pagenum != -1)
      _pagenum = pagenum;
   _scanning = scanning;
   }


void Pageinfo::markBlank (void)
   {
   _remove = _blank = true;
//    qDebug () << "mark blank" << _pagenum;
   }

