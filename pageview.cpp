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


#include <algorithm>

#include <QDebug>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QScrollBar>
#include <QTimer>
#include <QKeyEvent>
#include <QPainter>
#include <QWheelEvent>

#include "desktopmodel.h"
#include "pagedelegate.h"
#include "pagemodel.h"
#include "pageview.h"



Pageview::Pageview (QWidget *parent)
      : QListView (parent)
   {
   setViewMode (IconMode);
   setSpacing (4);   // a little gap so page previews do not run together

   setWrapping (true);
   setFlow (LeftToRight);

   // the user cannot have free movement, but this makes the destination cursor look nice
   setMovement (Free);
   setResizeMode (Adjust);
   setLayoutMode (SinglePass);
   /* uniform item sizes keep icon-view layout O(1) per change; without
      this, adding hundreds of pages during a scan is O(n^2) and the UI
      falls a long way behind. The cells still get the right shape because
      the delegate sizes them to the page and a relayout re-measures once
      the first pixmap is ready (see slotRelayout). */
   setUniformItemSizes (true);
   setSelectionRectVisible (true);
   setHorizontalScrollMode (QAbstractItemView::ScrollPerPixel);
   setVerticalScrollMode (QAbstractItemView::ScrollPerPixel);

   setEditTriggers (QAbstractItemView::EditKeyPressed);
   setStyleSheet ("QListView { background: palette(mid); }");

   _follow = false;
   _autoscroll = true;  // the user has not scrolled yet
   _ignore_scroll = false;
   /* actionTriggered() comes only from the user (scrollbar, wheel), never
      from setValue(), so it tells user scrolls from ones a relayout makes */
   connect (verticalScrollBar (), SIGNAL (actionTriggered (int)),
            this, SLOT (slotUserScrolled ()));
   connect (verticalScrollBar (), SIGNAL (rangeChanged (int, int)),
            this, SLOT (slotRangeChanged (int, int)));

   _relayoutTimer.setSingleShot (true);
   connect (&_relayoutTimer, SIGNAL (timeout ()), this, SLOT (slotRelayout ()));
   }


void Pageview::setModel (QAbstractItemModel *model)
   {
   QListView::setModel (model);
   if (model)
      /* a page's pixmap is not ready when it is first laid out, so its
         cell starts at the fallback height; re-measure once pixmaps
         arrive. The timer coalesces the burst of updates into one pass */
      connect (model, &QAbstractItemModel::dataChanged, this,
               [this] (const QModelIndex &, const QModelIndex &,
                       const QVector<int> &) { _relayoutTimer.start (150); });
   }


void Pageview::slotRelayout ()
   {
   /* uniform item sizes cache the size measured from the first item, and
      that first measurement happens before its pixmap is ready, so it is
      the tall fallback. Once pixmaps have arrived, set the grid cell from
      the real size hints, as tall as the tallest page: this fixes the
      cell height (no gap under a stack of landscape pages) and keeps
      layout O(1). */
   if (model () && model ()->rowCount () > 0)
      {
      QStyleOptionViewItem option = getViewOptions ();
      QSize hint = itemDelegate ()->sizeHint (option, model ()->index (0, 0));
      Pagedelegate *delegate = qobject_cast<Pagedelegate *> (itemDelegate ());

      /* Every cell is the same size, so it must hold the tallest page, or
         a portrait page in a stack which starts with a wide one, such as
         a book with its jacket, runs into the row below. Only pages whose
         pixmaps have arrived are measured, since asking for the others
         would make them all; each that arrives brings another relayout */
      if (delegate)
         {
         int tallest = 0;

         for (int row = 0; row < model ()->rowCount (); row++)
            {
            Pageinfo *pi = model ()->data (model ()->index (row, 0),
                  Pagemodel::Role_pageinfo).value<Pageinfo *> ();
            QSize size = pi ? pi->knownPixmapSize () : QSize ();

            if (size.isValid ())
               tallest = qMax (tallest, size.height ());
            }
         if (tallest)
            hint = hint.expandedTo (delegate->cellSize (option, tallest));
         }
      /* with a grid the view's spacing no longer applies, so include it
         in the cell, or the pages sit edge to edge */
      if (hint.isValid ())
         setGridSize (hint + QSize (spacing (), spacing ()));
      }
   scheduleDelayedItemsLayout ();
   }


void Pageview::slotRangeChanged (int min, int max)
   {
   Q_UNUSED (min);
   /* the end has moved (rows added, cells re-measured, the view resized):
      while following it, stay on it. scrollToRow() alone is not enough,
      as the layout it scrolled to can still change afterwards */
   if (followingEnd ())
      {
      _ignore_scroll = true;
      verticalScrollBar ()->setValue (max);
      _ignore_scroll = false;
      }
   }


