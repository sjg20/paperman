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

#include <assert.h>

#include "qapplication.h"
#include "qcursor.h"
#include "qinputdialog.h"
#include "qlabel.h"
#include "qmessagebox.h"
#include "qpopupmenu.h"
#include "qtimer.h"

#include "err.h"

#include "desktopitem.h"
#include "desktopviewer.h"
#include "desk.h"
#include "maxview.h"
#include "op.h"



#define AUTO_REPEAT_DELAY  300
#define AUTO_REPEAT_PERIOD 100

static QPixmap *unknown = 0;
static QPixmap *no_access = 0;


Desktopviewer::Desktopviewer(QWidget *parent)
      : QIconView (parent)
   {
   _timer = new QTimer ();
        connect (_timer, SIGNAL(timeout()), this, SLOT(autoRepeatTimeout()));
        connect (this, SIGNAL (doubleClicked (QIconViewItem *)),
                 this, SLOT (open (QIconViewItem *)));
        connect (this, SIGNAL (returnPressed (QIconViewItem *)),
                 this, SLOT (open (QIconViewItem *)));
   _desk = 0;
   _updateTimer = new QTimer (this);
   _debug_level = 0;
   _op = 0;
   _subdirs = false;
   _forceVisible = QString::null;
   setSelectionMode (QIconView::Extended);
   connect (_updateTimer, SIGNAL (timeout()), this, SLOT (nextUpdate ()));
   connect (this, SIGNAL (signalStackItem (Desktopitem *, Desktopitem *)),
               this, SLOT (stackItem (Desktopitem *, Desktopitem *)));
   connect (this, SIGNAL (signalStackItems (Desktopitem *, QPtrList<Desktopitem> &)),
               this, SLOT (stackItems (Desktopitem *, QPtrList<Desktopitem> &)));
   connect (this, SIGNAL (itemRenamed ( QIconViewItem * , const QString &  )),
               this, SLOT (doRename (QIconViewItem * , const QString &  )));
   connect (qApp, SIGNAL (aboutToQuit ()),
               this, SLOT (aboutToQuit ()));
   connect (this, SIGNAL (signalItemMoved (Desktopitem *)),
               this, SLOT (itemMoved (Desktopitem *)));
   if (!unknown)
      {
      no_access = new QPixmap ("images/no_access.xpm");
      unknown = new QPixmap ("images/unknown.xpm");
      assert (no_access);
      assert (unknown);
      }
   }


Desktopviewer::~Desktopviewer()
   {
   }


void Desktopviewer::setDebugLevel (int level)
   {
   _debug_level = level;
   }


void Desktopviewer::aboutToQuit (void)
   {
   if (_desk)
      {
      delete _desk;
      _desk = 0;
      }
   }


void Desktopviewer::itemMoved (Desktopitem *item)
   {
   file_info *f = (file_info *)item->userData ();

   QPoint pos = item->getPos ();
//   printf ("item moved %p: %d, %d; %d, %d\n", item, item->x (), item->y (),
//       pos.x (), pos.y ());

   f->pos = QPoint (pos.x (), pos.y ());
   }


void Desktopviewer::keyPressEvent (QKeyEvent * e)
   {
   int ctrl = e->state() & ControlButton;
   int shift = e->state() & ShiftButton;
   Desktopitem *item;

   // find first selected item
   for (item = (Desktopitem *)firstItem(); item; item = (Desktopitem *)item->nextItem())
      if (item->isSelected ())
         break;

//   printf ("key\n");
   switch (e->key ())
      {
      case Key_Delete :
         deleteStack ();
         break;

      case Key_G :
         if (ctrl)
            stackPages ();
         break;

      case Key_L :
         if (ctrl && item)
            locateFolder ((Desktopitem *)item);
         break;

      case Key_I :
         if (ctrl && shift && item)
            duplicatePage ((Desktopitem *)item);
         else if (ctrl && item)
            unstackPage ((Desktopitem *)item);
         break;

      case Key_U :
         if (ctrl && item)
            unstack ((Desktopitem *)item);
         break;

      case Key_R : // not implemented yet
//         if (ctrl && item)
//            renamePage ((Desktopitem *)item);
         break;

      case Key_D :
         if (shift && ctrl && item)
            duplicateMax ((Desktopitem *)item);
         else if (ctrl && item)
            duplicate ((Desktopitem *)item);
         break;

      case Key_T :
         if (ctrl && item)
            duplicateTiff ((Desktopitem *)item);
         break;

      default :
         QIconView::keyPressEvent (e);
         break;
      }
   }


