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
#include <QMouseEvent>
#include <QPainter>

#include "pagedelegate.h"
#include "pagemodel.h"


/*

The delegate for this is displayed as follows

   +----------------+
   |                |
   |                |
   |                |
   |                |
   |                |
   |                |
   |                |
   |                |
   +----------------+
   n  name          D

where
   n is the page number
   name is the page name
   D is the delete icon

In coverage mode, we insert the coverageL

   +----------------+
   |                |
   |                |
   |                |
   |                |
   |                |
   |                |
   |                |
   |                |
   +----------------+
   n name    cov  B D

If there is no room, the name gets dropped

*/



Pagedelegate::Pagedelegate (QObject *parent)
      : QAbstractItemDelegate (parent)
   {
   // get the images we need
   _icon_remove = QPixmap (":/images/images/pageremove.xpm");
   _icon_keep = QPixmap (":/images/images/pagekeep.xpm");
   _icon_blank = QPixmap (":/images/images/pageblank.xpm");
   }


Pagedelegate::~Pagedelegate ()
   {
   }


/** information we create about the display of a page
 */

typedef struct pmeasure_info
   {
   bool valid;          //!< true if the item data is fully loaded (not just a placeholder)
   bool dodgy;          //!< true if the pixmap scaling isn't ideal and will be redone later
   QString pagename;    //!< the page name string
   int pagenum;         //!< the page number
   QRect rect;          //!< overall rect for the page (includes all parts)
   QRect pixmapRect;    //!< rect for the pixmap
   QRect pagenameRect;  //!< rect for the page name
   QRect pagenumRect;   //!< rect for the page name
   QRect coverageRect;  //!< rect for coverage information
   QRect blankRect;     //!< rect for blank icon
   QRect removeRect;    //!< rect for remove icon
   QSize size;          //!< overall size for the page
   Pageinfo *pageinfo;  //!< the page info object for this page
   QPixmap pixmap;      //!< page image pixmap
   QString coverageStr; //!< information about page coverage (amount of black on page)
   bool blank;          //!< true if the page is marked blank
   bool remove;         //!< true if the page is marked for removal
   bool scanning;       //!< true if this page is being scanned
   } pmeasure_info;


void Pagedelegate::measureItem (const QStyleOptionViewItem &option, const QModelIndex &index,
      pmeasure_info &measure, QPoint *offset) const
   {
   QRect rect;
   QStyle *style = QApplication::style();

   QFont f = option.font;
   QFontMetrics fm (option.font);
   f.setBold (true);
   QFontMetrics fmb (f);

   measure.coverageStr = index.model ()->data (index, Pagemodel::Role_coverage).toString ();
   measure.blank = index.model ()->data (index, Pagemodel::Role_blank).toBool ();
   measure.remove = index.model ()->data (index, Pagemodel::Role_remove).toBool ();
   measure.scanning = index.model ()->data (index, Pagemodel::Role_scanning).toBool ();

   measure.valid = true;
   QVariant value = index.model ()->data (index, Pagemodel::Role_pageinfo);
   measure.pageinfo = value.value<Pageinfo *>();
   Pageinfo *pi = measure.pageinfo;

   if (!pi)
      {
      qDebug () << "Pagedelegate::measureItem: page not found";
      return;
      }

   int lines = 1;

   measure.rect = QRect (0, 0, _pagesize.width (), _pagesize.height ());
   measure.removeRect = QRect (QPoint (_pagesize.width () - _icon_remove.width (), 0),
      _icon_remove.size ());
   measure.blankRect = QRect (QPoint (_pagesize.width () - _icon_remove.width () * 2 - 3, 0),
      _icon_remove.size ());
   int leftmost = measure.blankRect.left ();
   if (_display & Display_coverage)
      {
      measure.coverageRect = style->itemTextRect (fm, rect, Qt::AlignLeft,
         false, measure.coverageStr);
      measure.coverageRect.translate (measure.blankRect.left () - measure.coverageRect.right (), 0);
      leftmost = measure.coverageRect.left ();
      }

   measure.size = _pagesize;
   measure.size.setHeight (_pagesize.height () + lines * fmb.height ());

   QPixmap pm = pi->pixmap (measure.dodgy);
   measure.pixmap = pm;
   measure.pixmapRect = QRect ((_pagesize.width () - pm.width ()) / 2, 0,
         pm.width (), pm.height ());

   measure.pagename = measure.scanning ? tr ("Scanning...") : pi->pagename ();
   measure.pagenum = pi->pagenum ();
   measure.pagenumRect = style->itemTextRect (fm, rect, Qt::AlignLeft, false,
      QString ("%1").arg (measure.pagenum));
   measure.pagenameRect = QRect (measure.pagenumRect.width () + 5, 0,
      leftmost - 5, fmb.height ());

   measure.pagenumRect.translate (0, _pagesize.height () + 3);
   measure.pagenameRect.translate (0, _pagesize.height () + 3);
   measure.coverageRect.translate (0, _pagesize.height () + 3);
   measure.blankRect.translate (0, _pagesize.height ());
   measure.removeRect.translate (0, _pagesize.height ());

   if (offset)
      {
      measure.pixmapRect.translate (offset->x (), offset->y ());
      measure.pagenumRect.translate (offset->x (), offset->y ());
      measure.pagenameRect.translate (offset->x (), offset->y ());
      measure.coverageRect.translate (offset->x (), offset->y ());
      measure.blankRect.translate (offset->x (), offset->y ());
      measure.removeRect.translate (offset->x (), offset->y ());
      measure.rect.translate (offset->x (), offset->y ());
      }
   }


