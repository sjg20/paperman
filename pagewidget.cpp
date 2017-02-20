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
//

#include <stdio.h>

#include <QDebug>
#include <QImage>
#include <QVBoxLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QMessageBox>
#include <QPaintEvent>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QTimer>

#include "qlabel.h"
#include "qlayout.h"
#include "qpainter.h"
#include "qpixmap.h"
#include "qwidget.h"

#include "err.h"
#include "desktopmodel.h"
#include "mainwidget.h"
#include "desk.h"
#include "file.h"
#include "maxview.h"
#include "ocr.h"
#include "pagedelegate.h"
#include "pagemodel.h"
#include "pagetools.h"
#include "pageview.h"
#include "pagewidget.h"
#include "utils.h"
#include "ui_ocrbar.h"
#include "ui_pageattr.h"

#include "qxmlconfig.h"


/** we use pixmaps to speed up redraw, but don't use one if the size in any dimension is
   greater than this */

#define MAX_PIXMAP_SIZE 2000

/** this is the distance we scroll in response to the mouse wheel */
#define WHEEL_SCROLL_Y  50

/** scale step (up and down) */
#define SCALE_FACTOR 1.125


Pagewidget::Pagewidget (Desktopmodelconv *modelconv, QString base, QWidget *parent)
      : QWidget (parent)
   {
   QSettings qs;

   _smoothing = false;
   _scale = qs.value (base + "preview_scale", 0.25).toDouble ();
//    _view = 0;
   _area = 0;
   _delayed_update = false;
   _subsys = SUBSYS_pixmap;
   _settings_base = base;
   _mode = Mode_none;
   _modelconv = modelconv;
   _scanning = false;
   _model = 0;
   _committing = false;
   _rotate = 0;

//    checkSubsystem (xmlConfig->intValue("DISPLAY_SUBSYSTEM"));
   _tools = new Pagetools (this);
   _delayed_update = false;
   connect (_tools->zoomFit, SIGNAL (clicked ()), this, SLOT (slotZoomFit ()));
   connect (_tools->zoomOrig, SIGNAL (clicked ()), this, SLOT (slotZoomOrig ()));
   connect (_tools->zoomIn, SIGNAL (clicked ()), this, SLOT (slotZoomIn ()));
   connect (_tools->zoomOut, SIGNAL (clicked ()), this, SLOT (slotZoomOut ()));
   connect (_tools->zoomLevel, SIGNAL (returnPressed ()), this, SLOT (slotZoom ()));
   connect (_tools->zoomOut, SIGNAL (clicked ()), this, SLOT (slotZoomOut ()));

   connect (_tools->selectMode, SIGNAL (toggled (bool)), this, SLOT (slotChangeMode (bool)));
   connect (_tools->moveMode, SIGNAL (toggled (bool)), this, SLOT (slotChangeMode (bool)));
   connect (_tools->scanMode, SIGNAL (toggled (bool)), this, SLOT (slotChangeMode (bool)));
   connect (_tools->infoMode, SIGNAL (toggled (bool)), this, SLOT (slotChangeMode (bool)));

   connect (_tools->save, SIGNAL (clicked ()), this, SLOT (slotSave ()));
   connect (_tools->revert, SIGNAL (clicked ()), this, SLOT (slotRevert ()));

   connect (_tools->rleft, SIGNAL (clicked ()), this, SLOT (slotRotateLeft ()));
   connect (_tools->vflip, SIGNAL (clicked ()), this, SLOT (slotRotate180 ()));
   connect (_tools->rright, SIGNAL (clicked ()), this, SLOT (slotRotateRight ()));

   _modified = false;

   _pageview = new Pageview (this);

   _pagemodel = new Pagemodel (this);
   _pageview->setModel (_pagemodel);
   _pagedelegate = new Pagedelegate (this);
   _pageview->setItemDelegate (_pagedelegate);

//    QVBoxLayout *layout = new QVBoxLayout (this);
   _stack = new QStackedWidget (this);
//    layout->addWidget (_stack);

   _textframe = new QFrame (this);
   _pageattr = new Ui_Pageattr ();
   _pageattr->setupUi (_textframe);
   _pageattr->errorMsg->setVisible (false);
   connect (_pageattr->author, SIGNAL (textEdited (const QString&)),
      this, SLOT (slotAnnotChanged ()));
   connect (_pageattr->title, SIGNAL (textEdited (const QString&)),
      this, SLOT (slotAnnotChanged ()));
   connect (_pageattr->keywords, SIGNAL (textEdited (const QString&)),
      this, SLOT (slotAnnotChanged ()));
   connect (_pageattr->notes, SIGNAL (textChanged ()),
      this, SLOT (slotAnnotChanged ()));

   QVBoxLayout *layout = new QVBoxLayout (this);
   _splitter = new QSplitter (this);
   layout->addWidget (_tools);
   layout->addWidget (_stack, 1);
   setLayout (layout);

   _ocr_split = new QSplitter (Qt::Vertical, this);

   _area = new MyScrollArea (this);
   _ocr_bar = new Ui_Ocrbar ();
   QWidget *_ocr_area = new QWidget (this);

   _ocr_edit = new QTextEdit (_ocr_area);
   QFrame *_ocrbar_frame = new QFrame (_ocr_area);
   _ocr_bar->setupUi (_ocrbar_frame);

   connect (_ocr_bar->ocr, SIGNAL (clicked ()), this, SLOT (ocrPage ()));
   connect (_ocr_bar->flipView, SIGNAL (clicked ()),
      this, SLOT (ocrFlipView ()));
   connect (_ocr_bar->clear, SIGNAL (clicked ()), this, SLOT (ocrClear ()));
   connect (_ocr_bar->copy, SIGNAL (clicked ()), this, SLOT (ocrCopy ()));

   connect (_ocr_edit, SIGNAL (textChanged ()),
      this, SLOT (slotAnnotChanged ()));

   QVBoxLayout *lay2 = new QVBoxLayout (_ocr_area);  //w  (was (this)
   //QLayout *lay2 = this->layout ();
   lay2->addWidget (_ocrbar_frame);
   lay2->addWidget (_ocr_edit);
   _ocr_area->setLayout (lay2);

   _splitter->addWidget (_pageview);
   _splitter->addWidget (_ocr_split);
   _splitter->setStretchFactor (0, 1);
   _splitter->setStretchFactor (1, 4);
   _splitter->setStretchFactor (2, 2);

   _ocr_split->addWidget (_area);
   _ocr_split->addWidget (_ocr_area);
   _ocr_split->setStretchFactor (0, 2);
   _ocr_split->setStretchFactor (0, 1);

   _stack->addWidget (_splitter);
   _stack->addWidget (_textframe);

   connect (_pagedelegate, SIGNAL (itemClicked (const QModelIndex &, int)),
         this, SLOT (slotItemClicked (const QModelIndex &, int)));

   connect (_area, SIGNAL (signalNewScale(double, bool)), this, SLOT (slotNewScale (double, bool)));
   connect (_area, SIGNAL (signalPainting()), this, SLOT(slotDelayedUpdate()));


   connect (_pageview, SIGNAL (signalNewScale (int)), this, SLOT (slotNewScale (int)));
//    connect (_pageview, SIGNAL (clicked (const QModelIndex &)),
//       this, SLOT (slotPreviewPage (const QModelIndex &)));
   connect (_pageview, SIGNAL (showInfo (const QModelIndex &)),
      this, SLOT (slotShowInfo (const QModelIndex &)));

   connect (_pagemodel, SIGNAL (pagePartChanged (const QModelIndex &, const QImage &, int)),
      _pageview, SLOT (slotPagePartChanged (const QModelIndex &, const QImage &, int)));

   _timer = new QTimer ();
   _timer->setSingleShot (true);
   connect (_timer, SIGNAL(timeout()), this, SLOT(updateWindow()));

   _returnToDesktop = new QAction ("Return to desktop", this);
   _returnToDesktop->setShortcut (tr ("Escape"));
   addAction (_returnToDesktop);
   updatePagetools ();
   }


