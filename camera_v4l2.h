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

#ifndef __camera_v4l2_h
#define __camera_v4l2_h

#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

#include "camera.h"

/** the thread which talks to a V4L2 camera once it is open

Everything which can wait for the camera happens here: starting and
stopping its stream, changing the size it sends and waiting for each
picture. The camera sends pictures of one size at a time, so taking a
still means stopping the small pictures, asking for the large size,
waiting for a good picture of that size and then going back.

What is wanted of it is set by V4l2Camera, under the mutex, and it
answers with signals */
class V4l2Worker : public QThread
   {
   Q_OBJECT

public:
   V4l2Worker (int fd, QObject *parent);

   //! start or stop the small pictures
   void wantPreview (bool on, const QSize &size);

   //! ask for a still
   void wantStill (const QSize &size);

   //! ask the thread to stop what it is doing and finish
   void quit (void);

signals:
   void previewFrame (const QImage &image);
   void captured (const QByteArray &jpeg);
   void failed (const QString &msg);

protected:
   void run () override;

private:
   //! a buffer shared with the driver, which pictures arrive in
   struct Buffer
      {
      void *start;
      size_t length;
      };

   //! what waitFrame() found
   enum Wait
      {
      Wait_frame,       //!< a picture arrived
      Wait_timeout,     //!< nothing arrived in the time given
      Wait_asked,       //!< something else has been asked of the thread
      Wait_error,       //!< the camera failed
      };

   /** have the camera send pictures of a size, stopping it first if it
       is sending

      \param size     the size wanted
      \param buffers  how many buffers to share with the driver
      \returns an empty string if OK, else what went wrong */
   QString startStream (const QSize &size, int buffers);

   //! stop the camera sending and give its buffers back
   void stopStream (void);

   /** wait for the camera's next picture

      \param ms       how long to wait, in milliseconds
      \param mind     true to return early if something else is asked
                      of the thread; quit() always does that
      \param frame    returns the picture, as the camera sent it
      \param damaged  returns true if the driver says it is not whole
      \param err      returns what went wrong, for Wait_error */
   Wait waitFrame (int ms, bool mind, QByteArray &frame, bool &damaged,
                   QString &err);

   //! send small pictures until something else is asked
   void preview (const QSize &size);

   //! take a still
   void still (const QSize &size);

   //! true if something has been asked which preview() should stop for
   bool asked (void);

   int _fd;
   QVector<Buffer> _buffers;
   bool _streaming = false;
   QSize _stream_size;        //!< the size the camera is sending

   QMutex _mutex;             //!< guards what follows
   QWaitCondition _wake;      //!< told when any of it changes
   bool _quit = false;
   bool _want_preview = false;
   QSize _preview_size;
   bool _want_still = false;
   QSize _still_size;
   };


/** a camera reached through V4L2, such as a UVC document camera

Only cameras which send Motion-JPEG are offered, since the pictures are
kept as the camera's own JPEGs */
class V4l2Camera : public Camera
   {
   Q_OBJECT

public:
   /** \param dev  the camera's device node */
   V4l2Camera (const QString &dev, QObject *parent);
   ~V4l2Camera () override;

   //! the cameras there are, for Camera::list()
   static QList<CameraInfo> cameras (void);

   //! true if there is any video device, for Camera::present()
   static bool present (void);

   QString open (void) override;
   void close (void) override;
   bool isOpen (void) const override { return _fd >= 0; }
   QList<QSize> stillSizes (void) const override { return _sizes; }
   void startPreview (const QSize &still) override;
   void stopPreview (void) override;
   void capture (const QSize &size) override;

private:
   QString _dev;
   int _fd = -1;
   QList<QSize> _sizes;       //!< sizes it can send, largest first
   V4l2Worker *_worker = nullptr;
   };

#endif
