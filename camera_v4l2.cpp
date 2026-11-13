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

#include <algorithm>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>

#include "camera_v4l2.h"

//! how many buffers to share with the driver for the small pictures
static const int PREVIEW_BUFFERS = 4;

//! and for stills, which are large and of which one is wanted
static const int STILL_BUFFERS = 2;

//! how long the small pictures can stop for before giving up, in ms
static const int STALL_MS = 5000;

//! how long to wait for a still, from asking for its size, in ms
static const int STILL_MS = 10000;

//! the least time between the small pictures passed on, in ms
static const int PREVIEW_GAP_MS = 60;

//! how often a wait for a picture looks at what else has been asked
static const int POLL_MS = 100;


//! ioctl() which carries on when a signal interrupts it
static int xioctl (int fd, unsigned long request, void *arg)
   {
   int ret;

   do
      ret = ioctl (fd, request, arg);
   while (ret < 0 && errno == EINTR);
   return ret;
   }


//! what went wrong with a call to the camera, for the user
static QString problem (const QString &what, int err)
   {
   if (err == EBUSY)
      return QObject::tr ("The camera is in use by another program");
   if (err == ENODEV)
      return QObject::tr ("The camera is no longer connected");
   return QObject::tr ("%1: %2").arg (what)
         .arg (QString::fromLocal8Bit (strerror (err)));
   }


//! true if the camera can send Motion-JPEG
static bool has_mjpeg (int fd)
   {
   for (int i = 0; ; i++)
      {
      struct v4l2_fmtdesc desc;

      memset (&desc, '\0', sizeof (desc));
      desc.index = i;
      desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (xioctl (fd, VIDIOC_ENUM_FMT, &desc) < 0)
         return false;
      if (desc.pixelformat == V4L2_PIX_FMT_MJPEG)
         return true;
      }
   }


//! the sizes of Motion-JPEG picture a camera can send, largest first
static QList<QSize> mjpeg_sizes (int fd)
   {
   QList<QSize> sizes;

   for (int i = 0; ; i++)
      {
      struct v4l2_frmsizeenum fse;
      QSize size;

      memset (&fse, '\0', sizeof (fse));
      fse.index = i;
      fse.pixel_format = V4L2_PIX_FMT_MJPEG;
      if (xioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fse) < 0)
         break;
      if (fse.type == V4L2_FRMSIZE_TYPE_DISCRETE)
         size = QSize (fse.discrete.width, fse.discrete.height);
      else
         // a camera which sends any size in a range: offer the largest
         size = QSize (fse.stepwise.max_width, fse.stepwise.max_height);
      if (!size.isEmpty () && !sizes.contains (size))
         sizes << size;
      if (fse.type != V4L2_FRMSIZE_TYPE_DISCRETE)
         break;
      }
   std::sort (sizes.begin (), sizes.end (),
              [] (const QSize &a, const QSize &b) {
      return (qint64)a.width () * a.height ()
            > (qint64)b.width () * b.height ();
   });
   return sizes;
   }


/** open a device node if it is a camera which can be used

   A camera can have other nodes, such as one for metadata, which send
   no pictures

   \param dev   the device node
   \param name  returns the camera's name
   \param err   returns why not, if it cannot be opened
   \returns the file descriptor, or -1 if it is not such a camera */
static int open_camera (const QString &dev, QString &name, QString &err)
   {
   struct v4l2_capability cap;
   int fd = ::open (QFile::encodeName (dev).constData (),
                    O_RDWR | O_NONBLOCK | O_CLOEXEC);

   if (fd < 0)
      {
      err = problem (QObject::tr ("Cannot open the camera"), errno);
      return -1;
      }
   memset (&cap, '\0', sizeof (cap));
   if (xioctl (fd, VIDIOC_QUERYCAP, &cap) < 0)
      {
      err = QObject::tr ("That is not a camera");
      ::close (fd);
      return -1;
      }

   quint32 caps = cap.capabilities & V4L2_CAP_DEVICE_CAPS
         ? cap.device_caps : cap.capabilities;

   if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)
       || !has_mjpeg (fd))
      {
      err = QObject::tr ("The camera does not send JPEG pictures");
      ::close (fd);
      return -1;
      }
   name = QString::fromUtf8 ((const char *)cap.card).trimmed ();
   return fd;
   }