Pagewidget::~Pagewidget ()
   {
   delete _pageattr;
   delete _ocr_bar;
   delete _timer;
//    checkSubsystem (-1);
//   delete _page;
   }


void Pagewidget::init (void)
   {
   setMode (Mode_select);
   }


void Pagewidget::closing (void)
   {
   setMode (Mode_none);
//    if (_mode == Mode_none)
//       return;
   }


void Pagewidget::updatePagetools (void)
   {
   bool have_page = _index != QModelIndex ();

   _tools->save->setEnabled (_modified);
   _tools->revert->setEnabled (_modified);

   _tools->zoomFit->setEnabled (have_page);
   _tools->zoomOrig->setEnabled (have_page);
   _tools->zoomIn->setEnabled (have_page);
   _tools->zoomOut->setEnabled (have_page);
   _tools->zoomLevel->setEnabled (have_page);
   _tools->zoomOut->setEnabled (have_page);

//    _tools->rLeft->setEnabled (false);
//    _tools->rRight->setEnabled (false);
//    _tools->hFlip->setEnabled (false);
//    _tools->vFlip->setEnabled (false);

   _pageattr->author->setEnabled (have_page);
   _pageattr->title->setEnabled (have_page);
   _pageattr->keywords->setEnabled (have_page);
   _pageattr->notes->setEnabled (have_page);
   if (!have_page)
      {
      _pageattr->title->clear ();
      _pageattr->author->clear ();
      _pageattr->keywords->clear ();
      _pageattr->notes->blockSignals (true);// because clearing a QTextEdit emits a signal
      _ocr_edit->blockSignals (true);
      _pageattr->notes->clear ();
      _ocr_edit->clear ();
      _pageattr->notes->blockSignals (false);
      _ocr_edit->blockSignals (false);
      }
   }


