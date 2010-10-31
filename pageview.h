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
/*
   Project:    Maxview
   Author:     Simon Glass
   Copyright:  2001-2009 Bluewater Systems Ltd, www.bluewatersys.com
   File:       pageview.h
   Started:    5/6/09

   This file implements the view for pages within a stack. It supports
   viewing multiple pages and allows reordering and other functions.

   The model is in pagemodel.
*/

#include <QListView>

class Pageview : public QListView
   {
   Q_OBJECT

public:
   Pageview (QWidget *parent = 0);
   ~Pageview ();

   /** handle a wheel event in the viewer. We use this to zoom in and out

      \param e    wheel event */
   void wheelEvent (QWheelEvent *e);

   /** set the nominal scale factor for the view

      \param scale_down    new scale factor (1/n) */
   void setScale (int scale_down);

   /** select a page and scroll to it

      \param ind     index of page to select / scroll to */
   void selectPage (QModelIndex ind);

   /** sets whether dragging of items is allowed, or just selection

      \param allow      allow dragging of items to change order */
   void allowMove (bool allow);

   /** gets the options used in a view */
   const QStyleOptionViewItem getViewOptions (void) { return viewOptions (); }

   /** scroll down to the maximum amount

      \param ifAtEnd    if true, then this function will do nothing if the
                        viewer is marked as having been moved away from the
                        end by the user */
   void scrollToEnd (bool ifAtEnd = false);

public slots:
   void slotPagePartChanged (const QModelIndex &index, const QImage &image, int scaled_linenum);

protected:
   void scrollContentsBy (int dx, int dy);

   //! update the view size based on the items within it
   void updateGeometries (void);

protected slots:
   void currentChanged (const QModelIndex &index, const QModelIndex &previous);

signals:
   void signalNewScale (int new_scale);

   void showInfo (const QModelIndex &index);

private:
   int _scale_down;     //!< scale factor to use (1/n)
   bool _autoscroll;    //!< true if the user has not manually adjusted the scrollbar
   bool _ignore_scroll; //!< true to ignore any scrolls (they are machine-generated)
   };