void Pagedelegate::paint (QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
   {
   pmeasure_info measure;

   // why do we get asked to paint a null item? We do...
   if (index == QModelIndex ())
      return;

   QPoint offset = QPoint (option.rect.x (), option.rect.y ());

   measureItem (option, index, measure, &offset);

//    Pageinfo *pi = measure.pageinfo;

   painter->save ();

// #define OUTLINE
   QStyle *style = QApplication::style();

   QFont f = painter->font ();
   f.setBold (true);
   painter->setFont (f);
   // draw the pixmap if it is valid
   if (measure.valid && !measure.pixmap.isNull ())
      style->drawItemPixmap (painter, measure.pixmapRect, Qt::AlignHCenter, measure.pixmap);
   else
      painter->fillRect (measure.rect, QBrush (Qt::DiagCrossPattern));

   style->drawItemText (painter, measure.pagenumRect, Qt::AlignLeft,
         style->standardPalette (), false, QString ("%1").arg (measure.pagenum));
   f.setBold (false);
   painter->setFont (f);

   if (option.state & QStyle::State_Selected)
      painter->fillRect (measure.pagenameRect, style->standardPalette().color(QPalette::Highlight));
   style->drawItemText (painter, measure.pagenameRect, Qt::AlignLeft,
         style->standardPalette (), false, measure.pagename,
         option.state & QStyle::State_Selected
             ? QPalette::HighlightedText : QPalette::WindowText);

   if (_display & Display_coverage)
      style->drawItemText (painter, measure.coverageRect, Qt::AlignLeft,
         style->standardPalette (), false, measure.coverageStr,
         QPalette::WindowText);

   if (measure.blank)
      style->drawItemPixmap (painter, measure.blankRect, Qt::AlignLeft, _icon_blank);
   style->drawItemPixmap (painter, measure.removeRect, Qt::AlignLeft,
      measure.remove ? _icon_remove : _icon_keep);

   if (measure.dodgy && !measure.pixmap.isNull ())
      {
      QPoint p = measure.pixmapRect.bottomRight () - QPoint (10, 10);

      QRect rect = QRect (p, QSize (10, 10));
      painter->setPen (Qt::NoPen);
      painter->setBrush (QBrush (Qt::green));
      painter->drawEllipse (rect);
      }

   painter->restore ();
   }


// get a size hint for the delegate
QSize Pagedelegate::sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const
   {
   pmeasure_info measure;

   measureItem (option, index, measure, 0);

   // doesn't seem to handle this changing, so if we don't know the size, use
   // a large number for now
   return measure.size.isValid () ? measure.size : QSize (147, 226);
//    return measure.size;
   }


void Pagedelegate::setPagesize (QSize size)
   {
   _pagesize = size;
   }


QSize Pagedelegate::getSpacing (const QStyleOptionViewItem &option)
   {
   QFont f = option.font;
   f.setBold (true);
   QFontMetrics fmb (f);

   int lines = 1;
   if (_display & Display_coverage)
      lines++;

   return QSize (10, lines * fmb.height () + 10);
   }


void Pagedelegate::setDisplayOptions (e_display options)
   {
   _display = options;
   }


enum Pagedelegate::e_point Pagedelegate::find_which (const QModelIndex &index,
         const QStyleOptionViewItem &option, const QPoint &pos)
   {
   pmeasure_info measure;
   enum e_point which;

   QPoint offset = QPoint (option.rect.x (), option.rect.y ());
   measureItem (option, index, measure, &offset);

//   printf ("%d,%d in ", point.x (), point.y ());
//   printf ("%d,%d - %d,%d\n", rect.x (), rect.y (), rect.width (), rect.height ());

   which = Point_other;
//    if (measure.rect.contains (pos))
      {
      if (/*measure.blank &&*/ measure.removeRect.contains (pos))
         which = Point_remove;
      }
   return which;
   }


bool Pagedelegate::editorEvent (QEvent * event, QAbstractItemModel *model,
         const QStyleOptionViewItem &option, const QModelIndex &index)
   {
   QMouseEvent *mouse = (QMouseEvent *)event;

   model = model;
   if (event->type () == QEvent::MouseButtonPress
      || event->type () == QEvent::MouseButtonDblClick)
      {
      int which = find_which (index, option, mouse->pos ());
//       printf ("hit = %d\n", _hitwhich);

      if (event->type () == QEvent::MouseButtonDblClick)
         qDebug () << "double";

//       if (which == Point_remove)
      emit itemClicked (index, which);
      }
   return false;
   }


void Pagedelegate::getPagePart (const QStyleOptionViewItem &option,
      const QModelIndex &index, int start_line, int height, QRect &rect) const
   {
   pmeasure_info measure;
   enum e_point which;

   QPoint offset = QPoint (option.rect.topLeft ());
   measureItem (option, index, measure, &offset);

   rect = measure.pixmapRect;
   rect = QRect (rect.left (), rect.top () + start_line,
      rect.width (), height);
   }