void Desktopviewer::contentsContextMenuEvent ( QContextMenuEvent *e )
   {
   QPoint pos = QPoint (e->x (), e->y ());

   _contextItem = (Desktopitem *)findItem (pos);

   QPopupMenu* contextMenu = new QPopupMenu( this );
   Q_CHECK_PTR( contextMenu );
   QLabel *caption = new QLabel( "<font color=darkblue><u><b>"
       "Stack</b></u></font>", this );
   caption->setAlignment( Qt::AlignCenter );
   contextMenu->insertItem( caption );
   contextMenu->insertItem( "&Locate folder",  this, SLOT(locateFolder()), CTRL+Key_L );
   contextMenu->insertItem( "&Stack",  this, SLOT(stackPages()), CTRL+Key_G );
   contextMenu->insertItem( "Unstack &page",  this, SLOT(unstackPage()), CTRL+Key_I );
   contextMenu->insertItem( "&Unstack all", this, SLOT(unstack()), CTRL+Key_U );
   contextMenu->insertItem( "&Duplicate", this, SLOT(duplicate()), CTRL+Key_D );
   contextMenu->insertItem( "&Rename stack", this, SLOT(renameStack()), Key_F2 );
   contextMenu->insertItem( "R&ename page", this, SLOT(renamePage()), CTRL+Key_R );
   QPopupMenu *submenu = new QPopupMenu( this );
   Q_CHECK_PTR( submenu );
   submenu->insertItem( "P&age", this, SLOT(duplicatePage()), CTRL+SHIFT+Key_I );
   submenu->insertItem( "as &Max", this, SLOT(duplicateMax()), CTRL+SHIFT+Key_D );
   submenu->insertItem( "as &PDF", this, SLOT(duplicatePdf()) );
   submenu->insertItem( "as &Tiff", this, SLOT(duplicateTiff()), CTRL+Key_T );
   contextMenu->insertItem( "&Duplicate...", submenu );
   contextMenu->insertItem( "De&lete stack", this, SLOT(deleteStack()), Key_Delete );
   contextMenu->exec( QCursor::pos() );
   delete contextMenu;
   }


err_info *Desktopviewer::addPaperstack (Paperstack *stack, file_info **fp)
   {
   file_info *f;

   CALL (_desk->addPaperstack (stack, &f));
//   buildItem (0, f);
   *fp = f;
   return NULL;
   }


err_info *Desktopviewer::confirmPaperstack (Desk *maxdesk, file_info *f)
   {
   // flush the item
   CALL (maxdesk->flushItem (f));

   // if we are sending to the same maxdesk, then update the view
   if (maxdesk == _desk)
      buildItem (0, f);

   // if a different maxdesk, but the same directory, update the view
   else if (maxdesk->getDir () == _desk->getDir ())
      {
      // find the filename in the new maxdesk
      file_info *newf = _desk->findFile (f->filename);
      delete maxdesk;
      if (newf)
         {
         Desktopitem *item = itemFind (newf);

         _desk->rescanFile (newf);
         if (item)
            item->repaint ();
         buildItem (item, newf);
         }
      }

   // otherwise destroy the old maxdesk
   else
      delete maxdesk;
   return NULL;
   }


err_info *Desktopviewer::cancelPaperstack (Desk *maxdesk, file_info *f)
   {
   // if this is still the current maxdesk, them remove the file
   if (maxdesk == _desk)
      return _desk->remove (f);

   // else don't bother, but destroy the old maxdesk
   else
      delete maxdesk;
   return NULL;
   }


bool Desktopviewer::checkerr (err_info *err)
   {
   if (err)
      QMessageBox::warning (0, "Maxview", err->errstr);
   return err != NULL;
   }


void Desktopviewer::stackItem (Desktopitem *dest, Desktopitem *src)
   {
   file_info *fdest, *fsrc;

   fdest = (file_info *)dest->userData ();
   fsrc = (file_info *)src->userData ();

   if (dest != src && !checkerr (_desk->stackItem (fdest, fsrc)))
      {
      // delete the source page
      if (!checkerr (_desk->remove (fsrc)))
         delete src;
      buildItem (dest, fdest);
      }
   }


