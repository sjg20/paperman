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


void PageRenderer::render (int itemnum, const QPersistentModelIndex &stack,
                          int pagenum, const QSize &size, bool blank,
                          quint64 gen)
   {
   QMutexLocker locker (&_mutex);

   _itemnum = itemnum;
   _stack = stack;
   _pagenum = pagenum;
   _size = size;
   _blank = blank;
   _gen = gen;
   _have = true;
   if (!isRunning ())
      start ();
   _cond.wakeAll ();
   }


void PageRenderer::flush (void)
   {
   QMutexLocker locker (&_mutex);

   /* drop a not-yet-started request and wait for any in-progress one */
   _have = false;
   while (_busy)
      _cond.wait (&_mutex);
   }


void PageRenderer::shutdown (void)
   {
      {
      QMutexLocker locker (&_mutex);
      _stop = true;
      _cond.wakeAll ();
      }
   wait ();
   }


void PageRenderer::run (void)
   {
   for (;;)
      {
      int itemnum, pagenum;
      QPersistentModelIndex stack;
      QSize size;
      bool blank;
      quint64 gen;

         {
         QMutexLocker locker (&_mutex);

         while (!_have && !_stop)
            _cond.wait (&_mutex);
         if (_stop)
            return;
         itemnum = _itemnum;
         stack = _stack;
         pagenum = _pagenum;
         size = _size;
         blank = _blank;
         gen = _gen;
         _have = false;
         _busy = true;
         }

      QImage image;

      if (_contents && stack.isValid ())
         _contents->getScaledImageData (stack, pagenum, size, blank, image);

      {
      QMutexLocker locker (&_mutex);

      _busy = false;
      _cond.wakeAll ();        // let flush() proceed
      }

      emit rendered (itemnum, image, gen);
      }
   }


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

   _generation = 0;
   _renderer = new PageRenderer (this);
   connect (_renderer, SIGNAL (rendered (int, QImage, quint64)),
            this, SLOT (slotRendered (int, QImage, quint64)));
   }


Pagemodel::~Pagemodel ()
   {
   delete _renderer;   // stops and joins the render thread
   }


void Pagemodel::stopRendering (void)
   {
   _renderer->shutdown ();
   }


void Pagemodel::clear (void)
   {
    QAbstractItemModel::beginResetModel ();
   disownScanning ();
   _contents = 0;
   _stackindex = QModelIndex ();
//    qDebug () << "Pagemodel::clear";
   _start = _count = _row_count = 0;
   _column_count = 1;
   _pages.resize (_count);
   _annot_updates.clear ();
   QAbstractItemModel::endResetModel ();
   }


void Pagemodel::reset (const Desktopmodel *model, const QModelIndex &index,
      int start, int count)
   {
   _renderer->flush ();
   _generation++;
   _renderer->setContents (model);
    QAbstractItemModel::beginResetModel ();
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
   QAbstractItemModel::endResetModel ();
//    qDebug () << "Pagemodel::reset";
   }


void Pagemodel::updatePage (int pagenum)
   {
   int row = pagenum - _start;

   if (row < 0 || row >= _row_count)
      return;

   /* regenerate the thumbnail straight away so the changed page (e.g.
      after a rotation) is shown in its new form at once.  Going through
      invalidate() would null the pixmap and defer the rescale to the
      background timer, so the page would blank briefly first */
   _pages [row]._rescale = true;
   if (!_pages [row].updatePixmap ())
      _pages [row].invalidate ();   // not set up yet: rebuild lazily

   QModelIndex ind = index (row, 0, QModelIndex ());
   emit dataChanged (ind, ind);
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
   if (_pagesize != size)
      {
      _renderer->flush ();
      _generation++;
      }
   _pagesize = size;
   }


