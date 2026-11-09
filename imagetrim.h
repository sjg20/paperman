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

#ifndef __imagetrim_h
#define __imagetrim_h

#include <QImage>
#include <QRect>
#include <QRectF>

/** works out which part of a page to keep when it is cropped */
class Imagetrim
   {
public:
   /** find the part of a scanned page worth keeping

      A dark band along an edge is the scanner's background showing
      around the sheet, so that side is cut at the edge of the paper.
      A side with no band is cut just outside whatever is printed there.

      \param image   the page
      \returns the area to keep, or a null rectangle if the page has
               nothing on it to go by */
   static QRect contentBounds (const QImage &image);

   /** turn an area given as proportions of a page into pixels

      The result is rounded outwards, so nothing asked for is lost.

      \param size    size of the page in pixels
      \param area    the area, each value from 0 to 1
      \returns the area in pixels, always within the page */
   static QRect areaToRect (const QSize &size, const QRectF &area);

   /** turn a rectangle in pixels into proportions of a page */
   static QRectF rectToArea (const QSize &size, const QRect &rect);

   /** is this a usable area, inside the page and not empty? */
   static bool areaValid (const QRectF &area);

   /** the pixels of a page to keep

      \param image   the page
      \param area    proportions to keep, or a null rectangle to fit the
                     page's content
      \returns the part to keep, or a null rectangle if the page should
               be left as it is */
   static QRect cropRect (const QImage &image, const QRectF &area);
   };

#endif
