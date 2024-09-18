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


#ifndef __desktopview
#define __desktopview


#include <QListView>
// #include <QTreeView>
#include "measure.h"

class Desktopmodelconv;
class Measure;

/**
 * @brief Provide a view of paper stacks
 *
 * This shows paper stacks in a scrolling view where each stack has its own
 * position. It uses Desktopmodel to provide the underlying model.
 *
 * The stacks normally come from a single directory, but when doing
 * a global locate they can be from multiple directories. See
 * Desktopwidget::matchUpdate for that.
 *
 * The user can move things around, combine and split stacks, etc.
 */
class Desktopview : public QListView
   {
   Q_OBJECT

public:
   Desktopview (QWidget *parent = 0);
   ~Desktopview ();

   // a set of flags to describe what is currently selected
   enum e_sel
      {
      SEL_none          = 0,
      SEL_more_than_one = 1,  // more than one stack
      SEL_at_least_one  = 2,  // one or more stacks
      SEL_one_multipage = 4,  // a single stack with >1 pages
      SEL_at_least_one_multipage = 8   // at least one stack with >1 pages
      };

   //! indcate that we don't want to actively set the position of each item
   void setFreeForm (void);

   /** returns a list of the selected items in the view. The list is
       in order of index row

      \param multiple   if true, then just returns the current selection
                        if false, returns the context item, or (if there
                        is none) the first item from the selection

      \param sourceModel  true to return indexes from the source model
                          rather than the proxy */
   QModelIndexList getSelectedList (bool multiple = true, bool sourceModel = false);

   /** convenience function */
   QModelIndexList getSelectedListSource (bool multiple = true)
      { return getSelectedList (multiple, true); }

   /** returns the currently selected item, or the first selected if there
       is more than one */
   QModelIndex getSelectedItem (bool sourceModel = false);

   /** \returns summary information about the current selection (enum e_sel) */
   int getSelectionSummary (void);

   /** \returns true if the given attribute is set in the selection summary */
   bool isSelection (enum e_sel sel);

   /** record the index on which the context menu was opened */
   void setContextIndex (QModelIndex index);

   /** clears the current selection and selects the given range

      \param row     first row to select
      \param count   number of rows to select */
   void setSelectionRange (int row, int count);

   /** adds the given range to the current selection

      \param row     first row to select
      \param count   number of rows to select */
   void addSelectionRange (int row, int count);

   /** returns the viewer root index translated to the source model */
   QModelIndex rootIndexSource (void);

   /** allow the user to edit a stack name */
   void renameStack (const QModelIndex &index);

   /** allow the user to edit the name of the current page of the current stack */
   void renamePage (const QModelIndex &index);

   /** returns the index at the given position */
   QModelIndex indexAt (const QPoint &point) const;

   /** arrange all items in the directory by the given sort order */
   void arrangeBy (int type);

   /** resize all items in the directory by scanning their required true size */
   void resizeAll (void);

   /** sets the model converter to use when needed for accessing the
       source model (as opposed to any proxy which might be in the way

      \param modelconv  new model converter to use */
   void setModelConv (Desktopmodelconv *modelconv);

   // Get the current measurer
   Measure *getMeasure();

signals:
   void pressed (QModelIndex &);

   /** register the current index for a context menu (0 for none) */
   void popupMenu (QModelIndex &index);

   void newContents (QString str);

   /** emitted when there are no pages selected */
   void pageLost (void);

   /** indicate that an item should be previewed

      \param index      item clicked
      \param which      which part of it was clicked
      \param now        true to preview now (else waits for user to finish clicking) */
   void itemPreview (const QModelIndex &index, int which, bool now);

   //! Indicates that the Escape key was pressed
   void escapePressed();

   //! Indicates that a new selection has been made
   void newSelection();

public slots:
   /** set up the position of each item

      \returns the maximum extent of the item positions (i.e. max x and max y) */
   QSize setPositions (void);

   //! ensure that the last item is visible
   void scrollToLast (void);

   void dataChanged (const QModelIndex & topLeft, const QModelIndex & bottomRight);

   void rowsInserted (const QModelIndex & parent, int start, int end);

   void slotIndexesMoved (const QModelIndexList & indexes);

   /** the model has been changed in some way - we reposition items */
   void slotModelChanged ();

   /** scroll to the given index */
   void slotScrollTo (const QModelIndex &index);

protected:
   /** handle a drop event - this saves the new position of items into
       the model*/
   void dropEvent (QDropEvent* event);

//    void wheelEvent (QWheelEvent *e);

   //! handle resizing the view (we adjust the scroll steps)
   void resizeEvent (QResizeEvent *event);

   /** handle moving the mouse when dragging (we change the cursor
       and autoscroll) */
   void dragMoveEvent (QDragMoveEvent *event);

   //! handle moving the mount when tracking (we autoscroll)
   void mouseMoveEvent (QMouseEvent *event);

   //! handle a mouse press event
   void mousePressEvent (QMouseEvent * event);

   //! open a context menu
   void contextMenuEvent (QContextMenuEvent *e);

   //! update the view size based on the items within it
   void updateGeometries (void);

   //! handle a timer event (used for autoscrolling)
   void timerEvent (QTimerEvent *event);

   //! handle a keypress event
   void keyReleaseEvent(QKeyEvent *event);

protected slots:
   void currentChanged (const QModelIndex &current, const QModelIndex &previous);

   void selectionChanged(const QItemSelection &sel,
                         const QItemSelection &desel);

private:
   void checkAutoscroll (QPoint pos);

private:
   QPersistentModelIndex _context_index;    //!< item that context menu was opened on
   int _timer_id;          //!< event timer
   bool _auto_scrolling;   //!< true if currently autoscrolling
   QPoint _mouse_pos;      //!< last mouse position
   Desktopmodelconv *_modelconv; //!< model conversion class
   int _sel_summary;   //!< last caculated selection summary
   bool _position_items;   //!< true to position items where we want them
   Measure *_measure;
   };


#endif