void Desktopviewer::stackItems (Desktopitem *dest, QPtrList<Desktopitem> &items)
   {
   Desktopitem *src;
   file_info *fdest, *fsrc;

//printf ("stackItems %d\n", items.count ());
   // do nothing if dropped onto one of the selected items
   for (src = items.first (); src; src = items.next ())
      if (dest == src)
         {
//         printf ("ignore\n");
         return;
         }

   fdest = (file_info *)dest->userData ();
   for (src = items.first (); src; src = items.next ())
      {
//      printf ("   %p %p\n", dest, src);
      if (dest == src)
         continue;
      fsrc = (file_info *)src->userData ();

      if (!checkerr (_desk->stackItem (fdest, fsrc)))
         {
         // delete the source page
         if (!checkerr (_desk->remove (fsrc)))
            delete src;
         }
      }
   buildItem (dest, fdest);
   }


void Desktopviewer::stackPages (void)
   {
   file_info *fdest;
   Desktopitem *master = 0, *src, *next;
   err_info *err = NULL;

   for (src = (Desktopitem *)firstItem(); !err && src; src = next)
      {
      next = (Desktopitem *)src->nextItem();
      if (!src->isSelected ())
         continue;
      if (!master)
         master = src;
      else if (master != src)
         {
         fdest = (file_info *)master->userData ();
         file_info *fsrc = (file_info *)src->userData ();

         err = _desk->stackItem (fdest, fsrc);
         if (!err)
            {
            err = _desk->remove (fsrc);
            if (!err)
               {
               delete src;
               src = (Desktopitem *)master->nextItem ();
               }
            }
         }
      }
   checkerr (err);
   if (master)
      buildItem (master, (file_info *)master->userData ());
   }


void Desktopviewer::doUnstackPage (Desktopitem *item, bool remove)
   {
   file_info *f = (file_info *)item->userData ();
   file_info *fnew;

   if (f->pagecount > 1
      && !checkerr (_desk->unstackPage (f, f->pagenum, remove, &fnew)))
      {
      buildItem (0, fnew);
      if (f->pagenum == f->pagecount)
         f->pagenum--;
      buildItem (item, f);
      }
   }


void Desktopviewer::unstackPage (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   doUnstackPage (item, true);
   }


void Desktopviewer::duplicatePage (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   doUnstackPage (item, false);
   }


void Desktopviewer::unstack (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();
   file_info *fnew;
   err_info *err = NULL;

   while (f->pagecount > 1)
      {
      err = _desk->unstackPage (f, 1, true, &fnew);
      if (err)
         break;
      buildItem (0, fnew);
      }
#if 0 // we leave one page on the original stack
   if (!err)
      {
      // delete the source page
      err = _desk->remove (f);
      if (!err)
         delete item;
      }
#else
   buildItem (item, f);
#endif
   checkerr (err);
   }


void Desktopviewer::duplicate (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();
   file_info *fnew;

   if (!checkerr (_desk->duplicate (f, &fnew)))
      buildItem (0, fnew);
   }


void Desktopviewer::doRename (QIconViewItem *it, const QString &name)
   {
   Desktopitem *item = (Desktopitem *)it;
   file_info *f = (file_info *)item->userData ();
   QString newname = name;

//   if (name == item->text ())
//      return;

//   printf ("renamed to '%s'\n", name.latin1 ());
   if (f->filename.right (4) == ".max")
      newname += ".max";

   if (!checkerr (_desk->rename (f, newname)))
      {
      // remove the .max extension
      newname = f->filename;
      if (newname.right (4) == ".max")
         newname.truncate (newname.length () - 4);
      if (f->pagecount == 1)
         {
         item->setPageTitle (newname);
         item->setText ("");
         }
      else
         item->setText (newname);
      }
   }


void Desktopviewer::renameStack (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   item->rename ();
   }


void Desktopviewer::renamePage (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   printf ("renamePage\n");
//   _contextItem;
   }


void Desktopviewer::duplicatePdf (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();
   file_info *fnew;
   Operation op ("Convert to .pdf", f->pagecount, this);

   if (!checkerr (_desk->duplicatePdf (f, &fnew, op)))
      buildItem (0, fnew);
   }


void Desktopviewer::duplicateMax (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();
   file_info *fnew;
   Operation op ("Convert to .max", f->pagecount, this);

   if (!checkerr (_desk->duplicateMax (f, &fnew, op)))
      buildItem (0, fnew);
   }


void Desktopviewer::duplicateTiff (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();
   file_info *fnew;
   Operation op ("Convert to .tiff", f->pagecount, this);

   if (!checkerr (_desk->duplicateTiff (f, &fnew, op)))
      buildItem (0, fnew);
   }