void Pagewidget::setMode (e_mode mode)
   {
   if (mode == _mode)
      return;

   // save old mode window settings
//    closing ();
   if (_mode != Mode_none)
      {
      QList<int> size;

      QString str = QString ("%1mode%2/").arg (_settings_base).arg (_mode);
      size = _splitter->sizes ();  // must use temporary var for reference
      setSettingsSizes (QString ("%1pagewidget/").arg (str), size);

      size = _ocr_split->sizes ();  // must use temporary var for reference
      setSettingsSizes (QString ("%1pagewidget/ocr/").arg (str), size);

      QSettings qs;
      qs.setValue (str + "preview_scale", _scale);
      qs.setValue (str + "pages_scale", _scale_down);
      qs.setValue (str + "ocr_horiz", _ocr_split->orientation () == Qt::Horizontal);
      }

   emit modeChanging (mode, _mode);

   _mode = mode;
   _prescan_mode = mode;
   if (_mode != Mode_none)
      {
      _pageview->allowMove (mode == Mode_move);
      _pagedelegate->setDisplayOptions (mode == Mode_scan
            ? Pagedelegate::Display_coverage : Pagedelegate::Display_none);
      _stack->setCurrentWidget (mode == Mode_info ? _textframe : _splitter);

      QSettings qs;

      QString str = QString ("%1mode%2/").arg (_settings_base).arg (_mode);
      QList<int> size;
      if (getSettingsSizes (QString ("%1pagewidget/").arg (str), size))
         _splitter->setSizes (size);

      _ocr_split->setOrientation (qs.value (str + "ocr_horiz").toBool ()
         ? Qt::Horizontal : Qt::Vertical);
      if (getSettingsSizes (QString ("%1pagewidget/ocr/").arg (str), size))
         _ocr_split->setSizes (size);

      // set the preview window scale
      double dscale = qs.value (str + "preview_scale", 0.25).toDouble ();

      dscale = qMax (dscale, 0.01);
      dscale = qMin (dscale, 64.0);

      slotNewScale (dscale, false);

      // set the pages window scale
      int scale = qs.value (str + "pages_scale", 24).toInt ();
      scale = qMax (scale, 1);
      scale = qMin (scale, 224);

      slotNewScale (scale);
      }
   }


Pagewidget::e_mode Pagewidget::toolToMode (QObject *tb)
   {
   if (tb == _tools->selectMode)
      return Mode_select;
   else if (tb == _tools->moveMode)
      return Mode_move;
   else if (tb == _tools->scanMode)
      return Mode_scan;
   else if (tb == _tools->infoMode)
      return Mode_info;
   return Mode_select;
   }


QToolButton * Pagewidget::modeToTool (e_mode mode)
   {
   switch (mode)
      {
      case Mode_info : return _tools->infoMode;
      case Mode_move : return _tools->moveMode;
      case Mode_scan : return _tools->scanMode;
      case Mode_select :
      default :
         return _tools->selectMode;
      }
   }


void Pagewidget::slotChangeMode (bool selected)
   {
   e_mode mode = toolToMode (sender());

   if (selected)
      setMode (mode);
   }


#if 0
void Pagewidget::checkSubsystem (int new_subsys)
   {
   if (new_subsys == _subsys)
      return;

   switch (_subsys)
      {
      case SUBSYS_pixmap : // pixmap
         delete _area;
         break;

      case SUBSYS_gview : // QGraphicsScene
         delete _view;
         delete _scene;
         break;
      }

   _subsys = new_subsys;
   switch (_subsys)
      {
      case SUBSYS_pixmap : // pixmap
         {
         _area = new MyScrollArea (this);
         QVBoxLayout *layout = new QVBoxLayout ();
         _tools = new Pagetools (this);
         layout->addWidget (_tools);
         layout->addWidget (_area);
         setLayout (layout);
         _delayed_update = false;
         connect (_area, SIGNAL (signalNewScale(double, bool)), this, SLOT (slotNewScale(double, bool)));
         connect (_area, SIGNAL (signalPainting()), this, SLOT(slotDelayedUpdate()));

         connect (_tools->zoomFit, SIGNAL (clicked ()), this, SLOT (slotZoomFit ()));
         connect (_tools->zoomOrig, SIGNAL (clicked ()), this, SLOT (slotZoomOrig ()));
         connect (_tools->zoomIn, SIGNAL (clicked ()), this, SLOT (slotZoomIn ()));
         connect (_tools->zoomOut, SIGNAL (clicked ()), this, SLOT (slotZoomOut ()));
         connect (_tools->zoomLevel, SIGNAL (returnPressed ()), this, SLOT (slotZoom ()));
         break;
         }

      case SUBSYS_gview : // QGraphicsScene
         _view = new QGraphicsView (this);
         _scene = new QGraphicsScene ();
         _pitem = _scene->addPixmap (_pixmap);
         _view->setScene(_scene);
         _view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
         _view->show ();
         break;
      }
   }
