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



#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QItemSelectionModel>
#include <QScrollBar>

#include "desktopdelegate.h"
#include "desktopmodel.h"
#include "desktopview.h"
#include "maxview.h"


Desktopview::Desktopview (QWidget *parent)
      : QListView (parent)
   {
//    printf ("Desktopview = %p\n", this);
   _auto_scrolling = false;
   _timer_id = 1;
   _position_items = true;
   setViewMode (IconMode);
//    setViewMode (ListMode);
   setSelectionMode (ExtendedSelection);
   setDropIndicatorShown (false);
   setSelectionRectVisible (true);
   setHorizontalScrollMode (QAbstractItemView::ScrollPerPixel);
   setVerticalScrollMode (QAbstractItemView::ScrollPerPixel);

   setEditTriggers (QAbstractItemView::EditKeyPressed);

   // we will do our own auto-scroll as QT's is too slow
   setAutoScroll (false);
//    setUniformItemSizes (false);
//    connect (this, SIGNAL (pressed (const QModelIndex &)),
//       this, SLOT (slotPressed (const QModelIndex &)));

   _context_index = QModelIndex ();

   connect (this, SIGNAL (indexesMoved (const QModelIndexList &)),
      this, SLOT (slotIndexesMoved (const QModelIndexList &)));
   setStyleSheet ("QListView { background : lightgray }");
   }


Desktopview::~Desktopview ()
   {
   }


void Desktopview::setFreeForm (void)
   {
   _position_items = false;
   }


void Desktopview::setModelConv (Desktopmodelconv *modelconv)
   {
   _modelconv = modelconv;
   }


QModelIndex Desktopview::rootIndexSource (void)
   {
   QModelIndex ind = rootIndex ();

   _modelconv->indexToSource (ind.model (), ind);
   return ind;
   }


void Desktopview::currentChanged (const QModelIndex &index, const QModelIndex &previous)
   {
   if (index == QModelIndex ())
      emit pageLost (); // no longer displaying a page
   else
      {
      QString str = index.model ()->data (index, Desktopmodel::Role_message).toString ();
      emit newContents (str);
      }
   }


void Desktopview::slotIndexesMoved (const QModelIndexList &indexes)
   {
//    printf ("slotIndexesMoved %d\n", indexes.size ());

   // here we update the position of the items
   QModelIndex parent = indexes [0].parent ();
   QAbstractItemModel *model = (QAbstractItemModel *)indexes [0].model ();

   // we should iterate and set the posiion of each item
   for (int i = 0; i < indexes.size (); i++)
      {
      QModelIndex index = indexes [i];

//       printf ("   - item %d\n", i);
      QRect rect = rectForIndex (index);
      QPoint p = rect.topLeft ();
      model->setData (index, p, Desktopmodel::Role_position);
      }
//    setPositions ();
   }


QSize Desktopview::setPositions ()
   {
   QModelIndex ind, parent;
   QSize maxsize;

//    reset ();
   parent = rootIndex ();
   if (_position_items && model () && parent != QModelIndex ())
      {
//       printf ("rows %d\n", model ()->rowCount (parent));
      for (int i = 0; i < model ()->rowCount (parent); i++)
         {
         ind = model ()->index (i, 0, parent);
         QPoint p = model ()->data (ind, Desktopmodel::Role_position).toPoint ();
         QSize size = model ()->data (ind, Desktopmodel::Role_maxsize).toSize ();
         setPositionForIndex (p, ind);
         size += QSize (p.x (), p.y ());
         maxsize = maxsize.expandedTo (size);
//          qDebug () << "   pos" << p << "size" << size << "maxsize" << maxsize;
         }
//    updateGeometries ();
      }
   return maxsize;
   }


