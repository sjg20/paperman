/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
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
*/

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "camera.h"
#include "camerapanel.h"
#include "mainwidget.h"
#include "utils.h"


CameraView::CameraView (QWidget *parent)
      : QWidget (parent)
   {
   setMinimumSize (320, 240);
   setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);
   }


void CameraView::setFrame (const QImage &image)
   {
   _frame = image;
   _count = image.isNull () ? 0 : _count + 1;
   update ();
   }


QRect CameraView::frameRect (void) const
   {
   if (_frame.isNull ())
      return QRect ();

   QSize size = _frame.size ().scaled (this->size (), Qt::KeepAspectRatio);
   QRect area (QPoint (0, 0), size);

   area.moveCenter (rect ().center ());
   return area;
   }


QSize CameraView::sizeHint (void) const
   {
   return QSize (800, 600);
   }


void CameraView::paintEvent (QPaintEvent *)
   {
   QPainter p (this);

   // the same in either theme: what surrounds the picture is not paper
   p.fillRect (rect (), QColor (32, 32, 32));
   if (_frame.isNull ())
      {
      p.setPen (QColor (224, 224, 224));
      p.drawText (rect (), Qt::AlignCenter, tr ("No picture"));
      return;
      }
   p.setRenderHint (QPainter::SmoothPixmapTransform);
   p.drawImage (frameRect (), _frame);
   }


Camerapanel::Camerapanel (Mainwidget *main, QWidget *parent)
      : QDialog (parent), _main (main)
   {
   setObjectName ("camerapanel");
   setWindowTitle (tr ("Camera"));

   _camera_label = new QLabel (tr ("Camera"), this);
   _cameras = new QComboBox (this);
   _cameras->setObjectName ("camera");
   _cameras->setToolTip (tr ("The camera to use"));

   QLabel *size_label = new QLabel (tr ("Size"), this);

   _sizes = new QComboBox (this);
   _sizes->setObjectName ("size");
   _sizes->setToolTip (tr ("The size of each page, in pixels"));

   _view = new CameraView (this);
   _view->setObjectName ("viewfinder");

   _count = new QLabel (this);
   _count->setObjectName ("count");
   _status = new QLabel (this);
   _status->setObjectName ("status");
   _status->setWordWrap (true);

   _capture = new QPushButton (this);
   _capture->setObjectName ("capture");
   _capture->setIcon (utilIcon ("camera"));
   _capture->setToolTip (tr ("Take a picture and add it to the stack as "
                             "a page (Space)"));
   _capture->setDefault (true);
   _done = new QPushButton (tr ("Done"), this);
   _done->setObjectName ("done");
   _done->setToolTip (tr ("Keep the stack and close the panel"));
   _done->setAutoDefault (false);
   _cancel = new QPushButton (tr ("Cancel"), this);
   _cancel->setObjectName ("cancel");
   _cancel->setToolTip (tr ("Throw away the pages taken and close the "
                            "panel"));
   _cancel->setAutoDefault (false);

   QHBoxLayout *top = new QHBoxLayout;

   top->addWidget (_camera_label);
   top->addWidget (_cameras, 1);
   top->addWidget (size_label);
   top->addWidget (_sizes);
   top->addStretch (1);

   QHBoxLayout *bottom = new QHBoxLayout;

   bottom->addWidget (_count);
   bottom->addWidget (_status, 1);
   bottom->addWidget (_capture);
   bottom->addWidget (_done);
   bottom->addWidget (_cancel);

   QVBoxLayout *layout = new QVBoxLayout (this);

   layout->addLayout (top);
   layout->addWidget (_view, 1);
   layout->addLayout (bottom);

   connect (_cameras, QOverload<int>::of (&QComboBox::activated), this,
            &Camerapanel::cameraChosen);
   connect (_sizes, QOverload<int>::of (&QComboBox::activated), this,
            &Camerapanel::sizeChosen);
   connect (_capture, &QPushButton::clicked, this, &Camerapanel::capture);
   connect (_done, &QPushButton::clicked, this, &Camerapanel::accept);
   connect (_cancel, &QPushButton::clicked, this, &Camerapanel::cancel);

   // see eventFilter()
   const QList<QWidget *> watched = {this, _cameras, _sizes, _view,
                                     _capture, _done, _cancel};

   for (QWidget *widget : watched)
      widget->installEventFilter (this);

   restoreGeometry (QSettings ().value ("camerapanel/geometry")
                    .toByteArray ());
   updateState ();
   }


Camerapanel::~Camerapanel ()
   {
   closeCamera ();
   }


bool Camerapanel::eventFilter (QObject *obj, QEvent *event)
   {
   if (event->type () == QEvent::KeyPress)
      {
      QKeyEvent *key = static_cast<QKeyEvent *> (event);

      if (key->key () == Qt::Key_Space
          && !(key->modifiers () & ~Qt::KeypadModifier))
         {
         // holding it down takes one picture, not a run of them
         if (!key->isAutoRepeat ())
            capture ();
         return true;
         }
      }
   return QDialog::eventFilter (obj, event);
   }