#endif


void Pagewidget::setSmoothing (bool smooth)
   {
   _smoothing = smooth;
   }


void Pagewidget::slotDelayedUpdate (void)
   {
   if (_delayed_update)
      {
//       printf ("handle delayed\n");
      updateViewport (_delayed_update_scale, _delayed_force_smooth);
      }
   }


void Pagewidget::clearViewport (void)
   {
   _too_big = false;
   _pixmap = QPixmap ();
   _image = QImage ();
   _area->setPixmapImage (_pixmap, _image, _too_big, 0);
//    _area->setScale (_scale);
   _area->viewport ()->update ();
   }


static void adjustMatrix (QPainter &p, QSize size, int rotate)
{
   if (rotate)
      {
      QPoint tp;

      // rotate and translate back to the origin
      p.rotate (rotate);
      if (rotate == 270)
         tp = QPoint (-size.width (), 0);
      else if (rotate == 90)
         tp = QPoint (0, -size.height ());
      else // 180
         tp = QPoint (-size.width (), -size.height ());
      p.translate (tp);
      }
}


void Pagewidget::updateViewport (bool updateScale, bool force_smooth, bool delay_smoothing)
   {
   QSize widgetSize;

//    qDebug () << "updateViewport" << updateScale << force_smooth << delay_smoothing;

   // the image is empty at the beginning
   widgetSize = _image.size ();
   if (!widgetSize.width ())
      return;

   widgetSize *= _scale;
   QSize finalSize = widgetSize;

   if (_rotate % 180 == 90)
      finalSize = QSize (widgetSize.height (), widgetSize.width ());

   switch (_subsys)
      {
      case SUBSYS_pixmap :
         QSize areaSize = _area->viewport ()->size ();

//         printf ("too_big = %d\n", _too_big);
//         printf ("   area %d x %d, widget %d x %d\n", areaSize.width (), areaSize.height (),
//               widgetSize.width (), widgetSize.height ());
//         _area->viewport ()->resize (widgetSize);

         _area->verticalScrollBar()->setPageStep(areaSize.height());
         _area->horizontalScrollBar()->setPageStep(areaSize.width());
         _area->verticalScrollBar()->setRange(0, widgetSize.height() - areaSize.height());
         _area->horizontalScrollBar()->setRange(0, widgetSize.width() - areaSize.width());
         break;
      }

   _tools->zoomLevel->setText (QString ("%1%").arg ((int)(_scale * 100)));

   if (!isVisible ())
      {
      _delayed_update = true;
      _delayed_update_scale = updateScale;
      _delayed_force_smooth = force_smooth;
      return;
      }
   _delayed_update = false;

   switch (_subsys)
      {
      case SUBSYS_pixmap :
         {
//         printf ("image size %d, %d\n", _image.width (), _image.height ());

//         printf ("updateScale = %d\n", updateScale);
         if (updateScale)
            {
            _too_big = widgetSize.width () > MAX_PIXMAP_SIZE || widgetSize.height () > MAX_PIXMAP_SIZE;
            //_too_big = true;

            if (!_too_big && _pixmap.size () != finalSize)
               {
//               printf ("pixmap resize %d, %d\n", pixmap_size.width (), pixmap_size.height ());
               _pixmap = QPixmap(finalSize);
               _pixmap.fill ();
               }

            if (!_too_big && !_image.isNull ())
               {
               QImage small = _smoothing || force_smooth
                  ? _image.scaled(widgetSize, Qt::IgnoreAspectRatio,
                                  Qt::SmoothTransformation)
                  : _image.scaledToWidth (widgetSize.width ());

               // create a pixmap of the right scale
               QPainter p (&_pixmap);

               adjustMatrix (p, small.size (), _rotate);
               p.drawImage (0, 0, small);
               }
            }

         _area->setSize (finalSize);
         _area->setPixmapImage (_pixmap, _image, _too_big, _rotate);
         _area->setScale (_scale);
         _area->viewport ()->update ();

         // if not smoothed, remember to do this later
         if (!_smoothing && !force_smooth && !_too_big)
            _timer->start (delay_smoothing ? 200 : 0);
         break;
         }
#if 0
      case SUBSYS_gview :
         _scene->setSceneRect (QRectF (0, 0, _image.width (), _image.height ()));
//      resizeContents (scaledSize.width (), scaledSize.height ());
//      updateContents ();
      //   setCacheMode(QGraphicsView::CacheBackground);
      //   setBackgroundBrush (*_pixmap);

         _pitem->setPixmap (_pixmap);
         _view->resetMatrix ();
         _view->scale (_scale, _scale);
         _scene->invalidate ();
         break;
#endif
      }
   }


void Pagewidget::updateWindow (void)
   {
//printf ("update\n");
   updateViewport (true, true);
//    updateViewport (true, false);
   }


void Pagewidget::slotNewScale (double scale, bool delay_smoothing)
   {
   _scale = scale;
   updateViewport (true, false, delay_smoothing);
   }