void Desktopview::dataChanged (const QModelIndex &topLeft, const QModelIndex &bottomRight)
   {
//    printf ("dataChanged\n");

//    setPositions ();

//    return;

   // here we update the position of the item
   QModelIndex parent = topLeft.parent ();

   if (_position_items) for (int row = topLeft.row (); row <= bottomRight.row (); row++)
      {
      QModelIndex index = model ()->index (row, 0, parent);
      QPoint p = model ()->data (index, Desktopmodel::Role_position).toPoint ();
//       printf ("   - row %d\n", row);
      setPositionForIndex (p, index);
      }

   // this is needed for stackItems()
   QListView::dataChanged (topLeft, bottomRight);
   }


void Desktopview::slotModelChanged ()
   {
//    printf ("slotModelChanged\n");
   setPositions ();
   }


void Desktopview::rowsInserted (const QModelIndex & parent, int start, int end)
   {
//    printf ("rowInserted\n");
   QListView::rowsInserted (parent, start, end);
   }


QModelIndex Desktopview::indexAt (const QPoint &in_point) const
   {
   QModelIndex ind;
   QModelIndex parent = rootIndex ();;
   QStyleOptionViewItem opt = viewOptions ();
   Desktopdelegate *del = (Desktopdelegate *)itemDelegate ();
   QPoint point = in_point;

   if (!model () || !_position_items)
      return QModelIndex ();
   int count = model ()->rowCount (parent);
   // convert point to contents coordinates
   point += QPoint (horizontalScrollBar ()->value (), verticalScrollBar ()->value ());

   if (count > 0)
      {
      for (int i = count - 1; i >= 0; i--)
         {
         ind = model ()->index (i, 0, parent);
//          QRect rect = rectForIndex (ind);
//          if (rect.contains (point))
            {
            QPoint p = model ()->data (ind, Desktopmodel::Role_position).toPoint ();
            opt.rect = QRect (p.x (), p.y (), 1, 1); //QRect (rect.x (), rect.y ());
            // check we really are inside the item - the delegate knows
            if (del->containsPoint (opt, ind, point))
               {
               static int upto = 0;

//                qDebug () << upto++ << "row" << ind.row ();
               return ind;
               }
            }
         }
      return QModelIndex ();
      }
   return QListView::indexAt (point);
   }


void Desktopview::dropEvent (QDropEvent* event)
   {
   QAbstractItemModel *model = this->model ();

   setCursor(Qt::ArrowCursor);
   QModelIndex ind, dest = indexAt (event->pos ());
   QModelIndexList list = getSelectedList (true);
   Desktopmodel *dmodel = _modelconv->getDesktopmodel (model);

   // drag has finished, so there is no drop target now
   dmodel->setDropTarget (QModelIndex ());

   // drop target cannot be a source item
//    for (int i = 0; i < list.size (); i++)
//       if (dest == list [i])
//          dest = QModelIndex ();
   if (list.contains (dest))
      dest = QModelIndex ();

   printf ("drop event, dest = %d, %d\n", dest.row (), dropIndicatorPosition ());

   // record the old and new positions in case we need them
   QList<QPoint> oldlist, plist;
   foreach (ind, list)
      oldlist << model->data (ind, Desktopmodel::Role_position).toPoint ();

   QListView::dropEvent (event);    // this will move the positions

   foreach (ind, list)
      plist << rectForIndex (ind).topLeft ();

   // if no drop target, this is a move, so update the model with the new positions
   _modelconv->listToSource (model, list);
   _modelconv->indexToSource (model, dest);
   if (dest == QModelIndex ())
      dmodel->move (list, rootIndexSource (), oldlist, plist);
   else
      // we need to stack the selected stacks on top of the destination one
      err_complain (dmodel->stackItems (dest, list, &oldlist));
//       setPositions ();
   }


/** arrange items by given type (t_arrangeBy...) */

void Desktopview::arrangeBy (int type)
   {
   QModelIndex parent = rootIndex ();
   QAbstractItemModel *mod = model ();
   int count = mod->rowCount (parent);
   QModelIndexList list;
   QModelIndex ind;
   QList<QPoint> plist;

   // make a list of all items
   for (int i = 0; i < count; i++)
      {
      ind = mod->index (i, 0, parent);
      list << ind;
      plist << rectForIndex (ind).topLeft ();
      }
   Desktopmodel *dmodel = _modelconv->getDesktopmodel (mod);
   _modelconv->listToSource (mod, list);
   dmodel->arrangeBy (type, list, rootIndexSource (), plist);
   }