void Camerapanel::start (void)
   {
   if (_camera)
      return;

   const QList<CameraInfo> list = Camera::list ();
   QString wanted = QSettings ().value ("camera/name").toString ();
   int chosen = 0;

   _cameras->clear ();
   for (const CameraInfo &info : list)
      {
      if (info.name == wanted)
         chosen = _cameras->count ();
      _cameras->addItem (info.name, info.id);
      }

   // there is nothing to choose with only the one
   _camera_label->setVisible (list.size () > 1);
   _cameras->setVisible (list.size () > 1);
   if (list.isEmpty ())
      {
      _sizes->clear ();
      _message = tr ("No camera found");
      updateState ();
      return;
      }
   _cameras->setCurrentIndex (chosen);
   openCamera (list [chosen].id);
   _capture->setFocus ();
   }


void Camerapanel::closing (void)
   {
   if (isVisible ())
      accept ();
   else
      closeCamera ();
   }


QSize Camerapanel::stillSize (void) const
   {
   return _sizes->currentData ().toSize ();
   }


void Camerapanel::openCamera (const QString &id)
   {
   closeCamera ();
   _message.clear ();
   _sizes->clear ();

   Camera *camera = Camera::create (id, this);
   QString err = camera ? camera->open ()
                        : tr ("The camera is no longer there");

   if (!err.isEmpty ())
      {
      delete camera;
      _message = err;
      updateState ();
      return;
      }
   _camera = camera;

   // the largest, unless another was chosen last time and is still there
   QSize wanted = QSettings ().value ("camera/size").toSize ();
   int chosen = 0;

   for (const QSize &size : _camera->stillSizes ())
      {
      double mp = size.width () * (double)size.height () / 1e6;

      if (size == wanted)
         chosen = _sizes->count ();
      _sizes->addItem (tr ("%1 x %2 (%3 MP)").arg (size.width ())
                       .arg (size.height ()).arg (mp, 0, 'f', mp < 10 ? 1 : 0),
                       size);
      }
   _sizes->setCurrentIndex (chosen);

   connect (_camera, &Camera::previewFrame, _view, &CameraView::setFrame);
   connect (_camera, &Camera::captured, this, &Camerapanel::stillArrived);
   connect (_camera, &Camera::stillPushed, this,
            &Camerapanel::stillPushed);
   connect (_camera, &Camera::failed, this, &Camerapanel::cameraFailed);
   _camera->startPreview (stillSize ());
   updateState ();
   }


void Camerapanel::closeCamera (void)
   {
   if (_camera)
      {
      _camera->disconnect (this);
      _camera->disconnect (_view);
      _camera->close ();
      delete _camera;
      _camera = nullptr;
      }
   _busy = false;
   _stalled = false;
   _view->setFrame (QImage ());
   }


void Camerapanel::cameraChosen (int index)
   {
   QSettings ().setValue ("camera/name", _cameras->itemText (index));
   openCamera (_cameras->itemData (index).toString ());
   }


void Camerapanel::sizeChosen (int index)
   {
   QSettings ().setValue ("camera/size", _sizes->itemData (index).toSize ());

   // the small pictures take the shape of the still
   if (_camera)
      _camera->startPreview (stillSize ());
   }


void Camerapanel::capture (void)
   {
   if (!_camera || _busy || !stillSize ().isValid ())
      return;

   // a camera which failed has stopped its small pictures: try again
   if (_stalled)
      _camera->startPreview (stillSize ());
   _stalled = false;
   _busy = true;
   _message.clear ();
   updateState ();
   _camera->capture (stillSize ());
   }


void Camerapanel::stillArrived (const QByteArray &jpeg)
   {
   _busy = false;
   _message = _main->cameraAddPage (jpeg);
   updateState ();
   }


void Camerapanel::stillPushed (const QByteArray &jpeg)
   {
   /* the camera took this one itself, so nothing was waiting for it:
      a still which was asked for is still on its way, if one was */
   _message = _main->cameraAddPage (jpeg);
   updateState ();
   }


void Camerapanel::cameraFailed (const QString &msg)
   {
   _busy = false;
   _stalled = true;
   _message = msg;
   updateState ();
   }


void Camerapanel::updateState (void)
   {
   _capture->setEnabled (_camera && !_busy);
   _capture->setText (_busy ? tr ("Capturing...") : tr ("Capture"));

   // so that the page on its way is not left out
   _done->setEnabled (!_busy);
   int pages = _main->cameraPages ();

   _count->setText (pages == 1 ? tr ("1 page")
                               : tr ("%1 pages").arg (pages));
   _status->setText (_message);
   }


void Camerapanel::finish (void)
   {
   closeCamera ();
   QSettings ().setValue ("camerapanel/geometry", saveGeometry ());
   }


void Camerapanel::accept (void)
   {
   finish ();
   _main->cameraDone ();
   updateState ();
   QDialog::accept ();
   }


void Camerapanel::reject (void)
   {
   accept ();
   }


void Camerapanel::cancel (void)
   {
   finish ();
   _main->cameraCancel ();
   updateState ();
   QDialog::reject ();
   }