void Pagewidget::showPages (const QAbstractItemModel *model, const QModelIndex &index,
         int start, int count, int curpage, bool reset)
   {
   QSize size, scaledSize;

//    qDebug () << "showPages" << model << index;
   _start = start;
   _count = count;
   int pagecount = model->data (index, Desktopmodel::Role_pagecount).toInt ();
   if (_count == -1)
      _count = pagecount;
   if (curpage == -1)
      curpage = model->data (index, Desktopmodel::Role_pagenum).toInt ();

   // if we have changed model or stack, we need a reset
   if (model != _model || index != _index)
      {
      _model = model;
      _index = index;

      /* save the current page if there are any changes
         this may cause the current stack to be changed. For example removing
         blank pages will call slotStackChanged() and thus showPages()
         recursively! So we change the model above to avoid coming in here again
         (this will make slotStackChanged() ignore the change since is is not
         the current stack that is being changed but the old one). So just call
         commit() directly here to avoid the tests at the start of slotSave() */
      if (_modified && !_scanning)
//          slotSave ();
         {
         _committing = true;
         commit ();
         _committing = false;
         }

      reset = true;

      /* if we were scanning into a stack, but have selected a different
         stack, disown the scanning stack */
      if (_scanning)
         {
         revertMode ();
         _pagemodel->disownScanning ();
         _scanning = false;
         }
      }
   if (reset)
      {
      // update the page view
      Desktopmodel *contents = _modelconv->getDesktopmodel (_model);
      QModelIndex sindex = _index;
      _modelconv->indexToSource (model, sindex);
      _pagemodel->reset (contents, sindex, _start, _count);

      // update the textframe
      updateAttr ();

      // unfortunately setting notes emits a signel which makes is think it has changed
//       _modified = false;
      }

   if ((reset || curpage != _pagenum) && curpage < _count)
      {
      QModelIndex ind = _pagemodel->index (curpage, 0, QModelIndex ());
      _pageview->selectPage (ind);
      slotPreviewPage (ind);
      }
   updatePagetools ();
//    qDebug () << "showPages done";
   }


void Pagewidget::slotPreviewPage (const QModelIndex &ind)
   {
   QSize scaledSize, size;
   int bpp;
   err_info *err = 0;

   Q_ASSERT (_index.isValid ());
   Desktopmodel *contents;
   QModelIndex sindex = _index;

   _pagenum = _start + ind.row ();
   contents = _modelconv->getDesktopmodel (_model);
   _modelconv->indexToSource (_model, sindex);
   err = contents->getImage (sindex, _pagenum, false,
                  _image, size, scaledSize, bpp);
   if (!err)
      updateViewport (true, false, false);
   else
      emit newContents (err->errstr);

   updateOcrText ();
   }


void Pagewidget::setPagesize (QSize size)
   {
   _pagemodel->setPagesize (size);
   _pagedelegate->setPagesize (size);

   const QStyleOptionViewItem option = _pageview->getViewOptions ();
   size += _pagedelegate->getSpacing (option);
   _pageview->setGridSize (size);
   }


void Pagewidget::slotNewScale (int new_scale)
   {
//    qDebug () << "scale" << new_scale;
   _scale_down = new_scale;
   _pageview->setScale (new_scale);
   _pagemodel->setScale (new_scale);
   setPagesize (QSize (2400, 3400) / new_scale);
   }


void Pagewidget::redisplay (void)
   {
//    showPage (0, _index);
   }


void Pagewidget::pageLeft (void)
   {
   if (_index.isValid () && _pagenum > 0)
      {
      _pagenum--;
//       showPage (0, _index, true);
      }
   }


void Pagewidget::pageRight (void)
   {
   if (!_index.isValid ())
      return;

   int pagecount = _model->data (_index, Desktopmodel::Role_pagecount).toInt ();
   if (_pagenum < pagecount - 1)
      {
      _pagenum++;
//       showPage (0, _index, true);
      }
   }


//void Pagewidget::drawContents (QPainter * p, int clipx, int clipy, int clipw, int cliph )
//   {
//   if (_pixmap)
//      p->drawPixmap (0, 0, *_pixmap);
//   }


err_info *Pagewidget::operation (Desk::operation_t type, int ival)
   {
#if 0 //p
   if (_file && _desk)
      {
      CALL (_desk->operation (_file, _pagenum, type, ival));
      // should hold the new image in a buffer and not update until user exits page
      }
#endif
   return NULL;
   }


bool Pagewidget::isVisible (void)
   {
   switch (_subsys)
      {
      case SUBSYS_pixmap : // pixmap
         QSize r = _area->viewport ()->geometry ().size ();

         return r.width () > 0 && r.height () > 0;
         break;
      }
   return true;
   }