Pageview::~Pageview ()
   {
   }


void Pageview::currentChanged (const QModelIndex &index, const QModelIndex &previous)
   {
   UNUSED (previous);
   emit showInfo (index);
   }


/* where an event which carries a drag is, as Qt 6 and Qt 5 each say it */
static QPoint dropPos (const QDropEvent *event)
   {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
   return event->position ().toPoint ();
#else
   return event->pos ();
#endif
   }


bool Pageview::pagesInColumn (void) const
   {
   QAbstractItemModel *pages = model ();

   return !pages || pages->rowCount () < 2
       || visualRect (pages->index (0, 0)).left ()
          == visualRect (pages->index (1, 0)).left ();
   }


QLine Pageview::dropLine (const QPoint &pos) const
   {
   int rows = model () ? model ()->rowCount () : 0;

   if (!rows)
      return QLine ();

   int row = dropRow (pos);
   int gap = spacing () / 2;
   QRect before = row > 0 ? visualRect (model ()->index (row - 1, 0)) : QRect ();
   QRect after = row < rows ? visualRect (model ()->index (row, 0)) : QRect ();

   if (pagesInColumn ())
      {
      QRect page = after.isValid () ? after : before;
      int y = !after.isValid () ? before.bottom () + gap
            : !before.isValid () ? after.top () - gap
            : (before.bottom () + after.top ()) / 2;

      return QLine (page.left (), y, page.right (), y);
      }

   /* the pages run on to the next row, so between the last of one row and
      the first of the next, go beside whichever the pointer is level with */
   QRect page = before;
   int x;

   if (before.isValid () && after.isValid () && before.top () == after.top ())
      x = (before.right () + after.left ()) / 2;
   else if (after.isValid ()
            && (!before.isValid () || pos.y () > before.bottom ()))
      {
      page = after;
      x = after.left () - gap;
      }
   else
      x = before.right () + gap;
   return QLine (x, page.top (), x, page.bottom ());
   }


void Pageview::setDropLine (const QLine &line)
   {
   auto area = [] (const QLine &l) {
      return QRect (l.p1 (), l.p2 ()).normalized ().adjusted (-3, -3, 3, 3);
   };

   if (line == _dropLine)
      return;
   if (!_dropLine.isNull ())
      viewport ()->update (area (_dropLine));
   _dropLine = line;
   if (!_dropLine.isNull ())
      viewport ()->update (area (_dropLine));
   }


void Pageview::paintEvent (QPaintEvent *event)
   {
   QListView::paintEvent (event);
   if (_dropLine.isNull ())
      return;

   QPainter painter (viewport ());

   painter.setPen (QPen (palette ().color (QPalette::Highlight), 3,
                         Qt::SolidLine, Qt::RoundCap));
   painter.drawLine (_dropLine);
   }


void Pageview::dragLeaveEvent (QDragLeaveEvent *event)
   {
   setDropLine (QLine ());
   QAbstractItemView::dragLeaveEvent (event);
   }


int Pageview::dropRow (const QPoint &pos) const
   {
   int rows = model () ? model ()->rowCount () : 0;
   QModelIndex index = indexAt (pos);

   /* between pages, or past the last: before the first page which comes
      after the drop, in reading order */
   if (!index.isValid ())
      {
      for (int row = 0; row < rows; row++)
         {
         QRect rect = visualRect (model ()->index (row, 0));

         if (rect.top () > pos.y ()
             || (rect.bottom () >= pos.y () && rect.left () > pos.x ()))
            return row;
         }
      return rows;
      }

   /* on a page: before it if on its nearer half, which is its left half
      where the pages are side by side and its top half where they are in
      a column, as in a narrow page list */
   QRect rect = visualRect (index);
   bool after = pagesInColumn () ? pos.y () > rect.center ().y ()
                                 : pos.x () > rect.center ().x ();

   return index.row () + (after ? 1 : 0);
   }


void Pageview::dragEnterEvent (QDragEnterEvent *event)
   {
   if (event->source () == this && _moveAllowed)
      event->acceptProposedAction ();
   else
      event->ignore ();
   }


void Pageview::dragMoveEvent (QDragMoveEvent *event)
   {
   if (event->source () != this || !_moveAllowed)
      {
      event->ignore ();
      return;
      }

   // this scrolls the view near its edges
   QAbstractItemView::dragMoveEvent (event);
   event->acceptProposedAction ();
   setDropLine (dropLine (dropPos (event)));
   }


