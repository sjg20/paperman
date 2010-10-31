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
#include <QFontMetrics>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>

#include "desktopdelegate.h"
#include "desktopmodel.h"


/*
This is what a desktop item looks like:

       stack_name
       page_name
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
   <-    n of c    ->

<- and ->   arrows to change pages
n           current page number
c           number of pages
stack_name  name of the whole stack (filename)
page_name   name of current page

The bounding box includes space for the whole lot. So the image is drawn two
lines down from the top.

Note that if there is only one page in the stack, then the page_name and
page select region are omitted.

A rather complex feature of stacks is that they need to be displayed with
a fixed width. That means that if page 1 is narrow, but page 2 is wide, then
both need to be displayed centred within an area big enough for page 2.
The problem is that a number of things can affect the width:
   - the pixmap width
   - the title
   - the individual page names

(for height it is just the pixmap height, since the others are single line
text items)

To deal with this we have a bit of a messy arrangement:

1. The maxdesk file holds information about the maximum pixmap size, maximum
title size and maximum page name size. Where not know, these values are -1.

2. Desk can calculate only the pixmap size (since while it knows the stack
title and page names, it does not know the font which Desktopdelegate will use
to render them.

3. Desktopdelegate requests this information from Desktopmodel, which in turn
finds it out from Desk. Where it is missing (-1) it is calculated by
Desktopdelegate, and then written back to the model (and on to Desk).

4. The next time the maxdesk file is read, it will hopefully have the
information so that Desktopdelegate does not need to request it, and things
will move along much more quickly.

5. If the font used by Desktopdelegate changes, then the values will be
wrong. To cope with this, Desktopdelegate detects any increase in a stack
width / height, and in that case simply updates the sizes.

6. However it is possible that the Desktopdelegate might change to using
a smaller font. In this case the stack will be wider than it should be and
no one will know. To cope with this, there is a way of opening a directory
and telling maxdesk to ignore the size information. That way it will be
(slowly) recalculated as needed

*/



#define AUTO_REPEAT_DELAY  300
#define AUTO_REPEAT_PERIOD 100


static QPixmap *pleft = 0;
static QPixmap *pright = 0;
static QPixmap *pages = 0;


Desktopdelegate::Desktopdelegate (Desktopmodelconv *modelconv, QObject * parent)
      : QAbstractItemDelegate (parent)
   {
   _userbusy = false;
   _timer = new QTimer ();
   _hitwhich = Point_other;
   _modelconv = modelconv;
   connect (_timer, SIGNAL(timeout()), this, SLOT(autoRepeatTimeout()));
   if (!pleft)
      {
      pleft = new QPixmap (":/images/images/left.xpm");
      pright = new QPixmap (":/images/images/right.xpm");
      pages = new QPixmap (":/images/images/pages.xpm");
      }
   }


Desktopdelegate::~Desktopdelegate ()
   {
   delete _timer;
   }


/* update the model with positional information when we have it */

#define UPDATE_MODEL


/** information we create about the display of a paper stack

Note: to save time when opening a directory, the viewer will display all the
items as 'unknown' types. Once it has time to load each item, it marks it as
valid. Until then, some information about the item may not be valid, and it
is certainly not a good idea to write to any of the item's info, since it
is about to be loaded and any prior changes will be lost.
 */