void Desktopviewer::deleteStack (void)
   {
//   file_info *f = (file_info *)_contextItem->userData ();
   file_info *f;
   Desktopitem *src, *next;
   err_info *err = NULL;

   for (src = (Desktopitem *)firstItem(); !err && src; src = next)
      {
      next = (Desktopitem *)src->nextItem();
      if (!src->isSelected ())
         continue;
      f = (file_info *)src->userData ();
      printf ("remove %s\n", f->filename.latin1 ());
      err = _desk->remove (f);
      if (!err)
         delete src;
      }
   // delete the source page
//   err = _desk->remove (f);
//   if (!err)
//      delete _contextItem;
   }


void Desktopviewer::moveSelectedToDir (QString &dir)
   {
   file_info *f;
   Desktopitem *src, *next;
   err_info *err = NULL;

   for (src = (Desktopitem *)firstItem(); !err && src; src = next)
      {
      next = (Desktopitem *)src->nextItem();
      if (!src->isSelected ())
         continue;
      f = (file_info *)src->userData ();
      err = _desk->move (f, dir);
      if (!err)
         delete src;
      }
   }


void Desktopviewer::moveDir (QString &src, QString &dst)
{
   err_complain (_desk->moveDir (src, dst));
}


QDragObject *Desktopviewer::dragObject()
{
    if ( !currentItem() )
        return 0;

    QPoint orig = viewportToContents( viewport()->mapFromGlobal( QCursor::pos() ) );
    DesktopitemDrag *drag = new DesktopitemDrag( viewport() );
//    drag->setPixmap( *currentItem()->pixmap(),
//                     QPoint( currentItem()->pixmapRect().width() / 2, currentItem()->pixmapRect().height() / 2 ) );
    for ( Desktopitem *item = (Desktopitem*)firstItem(); item;
          item = (Desktopitem*)item->nextItem() ) {
        if ( item->isSelected() ) {
            QIconDragItem id;
            char str [20];

            sprintf (str, "%p", item);
//            printf ("str=%s\n", str);
//            str.setNum ((int)item);

//    QByteArray ba( str.length() + 1 );
//    memcpy( ba.data(), str.latin1(), ba.size() );
    QByteArray ba( strlen (str) + 1 );
    memcpy( ba.data(), str, ba.size() );

            id.setData( ba );
            drag->append( id,
                          QRect( item->pixmapRect( false ).x() - orig.x(),
                                 item->pixmapRect( false ).y() - orig.y(),
                                 item->pixmapRect().width(), item->pixmapRect().height() ),
                          QRect( item->textRect( false ).x() - orig.x(),
                                 item->textRect( false ).y() - orig.y(),
                                 item->textRect().width(), item->textRect().height() ),
                          item );
        }
    }

    return drag;
}


/*!
    Internal slot used for auto repeat.
*/
void Desktopviewer::autoRepeatTimeout()
   {
        emit itemClicked (_hit, _hitwhich);
   _timer->start (AUTO_REPEAT_PERIOD, true);
   }


Desktopitem *Desktopviewer::buildItem (Desktopitem *item, file_info *f)
   {
   QPixmap *pixmap;
   QString name = f->filename;
   err_info *err;
   QString pagename;
   int bpp;

   err = _desk->getPreviewPixmap (f, f->pagenum, pagename, &pixmap, bpp);
   QPixmap &pm = err == 0 ? *pixmap : err->errnum == ERR_cannot_open_file1 ? *no_access : *unknown;
//   if (err)
//      printf ("err=%s\n", err->errstr);
   if (name.right (4) == ".max")
      name.truncate (name.length () - 4);
   QString pagestr = QString ("%1 of %2").arg (f->pagenum + 1).arg (f->pagecount);

   if (f->pagecount == 1)
      pagename = name;

   if (item)
      item->changeData (name, pagename, pagestr, pm, f->pagecount != 1);
   else
      {
//      printf ("build: pos=%d, %d\n", f->pos.x (), f->pos.y ());
      item = new Desktopitem (this, name, pagename,
            pagestr, f->maxsize, f, pm, f->pagecount != 1, f->pos);
      }
   return item;
   }


void Desktopviewer::progress (const char *fmt, ...)
   {
   char str [256];
   va_list ptr;

   va_start (ptr, fmt);
   vsprintf (str, fmt, ptr);
   va_end (ptr);
   emit newContents (str);
   }


