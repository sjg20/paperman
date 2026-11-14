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

   //! how a book drawn by curvedBook() is made
   struct Book {
      double focal = 0.7;    //!< the lens's focal length, of the width
      double rise = 100;     //!< how far each page rises, in pixels
      int width = 500;       //!< how wide each page looks, lying flat
      int height = 760;      //!< how tall it is
      int lower = 0;         //!< how far below the middle the book lies
      bool flap = false;     //!< true to put a jacket's flap by each page
      bool level = false;    //!< true if the flap is as light as the page
                             //!< and its head is level with the page's
      int block = 0;         //!< how wide the pages beneath show beside
                             //!< each page, 0 if they do not
      double lift = 0;       //!< how far the left page's outer edge is
                             //!< lifted above the gutter, in pixels
      double turn = 0;       //!< how far the book is turned, in degrees
      int flap_wide = 0;     //!< how wide the flap is, 0 to leave it be
   };

   //! how far apart the lines down a page of curvedBook() are
   enum { RULE = 40 };

   /** draw an open book whose pages curve, as a camera above it sees it

      Each page rises from the gutter and falls to its outer edge. It has
      a row of marks across it near its head, its middle and its foot,
      one every RULE pixels of its surface from the gutter, and a line
      across it at a quarter and at three quarters of its height.

      \param image   the picture to draw on
      \param book    how to make the book
      \param left    if not null, returns the width of the left page's
                     surface, which is the greater if that page is lifted
      \returns the width of the right page's surface, which is more than
               how wide it looks */
   static double curvedBook(QImage &image, const Book &book,
                            double *left = nullptr);

private:
   /** check a page of curvedBook() which has been made flat

      \param page     the page
      \param left     true if it is the left page
      \param surface  the width of its surface, from curvedBook()
      \param book     how the book was made
      \param within   how far a line may be from its place, as a
                      proportion of the page's width
      \returns what is wrong with it, empty if nothing */
   static QString checkFlat(const QImage &page, bool left, double surface,
                            const Book &book, double within);

   /** see how far a page of curvedBook() which has been made flat leans

      \param page     the page
      \returns how far the marks near its head are from those near its
               foot, at most, as a proportion of its width; 1 if they
               cannot be matched up */
   static double markLean(const QImage &page);

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

   //! a gutter which is only a thin, broken line is seen among print
   void testThinGutter();

   //! a book whose pages curve is given a box, and still split
   void testCurvedBook();

   //! a book whose pages curve comes out flat, without its jacket
   void testFlatten();

   //! the two pages of a book come out at one scale
   void testFlattenSpread();

   //! and does so where its foot runs off the picture
   void testFlattenOffEdge();

   //! what lies beside a page and level with it is not taken for page
   void testSheetEdge();

   //! the line of a gutter which leans is found closely
   void testGutterLine();

   //! edges which end a little apart are made level with the gutter
   void testLevelEnds();

   //! edges which cannot be used give nothing
   void testFlattenBad();

   //! a bare mat, a picture which is all page and no picture: no page
   void testNotFound();

   //! a shape is cut out upright, at the size of its sides
   void testRectify();
};

#endif