QPixmap Pageview::dragPixmap (const QModelIndex &index, int count) const
   {
   static const int page_size = 64;   // the longer side of the page
   static const int badge_size = 20;  // the height of the count

   Pageinfo *info = index.data (Pagemodel::Role_pageinfo).value<Pageinfo *> ();
   bool dodgy = false;
   QPixmap page = info ? info->pixmap (dodgy) : QPixmap ();
   QSize size = page.isNull () ? QSize (page_size * 3 / 4, page_size)
      : page.size ().scaled (page_size, page_size, Qt::KeepAspectRatio);

   // room for the count, which overlaps a corner of the page
   qreal ratio = devicePixelRatioF ();
   int margin = badge_size / 2;
   QSize whole = size + QSize (margin, margin);
   QPixmap out (whole * ratio);

   out.setDevicePixelRatio (ratio);
   out.fill (Qt::transparent);

   QPainter painter (&out);
   QRect rect (QPoint (0, 0), size);

   painter.setRenderHint (QPainter::Antialiasing);
   painter.setRenderHint (QPainter::SmoothPixmapTransform);
   painter.fillRect (rect, Qt::white);
   if (!page.isNull ())
      painter.drawPixmap (rect, page);
   painter.setPen (palette ().color (QPalette::Dark));
   painter.setBrush (Qt::NoBrush);
   painter.drawRect (rect.adjusted (0, 0, -1, -1));

   if (count > 1)
      {
      QString text = QString::number (count);
      int width = qMax (badge_size,
                        fontMetrics ().horizontalAdvance (text) + 10);
      QRect badge (whole.width () - width, whole.height () - badge_size,
                   width, badge_size);

      painter.setPen (Qt::NoPen);
      painter.setBrush (palette ().color (QPalette::Highlight));
      painter.drawRoundedRect (badge, badge_size / 2, badge_size / 2);
      painter.setPen (palette ().color (QPalette::HighlightedText));
      painter.drawText (badge, Qt::AlignCenter, text);
      }
   return out;
   }


void Pageview::startDrag (Qt::DropActions)
   {
   const QModelIndexList selected = selectionModel ()->selectedIndexes ();

   if (selected.isEmpty ())
      return;

   // show the page taken hold of
   QModelIndex shown = selected.contains (currentIndex ()) ? currentIndex ()
                                                           : selected.first ();
   QPixmap image = dragPixmap (shown, selected.size ());
   QDrag *drag = new QDrag (this);

   drag->setMimeData (model ()->mimeData (selected));
   drag->setPixmap (image);
   drag->setHotSpot (QPoint (image.width (), image.height ())
                     / image.devicePixelRatio () / 2);

   // dropEvent() moves the pages, so there is nothing to do when it returns
   drag->exec (Qt::MoveAction);
   }


void Pageview::dropEvent (QDropEvent *event)
   {
   if (event->source () != this || !_moveAllowed)
      {
      event->ignore ();
      return;
      }

   QList<int> rows;

   for (const QModelIndex &index : selectionModel ()->selectedIndexes ())
      rows << index.row ();
   std::sort (rows.begin (), rows.end ());
   QPoint pos = dropPos (event);

   setDropLine (QLine ());

   // the view itself moves nothing: the pages come back in their new places
   event->setDropAction (Qt::IgnoreAction);
   event->accept ();
   if (!rows.isEmpty ())
      emit pagesMoved (rows, dropRow (pos));
   }


void Pageview::allowMove (bool allow)
   {
   setSelectionMode (ExtendedSelection);
   setDropIndicatorShown (false);
   setDragDropMode (allow ? InternalMove : NoDragDrop);
   setDragEnabled (allow);
   _moveAllowed = allow;
   }


void Pageview::mousePressEvent (QMouseEvent *event)
   {
   if (_moveAllowed)
      {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      QPoint pos = event->position ().toPoint ();
#else
      QPoint pos = event->pos ();
#endif
      QModelIndex index = indexAt (pos);
      QStyleOptionViewItem option = getViewOptions ();
      const Pagedelegate *del = (Pagedelegate *)itemDelegate ();

      option.rect = visualRect (index);
      setDragEnabled (index.isValid () && del->onImage (index, option, pos));
      }
   QListView::mousePressEvent (event);
   }


void Pageview::wheelEvent (QWheelEvent *e)
   {
   int new_scale = _scale_down;

   int step = 1, jump = new_scale;
   while (jump >= 12)
      {
      step *= 2;
      jump /= 2;
      }

   if (e->modifiers () & Qt::ControlModifier)
      {
//printf ("delta %d\n", e->delta ());
      if (e->angleDelta ().y() < 0)
         new_scale += step;
      else if (e->angleDelta ().y() > 0 && new_scale > step)
         new_scale -= step;
      else
         e->ignore ();

      if (new_scale > 224)
         new_scale = 224;
      if (new_scale != _scale_down)
         emit signalNewScale (new_scale);
      }
   else
      QListView::wheelEvent (e);
   }