void Desktopviewer::nextUpdate (void)
   {
   static bool working = false;

//   printf ("nextUpdate\n");
   if (!_desk || !_upto)
      {
      delete _op;
      _op = 0;
      _timer->stop ();
      emit updateDone ();
      return;
      }

   // build the next item
//   printf ("build %s\n", _upto->filename.latin1 ());
   if (working)
      printf ("   - busy\n");
   else
      {
      working = true;
      progress ("Scanning %s...", _upto->filename.latin1 ());
      _op->setProgress (_addCount++);
      buildItem (0, _upto);

      // and set up to do the next one
      _upto = _stopUpdate ? 0 : _desk->next ();
      working = false;
      }

   if (_upto)
      _updateTimer->start (0, true);
   else
      {
      // operation complete
      progress ("");
      delete _op;
      _op = 0;

      if (_forceVisible)
         {
//         printf ("complete\n");
         QIconViewItem * item;
         Desktopitem *it;

         for (item = firstItem (); item; item = item->nextItem ())
            {
            it = (Desktopitem *)item;
            file_info *f = (file_info *)it->userData ();

//            printf ("%s %s\n", _forceVisible.latin1 (), f->filename.latin1 ());
            if (_forceVisible == f->filename)
               ensureItemVisible (item);
            }
         _forceVisible = QString::null;
         }
      emit updateDone ();
      }
   }


void Desktopviewer::stopUpdate (void)
   {
   // cancel an update
   _stopUpdate = true;
   }


void Desktopviewer::refresh (QString dirPath, bool do_readDesk,
         const QString &match, bool subdirs, Operation *op)
   {
//   printf ("refresh %s, %s\n", dirPath.latin1 (), match.latin1 ());

   // dispose of the old maxview if not claimed by a paper stack
   if (_desk && _desk->getAllowDispose ())
      delete _desk;
   _desk = new Desk (dirPath + "/", do_readDesk);
   _desk->setDebugLevel (_debug_level);

   // add files that are not in the maxdesk.ini file
   _desk->addMatches (dirPath, match, subdirs, op);

   _subdirs = subdirs;
   clear ();

   // work out which item to add first
   _upto = _desk->first ();

   if (_op)
      delete _op;
   _op = new Operation ("Scanning Directory", _desk->fileCount (), this);
   _addCount = 0;

   // start a timer to fire when the machine is idle
   _stopUpdate = false;
   _updateTimer->start (0, true);
#if 0
   for (file_info *f = _desk->first (); f; f = _desk->next ())
      {
//      printf ("build %s\n", f->filename.latin1 ());
      buildItem (0, f);
      }
#endif
   }


void Desktopviewer::repaintItem( Desktopitem *item )
{
    if ( !item ) //|| item->dirty )
        return;

    QRect rect = item->itemRect ();

    rect.moveBy (item->x () - 1, item->y () - 1);
    rect.setWidth (rect.width () + 2);
    rect.setHeight (rect.height () + 2);

//printf ("repaint %d,%d %d,%d\n", rect.x (), rect.y (), rect.width (), rect.height ());
    if ( QRect( contentsX(), contentsY(), visibleWidth(), visibleHeight() ).
//       intersects( QRect( item->x() - 1, item->y() - 1, item->width() + 2, item->height() + 2 ) ) )
         intersects( rect ))
        repaintContents( rect.x (), rect.y (), rect.width (), rect.height (), false);
//      item->x() - 1, item->y() - 1, item->width() + 2, item->height() + 2, false );
}


Desktopitem *Desktopviewer::itemFind (file_info *file)
   {
   QIconViewItem *item;
   Desktopitem *it;

   for (item = firstItem (); item; item = item->nextItem ())
      {
      it = (Desktopitem *)item;
      if (file == (file_info *)it->userData ())
         return it;
      }
   return 0;
   }


Desktopitem *Desktopviewer::itemFind (const QPoint &pos, int &which)
   {
   QIconViewItem * item;
   Desktopitem *it;
/*
   QPoint pos;
   QPoint orig;

   orig = mapToGlobal (QPoint (0, 0));
   pos.setX (opos.x () - orig.x () + contentsX ());
   pos.setY (opos.y () - orig.y () + contentsY ());
*/
//   printf ("x = %d, y = %d: %d, %d: %d, %d\n", opos.x (), opos.y (), pos.x (), pos.y (),
//         _contents->contentsX (), _contents->contentsY ());
//   printf ("clicked %p\n", item);
   for (item = firstItem (); item; item = item->nextItem ())
      {
      it = (Desktopitem *)item;
      if (it->containsFull (pos, which))
         return it;
      }
   return 0;
   }


