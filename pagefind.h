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

#ifndef __pagefind_h
#define __pagefind_h

#include <QImage>
#include <QList>
#include <QPolygonF>
#include <QSize>

/** finds the page in a picture taken by a camera, and cuts it out

A document camera looks down on a page lying on a darker mat. The picture
it takes shows the mat around the page, the page a little turned and, as
the camera is seldom square above it, narrower at one end than the
other. An open book shows two pages, with the gutter between them.

Finding the page and cutting it out are kept apart. find() says where
the page is, as a shape with four corners for each page. rectify() makes
an upright picture of what is inside one of those. Something which maps
a page another way, to flatten its curve say, can take the place of
rectify() and use the same shapes.

None of this needs a display: it works on pictures alone */
class Pagefind
   {
public:
   //! how many pages to look for in a picture
   enum Mode
      {
      Mode_whole,    //!< none: the picture is kept as it is
      Mode_one,      //!< the picture shows one page
      Mode_two,      //!< it shows an open book, which is two
      Mode_auto,     //!< two if it shows a gutter and is wide, else one
      };

   //! what find() found
   struct Result
      {
      /** true if a page was found. If not, the rest means nothing and
          the picture is best kept whole */
      bool found = false;

      /** how sure this is, from 0 to 1: how much of the shape found is
          page rather than mat */
      double confidence = 0;

      /** true if what was found is flat, with four straight edges, so
          that the outline is its own four corners. If not, as with an
          open book whose pages curve, the outline is a box square to
          the picture which holds it, and cutting that out trims the
          picture without changing its shape */
      bool flat = false;

      /** the sheet, or the open book: its corners in the picture, in
          pixels, in the order top left, top right, bottom right, bottom
          left. A corner can lie outside the picture, when the page runs
          off its edge */
      QPolygonF outline;

      /** each page, in reading order, with its corners as for outline.
          There is one, the outline, or two which meet at the gutter */
      QList<QPolygonF> pages;

      //! true if a gutter was seen, whether or not the pages were split
      bool gutter = false;

      /** where the gutter meets the top and the bottom of the outline,
          as a proportion of the way along each from the left */
      double gutter_top = 0.5, gutter_bottom = 0.5;
      };

   /** find the page, or pages, in a picture

      \param picture  the picture, with the page the right way up, give
                      or take a turn of a few tens of degrees
      \param mode     how many pages to look for; with Mode_whole
                      nothing is looked for or found
      \returns what was found */
   static Result find (const QImage &picture, Mode mode);

   /** the size of picture rectify() makes of a shape, which is as wide
       as the longer of its top and bottom and as tall as the longer of
       its sides */
   static QSize rectifiedSize (const QPolygonF &quad);

   /** make an upright picture of a page

      The shape's corners go to the corners of the result, which puts
      right both the turn of the page and its narrowing with distance.
      What lies outside the picture comes out white.

      \param picture  the picture
      \param quad     the page's corners, as find() gives them
      \returns the page, or a null image if the shape is not usable */
   static QImage rectify (const QImage &picture, const QPolygonF &quad);

   /** find the pages in a picture and make an upright picture of each

      \param picture  the picture
      \param mode     how many pages to look for
      \param result   if not null, returns what was found
      \returns the pages in reading order, none if no page was found */
   static QList<QImage> pages (const QImage &picture, Mode mode,
                               Result *result = nullptr);

   //! the length of the longer side of the copy the page is looked for in
   enum { SearchSize = 1000 };
   };

#endif