void Pagemodel::setScale (int scale_down)
   {
   if (_scale_down != scale_down)
      {
      _scale_down = scale_down;
      _renderer->flush ();
      _generation++;

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

   // if this is the image being scanned, use the page's own scan image
   // (each Pageinfo keeps its own working surface so a progressive duplex
   // scan can paint front and back in parallel without collision).
   if (pi.scanning ())
      pixmap = QPixmap::fromImage (pi._scan_image);
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
      int row = _update_upto;
      pi = &_pages [row];
      if (++_update_upto == _count)
         _update_upto = 0;
      if (pi->wantsRescale ())
         {
         int pagenum = _start + row;

         /* a big page is slow to decode, so hand it to the render thread
            and carry on when it comes back; the scan image and small
            previews are cheap, so do those here */
         if (_stackindex.isValid () && !pi->scanning () && !_own_scan
             && _contents
             && _contents->imageNeedsDecode (_stackindex, pagenum, _pagesize))
            {
            pi->markRendering ();
            _renderer->render (row, _stackindex, pagenum, _pagesize,
                               pi->isBlank (), _generation);
            return;
            }
         if (pi->updatePixmap ())
            {
            QModelIndex ind = index (row, 0, QModelIndex ());
            emit dataChanged (ind, ind);
            _updateTimer->start (0);
            break;
            }
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


void Pagemodel::slotRendered (int itemnum, QImage image, quint64 gen)
   {
   /* drop results from before the last reset/scale change, and empty
      images from a failed decode */
   if (gen == _generation && itemnum >= 0 && itemnum < _count
       && !image.isNull ())
      {
      _pages [itemnum].setPixmap (QPixmap::fromImage (image));
      QModelIndex ind = index (itemnum, 0, QModelIndex ());
      emit dataChanged (ind, ind);
      }
   if (_rescaling)
      _updateTimer->start (0);   // on to the next page
   }


/***************************** scanning *****************************/

void Pagemodel::beginningScan (void)
   {
   // we 'own' the scanning now, so will display previews as pages are scanning
   _renderer->flush ();     // no render thread reads while the scan writes
   _own_scan = true;
   _lost_scan = false;
   _scan_pages.clear ();
   }


void Pagemodel::endingScan (void)
   {
   bool pending = false;

//    qDebug () << "endingScan";
   _own_scan = false;
   _lost_scan = false;

   /* the pages just scanned are showing their live previews: now that
      the render thread may read the file again, have it generate the
      proper thumbnails in the background */
   for (int i = 0; i < _count; i++)
      pending |= _pages [i].finishProvisional ();
   if (pending)
      scheduleRescale ();
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



int Pagemodel::slotNewScannedPage (const QString &coverageStr,
      bool mark_blank)
   {
   Pageinfo *page;
   int pagenum = -1;

   // if we own the scan, proceed normally
   if (_own_scan)
      {
      /* During a progressive duplex scan, BOTH pages are in the
       * scanning state at the time the front-side confirmImage runs.
       * Confirm pages in start order: pick the lowest-index page that
       * is still flagged scanning. Falls back to (_count - 1) for the
       * legacy single-page case. */
      pagenum = _count - 1;
      for (int i = 0; i < _pages.size (); i++)
         if (_pages [i].scanning ())
            {
            pagenum = i;
            break;
            }
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

   return _own_scan ? pagenum : -1;
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
   page._scan_image = QImage ();
   page._scan_painted = 0;
   endInsertRows ();
//    emit dataChanged (index (pagenum, 0, QModelIndex ()),
//       index (pagenum, 0, QModelIndex ()));
   }


void Pagemodel::newScaledImage (const QImage &image, int scaled_linenum,
                                int pagenum)
   {
   if (!_stackindex.isValid ())
      return;

   /* pagenum is the index of the active scanning page in our _pages
    * list, supplied by Desktopmodel::pageProgress (PPage::pagenum()).
    * If it's out of range — e.g. a stale event arriving after the
    * stack has been replaced — fall back to the legacy behaviour of
    * routing to the page count, which is correct for the sequential
    * single-page case. */
   if (pagenum < 0 || pagenum >= (int)_pages.size ())
      pagenum = _contents->data (_stackindex,
                                 Desktopmodel::Role_pagecount).toInt ();
   if (pagenum < 0 || pagenum >= (int)_pages.size ())
      return;
   Pageinfo &page = _pages [pagenum];
   bool ok;

//    qDebug () << "Pagemodel::newScaledImage page " << pagenum
//       << "width" << image.width () << "height" << image.height ()
//       << "scaled_linenum" << scaled_linenum;

   // update our scanned image. Always use RGB32 so we can paint any
   // incoming image format on top regardless of bit depth.
   QPainter p;
   QImage &surface = page._scan_image;

   if (surface.isNull ())
      {
      surface = QImage (_pagesize.width (), _pagesize.height (),
                        QImage::Format_RGB32);
      surface.fill (Qt::white);
      ok = p.begin (&surface);
      if (ok)
         p.fillRect (QRect (QPoint (0, 0), surface.size ()),
                     QBrush (Qt::DiagCrossPattern));
      }
   else
      ok = p.begin (&surface);

   QImage src = image;
   if (src.format () != QImage::Format_RGB32
       && src.format () != QImage::Format_ARGB32)
      src = src.convertToFormat (QImage::Format_RGB32);

   if (ok)
      {
      p.drawImage (QPoint (0, scaled_linenum), src);
      p.end ();

      page._scan_painted = qMax (page._scan_painted,
                                 scaled_linenum + src.height ());
      page.updateScanImage (surface);

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
   _pixmap = QPixmap();
   _blank = false;
   _remove = false;
   _scanning = false;
   _pagenum = -1;
   _itemnum = -1;
   _model = 0;
   _size = QSize ();
   _rescale = false;
   _provisional = false;
   _scan_painted = 0;
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
   _provisional = false;
   _coverage = "";
   _blank = _remove = _scanning = false;
   }


void Pageinfo::scanDone (QString coverage, bool mark_blank)
   {
   setCoverage (coverage);

   /* The preview built up while the page was scanning is a good enough
      thumbnail for now. Regenerating it here would decode the whole page
      on the GUI thread, once per page and one page per event-loop turn,
      since the render thread must stay off the file while the scan
      writes it: that is what lets the display fall behind a fast feeder.
      Keep the preview and regenerate it once the scan is over. A page
      that never got a preview has to be generated now */
   if (_pixmap.isNull ())
      _rescale = true;
   else
      {
      /* the preview surface is the full page box; a shorter page (a
         landscape sheet) fills only the top of it. Cut it to what was
         painted, so the cell has the page's own shape, as the thumbnail
         made from the file later will */
      if (_scan_painted > 0 && _scan_painted < _pixmap.height ())
         _pixmap = _pixmap.copy (0, 0, _pixmap.width (), _scan_painted);
      _provisional = true;
      /* the page was painted before its first preview arrived, with no
         pixmap, which asked for a rescale: the preview answers that now,
         so drop the request, or the page is decoded in full once the
         update timer fires */
      _rescale = false;
      }
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


bool Pageinfo::finishProvisional (void)
   {
   if (!_provisional)
      return false;
   _provisional = false;
   _rescale = true;
   return true;
   }


bool Pageinfo::updatePixmap (void)
   {
   // any need for rescale?
   if (!_rescale || !_model)
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
   /* the preview surface is the page box, so the pixmap is for the
      current page size: without saying so, every paint would find it the
      wrong size and ask for a rescale from the file, which decodes the
      page on the GUI thread while the scan is still writing the stack and
      replaces the preview with a full-height box */
   _size = image.size ();
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

