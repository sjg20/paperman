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

#ifndef __laserlines_h
#define __laserlines_h

#include <QImage>
#include <QLineF>
#include <QSize>
#include <QString>
#include <QVector>

/** reads the height of a page from lines of light shone across it

Some document cameras shine a few lines of red light across what lies
beneath them, from beside the lens, and send a picture of that with each
picture of the page. On the mat the lines are straight. Where something
stands above the mat a line is seen moved towards the light, the further
the higher it stands, so the lines show the shape of a book's pages.

trace() finds the lines in such a picture. mat() says whether they are
as they fall on the bare mat, and where that is, which has to be known
before anything else can be measured. rise() gives the height of a page
across its width.

How far a line moves for a given height depends on where the light is,
which is not known. But the part of a page which stands nearer the
camera also looks taller, and that says how high it is too. rise() uses
the two together: how tall the page looks across its width gives the
measure for each line, and the lines then give the height, where the
page's edges show it well and where they do not.

None of this needs a display: it works on pictures alone */
class Laserlines
   {
public:
   //! the number of lines looked for
   enum { Count = 3 };

   //! the lines found in a picture
   struct Lines
      {
      //! true if any part of the lines was found
      bool found = false;

      //! the size of the picture they were found in
      QSize size;

      /** how far down the picture each line is, in pixels, for each
          column of the picture: the lines in order from the top. A
          column where the lines were not all seen has NaN */
      QVector<double> row [Count];
      };

   //! where the lines fall on the bare mat
   struct Mat
      {
      //! true if this is known
      bool valid = false;

      /** each line, as how far down the picture it is at the left, and
          how much further down it is at the right, both as a proportion
          of the picture's height */
      double at [Count] = {}, slope [Count] = {};

      //! \returns how far down a picture of height h line n is at x of w
      double down (int n, double x, int w, int h) const
         { return (at [n] + slope [n] * x / w) * h; }

      //! \returns this as text, to keep in the settings: empty if unknown
      QString toString (void) const;

      //! \returns what toString() gave, not valid if it cannot be read
      static Mat fromString (const QString &str);
      };

   /** find the lines in a picture

      \param frame   the picture, taken with the lines on
      \returns the lines, not found if there are none to be seen */
   static Lines trace (const QImage &frame);

   /** see whether lines are as they fall on a bare mat: straight, close
       to level and seen across most of the picture

      \param lines   the lines found
      \param mat     returns where they are, if so
      \returns true if they are */
   static bool mat (const Lines &lines, Mat &mat);

   /** find how high a page stands across its width

      \param lines   the lines found across the page
      \param mat     where the lines fall on the bare mat
      \param ups     lines up the page, from its top edge to its bottom
                     one, in order across it, in the pixels of the
                     picture of the page: level with the gutter, so that
                     the page is equally high all along each
      \param size    the size of that picture, which shows the same view
                     as the one the lines were found in
      \returns the height at each of ups, as a proportion of the height
               of the camera above the mat; empty if it cannot be told */
   static QVector<double> rise (const Lines &lines, const Mat &mat,
                                const QVector<QLineF> &ups,
                                const QSize &size);
   };

#endif