void Desktopviewer::itemClicked (Desktopitem *item, int which)
   {
   file_info *f;
   bool update;

   if (item)
      {
      update = false;
      f = (file_info *)item->userData ();
      switch (which)
         {
         case Desktopitem::Point_left : // left
            if (f->pagenum > 0)
               {
               f->pagenum--;
               update = true;
               }
            break;

         case Desktopitem::Point_right : // right
            if (f->pagenum < f->pagecount - 1)
               {
               f->pagenum++;
               update = true;
               }
            break;

         case Desktopitem::Point_page : // page button
            int num;
            bool ok;

            // don't do this for single pages
            if (f->pagecount < 2)
               break;
            num = QInputDialog::getInteger ("Select page number", "Page",
                       f->pagenum + 1, 1, f->pagecount, 1, &ok, this);
            if (ok)
               {
               f->pagenum = num - 1;
               update = true;
               }
            break;

         default :
            if (f->err)
               emit newContents (f->err->errstr);
            else
               {
               char numstr [20];
               QString str;

               util_bytes_to_user (numstr, f->size);
               str = QString ("%1 page%2, %3").arg (f->pagecount).arg (f->pagecount > 1 ? "s" : "").arg (numstr);
               if (_subdirs)
                  str += ", at " + f->pathname;
               emit newContents (str);
               }
//            emit itemSelected (item);
            break;
         }
      if (update)
         {
         item = buildItem (item, f);
         item->repaint ();
         emit itemSelected (item);
         }
      }
   }


void Desktopviewer::contentsMousePressEvent( QMouseEvent *e )
   {
//   printf ("press\n");
   if (e->button() == LeftButton)
      {
   //   printf ("%d %d\n", e->pos ().x (), e->pos ().y ());
      _hit = itemFind (e->pos (), _hitwhich);
      if (_hit)
         {                              // mouse press on button
        emit itemClicked (_hit, _hitwhich);
        if (_hitwhich == Desktopitem::Point_left ||
           _hitwhich == Desktopitem::Point_right)
            _timer->start( AUTO_REPEAT_DELAY, true );
         if (_hitwhich)
            return;
         }
      else if (!_hit)
         {
//         printf ("clear\n");
         clearSelection ();
         }
      }
   QIconView::contentsMousePressEvent (e);
   }


void Desktopviewer::contentsMouseReleaseEvent( QMouseEvent *e)
{
   if (e->button() == LeftButton && _hit && _hitwhich)
      _timer->stop();

   else
      QIconView::contentsMouseReleaseEvent (e);
   }


void Desktopviewer::open(QIconViewItem *it)
   {
   Desktopitem *item = (Desktopitem *)it;
   file_info *f;

   f = (file_info *)item->userData ();
   emit showPage (_desk, f);
   }


/** arrange items by given type (t_arrangeBy...) */

void Desktopviewer::arrangeBy (int type)
   {
   Desktopitem *item;
   file_info *f;

   //   printf ("arrange %d\n", type);
   if (_desk)
      {
      _desk->arrangeBy (type);

      // work through the items, putting each into the right place
      for (item = (Desktopitem *)firstItem(); item;
           item = (Desktopitem *)item->nextItem ())
         {
         f = (file_info *)item->userData ();
         item->move (f->pos.x (), f->pos.y ());
         }

      repaintContents (0, 0, 5000, 5000, true);
      }
   }


err_info *Desktopviewer::operation (Desk::operation_t type, int ival)
   {
   file_info *f;
   Desktopitem *src, *next;
   err_info *err = NULL;

   for (src = (Desktopitem *)firstItem(); !err && src; src = next)
      {
      next = (Desktopitem *)src->nextItem();
      if (!src->isSelected ())
         continue;
      f = (file_info *)src->userData ();
      CALL (_desk->operation (f, -1, type, ival));
//      printf ("done operation\n");
      buildItem (src, f);
      }

   return NULL;
   }


void Desktopviewer::locateFolder (Desktopitem *item)
   {
   if (!item)
      item = _contextItem;

   file_info *f = (file_info *)item->userData ();

   QString dir;

   dir = f->pathname;
   dir.truncate (dir.length () - f->filename.length ());
//   printf ("dir = %s\n", dir.latin1 ());
//   refresh (dir);
   emit selectFolder (dir);

   // once the folder has finished refreshing, we want to ensure that this item is visible
   _forceVisible = f->filename;
   }


void Desktopviewer::getItemData (Desktopitem *item, Desk **maxdeskp, file_info **filep)
   {
   *filep = (file_info *)item->userData ();
   *maxdeskp = _desk;
   }