void Pagewidget::slotZoomFit (void)
   {
   switch (_subsys)
      {
      case SUBSYS_pixmap :
         QSize areaSize = _area->viewport ()->size ();
         QSize imageSize = _image.size ();

         if (imageSize.width () > 0)
            {
            double xscale = (double)areaSize.width () / imageSize.width ();
            double yscale = (double)areaSize.height () / imageSize.height ();

            double scale = qMin (xscale, yscale);

//            printf ("new scale = %1.3lf\n", scale);
            slotNewScale (scale, false);
            }
         break;
      }
   }


void Pagewidget::slotZoomOrig (void)
   {
   slotNewScale (0.25, false);
   }


void Pagewidget::slotZoomIn (void)
   {
   slotNewScale (_scale * SCALE_FACTOR, true);
   }


void Pagewidget::slotZoomOut (void)
   {
   slotNewScale (_scale / SCALE_FACTOR, true);
   }


void Pagewidget::slotZoom (void)
   {
   bool ok;

   double scale = _tools->zoomLevel->text ().toInt (&ok);
   if (ok)
      slotNewScale (((double)scale + .05) / 100, false);
      slotNewScale (((double)scale + .05) / 100, false);
   }


/********************************** scanning ********************************/

void Pagewidget::beginningScan (const QModelIndex &ind)
   {
   e_mode mode = _mode;

   _tools->scanMode->setChecked (true);
   _prescan_mode = mode;
   showPages (ind.model (), ind, 0, -1, -1);
   _scanning = true;
   _pagemodel->beginningScan ();
   }


void Pagewidget::endingScan (bool cancel)
   {
   // are we still previewing this scan?
   if (_scanning)
      {
      // if we cancelled, clear the model
      if (cancel)
         _pagemodel->clear ();

      // otherwise let the user see the blank pages and decide what to do
      }

   // if not, confirm the page changes (remove blank pages)
   else
      commit ();
//       slotSave ();
   }


void Pagewidget::revertMode (void)
   {
   if (_prescan_mode != Mode_scan && _prescan_mode != _mode)
      {
      QToolButton *tb = modeToTool (_prescan_mode);

      tb->setChecked (true);
      _prescan_mode = _mode;
      }
   }


void Pagewidget::scanComplete (void)
   {
   _scanning = false;
   _pagemodel->endingScan ();
   revertMode ();
   }


void Pagewidget::slotNewScannedPage (const QString &coverageStr, bool blank)
   {
   if (_scanning)
      {
      // if it is marked as blank, then we mark it as modified, so the user can revert it
      if (blank && !_modified)
         {
         _modified = true;
         updatePagetools ();
         }
      }

   _pagemodel->slotNewScannedPage (coverageStr, blank);
//    _pageview->scrollToEnd (true);
//    _pageview->scrollTo (_pagemodel->index (_pagemodel->rowCount (QModelIndex ()) - 1, 0, QModelIndex ()));
   }


void Pagewidget::slotBeginningPage (void)
   {
   if (_scanning)
      {
   //    qDebug () << "slotBeginningPage";

      // tell the model that we have a new page
      _pagemodel->beginningPage ();

      // show it
      _pageview->scrollToEnd (true);
      }
   }


void Pagewidget::slotNewScaledImage (const QImage &image, int scaled_linenum)
   {
//    qDebug () << "slotNewScaledImage";

   // tell the model that we have a new image
   if (_scanning)
      _pagemodel->newScaledImage (image, scaled_linenum);
   }


/************************** end of scanning *****************************/

void Pagewidget::toggleRemove (const QModelIndex &index)
   {
   QAbstractItemModel *model = (QAbstractItemModel *)index.model ();
   bool remove = model->data (index, Pagemodel::Role_remove).toBool ();
   QVariant v = !remove;

   model->setData (index, v, Pagemodel::Role_remove);
   }


void Pagewidget::slotShowInfo (const QModelIndex &index)
   {
   Desktopmodel *contents = _modelconv->getDesktopmodel (_model);
   QModelIndex sindex = _index;
   _modelconv->indexToSource (_model, sindex);

//       QString str = contents->data (_index, Desktopmodel::Role_message).toString ();
   QString str = contents->imageInfo (sindex, index.row (), true);
   emit newContents (str);
   slotPreviewPage (index);
   }


void Pagewidget::slotItemClicked (const QModelIndex &index, int which)
   {
//    QAbstractItemModel *model = (QAbstractItemModel *)index.model ();

   if (index != QModelIndex ())
      {
      switch (which)
         {
         case Pagedelegate::Point_remove : // remove
            toggleRemove (index);
            _modified = true;
            updatePagetools ();
            break;
         }

      // not needed as the selection will change anyway
//       slotShowInfo (index);
      }
   }


void Pagewidget::commit (void)
   {
   _modified = false;
   _pagemodel->updateAnnot (File::Annot_author, _pageattr->author->text ());
   _pagemodel->updateAnnot (File::Annot_title, _pageattr->title->text ());
   _pagemodel->updateAnnot (File::Annot_keywords, _pageattr->keywords->text ());
   _pagemodel->updateAnnot (File::Annot_notes, _pageattr->notes->toPlainText());
   err_complain (_pagemodel->commit ());
   updatePagetools ();
   }