/** recalculate the sizes of all the items in this directory */

void Desktopview::resizeAll ()
   {
   Desktopmodel *dmodel = _modelconv->getDesktopmodel (model ());

   dmodel->resizeAll (rootIndexSource ());
   }


void Desktopview::resizeEvent (QResizeEvent *event)
   {
//    printf ("Desktopview::resizeEvent\n");
   QListView::resizeEvent (event);
//    QScrollBar *vert = verticalScrollBar ();
//    vert->setSingleStep (30);
   }


#define AUTOSCROLL_PERIOD 100

void Desktopview::checkAutoscroll (QPoint pos)
   {
   // enable autoscroll
   if (!_auto_scrolling)
      {
      _timer_id = startTimer (AUTOSCROLL_PERIOD);
      _auto_scrolling = true;
//       printf ("** created timer %d\n", _timer_id);
      }
   _mouse_pos = pos;
   }


void Desktopview::mouseMoveEvent (QMouseEvent *event)
   {
   checkAutoscroll (event->pos ());
   QListView::mouseMoveEvent (event);
   }


void Desktopview::timerEvent (QTimerEvent *event)
   {
   if (event->timerId () != _timer_id)
      {
      QListView::timerEvent (event);
      return;
      }

//    printf ("timer event %d\n", event->timerId ());

   // if no buttons down, then we have finished
   if (!_auto_scrolling || !qApp->mouseButtons ())
      {
      _auto_scrolling = false;
      killTimer (_timer_id);
      _timer_id = -1;
      }

   QPoint pos = _mouse_pos; //event->pos ();
   QRect r = rect ();

//    printf ("rect: %d, %d - %d, %d: pos %d, %d\n", r.left (), r.top (),
//       r.width (), r.height (), pos.x (), pos.y ());
#define AUTOSCROLL_MARGIN 64

   QRect normal = rect ().adjusted (AUTOSCROLL_MARGIN, AUTOSCROLL_MARGIN,
         -AUTOSCROLL_MARGIN, -AUTOSCROLL_MARGIN);

//    printf ("   - normal: %d, %d - %d, %d: pos %d, %d\n", normal.left (), normal.top (),
//       normal.width (), normal.height (), pos.x (), pos.y ());

   QPoint scroll;

   // if we are outside the 'normal' region, we are autoscrolling
   if (!normal.contains (pos))
      {
      // work out how much we are out by
      if (pos.x () > normal.right ())
         scroll.setX (pos.x () - normal.right ());
      if (pos.y () > normal.bottom ())
         scroll.setY (pos.y () - normal.bottom ());
      if (pos.x () < normal.left ())
         scroll.setX (pos.x () - normal.left ());
      if (pos.y () < normal.top ())
         scroll.setY (pos.y () - normal.top ());
//       printf ("autoscroll %d, %d\n", scroll.x (), scroll.y ());

      QScrollBar *hs = horizontalScrollBar ();
      QScrollBar *vs = verticalScrollBar ();
      if (scroll.x ())
         hs->setValue (hs->value () + scroll.x ());
      if (scroll.y ())
         vs->setValue (vs->value () + scroll.y ());
      }
   }


void Desktopview::dragMoveEvent (QDragMoveEvent *event)
   {
   QListView::dragMoveEvent (event);
   QAbstractItemModel *model = this->model ();

   QPoint pos = event->pos ();
   QModelIndex dest = indexAt (pos);
   QModelIndexList list = selectedIndexes ();

   // drop target cannot be a source item
   if (list.contains (dest))
//    for (int i = 0; i < list.size (); i++)
//       if (dest == list [i])
      dest = QModelIndex ();

//    QWidget *vp = viewport ();

   /* tell the model that this is the destination item - the model
      will draw this item differently to indicate this */
   Desktopmodel *dmodel = _modelconv->getDesktopmodel (model);
   QModelIndex sdest = dest;
   _modelconv->indexToSource (model, sdest);
   dmodel->setDropTarget (sdest);
   if (dest != QModelIndex ())
      {
//       QCursor cursor (Qt::UpArrowCursor);

//      qApp->setOverrideCursor (cursor);
//       setDragCursor ();
      // update the view to show the destination item highlighted
      update (dest);
      }

   checkAutoscroll (event->pos ());
   }


