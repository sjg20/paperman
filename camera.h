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

#ifndef __camera_h
#define __camera_h

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSize>
#include <QString>

class QTimer;

//! a camera which can be opened
struct CameraInfo
   {
   QString id;     //!< what Camera::create() takes
   QString name;   //!< what to call it for the user
   };


/** a camera which pages are photographed with

A document camera looks down on a page, or an open book, and sends a
stream of pictures of it. Two things are wanted from it: a small picture
several times a second, to show what the camera sees, and now and then a
picture at the camera's full size, to keep as a page.

The camera sends each picture as a JPEG, and the still is handed over as
those same bytes, so that the page kept is the picture the camera took.

A camera is used from the thread it belongs to, which is the display's.
Asking for something returns at once and the answer arrives as a signal,
since a camera can take a second or more to change the size it sends, so
whatever waits for it must be kept off that thread */
class Camera : public QObject
   {
   Q_OBJECT

public:
   explicit Camera (QObject *parent = nullptr);
   ~Camera () override;

   /** the cameras there are

      These are the fake cameras if those are in use, see
      FakeCamera::use(), otherwise the platform's

      \returns each camera, an empty list if there are none or the
               platform has no way to reach one */
   static QList<CameraInfo> list (void);

   /** whether there may be a camera

      This opens nothing, so it is cheap enough to ask often and does
      not disturb a camera which is in use, but it can say yes when
      list() then finds no camera which can be used

      \returns true if list() is worth calling */
   static bool present (void);

   /** make the object for a camera, which starts off closed

      \param id      the camera, from list()
      \param parent  the object's parent
      \returns the camera, nullptr if there is none of that id */
   static Camera *create (const QString &id, QObject *parent = nullptr);

   /** open the camera and find out what it can do

      \returns an empty string if it is open, else what went wrong */
   virtual QString open (void) = 0;

   //! stop anything the camera is doing and let go of it
   virtual void close (void) = 0;

   virtual bool isOpen (void) const = 0;

   /** the sizes of still the camera can take
      \returns the sizes, the one with the most pixels first */
   virtual QList<QSize> stillSizes (void) const = 0;

   /** start sending small pictures, each with previewFrame()

      They carry on, but for while a still is being taken, until
      stopPreview() or close()

      \param still  the size of still to be taken, which the pictures
                    are to have the shape of where the camera can */
   virtual void startPreview (const QSize &still) = 0;

   virtual void stopPreview (void) = 0;

   /** take a still, which arrives with captured(), or failed() if it
       cannot be taken. A second one asked for before then is ignored

      \param size  the size wanted, one of stillSizes() */
   virtual void capture (const QSize &size) = 0;

   /** choose the size of the small pictures

      Of the sizes at least as wide as #PreviewWidth, the smallest with
      the shape of the still is wanted. Failing that, the smallest of
      that width whatever its shape, then the largest there is

      \param sizes   the sizes the camera has
      \param still   the size of still to be taken
      \returns the size to use, an invalid one if there are no sizes */
   static QSize previewSize (const QList<QSize> &sizes, const QSize &still);

   //! the least width wanted of the small pictures, in pixels
   enum { PreviewWidth = 640 };

signals:
   //! a small picture, the latest the camera has sent
   void previewFrame (const QImage &image);

   /** the still asked for with capture()
      \param jpeg   a whole JPEG file, see cameraStill() */
   void captured (const QByteArray &jpeg);

   /** a still the camera sent of its own accord, as when a button on
       it is pressed

       Such a camera takes the still itself and sends it among the small
       pictures, at whatever size it likes, which need not be one asked
       for with capture()

      \param jpeg   a whole JPEG file, see cameraStill() */
   void stillPushed (const QByteArray &jpeg);

   /** the picture which came straight after a still the camera sent of
       its own accord. It is not a view of the page as it is: a camera
       may light the page differently for it, so it is kept from
       previewFrame() and given here instead, for whatever can use it

      \param jpeg   a whole JPEG file, of the small pictures' size */
   void pushFollowed (const QByteArray &jpeg);

   /** the camera cannot do what was asked, or has stopped sending
      \param msg   what went wrong, for the user */
   void failed (const QString &msg);
   };