/***************************** V4l2Worker ********************************/

V4l2Worker::V4l2Worker (int fd, QObject *parent)
      : QThread (parent), _fd (fd)
   {
   }


void V4l2Worker::wantPreview (bool on, const QSize &size)
   {
   QMutexLocker locker (&_mutex);

   _want_preview = on;
   _preview_size = size;
   _wake.wakeAll ();
   }


void V4l2Worker::wantStill (const QSize &size)
   {
   QMutexLocker locker (&_mutex);

   // one at a time
   if (_want_still)
      return;
   _want_still = true;
   _still_size = size;
   _wake.wakeAll ();
   }


void V4l2Worker::quit (void)
   {
   QMutexLocker locker (&_mutex);

   _quit = true;
   _wake.wakeAll ();
   }


bool V4l2Worker::asked (void)
   {
   QMutexLocker locker (&_mutex);

   return _quit || _want_still || !_want_preview
         || _preview_size != _stream_size;
   }


void V4l2Worker::stopStream (void)
   {
   int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (_streaming)
      xioctl (_fd, VIDIOC_STREAMOFF, &type);
   _streaming = false;

   if (_buffers.isEmpty ())
      return;
   for (const Buffer &buf : _buffers)
      munmap (buf.start, buf.length);
   _buffers.clear ();

   // give the driver's buffers back, so that the size can be changed
   struct v4l2_requestbuffers req;

   memset (&req, '\0', sizeof (req));
   req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   req.memory = V4L2_MEMORY_MMAP;
   xioctl (_fd, VIDIOC_REQBUFS, &req);
   }


QString V4l2Worker::startStream (const QSize &size, int buffers)
   {
   struct v4l2_format fmt;
   struct v4l2_requestbuffers req;
   int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   stopStream ();

   memset (&fmt, '\0', sizeof (fmt));
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width = size.width ();
   fmt.fmt.pix.height = size.height ();
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
   fmt.fmt.pix.field = V4L2_FIELD_ANY;
   if (xioctl (_fd, VIDIOC_S_FMT, &fmt) < 0)
      return problem (tr ("Cannot set the size of picture"), errno);

   // the driver gives the nearest it has, which must be what was asked
   if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG
       || (int)fmt.fmt.pix.width != size.width ()
       || (int)fmt.fmt.pix.height != size.height ())
      return tr ("The camera cannot send pictures of %1 x %2")
            .arg (size.width ()).arg (size.height ());

   memset (&req, '\0', sizeof (req));
   req.count = buffers;
   req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   req.memory = V4L2_MEMORY_MMAP;
   if (xioctl (_fd, VIDIOC_REQBUFS, &req) < 0)
      return problem (tr ("Cannot get memory for the pictures"), errno);

   for (unsigned i = 0; i < req.count; i++)
      {
      struct v4l2_buffer vbuf;
      Buffer buf;
      int err = 0;

      memset (&vbuf, '\0', sizeof (vbuf));
      vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      vbuf.memory = V4L2_MEMORY_MMAP;
      vbuf.index = i;
      if (xioctl (_fd, VIDIOC_QUERYBUF, &vbuf) < 0)
         err = errno;
      else
         {
         buf.length = vbuf.length;
         buf.start = mmap (NULL, vbuf.length, PROT_READ | PROT_WRITE,
                           MAP_SHARED, _fd, vbuf.m.offset);
         if (buf.start == MAP_FAILED)
            err = errno;
         else
            {
            _buffers << buf;
            if (xioctl (_fd, VIDIOC_QBUF, &vbuf) < 0)
               err = errno;
            }
         }
      if (err)
         {
         stopStream ();
         return problem (tr ("Cannot get memory for the pictures"), err);
         }
      }

   if (xioctl (_fd, VIDIOC_STREAMON, &type) < 0)
      {
      int err = errno;

      stopStream ();
      return problem (tr ("Cannot start the camera"), err);
      }
   _streaming = true;
   _stream_size = size;
   return QString ();
   }


