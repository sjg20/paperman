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


#include <QDebug>
#include <QScrollBar>
#include <QWheelEvent>

#include "desktopmodel.h"
#include "pagedelegate.h"
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

   _autoscroll = true;  // the user has not scrolled yet
   _ignore_scroll = false;

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
      the first item's real size hint: this fixes the cell height (no gap
      under a landscape page) and keeps layout O(1). */
   if (model () && model ()->rowCount () > 0)
      {
      QSize hint = itemDelegate ()->sizeHint (getViewOptions (),
                                              model ()->index (0, 0));
      /* with a grid the view's spacing no longer applies, so include it
         in the cell, or the pages sit edge to edge */
      if (hint.isValid ())
         setGridSize (hint + QSize (spacing (), spacing ()));
      }
   scheduleDelayedItemsLayout ();
   }


Pageview::~Pageview ()
   {
   }


void Pageview::currentChanged (const QModelIndex &index, const QModelIndex &previous)
   {
   UNUSED (previous);
   emit showInfo (index);
   }


void Pageview::allowMove (bool allow)
   {
   setSelectionMode (ExtendedSelection);
   setDropIndicatorShown (false);
   setDragDropMode (allow ? InternalMove : NoDragDrop);
   setDragEnabled (allow);
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
   if (!ifAtEnd || _autoscroll)
      {
      _ignore_scroll = true;
      scrollTo (model ()->index (row, 0, QModelIndex ()), PositionAtBottom);
      _ignore_scroll = false;
      }
   }


void Pageview::scrollContentsBy (int dx, int dy)
   {
   /* if we caused the scroll, then don't worry. Otherwise the user is trying
      to adjust the scrollbars, so if they are not at the bottom, we turn off
      autoscroll */
   if (!_ignore_scroll)
      {
      QScrollBar *vs = verticalScrollBar ();

      _autoscroll = vs->value () == vs->maximum ();
//       qDebug () << "autoscroll" << _autoscroll;
      }
   QListView::scrollContentsBy (dx, dy);
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