void Desktopview::contextMenuEvent (QContextMenuEvent *e)
   {
   QPoint pos = QPoint (e->x (), e->y ());

   QModelIndex index = indexAt (pos);
   emit popupMenu (index);
   }


void Desktopview::updateGeometries (void)
   {
   static int level = 0;
   QSize size;

   // should allow a margin here after the last item in the view
//    printf ("enter Desktopview::updateGeometries %d\n", level);

   // this call caused recursion back to here when we stack 4 items onto one
//    if (!level)
//       {
//       level++;
//       }
//    else
//       level++;
   if (level)
      return;
   level++;

   if (_position_items)
      {
      size = setPositions ();
   //    qDebug () << "total size" << size;
   
      QScrollBar *vs = verticalScrollBar ();
      QScrollBar *hs = horizontalScrollBar ();
   
      int value = vs->value ();
   
      vs->setSingleStep (30);
      hs->setSingleStep (30);
   
      vs->setMaximum (size.height ());
      vs->setMinimum (0);
      vs->setPageStep (viewport()->height ());
      vs->setValue (value);
   
      int width = viewport()->width ();
   //    qDebug () << "horiz: width" << width << "size" << size;
      hs->setMaximum (size.width () + 100 /*margin*/ - width);
      hs->setMinimum (0);
      hs->setPageStep (width);
   //    printf ("size %d, %d, vp %d, %d: %d\n", size.width (), size.height (),
   //          r.right (), r.bottom () + vs->value (), r.height ());
      }
   else
      // let the base class do it
      QListView::updateGeometries ();

   level--;
//    printf ("exit Desktopview::updateGeometries %d\n", level);
   }


void Desktopview::slotScrollTo (const QModelIndex &index)
   {
   scrollTo (index);
   }


void Desktopview::scrollToLast (void)
   {
   int bottomy = 0;
   QModelIndex bottom, parent;

   parent = rootIndex ();

   /* find the position of the bottom of the bottom-most item */
//    printf ("rows %d\n", model ()->rowCount ());
   for (int i = 0; i < model ()->rowCount (parent); i++)
      {
      // get index
      QModelIndex ind = model ()->index (i, 0, parent);

      // find out delegate and ask its size
      QStyleOptionViewItem opt;
      QAbstractItemDelegate *del = itemDelegate (ind);
      QSize size = del->sizeHint (opt, ind);

      // finally get the item's position
      QPoint p = model ()->data (ind, Desktopmodel::Role_position).toPoint ();
      //qDebug () << "scrollToLast" << p.y () << size.height () << "cf" << all.bottom ();

      // if the bottom of this item is below our current max, update it
      if (p.y () + size.height () > bottomy)
         {
         bottomy = p.y () + size.height ();
         bottom = ind;
         }
      }

   /* scroll so that the bottom of the last item is 200 pixels above the
      bottom of the window */
   QScrollBar *vs = verticalScrollBar ();
   QRect r = rect ();
   int value = bottomy - r.height () + 200;
   qDebug () << "scrollToLast" << "bottom" << bottomy
         << "height" << r.height ()
         << "result" << value;

   /* the scroll bar maximum may not have been set yet, so manually fix it here */
   if (value > vs->maximum ())
      vs->setMaximum (value);
   //if (value >= 0)
      vs->setValue (value > 0 ? value : 0);

   qDebug () << "scroll bars" << horizontalScrollBar ()->value () << verticalScrollBar ()->value ();
//   scrollTo (bottom);
   }


void Desktopview::setContextIndex (QModelIndex index)
   {
   _context_index = index;
   }


