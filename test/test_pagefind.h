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

#ifndef TEST_PAGEFIND_H
#define TEST_PAGEFIND_H

#include <QImage>
#include <QPolygonF>

#include "suite.h"

class TestPagefind : public Suite
{
   Q_OBJECT

public:
   using Suite::Suite;

   //! a dark mat, with a little texture, as a camera sees one
   static QImage mat(const QSize &size);

   /** draw a page on a picture, as a camera sees it

      The page has lines of 'text', a red square at its top left and a
      blue one at its bottom right

      \param image   the picture to draw on
      \param quad    where the page's corners are in the picture: top
                     left, top right, bottom right, bottom left
      \param size    the size of the page itself, before it is drawn
                     turned and narrowed
      \param gutter  where to draw a gutter down it, as a proportion of
                     its width, or 0 for none */
   static void drawPage(QImage &image, const QPolygonF &quad,
                        const QSize &size, double gutter = 0);

   /** the middle of what is red, or blue, in a picture
      \param red   true for red, false for blue
      \returns the middle, or (-1, -1) if there is none */
   static QPointF markAt(const QImage &image, bool red);

private:
   //! how far the corners found are from where they should be, at most
   static double cornerError(const QPolygonF &found, const QPolygonF &want);

private slots:
   //! a sheet which is turned and narrows is found by its corners
   void testSheet();

   //! and comes out upright, with what is on it where it should be
   void testSheetRectified();

   //! a shadow over an edge and a finger across one do not move it
   void testNibbled();

   //! a sheet which runs off the picture ends at the picture's edge
   void testOffEdge();

   //! an open book is split at its gutter, left page first
   void testBook();

   //! how many pages to look for is as asked
   void testModes();

   //! a book whose pages curve is given a box, and still split
   void testCurvedBook();

   //! a bare mat, a picture which is all page and no picture: no page
   void testNotFound();

   //! a shape is cut out upright, at the size of its sides
   void testRectify();
};

#endif