typedef struct measure_info
   {
   bool valid;          //!< true if the item data is fully loaded (not just a placeholder)
   QString title;       //!< stack title (excludes filename extension)
   QString pagename ;   //!< current page name
   QString pagestr;     //!< page number string (e.g. '4 of 9')
   QSize size;          //!< total pixel size of the stack including all elements
   QSize title_maxsize; //!< max total pixel size of the titles
   QSize pagename_maxsize;  //!< max pixel size of the stack's pages' names
   QSize pixmapSize;    //!< size of pixmap (either actual, or the model's idea, whichever is larger)
   QRect rect;          //!< bounding rectangle of whole item
   QRect titleRect;     //!< bounding rectangle of title
   QRect pagenameRect;  //!< bounding rectangle of page name
   QRect pagenumRect;   //!< bounding rectangle of pagenum region (includes arrows)
   QRect pixmapRect;    //!< bounding rectangle of pixmap - the actual image preview
   QRect pixmapRectHint; //!< same, but taking account of model's possible larger pixmap size
   QPixmap pm;          //!< image preview pixmap
   int pagenum;         //!< current page number
   int pagecount;       //!< number of pages in stack
   bool multiple;       //!< true if there are multiple pages
   bool target;         //!< true if this is the target of a drag
   bool source;         //!< true if this is one of the items being dragged
   } measure_info;