void Pageview::setScale (int scale_down)
   {
   _scale_down = scale_down;
   }


void Pageview::selectPage (QModelIndex ind)
   {
   QItemSelectionModel *sel = selectionModel ();

   sel->select (QItemSelection (ind, ind),
      QItemSelectionModel::Clear | QItemSelectionModel::Select);
   scrollTo (ind);
   }


void Pageview::scrollToEnd (bool ifAtEnd)
   {
//    QScrollBar *vs = verticalScrollBar ();

//    qDebug () << "scrollToEnd" << _autoscroll;
   if (!ifAtEnd || _autoscroll)
      {
      _ignore_scroll = true;
      // doesn't seem to work, perhaps because the window extent hasn't been updated yet
//       vs->setValue (vs->maximum ());

      // so use this instead
      scrollTo (model ()->index (model ()->rowCount (QModelIndex ()) - 1, 0, QModelIndex ()));
      _ignore_scroll = false;
      }
   }


void Pageview::scrollToRow (int row, bool ifAtEnd)
   {
   if (row < 0)
      return;
   if (!ifAtEnd || followingEnd ())
      {
      /* a relayout may be pending from the rows just added: do it first,
         or the scroll is worked out from the old geometry and stops short
         of the end */
      executeDelayedItemsLayout ();
      _ignore_scroll = true;
      scrollTo (model ()->index (row, 0, QModelIndex ()), PositionAtBottom);
      _ignore_scroll = false;
      }
   }


void Pageview::scrollContentsBy (int dx, int dy)
   {
   /* Whether to keep following the end is decided by the user's scrolls
      alone (slotUserScrolled, keyPressEvent). It used to be decided here,
      by any scroll not of our own making: but a relayout as pages are
      added moves the contents too, and the moment that happened with the
      bar not quite at the end, following stopped for good and the view
      sat on the first screenful while the scan went on below it */
   QListView::scrollContentsBy (dx, dy);
   }


void Pageview::slotUserScrolled ()
   {
   /* the slider moves after the signal, so look once that is done */
   QTimer::singleShot (0, this, [this] () {
      QScrollBar *vs = verticalScrollBar ();

      _autoscroll = vs->value () == vs->maximum ();
      });
   }


void Pageview::keyPressEvent (QKeyEvent *event)
   {
   bool earlier = event->key () == Qt::Key_Left || event->key () == Qt::Key_Up;
   bool later = event->key () == Qt::Key_Right || event->key () == Qt::Key_Down;

   if (_moveAllowed && event->modifiers () == Qt::ControlModifier
       && (earlier || later))
      {
      QList<int> rows;

      for (const QModelIndex &index : selectionModel ()->selectedIndexes ())
         rows << index.row ();
      std::sort (rows.begin (), rows.end ());

      // before the page before the first, or after the page after the last
      int dest = rows.isEmpty () ? 0 : earlier ? rows.first () - 1
                                               : rows.last () + 2;

      if (!rows.isEmpty () && dest >= 0 && dest <= model ()->rowCount ())
         emit pagesMoved (rows, dest);
      event->accept ();
      return;
      }
   QListView::keyPressEvent (event);

   /* moving around with the keys scrolls the view without the scrollbar
      knowing it was the user: treat it the same way */
   QScrollBar *vs = verticalScrollBar ();

   _autoscroll = vs->value () == vs->maximum ();
   }


void Pageview::slotPagePartChanged (const QModelIndex &index,
      const QImage &image, int scaled_linenum)
   {
   const Pagedelegate *del = (Pagedelegate *)itemDelegate ();
   QStyleOptionViewItem option = getViewOptions ();

   int hvalue = horizontalScrollBar()->value();
   int vvalue = verticalScrollBar()->value();

   QRect rect = rectForIndex (index);
   QRect part_rect;
   option.rect = rect;
   del->getPagePart (option, index, scaled_linenum, image.height (), part_rect);

   part_rect.translate (-hvalue, -vvalue);

   QWidget *vp = viewport ();
//    qDebug () << "repaint" << part_rect;
//    part_rect.setHeight (part_rect.height () - 1);   // leave blank line (for testing!)
//    vp->repaint (part_rect);
   vp->update (part_rect);
   }


void Pageview::updateGeometries (void)
   {
   QScrollBar *vs = verticalScrollBar ();
   QScrollBar *hs = horizontalScrollBar ();

   QListView::updateGeometries ();
   vs->setSingleStep (30);
   hs->setSingleStep (30);
   }