void Pagewidget::slotSave (void)
   {
   /* in order to save, we must have a valid model and current stack, plus we
      can't save in the middle of a scan */
   if (_model && _index != QModelIndex () && !_scanning && _modified)
      commit ();
   }


void Pagewidget::slotRevert (void)
   {
   // mark all as kept
   _pagemodel->keepAllPages ();

   // get the original text back
   updateAttr ();
   updateOcrText ();

   // indicate that no changes have been made
   _modified = false;
   updatePagetools ();
   }


void Pagewidget::updateAttr (void)
   {
   _pageattr->author->setText (_model->data (_index, Desktopmodel::Role_author).toString ());
   _pageattr->filename->setText (_model->data (_index, Desktopmodel::Role_filename).toString ());
   _pageattr->title->setText (_model->data (_index, Desktopmodel::Role_title).toString ());
   _pageattr->keywords->setText (_model->data (_index, Desktopmodel::Role_keywords).toString ());
   _pageattr->notes->blockSignals (true);// because setting a QTextEdit emits a signal
   _pageattr->notes->setText (_model->data (_index, Desktopmodel::Role_notes).toString ());
   _pageattr->notes->blockSignals (false);

   // Update error
   QString err = _model->data (_index, Desktopmodel::Role_error).toString ();

   _pageattr->errorMsg->setVisible (!err.isEmpty ());
   _pageattr->errorMsg->setText (err);
   }


void Pagewidget::updateOcrText (void)
   {
   _ocr_edit->blockSignals (true);
   if (_index.isValid ())
      {
      Desktopmodel *contents;
      QModelIndex sindex = _index;

      contents = _modelconv->getDesktopmodel (_model);
      _modelconv->indexToSource (_model, sindex);

      // update OCR text
      QString str;

      contents->getPageText (sindex, _pagenum, str);
      // ignore error, text will report it to the user

      _ocr_edit->setText (str);
      }
   else
      _ocr_edit->clear ();
   _ocr_edit->blockSignals (false);
   }


void Pagewidget::slotStackChanged (const QModelIndex &from, const QModelIndex &to)
   {
   if (!_scanning && _model && _index.row () >= from.row () && _index.row () <= to.row ())
      {
      Desktopmodel *contents = _modelconv->getDesktopmodel (_model);
      bool minor = contents->isMinorChange ();

      showPages (_model, _index, 0, -1, minor ? -1 : 0, !minor);
      }
   }


void Pagewidget::slotCommitScanStack (void)
   {
   // if it is not us doing the committing, comit, by reseting the view
   if (!_committing)
      slotReset ();
   }


void Pagewidget::slotReset (void)
   {
//    qDebug () << "slotReset, model=0";
   slotSave ();
   _index = QModelIndex ();
   _model = 0;
   _pagemodel->clear ();
   clearViewport ();
   if (_scanning)
      {
      revertMode ();
      _scanning = false;
      }
   updatePagetools (); // for the benefit of _pageattr
   }


bool Pagewidget::getCurrentIndex (QModelIndex &index, bool source)
   {
   index = _index;
   if (source)
      _modelconv->indexToSource (_model, index);
   return index.isValid ();
   };


void Pagewidget::slotAnnotChanged (void)
   {
   if (!_modified)
      {
      _modified = true;
      updatePagetools ();
      }
   }


void Pagewidget::ocrPage (void)
   {
   QString str;

   if (_image.isNull ())
//    if (!_model || !_index.isValid ()
      {
      QMessageBox::warning (0, "Maxview",
         tr ("Please select a page to OCR"));
      return;
      }

   // pass the page to our OCR engine
   err_info *err;
   Ocr *ocr = Ocr::getOcr (err);

   if (ocr)
      err = ocr->imageToText (_image, str);
   if (!err_complain (err))
      _ocr_edit->setText (str);
   }


void Pagewidget::ocrFlipView (void)
   {
   _ocr_split->setOrientation (_ocr_split->orientation () == Qt::Vertical
      ? Qt::Horizontal : Qt::Vertical);
   }


void Pagewidget::ocrClear (void)
   {
   _ocr_edit->clear ();
   }


void Pagewidget::ocrCopy (void)
   {
   QTextCursor curs = _ocr_edit->textCursor ();

   // if no text selected, select all text
   if (!curs.hasSelection ())
      _ocr_edit->selectAll ();
   _ocr_edit->copy ();

/*   curs.clearSelection ();
   _ocr_edit->setTextCursor (curs);*/
   }


void Pagewidget::addRotate (int add)
{
   _rotate = (_rotate + 360 + add) % 360;
   updateWindow ();
}


void Pagewidget::slotRotateRight (void)
{
   addRotate (90);
}


void Pagewidget::slotRotate180 (void)
{
   addRotate (180);
}