void Desktopdelegate::measureItem (const QStyleOptionViewItem &option, const QModelIndex &index,
      measure_info &measure, QPoint *offset, bool size_only) const
   {
//   Desktopmodel *model = (Desktopmodel *)index.model ();
   QAbstractItemModel *model = (QAbstractItemModel *)index.model ();
   Q_ASSERT (model);
   measure.valid = model->data (index, Desktopmodel::Role_valid).toBool ();
   measure.title = index.model ()->data (index, Qt::DisplayRole).toString ();
   measure.pagenum = index.model ()->data (index, Desktopmodel::Role_pagenum).toInt ();
   measure.title_maxsize = model->data (index, Desktopmodel::Role_title_maxsize).toSize ();
   measure.pagename_maxsize = model->data (index, Desktopmodel::Role_pagename_maxsize).toSize ();
   /* get the model's idea of the maximum preview pixmap size */
   QSize oldsize = model->data (index, Desktopmodel::Role_preview_maxsize).toSize ();

   measure.pagecount = 1;
   measure.target = false;
   measure.source = false;
   if (measure.valid)
      {
      measure.pagecount = index.model ()->data (index, Desktopmodel::Role_pagecount).toInt ();
      measure.multiple = measure.pagecount > 1;
      measure.target = model->data (index, Desktopmodel::Role_droptarget).toBool ();
      }
   /* unfortunately we don't know if an item has multiple pages until we
      actually open the max file (i.e. measure.valid is true). However, we
      can take a guess by looking if the pagename_maxsize is valid. It is
      only used for stacks with multiple pages so is a good indicator. This
      allows us to produce a more accurate size measurement before opening
      the max file, which in turn stops the view from having to deal with
      item sizes changing. Also if the current page is not 0 then we must
      have multiple pages */
   else
      {
      measure.multiple = measure.pagename_maxsize.isValid () || measure.pagenum > 0;
      if (size_only)
         {
         int width = oldsize.width ();

         // make sure it is as wide as the stack's maximum width
         if (measure.title_maxsize.width () > width)
            width = measure.title_maxsize.width ();
         if (measure.pagename_maxsize.width () > width)
            width = measure.pagename_maxsize.width ();

         measure.size = QSize (width + 0, 0 + oldsize.height () + measure.title_maxsize.height ()
               + measure.pagename_maxsize.height ()
               + (measure.multiple ? measure.pagename_maxsize.height () : 0));
         if (measure.size.width () < 0 || measure.size.height () < 0)
            measure.size = QSize (147, 226);  // a suitable default if we have nothing
         return;
         }
      }
   /* note: probably can fix problems caused by item sizes changing by emitting

void sizeHintChanged ( const QModelIndex & index ) */

   // is this item begin dragged?
   Desktopmodel *contents = _modelconv->getDesktopmodel (model);
   measure.source = (option.state & QStyle::State_Selected)
      && contents->dropTarget ();

   if (measure.multiple)
      measure.pagename = index.model ()->data (index, Desktopmodel::Role_pagename).toString ();
   QVariant value = index.model ()->data (index, Desktopmodel::Role_pixmap);
   measure.pm = value.value<QPixmap>();

   QFont f = option.font;
   QFontMetrics fm (option.font);
   f.setBold (true);
   QFontMetrics fmb (f);
   QRect rect;

   QStyle *style = QApplication::style();

   if (measure.valid)
      measure.pagestr = QString (tr ("%1 of %2")).arg (measure.pagenum + 1).arg (measure.pagecount);
   else
      measure.pagestr = QString (tr ("%1 of ?")).arg (measure.pagenum + 1);
   measure.titleRect = style->itemTextRect (fmb, rect, Qt::AlignLeft, false, measure.title);
   if (measure.multiple)
      {
      measure.pagenameRect = style->itemTextRect (fm, rect, Qt::AlignLeft,
            false, measure.pagename);
      measure.pagenumRect = style->itemTextRect (fm, rect, Qt::AlignLeft,
            false, measure.pagestr);
      measure.pagenumRect.setWidth (measure.pagenumRect.width () + 8 +
            + pleft->width () + pright->width ());
      measure.pagenumRect.setHeight (pleft->height ());

      // if we don't have information about the max title rect, we can fill it in
      // after all, it is just calculated above
      if (measure.valid && !measure.title_maxsize.isValid ())
         {
         measure.title_maxsize = measure.pagenameRect.size ();
#ifdef UPDATE_MODEL
         model->setData (index, measure.title_maxsize,
            Desktopmodel::Role_title_maxsize);
#endif
         }
      if (measure.valid && !measure.pagename_maxsize.isValid ())
         {
         QStringList name;
         QSize size;

         // get all page titles
         // note this is very expensive - it reads in all chunks from the stack
         name = index.model ()->data (index, Desktopmodel::Role_pagename_list).toStringList ();

         // work through all pages, getting the size information
         for (int i = 0; i < measure.pagecount; i++)
            {
            QRect rect;

            rect = style->itemTextRect (fm, rect, Qt::AlignLeft, false, name [i]);
            if (rect.width () > size.width ())
               size = rect.size ();
            }
         measure.pagename_maxsize = size;

         // update the model with this new information
#ifdef UPDATE_MODEL
         model->setData (index, measure.pagename_maxsize,
            Desktopmodel::Role_pagename_maxsize);
#endif
         }
      }

   /** work out the size of the preview pixmap - if the model thinks it
       should be larger, agree with it. This happens a lot
       Maybe should only do this when measure.valid is false? */
   QSize size = oldsize.expandedTo (measure.pm.size ());
   measure.pixmapSize = size;

#ifdef UPDATE_MODEL
   if (oldsize != size)
      // update the model with this new information
      model->setData (index, size, Desktopmodel::Role_preview_maxsize);
#endif
   // find the largest width
   int width = size.width ();

   if (measure.titleRect.width () > width)
      width = measure.titleRect.width ();
   if (measure.pagenameRect.width () > width)
      width = measure.pagenameRect.width ();
   if (measure.pagenumRect.width () > width)
      width = measure.pagenumRect.width ();

   // also make sure it is as wide as the stack's maximum width
   if (measure.title_maxsize.width () > width)
      width = measure.title_maxsize.width ();
   if (measure.pagename_maxsize.width () > width)
      width = measure.pagename_maxsize.width ();

   measure.size = QSize (width + 0, 0 + size.height () + measure.titleRect.height ()
         + measure.pagenameRect.height () + measure.pagenumRect.height ());
//    printf ("** %s: mult %d, pc %d, model %d, %d - pm %d, %d, result %d, %d\n", measure.title.latin1 (),
//          measure.multiple, measure.pagecount, oldsize.width (), oldsize.height (),
//          measure.pm.size ().width (), measure.pm.size ().height (),
//          measure.size.width (), measure.size.height ());

   if (!size_only)
      {
      // centre all the rect within the maximum width
      measure.titleRect.translate ((width - measure.titleRect.width ()) / 2, 0);
      measure.pixmapRect = QRect ((width - measure.pm.width ()) / 2,
            fmb.height ()
               + (measure.multiple ? fm.height () : 0),
            measure.pm.width (), measure.pm.height ());
      measure.pixmapRectHint = QRect ((width - measure.pixmapSize.width ()) / 2,
            fmb.height ()
               + (measure.multiple ? fm.height () : 0),
            measure.pixmapSize.width (), measure.pixmapSize.height ());
      if (measure.multiple)
         {
         // use the pixmap rect hint, since if !valid we won't have the real pixmap
         measure.pagenameRect.translate ((width - measure.pagenameRect.width ()) / 2, 0);
         int diff = measure.pixmapRectHint.width () - measure.pagenumRect.width ();
         if (diff > 0)
            measure.pagenumRect.setWidth (measure.pixmapRectHint.width ());
         measure.pagenumRect.translate ((width - measure.pagenumRect.width ()) / 2, 0);

         // move the pagename below title
         measure.pagenameRect.translate (0, fmb.height ());
         measure.pagenumRect.translate (0, measure.size.height () - measure.pagenumRect.height ());
         }

      measure.rect = QRect (QPoint (0, 0), measure.size);

      // translate the coords
      if (offset)
         {
         measure.pixmapRect.translate (*offset);
         measure.pixmapRectHint.translate (*offset);
         measure.titleRect.translate (*offset);
         measure.pagenameRect.translate (*offset);
         measure.pagenumRect.translate (*offset);
         measure.rect.translate (*offset);
         }
      }
   }