V4l2Worker::Wait V4l2Worker::waitFrame (int ms, bool mind,
      QByteArray &frame, bool &damaged, QString &err)
   {
   QElapsedTimer timer;

   timer.start ();
   while (true)
      {
      {
      QMutexLocker locker (&_mutex);

      if (_quit)
         return Wait_asked;
      }
      if (mind && asked ())
         return Wait_asked;

      int left = ms - (int)timer.elapsed ();

      if (left <= 0)
         return Wait_timeout;

      struct pollfd pfd;

      pfd.fd = _fd;
      pfd.events = POLLIN;
      pfd.revents = 0;

      int ret = poll (&pfd, 1, qMin (left, POLL_MS));

      if (ret < 0 && errno != EINTR)
         {
         err = problem (tr ("Cannot wait for the camera"), errno);
         return Wait_error;
         }
      if (ret <= 0)
         continue;

      struct v4l2_buffer vbuf;

      memset (&vbuf, '\0', sizeof (vbuf));
      vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      vbuf.memory = V4L2_MEMORY_MMAP;
      if (xioctl (_fd, VIDIOC_DQBUF, &vbuf) < 0)
         {
         if (errno == EAGAIN)
            {
            /* nothing to take, yet the camera is ready: it has failed,
               or gone. Without this the loop would spin */
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
               {
               err = tr ("The camera is no longer connected");
               return Wait_error;
               }
            continue;
            }
         err = problem (tr ("Cannot get a picture from the camera"),
                        errno);
         return Wait_error;
         }
      if (vbuf.index >= (unsigned)_buffers.size ())
         {
         err = tr ("The camera gave a picture which was not asked for");
         return Wait_error;
         }

      const Buffer &buf = _buffers [vbuf.index];

      // copy it, since the driver has the buffer back for the next one
      frame = QByteArray ((const char *)buf.start,
                          (int)qMin ((size_t)vbuf.bytesused, buf.length));
      damaged = vbuf.flags & V4L2_BUF_FLAG_ERROR;
      xioctl (_fd, VIDIOC_QBUF, &vbuf);
      return Wait_frame;
      }
   }


void V4l2Worker::preview (const QSize &size)
   {
   QString err = startStream (size, PREVIEW_BUFFERS);
   QElapsedTimer sent;
   bool first = true;

   while (err.isEmpty ())
      {
      QByteArray frame, jpeg;
      bool damaged = false;
      Wait wait = waitFrame (STALL_MS, true, frame, damaged, err);

      if (wait == Wait_asked)
         break;
      if (wait == Wait_timeout)
         err = tr ("The camera has stopped sending pictures");
      if (wait != Wait_frame || damaged)
         continue;

      // no more often than can be shown, and only whole pictures
      if (!first && sent.elapsed () < PREVIEW_GAP_MS)
         continue;
      if (!cameraStill (frame, jpeg).isEmpty ())
         continue;

      QImage image = QImage::fromData (jpeg, "JPG");

      if (image.isNull ())
         continue;
      first = false;
      sent.start ();
      emit previewFrame (image);
      }
   stopStream ();
   if (!err.isEmpty ())
      {
      // do not try again until asked
      {
      QMutexLocker locker (&_mutex);

      _want_preview = false;
      }
      emit failed (err);
      }
   }


void V4l2Worker::still (const QSize &size)
   {
   /* The first pictures of a new size can be ones to leave: some
      cameras send a dark or half-exposed one as they start. How many to
      leave is a setting, since a camera which needs none takes its
      still sooner without */
   int skip = qBound (0, QSettings ().value ("camera/skip_frames", 0)
                         .toInt (), 10);
   QString err = startStream (size, STILL_BUFFERS);
   QString why;
   QByteArray jpeg;
   QElapsedTimer timer;

   timer.start ();
   while (err.isEmpty () && jpeg.isEmpty ())
      {
      QByteArray frame;
      QSize got;
      bool damaged = false, has_dht;
      int depth;
      Wait wait = waitFrame (STILL_MS - (int)timer.elapsed (), false,
                             frame, damaged, err);

      if (wait == Wait_asked)
         break;
      if (wait == Wait_timeout)
         err = why.isEmpty () ? tr ("The camera sent no picture") : why;
      if (wait != Wait_frame)
         continue;
      if (damaged)
         {
         why = tr ("The picture was damaged on its way from the camera");
         continue;
         }
      if (skip-- > 0)
         continue;

      // keep waiting for a whole one of the right size
      why = cameraStill (frame, jpeg);
      if (why.isEmpty () && !cameraJpegInfo (jpeg, got, depth,
                                             has_dht).isEmpty ())
         jpeg.clear ();
      else if (why.isEmpty () && got != size)
         {
         why = tr ("The camera sent a picture of the wrong size");
         jpeg.clear ();
         }
      }
   stopStream ();

   {
   QMutexLocker locker (&_mutex);

   _want_still = false;
   if (_quit)
      return;
   }
   if (!jpeg.isEmpty ())
      emit captured (jpeg);
   else
      emit failed (err);
   }


