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

#include <QDateTime>
#include <QFile>
#include <QCheckBox>
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
#include "pagefind.h"
#include "utils.h"


CameraView::CameraView (QWidget *parent)
      : QWidget (parent)
   {
   setMinimumSize (320, 240);
   setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);
   }


//! the least time between looks for the pages in the view, in ms
static const int LOOK_MS = 250;


void CameraView::setFrame (const QImage &image)
   {
   _frame = image;
   _count = image.isNull () ? 0 : _count + 1;

   /* The small picture is quick to look through, about 20 ms, so it is
      done here rather than in a thread, but not for every one */
   if (image.isNull () || _mode == Pagefind::Mode_whole)
      _outlines.clear ();
   else if (!_looked.isValid () || _looked.elapsed () >= LOOK_MS)
      {
      Pagefind::Result found = Pagefind::find (image,
                                               (Pagefind::Mode)_mode);

      _outlines.clear ();
      for (int i = 0; i < found.pages.size (); i++)
         _outlines << found.shape (i);
      _looked.start ();
      }
   update ();
   }


void CameraView::setPages (int mode)
   {
   _mode = mode;
   _looked.invalidate ();
   if (mode == Pagefind::Mode_whole)
      _outlines.clear ();
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

   QRect area = frameRect ();

   p.drawImage (area, _frame);
   if (_outlines.isEmpty ())
      return;

   // each page which would be cut out, in a colour which shows on paper
   QTransform map;

   map.translate (area.x (), area.y ());
   map.scale ((double)area.width () / _frame.width (),
              (double)area.height () / _frame.height ());
   p.setRenderHint (QPainter::Antialiasing);
   p.setPen (QPen (QColor (0, 160, 255), 2));
   p.setBrush (Qt::NoBrush);
   p.setClipRect (area);
   for (const QPolygonF &outline : _outlines)
      p.drawPolygon (map.map (outline));
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

   QLabel *pages_label = new QLabel (tr ("Pages"), this);

   _pages = new QComboBox (this);
   _pages->setObjectName ("pages");
   _pages->setToolTip (tr ("What to keep of each picture: all of it, the "
                           "page found in it, or the two pages of an "
                           "open book. Auto splits a book and not a "
                           "sheet"));
   _pages->addItem (tr ("Whole picture"), Pagefind::Mode_whole);
   _pages->addItem (tr ("One page"), Pagefind::Mode_one);
   _pages->addItem (tr ("Two pages"), Pagefind::Mode_two);
   _pages->addItem (tr ("Auto"), Pagefind::Mode_auto);

   _keep = new QCheckBox (tr ("Keep the camera's pictures"), this);
   _keep->setObjectName ("keep");
   _keep->setToolTip (tr ("Keep each picture as the camera took it, in a "
                          "second stack, as well as the pages cut from "
                          "it"));

   QSettings qs;
   int mode = qs.value ("camera/pages", Pagefind::Mode_auto).toInt ();

   _pages->setCurrentIndex (qMax (0, _pages->findData (mode)));
   _keep->setChecked (qs.value ("camera/keep", true).toBool ());

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
   top->addWidget (pages_label);
   top->addWidget (_pages);
   top->addWidget (_keep);
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
   connect (_pages, QOverload<int>::of (&QComboBox::activated), this,
            &Camerapanel::pagesChosen);
   connect (_keep, &QCheckBox::toggled, this, &Camerapanel::keepToggled);
   connect (_main, &Mainwidget::cameraFiled, this,
            &Camerapanel::pagesFiled);
   connect (_capture, &QPushButton::clicked, this, &Camerapanel::capture);
   connect (_done, &QPushButton::clicked, this, &Camerapanel::accept);
   connect (_cancel, &QPushButton::clicked, this, &Camerapanel::cancel);

   // see eventFilter()
   const QList<QWidget *> watched = {this, _cameras, _sizes, _pages,
                                     _keep, _view, _capture, _done,
                                     _cancel};

   for (QWidget *widget : watched)
      widget->installEventFilter (this);

   restoreGeometry (QSettings ().value ("camerapanel/geometry")
                    .toByteArray ());
   _view->setPages (pagesMode ());
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
   // nor while Done is waiting for the last pages
   if (_camera || _finishing)
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
   // TRIAL ONLY: keep the picture which follows a pushed still
   connect (_camera, &Camera::pushFollowed, this,
            [] (const QByteArray &jpeg)
      {
      QString dir = qEnvironmentVariable ("PAPERMAN_LASER_DIR");
      static int seq;

      if (dir.isEmpty ())
         return;
      QFile file (QString ("%1/laser_%2_%3.jpg").arg (dir)
            .arg (QDateTime::currentDateTime ().toString ("HHmmss"))
            .arg (++seq));
      if (file.open (QIODevice::WriteOnly))
         file.write (jpeg);
      });
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


int Camerapanel::pagesMode (void) const
   {
   return _pages->currentData ().toInt ();
   }


void Camerapanel::pagesChosen (int index)
   {
   QSettings ().setValue ("camera/pages", _pages->itemData (index).toInt ());
   _view->setPages (pagesMode ());
   updateState ();
   }


void Camerapanel::keepToggled (bool on)
   {
   QSettings ().setValue ("camera/keep", on);
   }


void Camerapanel::takePicture (const QByteArray &jpeg)
   {
   // a picture kept whole is its own page, so there is no other to keep
   _message = _main->cameraAddPicture (jpeg, pagesMode (),
         _keep->isChecked () && pagesMode () != Pagefind::Mode_whole);
   updateState ();
   }


void Camerapanel::pagesFiled (const QString &problem)
   {
   if (!problem.isEmpty ())
      _message = problem;
   updateState ();

   // Done was waiting for these
   if (_finishing && !_main->cameraPending ())
      accept ();
   }


void Camerapanel::stillArrived (const QByteArray &jpeg)
   {
   _busy = false;
   takePicture (jpeg);
   }


void Camerapanel::stillPushed (const QByteArray &jpeg)
   {
   /* the camera took this one itself, so nothing was waiting for it:
      a still which was asked for is still on its way, if one was */
   takePicture (jpeg);
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
   int waiting = _main->cameraPending ();

   _capture->setEnabled (_camera && !_busy && !_finishing);
   _capture->setText (_busy ? tr ("Capturing...") : tr ("Capture"));

   // so that the page on its way is not left out
   _done->setEnabled (!_busy && !_finishing);
   _pages->setEnabled (!_finishing);
   _keep->setEnabled (!_finishing && pagesMode () != Pagefind::Mode_whole);

   int pages = _main->cameraPages ();

   _count->setText (pages == 1 ? tr ("1 page")
                               : tr ("%1 pages").arg (pages));

   // what went wrong comes first, then what is still being done
   if (!_message.isEmpty ())
      _status->setText (_message);
   else if (_finishing)
      _status->setText (waiting == 1
         ? tr ("Finishing: 1 picture still to do")
         : tr ("Finishing: %1 pictures still to do").arg (waiting));
   else if (waiting)
      _status->setText (waiting == 1
         ? tr ("Working on 1 picture")
         : tr ("Working on %1 pictures").arg (waiting));
   else
      _status->setText (QString ());
   }


void Camerapanel::finish (void)
   {
   closeCamera ();
   QSettings ().setValue ("camerapanel/geometry", saveGeometry ());
   }


void Camerapanel::accept (void)
   {
   finish ();

   /* The pages of the last pictures may still be being made, a second
      or so each. Wait for them with the panel showing how many there
      are, rather than with the window stuck: pagesFiled() comes back
      here. Cancel still works meanwhile */
   if (_main->cameraPending ())
      {
      _finishing = true;
      updateState ();
      return;
      }
   _finishing = false;
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
   _finishing = false;
   _main->cameraCancel ();
   _message.clear ();
   updateState ();
   QDialog::reject ();
   }