void Desktopview::setSelectionRange (int row, int count)
   {
   QItemSelectionModel *sel = selectionModel ();
   QAbstractItemModel *mod = model ();

   sel->select (QItemSelection (mod->index (row, 0, rootIndex ()),
      mod->index (row + count - 1, 0, rootIndex ())),
      QItemSelectionModel::Clear | QItemSelectionModel::Select);
   }


void Desktopview::addSelectionRange (int row, int count)
   {
   QItemSelectionModel *sel = selectionModel ();
   QAbstractItemModel *mod = model ();

   sel->select (QItemSelection (mod->index (row, 0, rootIndex ()),
      mod->index (row + count - 1, 0, rootIndex ())),
      QItemSelectionModel::Select);
   }


QModelIndex Desktopview::getSelectedItem (bool sourceModel)
   {
   QModelIndexList list = getSelectedList (false, sourceModel);

   return list.size () ? list [0] : QModelIndex ();
   }


QModelIndexList Desktopview::getSelectedList (bool multiple, bool sourceModel)
   {
   QModelIndexList list;

   /* if this is a single-item operation, and we have a context item,
      return just that item */
   if (!multiple && _context_index != QModelIndex ())
      {
      list << _context_index;
      if (sourceModel)
         _modelconv->listToSource (model (), list);
      return list;
      }

   /* otherwise return all the selected items, sorted by row. We must sort
      by row so that functions which use this list to delete can work
      their way backwards through the list and avoid having the row numbers
      change under their feet. Note if !multiple then we return only the
      first */
   else
      {
      list = selectedIndexes ();
      if (!list.size ())
         return list;

      // if only a single item is required, return it
      if (!multiple)
         {
         QModelIndex index = list [0];

         if (sourceModel)
            _modelconv->indexToSource (model (), index);
         list.clear ();
         list << index;
         return list;
         }
      QModelIndex parent = list [0].parent ();

//       static_cast<Desktopmodel *> (model ())->sortByPosition (list);
      Desktopmodel *dmodel = _modelconv->getDesktopmodel (model ());
      _modelconv->listToSource (model (), list);
      dmodel->sortByPosition (list);
      if (!sourceModel)
         _modelconv->listToProxy (dmodel, list);
      return list;
      }
   }


int Desktopview::getSelectionSummary (void)
   {
   QModelIndexList list = getSelectedList ();
   QModelIndex ind = getSelectedItem ();

   _sel_summary = SEL_none;
   if (list.size () > 1)
      _sel_summary |= SEL_more_than_one;

   if (list.size () > 0)
      _sel_summary |= SEL_at_least_one;

   if (ind.isValid ())
      {
      int pagecount = ind.model ()->data (ind, Desktopmodel::Role_pagecount).toInt ();

      if (pagecount > 1)
         _sel_summary |= SEL_one_multipage;
      }

   foreach (ind, list)
      {
      int pagecount = ind.model ()->data (ind, Desktopmodel::Role_pagecount).toInt ();

      if (pagecount > 1)
         {
         _sel_summary |= SEL_at_least_one_multipage;
         break;
         }
      }

   return _sel_summary;
   }


bool Desktopview::isSelection (enum e_sel sel)
   {
   return _sel_summary & sel ? true : false;
   }


void Desktopview::renameStack (const QModelIndex &index)
   {
   Desktopdelegate *del = (Desktopdelegate *)itemDelegate ();

   setCurrentIndex (index);
   del->setEditPageHint (false);
   edit (index);
   }


void Desktopview::renamePage (const QModelIndex &index)
   {
   Desktopdelegate *del = (Desktopdelegate *)itemDelegate ();

   setCurrentIndex (index);
   del->setEditPageHint (true);
   edit (index);
   }


void Desktopview::mousePressEvent (QMouseEvent *event)
   {
   QModelIndex index = indexAt (event->pos ());

   // if the user clicks in space, deselect everything
   if (!index.isValid () && !event->modifiers ())
      selectionModel ()->clear ();
   QListView::mousePressEvent (event);
   }

