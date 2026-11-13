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

#ifndef __camerapages_h
#define __camerapages_h

#include <QAtomicInt>
#include <QByteArray>
#include <QByteArrayList>
#include <QObject>
#include <QThread>

#include "pagefind.h"

/** does the work on one picture at a time, in the thread it is moved to */
class CameraPagesWorker : public QObject
   {
   Q_OBJECT

public:
   /** \param current   the number of the run of pictures now wanted: a
                        picture of an earlier run is passed over */
   explicit CameraPagesWorker (const QAtomicInt *current);

public slots:
   void process (int run, const QByteArray &jpeg, int mode, int quality,
                 double focal);

signals:
   void done (int run, const QByteArrayList &pages, bool found);

private:
   const QAtomicInt *_current;
   };


/** makes pages of the pictures a camera takes, without holding up the
    display or the next picture

Each picture is found its page, or pages, which are cut out and made
upright, and flat if they curve: see Pagefind. That means decoding the
camera's JPEG, working on it and encoding each page as a JPEG again,
which for a large picture takes long enough to notice. So it is done in
a thread of its own, one picture at a time and in the order they were
given, and the pages of each arrive with processed() in that same order.

What waits to be done is kept as the JPEG the camera sent, which is
small. Only the picture being worked on is held decoded, so the memory
used does not grow with the number waiting */
class CameraPages : public QObject
   {
   Q_OBJECT

public:
   explicit CameraPages (QObject *parent = nullptr);
   ~CameraPages () override;

   /** give it a picture to make pages of

      \param jpeg     the picture, a whole JPEG file
      \param mode     how many pages to look for in it; with
                      Pagefind::Mode_whole it goes through as it is, in
                      its turn
      \param quality  the JPEG quality to encode each page with
      \param focal    the focal length of the camera's lens, to flatten
                      a curved page with: see Pagefind::flatten() */
   void add (const QByteArray &jpeg, Pagefind::Mode mode, int quality,
             double focal = Pagefind::DefaultFocal);

   //! how many pictures have been given whose pages have yet to arrive
   int pending (void) const { return _pending; }

   /** forget the pictures which are waiting: their pages never arrive.
       The one being worked on is finished, and then thrown away */
   void abandon (void);

   /** make the pages of a picture, here and now

      \param jpeg     the picture, a whole JPEG file
      \param mode     how many pages to look for
      \param quality  the JPEG quality to encode each page with
      \param found    returns true if a page was found
      \param focal    the focal length to flatten a curved page with
      \returns each page as a JPEG, in reading order. If no page was
               found, or none was looked for, this is the picture
               itself, untouched */
   static QByteArrayList process (const QByteArray &jpeg,
                                  Pagefind::Mode mode, int quality,
                                  bool &found,
                                  double focal = Pagefind::DefaultFocal);

signals:
   /** the pages of the next picture
      \param pages   as process() gives them
      \param found   true if a page was found in the picture */
   void processed (const QByteArrayList &pages, bool found);

   //! for the worker: a picture to do
   void work (int run, const QByteArray &jpeg, int mode, int quality,
              double focal);

private slots:
   void workDone (int run, const QByteArrayList &pages, bool found);

private:
   QThread _thread;
   CameraPagesWorker *_worker;
   QAtomicInt _run;         //!< goes up each time the pictures are forgotten
   int _pending = 0;
   };

#endif