void V4l2Worker::run ()
   {
   while (true)
      {
      QSize preview_size, still_size;
      bool want_still;

      {
      QMutexLocker locker (&_mutex);

      while (!_quit && !_want_still && !_want_preview)
         _wake.wait (&_mutex);
      if (_quit)
         break;
      want_still = _want_still;
      still_size = _still_size;
      preview_size = _preview_size;
      }
      if (want_still)
         still (still_size);
      else
         preview (preview_size);
      }
   stopStream ();
   }


/***************************** V4l2Camera ********************************/

V4l2Camera::V4l2Camera (const QString &dev, QObject *parent)
      : Camera (parent), _dev (dev)
   {
   }


V4l2Camera::~V4l2Camera ()
   {
   close ();
   }


QList<CameraInfo> V4l2Camera::cameras (void)
   {
   QList<CameraInfo> list;
   QDir dir ("/dev");
   QStringList nodes = dir.entryList (QStringList () << "video*",
                                      QDir::System);
   static const QRegularExpression digits ("^video(\\d+)$");
   QMap<int, QString> ordered;

   for (const QString &node : nodes)
      {
      QRegularExpressionMatch match = digits.match (node);

      if (match.hasMatch ())
         ordered.insert (match.captured (1).toInt (),
                         dir.filePath (node));
      }
   for (const QString &dev : ordered)
      {
      CameraInfo info;
      QString err;
      int fd = open_camera (dev, info.name, err);

      if (fd < 0)
         continue;
      ::close (fd);
      info.id = dev;
      if (info.name.isEmpty ())
         info.name = dev;

      // two of the same kind need telling apart
      for (const CameraInfo &other : list)
         if (other.name == info.name)
            info.name = QString ("%1 (%2)").arg (info.name).arg (dev);
      list << info;
      }
   return list;
   }


bool V4l2Camera::present (void)
   {
   // what the kernel lists, which needs no device to be opened
   return !QDir ("/sys/class/video4linux")
         .entryList (QStringList () << "video*",
                     QDir::Dirs | QDir::NoDotAndDotDot).isEmpty ();
   }


QString V4l2Camera::open (void)
   {
   QString name, err;

   if (_fd >= 0)
      return QString ();
   _fd = open_camera (_dev, name, err);
   if (_fd < 0)
      return err;
   _sizes = mjpeg_sizes (_fd);
   if (_sizes.isEmpty ())
      {
      ::close (_fd);
      _fd = -1;
      return tr ("The camera does not say what size its pictures are");
      }

   _worker = new V4l2Worker (_fd, this);
   connect (_worker, &V4l2Worker::previewFrame, this,
            &Camera::previewFrame);
   connect (_worker, &V4l2Worker::captured, this, &Camera::captured);
   connect (_worker, &V4l2Worker::failed, this, &Camera::failed);
   _worker->start ();
   return QString ();
   }


void V4l2Camera::close (void)
   {
   if (_worker)
      {
      /* it looks at what is asked of it several times a second, and
         stops the camera sending before it finishes */
      _worker->quit ();
      _worker->wait ();
      delete _worker;
      _worker = nullptr;
      }
   if (_fd >= 0)
      ::close (_fd);
   _fd = -1;
   }


void V4l2Camera::startPreview (const QSize &still)
   {
   if (!_worker)
      {
      emit failed (tr ("The camera is not open"));
      return;
      }
   _worker->wantPreview (true, previewSize (_sizes, still));
   }


void V4l2Camera::stopPreview (void)
   {
   if (_worker)
      _worker->wantPreview (false, QSize ());
   }


void V4l2Camera::capture (const QSize &size)
   {
   if (!_worker)
      {
      emit failed (tr ("The camera is not open"));
      return;
      }
   _worker->wantStill (size);
   }