void Desktopdelegate::paint (QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
   {
   measure_info measure;

   // why do we get asked to paint a null item? We do...
   if (index == QModelIndex ())
      return;

   QPoint offset = QPoint (option.rect.x (), option.rect.y ());

   measureItem (option, index, measure, &offset);
   painter->save ();

// #define OUTLINE
   QStyle *style = QApplication::style();

   QFont f = painter->font ();
   f.setBold (true);
   painter->setFont (f);

   // draw the pixmap if it is valid
   if (measure.valid)
      style->drawItemPixmap (painter, measure.pixmapRect, Qt::AlignHCenter, measure.pm);
   else
      {
      // otherwise we just draw an outline
      painter->fillRect (measure.pixmapRectHint, QBrush (Qt::DiagCrossPattern));
         //style->standardPalette().color(QPalette::BrightText));
      }

   // if this is the target of a drag, draw a little 'pages' symbol over it
   if (measure.target) // || measure.source)
      {
      style->drawItemPixmap (painter, measure.pixmapRect, Qt::AlignHCenter, *pages);
      painter->fillRect (measure.titleRect, style->standardPalette().color(QPalette::BrightText));
      }
   if (option.state & QStyle::State_Selected)
      painter->fillRect (measure.titleRect, style->standardPalette().color(QPalette::Highlight));
   style->drawItemText (painter, measure.titleRect, Qt::AlignHCenter,
         style->standardPalette (), false, measure.title,
         option.state & QStyle::State_Selected
             ? QPalette::HighlightedText : QPalette::WindowText);
   f.setBold (false);
#ifdef OUTLINE
   painter->setPen (Qt::red);
   painter->drawRect (measure.pixmapRect);
   painter->drawRect (measure.titleRect);
#endif
   painter->setFont (f);
   if (measure.multiple)
      {
      painter->setPen (Qt::black);
      style->drawItemText (painter, measure.pagenameRect, Qt::AlignHCenter,
         style->standardPalette (), false, measure.pagename);
//    printf ("%s\n", measure.title.latin1 ());

#ifdef OUTLINE
      painter->setPen (Qt::green);
      painter->drawRect (measure.pagenameRect);
#endif
      // draw control area
      QRect rect = measure.pagenumRect;

      painter->fillRect (rect, style->standardPalette().color(QPalette::Button));
      painter->drawPixmap (rect.x (), rect.y (), *pleft);
      painter->drawPixmap (rect.right () - pright->width (), rect.y (), *pright);

      rect.adjust (pleft->width (), 0, -pright->width (), 0);

      style->drawItemText (painter, rect, Qt::AlignHCenter,
            style->standardPalette (), false, measure.pagestr, QPalette::WindowText);
#ifdef OUTLINE
      painter->setPen (Qt::blue);
      painter->drawRect (measure.pagenumRect);
#endif
      }
#ifdef OUTLINE
   painter->setPen (Qt::black);
   painter->drawRect (measure.rect);
#endif
   painter->restore ();
   }


// get a size hint for the delegate
QSize Desktopdelegate::sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const
   {
   measure_info measure;

   measureItem (option, index, measure, 0, true);

//    measure.size = QSize (width + 0, 0 + size.height () + measure.titleRect.height ()
//          + measure.pagenameRect.height () + measure.pagenumRect.height ());


/*   printf ("%s: %d, size %d x %d, %dx%d, %dx%d, %dx%d\n", measure.title.latin1 (),
         measure.size.isValid (),
         measure.size.width (), measure.size.height (),
         measure.titleRect.width (), measure.titleRect.height (),
         measure.pagenameRect.width (), measure.pagenameRect.height (),
         measure.pagenumRect.width (), measure.pagenumRect.height ());*/

   // doesn't seem to handle this changing, so if we don't know the size, use
   // a large number for now
//    qDebug () << "sizeHint" << measure.size.isValid () << measure.size;
   return measure.size.isValid () ? measure.size : QSize (147, 226);
//    return measure.size;
   }


bool Desktopdelegate::editorEvent (QEvent * event, QAbstractItemModel *model,
         const QStyleOptionViewItem &option, const QModelIndex &index)
   {
   QMouseEvent *mouse = (QMouseEvent *)event;

   model = model;
   if (event->type () == QEvent::MouseButtonPress
      || event->type () == QEvent::MouseButtonDblClick)
      {
      _hit = QPersistentModelIndex (index);
      _hitwhich = find_which (index, option, mouse->pos ());
//       printf ("hit = %d\n", _hitwhich);

      // handle auto-repeat
      if (_hitwhich == Point_left ||
         _hitwhich == Point_right)
         {
         _userbusy = true;
         _timer->start( AUTO_REPEAT_DELAY, TRUE);
         }
      if (_hitwhich == Point_other
         && event->type () == QEvent::MouseButtonDblClick)
         emit itemDoubleClicked (index);
      else
         {
         emit itemClicked (index, _hitwhich);
         _userbusy = true;
         }
      if (_hitwhich)
         return false;
      }
   else if (event->type () == QEvent::MouseButtonRelease)
      {
      QMouseEvent *mouse = (QMouseEvent *)event;

      if (mouse->button() == Qt::LeftButton
         && _hit != QModelIndex ()
         && _hitwhich != Point_other)
         {
         _timer->stop();
         }
      _userbusy = false;

      /* preview the item - if the user clicked on an arrow, wait a bit
         to allow them to click again, else just preview now */
      if (_hit.isValid () && _hit != QModelIndex ())
         emit itemPreview (_hit, Point_other, _hitwhich == Point_other);
      }
   return false;
   }


/*********************************** Editing ******************************/

QWidget *Desktopdelegate::createEditor (QWidget *parent,
         const QStyleOptionViewItem &option, const QModelIndex &index) const
   {
   printf ("createEditor\n");
   measure_info measure;

   measureItem (option, index, measure, 0);
   Desktopeditor *editor = new Desktopeditor (parent);
   connect (editor, SIGNAL (myEditingFinished (Desktopeditor *, bool)),
         this, SLOT (slotEditingFinished (Desktopeditor *, bool)));

//    editor->show ();
//    editor->setMinimum(0);
//    editor->setMaximum(100);
   return editor;
   }


void Desktopdelegate::setEditorData(QWidget *edit,
                                    const QModelIndex &index) const
   {
   QString value = index.model()->data(index, _edit_role).toString();
   Desktopeditor *editor = static_cast<Desktopeditor *>(edit);

   editor->setText (value);
   }


void Desktopdelegate::setModelData(QWidget *edit, QAbstractItemModel *mod,
                                    const QModelIndex &index) const
   {
   Desktopeditor *editor = static_cast<Desktopeditor *>(edit);
   Desktopmodel *model = (Desktopmodel *)mod;
   QString text = editor->text ();

   model->setData (index, text, _edit_role);
   }


void Desktopdelegate::updateEditorGeometry(QWidget *edit,
     const QStyleOptionViewItem &option, const QModelIndex &index) const
   {
   Desktopeditor *editor = static_cast<Desktopeditor *>(edit);
   QPoint offset = QPoint (option.rect.x (), option.rect.y ());
   measure_info measure;

   measureItem (option, index, measure, &offset);
   editor->setGeometry (_edit_page ? measure.pagenameRect : measure.titleRect);
   }


void Desktopdelegate::slotEditingFinished (Desktopeditor *edit, bool save)
   {
   if (save)
      emit commitData (edit);
   emit closeEditor (edit);
   }


void Desktopdelegate::setEditPageHint (bool edit_page)
   {
   _edit_page = edit_page;
   _edit_role = _edit_page ? Desktopmodel::Role_pagename : (Desktopmodel::e_role)Qt::EditRole;
   }


/*!
    Internal slot used for auto repeat.
*/
void Desktopdelegate::autoRepeatTimeout()
   {
   qDebug () << "autoRepeatTimeout";
   emit itemClicked (_hit, _hitwhich);
   _timer->start (AUTO_REPEAT_PERIOD, TRUE);
   }


enum Desktopdelegate::e_point Desktopdelegate::find_which (const QModelIndex &index,
         const QStyleOptionViewItem &option, const QPoint &pos)
   {
   measure_info measure;
   enum e_point which;

   QPoint offset = QPoint (option.rect.x (), option.rect.y ());
   measureItem (option, index, measure, &offset);

//   printf ("%d,%d in ", point.x (), point.y ());
//   printf ("%d,%d - %d,%d\n", rect.x (), rect.y (), rect.width (), rect.height ());

   which = Point_other;
   if (measure.rect.contains (pos))
      {
      QRect ctrl = measure.pagenumRect;

      if (!ctrl.contains (pos))
         ;
      else if (pos.x () < ctrl.left () + pleft->width ())
         which = Point_left;
      else if (pos.x () >= ctrl.right () - pright->width ())
         which = Point_right;
      else
         which = Point_page;
      }
   return which;
   }


bool Desktopdelegate::containsPoint (const QStyleOptionViewItem &option,
         const QModelIndex &index, const QPoint &point) const
   {
   measure_info measure;

   QPoint offset = QPoint (option.rect.x (), option.rect.y ());
   measureItem (option, index, measure, &offset);
   return measure.titleRect.contains (point)
      || measure.pagenameRect.contains (point)
      || measure.pagenumRect.contains (point)
      || measure.pixmapRect.contains (point);
   }


/************** the editor *****************/

Desktopeditor::Desktopeditor (QWidget *parent)
      : QLineEdit (parent)
   {
   setFocusPolicy (Qt::StrongFocus);
   connect (this, SIGNAL (editingFinished ()),
         this, SLOT (slotEditingFinished ()));
   }


Desktopeditor::~Desktopeditor ()
   {
   }


void Desktopeditor::slotEditingFinished (void)
   {
   emit myEditingFinished (this, true);
   }


void Desktopeditor::keyPressEvent (QKeyEvent * event)
   {
   if (event->key () == Qt::Key_Escape)
       emit myEditingFinished (this, false);
   else
      QLineEdit::keyPressEvent (event);
   }
