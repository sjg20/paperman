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
   File:       pagedelegate.h
   Started:    6/6/09

   This file implements the delegate for pages within a stack. It displays
   a scaled preview image of variable size
*/


#include <QAbstractItemDelegate>


struct pmeasure_info;


class Pagedelegate : public QAbstractItemDelegate
   {
   Q_OBJECT

public:
   Pagedelegate (QObject * parent = 0);
   ~Pagedelegate ();

   enum e_point  // which point of an item was clicked on
      {
      Point_other,  // somewhere else
      Point_remove  // remove icon
      };

   /** bitmask which describes which elements of the delegate should be visible */
   enum e_display
      {
      Display_none      = 0,
      Display_coverage  = 1   //!< display coverage stats
      };

   //! paint the delegate
   void paint (QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

   //! get a size hint for the delegate
   QSize sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const;
#if 0
   //! handle an editor event
   bool editorEvent (QEvent * event, QAbstractItemModel *model,
      const QStyleOptionViewItem &option, const QModelIndex &index);

   QWidget *createEditor(QWidget *parent,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

   void setEditorData(QWidget *editor,
                                       const QModelIndex &index) const;

   void setModelData(QWidget *editor, QAbstractItemModel *model,
                                       const QModelIndex &index) const;

   void updateEditorGeometry(QWidget *editor,
      const QStyleOptionViewItem &option, const QModelIndex &/* index */) const;
#endif

   /** sets the maximum size for each page image. The preview images shown
       will match this in at least one dimension, and exceed it in neither

      \param size    maximum pixel size */
   void setPagesize (QSize size);

   /** gets the spacing to use between items in the page viewer */
   QSize getSpacing (const QStyleOptionViewItem &option);

   /** set the display options to use with the delegate

      \param options    set of options to use */
   void setDisplayOptions (e_display options);

   bool editorEvent (QEvent * event, QAbstractItemModel *model,
         const QStyleOptionViewItem &option, const QModelIndex &index);

   /** calculates the area (in contents coordinates) of a part of a page in
       the viewer. The part is the full width of the page pixmap, and extends
       from pixel line 'start_line' down to 'height' pixels from there. This
       is used to update a subset of scanlines during a scan

       \param option    style options
       \param index     page to update
       \param start_line   start scanline of pixmap to update
       \param height       number of scanlines to update
       \param rect      output rectangle - in contents coordinates */
   void getPagePart (const QStyleOptionViewItem &option,
      const QModelIndex &index, int start_line, int height, QRect &rect) const;

signals:
   /** indicate that an item has been clicked

      \param index      item clicked
      \param which      which part of it was clicked (e_point) */
   void itemClicked (const QModelIndex &index, int which);

protected:
   // calculate measurements for an item
   void measureItem (const QStyleOptionViewItem &option, const QModelIndex &index,
      pmeasure_info &measure, QPoint *offset) const;

   enum e_point find_which (const QModelIndex &index,
         const QStyleOptionViewItem &option, const QPoint &pos);

private:
   QSize _pagesize;        //!< max pixel size of the page images
   e_display _display;     //!< display options
   QPixmap _icon_remove;
   QPixmap _icon_keep;
   QPixmap _icon_blank;
   };


