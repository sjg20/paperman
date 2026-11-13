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

#ifndef __camerapanel_h
#define __camerapanel_h

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QPolygonF>
#include <QSize>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

class Camera;
class Mainwidget;

/** shows what a camera sees: its latest small picture, as large as fits
    with its shape kept */
class CameraView : public QWidget
   {
   Q_OBJECT

public:
   explicit CameraView (QWidget *parent = nullptr);

   //! show a picture, or nothing if it is null
   void setFrame (const QImage &image);

   //! the picture shown
   QImage frame (void) const { return _frame; }

   //! how many pictures have been shown since the last null one
   int frameCount (void) const { return _count; }

   //! where in the widget the picture is drawn
   QRect frameRect (void) const;

   /** say whether to mark the pages which would be cut from the picture

      The pages are looked for in a picture a few times a second and
      their outlines drawn over the view, so that what will be kept can
      be seen before it is taken

      \param mode   how many pages to look for, a Pagefind::Mode;
                    Pagefind::Mode_whole to mark nothing */
   void setPages (int mode);

   //! the outlines being drawn, in the picture's own pixels
   QList<QPolygonF> outlines (void) const { return _outlines; }

   QSize sizeHint (void) const override;

protected:
   void paintEvent (QPaintEvent *event) override;

private:
   QImage _frame;
   int _count = 0;
   int _mode = 0;                 //!< how many pages to look for
   QList<QPolygonF> _outlines;    //!< where they were last found
   QElapsedTimer _looked;         //!< when they were last looked for
   };


/** the camera panel: photograph pages, one at a time, into a stack

The panel shows what the camera sees. Each press of Capture, or of the
space bar, takes a still at the size chosen and adds it as a page to a
stack in the folder which was shown when the first was taken. Done keeps
the stack and Cancel throws it away; closing the panel any other way
keeps it, so that a slip does not lose a book's worth of pages.

Unless the whole picture is asked for, the page is found in each still,
cut out and made upright, and an open book is split into its two pages.
That is done in the background, so the count of pages goes up a moment
after each still is taken, and Done waits for what is still being done.
The camera's own pictures can be kept as well, in a second stack.

A camera with a button of its own may take a still when that is pressed
and send it unasked. That is added to the stack just as one taken with
Capture is, at whatever size the camera sent it.

The camera is open for as long as the panel is shown, and is let go of
when the panel closes */
class Camerapanel : public QDialog
   {
   Q_OBJECT

public:
   Camerapanel (Mainwidget *main, QWidget *parent = nullptr);
   ~Camerapanel () override;

   /** look for cameras and start showing what the one chosen sees. This
       is for when the panel is shown; it does nothing if a camera is
       already open */
   void start (void);

   //! let go of the camera, keeping any pages: Paperman is closing
   void closing (void);

   //! true while a still is on its way from the camera
   bool busy (void) const { return _busy; }

public slots:
   //! take a still and add it to the stack
   void capture (void);

   //! keep the stack and close the panel
   void accept (void) override;

   //! as accept(): the Escape key and the window's close button
   void reject (void) override;

   //! throw the stack away and close the panel
   void cancel (void);

protected:
   /** take the space bar for Capture, whichever control it is typed at,
       rather than have it press the button which has the focus */
   bool eventFilter (QObject *obj, QEvent *event) override;

private slots:
   void cameraChosen (int index);
   void sizeChosen (int index);
   void stillArrived (const QByteArray &jpeg);
   void stillPushed (const QByteArray &jpeg);
   void pagesChosen (int index);
   void keepToggled (bool on);
   void pagesFiled (const QString &problem);
   void cameraFailed (const QString &msg);

private:
   //! open a camera, by its id, and start its small pictures
   void openCamera (const QString &id);

   //! let go of the camera
   void closeCamera (void);

   //! the size of still which is chosen
   QSize stillSize (void) const;

   //! set the buttons, the count and the message from where things are
   void updateState (void);

   //! stop the camera and hide the panel, noting where it was
   void finish (void);

   //! take a still which has arrived, however it was asked for
   void takePicture (const QByteArray &jpeg);

   //! how many pages to look for in each picture, a Pagefind::Mode
   int pagesMode (void) const;

   Mainwidget *_main;
   Camera *_camera = nullptr;
   bool _busy = false;       //!< a still has been asked for
   bool _stalled = false;    //!< the camera failed, so may have stopped
   bool _finishing = false;  //!< Done was pressed with pages still to come
   QString _message;         //!< what went wrong last, if anything

   QLabel *_camera_label;
   QComboBox *_cameras;
   QComboBox *_sizes;
   QComboBox *_pages;
   QCheckBox *_keep;
   CameraView *_view;
   QLabel *_count;
   QLabel *_status;
   QPushButton *_capture;
   QPushButton *_done;
   QPushButton *_cancel;
   };

#endif