/** a camera which sends pictures it is given or makes up, for testing
    what uses a camera where there is none

Once use() has been called these are the only cameras there are, so that
a test neither sees nor disturbs a real camera on the machine */
class FakeCamera : public Camera
   {
   Q_OBJECT

public:
   //! a fake camera to offer
   struct Spec
      {
      QString id;
      QString name;
      QList<QSize> sizes;       //!< sizes of still, in any order

      /** the JPEGs to send as stills, in turn, going round again after
          the last, the small pictures showing the one to come. With
          none, a picture of the size asked for is made for each still,
          with its number drawn on it, and for each small picture */
      QList<QByteArray> stills;

      int capture_ms = 0;       //!< how long a still takes to arrive
      QString open_err;         //!< set to have open() fail with this
      QString capture_err;      //!< set to have each still fail with this
      };

   /** offer these cameras in place of the platform's
      \param specs   the cameras, which may be none at all */
   static void use (const QList<Spec> &specs);

   //! go back to offering the platform's cameras
   static void useReal (void);

   //! true if use() is in force
   static bool inUse (void);

   /** make a picture of a page, with a number drawn on it

      \param size   the size of picture
      \param num    the number to draw
      \returns the picture as a JPEG */
   static QByteArray makeJpeg (const QSize &size, int num);

   FakeCamera (const Spec &spec, QObject *parent);
   ~FakeCamera () override;

   QString open (void) override;
   void close (void) override;
   bool isOpen (void) const override { return _open; }
   QList<QSize> stillSizes (void) const override;
   void startPreview (const QSize &still) override;
   void stopPreview (void) override;
   void capture (const QSize &size) override;

   /** have the fake cameras of an id send a still of their own accord,
       as a camera whose button is pressed does. Only one which is
       sending small pictures does so

      \param id      the camera's id
      \param still   the still to send, as a camera may send it
      \param after   the picture to send straight after it, in place
                     of the next small picture; empty to use that one
      \returns true if a camera sent it */
   static bool push (const QString &id, const QByteArray &still,
                     const QByteArray &after = QByteArray ());

   //! the cameras on offer, for Camera::list()
   static QList<CameraInfo> cameras (void);

   //! the camera with an id, for Camera::create()
   static FakeCamera *make (const QString &id, QObject *parent);

private slots:
   void sendPreview (void);
   void sendStill (void);
   void sendPushed (void);

private:
   Spec _spec;
   bool _open = false;
   bool _capturing = false;
   QTimer *_preview;        //!< sends the small pictures
   QSize _preview_size;
   QSize _still_size;       //!< the size of the still being taken
   int _stills = 0;         //!< how many stills have been taken
   int _previews = 0;       //!< how many small pictures have been sent
   QImage _view;            //!< the small picture of the still to come
   int _shown = -1;         //!< which still that is, -1 if none
   QList<QByteArray> _pushed;   //!< stills waiting to be sent unasked
   QList<QByteArray> _after;    //!< and the picture to follow each
   bool _follow = false;    //!< the next small picture follows a still
   QByteArray _follow_with; //!< what to send as it, if not empty
   };


/** what a JPEG holds, from its headers alone

   \param jpeg     the JPEG
   \param size     returns the size of the picture in pixels
   \param depth    returns its bits per pixel: 8 for grey, 24 for colour
   \param has_dht  returns true if it has its Huffman tables
   \returns an empty string if this is a whole JPEG, from its start
            marker to its end marker with the picture's size before the
            picture's data, else what is wrong with it */
QString cameraJpegInfo (const QByteArray &jpeg, QSize &size, int &depth,
                        bool &has_dht);

/** make a JPEG file of a picture a camera has sent

   A camera sends its pictures as Motion-JPEG, in which each picture is
   a JPEG but may leave out the Huffman tables, the standard ones then
   being meant. A JPEG file must have them, so they are put in where
   they are missing. The buffer a picture arrives in can also be longer
   than the picture, and anything after its end marker is left off.

   Nothing of the picture itself is changed: its data is as the camera
   sent it.

   \param frame   the picture as the camera sent it
   \param jpeg    returns the JPEG file
   \returns an empty string if OK, else what is wrong with the picture */
QString cameraStill (const QByteArray &frame, QByteArray &jpeg);

#endif