void Pagewidget::slotRotateLeft (void)
{
   addRotate (-90);
}



/**************************** MyScrollArea ********************************/

MyScrollArea::MyScrollArea (QWidget *parent)
      : QAbstractScrollArea (parent)
   {
   setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
   setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);
   _size = QSize (-1, -1);
   _rotate = 0;
   }


MyScrollArea::~MyScrollArea ()
   {
   }


void MyScrollArea::setScale (double scale)
   {
   _scale = scale;
   }


void MyScrollArea::setSize (QSize &size)
   {
   _size = size;;
   }


void MyScrollArea::setPixmapImage (QPixmap &pixmap, QImage &image, bool too_big, int rotate)
   {
   _pixmap = pixmap;
   _image = image;
   _too_big = too_big;
   _rotate = rotate;
   }


void MyScrollArea::paintEvent (QPaintEvent *event)
   {
   emit signalPainting ();

   QPainter painter (viewport ());
   QPoint tp;

   if (_too_big)
      {
      if (_rotate == 90)
         {
         tp = QPoint (0, -_image.height ());
         }

      //adjustMatrix (painter, _image.size (), _rotate);
      painter.scale(_scale, _scale);
      }

   QMatrix m = painter.matrix ();
   QRect exposedRect = m
                     .translate (-horizontalScrollBar()->value (), -verticalScrollBar()->value ())
                     .inverted ()
                     .mapRect (event->rect ())
                     .adjusted (-1, -1, 1, 1);

   if (_rotate % 180 == 90)
      exposedRect = QRect (exposedRect.y (), exposedRect.x (), exposedRect.height (), exposedRect.width ());

   // the adjust is to account for half pixels along edges
   //qDebug () << "exposed" << exposedRect << "image" << _image.size () << "scale" << _scale << "tp" << tp;
   QPointF target = QPointF (0, 0);

   // set up the blanking rectangles so that the page is clearly visible

   // right rect draws to the right of the image, extending to the bottom of the viewer
   QRect rrect = exposedRect;
//    rrect.translate (-horizontalScrollBar()->value (), -verticalScrollBar()->value ());

   // bottom rect draws under the image, extending only to the right side of the image
   QRect brect = rrect;

   painter.rotate (_rotate);
   painter.translate (tp);
   if (_too_big && !_image.isNull ())
      {
      painter.drawImage(target, _image, exposedRect);
//       rrect.setLeft (_image->width ());
//       brect.setTop (_image->height ());
//       brect.setRight (_image->width ());
      }
   else if (!_pixmap.isNull ())
      {
      painter.drawPixmap(target, _pixmap, exposedRect);
      rrect.setLeft (_pixmap.width () -horizontalScrollBar()->value ());
      rrect.setTop (-verticalScrollBar()->value ());
      brect.setTop (_pixmap.height ());
      brect.setRight (_pixmap.width ());
      painter.fillRect (rrect, QBrush (Qt::lightGray));
      painter.fillRect (brect, QBrush (Qt::lightGray));
      }
   }


QSize MyScrollArea::sizeHint () const
   {
//   printf ("sizeHint %d, %d\n", _size.width (), _size.height ());
   return _size;
   }


void MyScrollArea::wheelEvent (QWheelEvent *e)
   {
   double newScale = _scale;

   if (e->modifiers () & Qt::ControlModifier)
      {
//printf ("delta %d\n", e->delta ());
      if (e->delta () > 0)
         newScale = _scale * SCALE_FACTOR;
      else if (e->delta () < 0)
         newScale = _scale / SCALE_FACTOR;
      else
         e->ignore ();

      if (newScale != _scale)
         emit signalNewScale (newScale, true);
      }
   else if (e->modifiers () & Qt::ShiftModifier) // holding shifts scrolls left/right
      {
      int value = horizontalScrollBar()->value ();

      if (e->delta () > 0)
         value -= WHEEL_SCROLL_Y;
      else if (e->delta () < 0)
         value += WHEEL_SCROLL_Y;
      else
         e->ignore ();
      horizontalScrollBar()->setValue (value);
      }
   else
      {
      int value = verticalScrollBar()->value ();

      if (e->delta () > 0)
         value -= WHEEL_SCROLL_Y;
      else if (e->delta () < 0)
         value += WHEEL_SCROLL_Y;
      else
         e->ignore ();
      verticalScrollBar()->setValue (value);
      }
   }


void MyScrollArea::mousePressEvent ( QMouseEvent * e )
   {
   _mouse_origin = e->pos ();
   _scroll_origin.setX (horizontalScrollBar()->value ());
   _scroll_origin.setY (verticalScrollBar()->value ());
   }


void MyScrollArea::mouseMoveEvent ( QMouseEvent * e )
   {
   QPoint pos;

   pos = e->pos () - _mouse_origin;
   pos = _scroll_origin - pos;
   horizontalScrollBar()->setValue (pos.x ());
   verticalScrollBar()->setValue (pos.y ());
   }


