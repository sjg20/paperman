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
#include <cmath>
#include <vector>

#include "imagetrim.h"

enum
   {
   WORK_SIZE = 1000,    //!< longest side to look at a page in, in pixels
   INK_LEVEL = 40,      //!< how far from the paper's shade counts as ink
   };

//! a row or column this full of ink, at the edge, is scanner background
static const double BAND_FILL = 0.6;

//! most of the page which scanner background can take up on each side
static const double BAND_MAX = 0.25;

//! a row or column with less ink than this is only specks
static const double SPECK_FILL = 0.003;

//! room to leave around the content, as a proportion of the longer side
static const double PADDING = 0.01;


/* the shade that half the pixels in the histogram are at or below */
static int median_shade (const std::vector<long> &hist)
   {
   long total = 0, seen = 0;

   for (long count : hist)
      total += count;
   for (int shade = 0; shade < 256; shade++)
      {
      seen += hist [shade];
      if (seen * 2 >= total)
         return shade;
      }
   return 255;
   }


/* the shade of the paper. Usually most of a page is paper, but a page
   covered in colour has it only around the edge, and a sheet on a dark
   background has it everywhere but the edge. Paper is light, so it is
   whichever of those two is lighter */
static int paper_shade (const QImage &grey)
   {
   std::vector<long> page (256), edge (256);
   int width = grey.width (), height = grey.height ();
   int ring = std::max (2, int (std::max (width, height) * PADDING));

   for (int y = 0; y < height; y++)
      {
      const uchar *line = grey.constScanLine (y);
      bool end = y < ring || y >= height - ring;

      for (int x = 0; x < width; x++)
         {
         page [line [x]]++;
         if (end || x < ring || x >= width - ring)
            edge [line [x]]++;
         }
      }
   return std::max (median_shade (page), median_shade (edge));
   }


/* how much ink is in each row, or in each column, of an area */
static std::vector<int> count_ink (const QImage &grey, int paper,
                                   const QRect &area, bool rows)
   {
   std::vector<int> count (rows ? grey.height () : grey.width ());

   for (int y = area.top (); y <= area.bottom (); y++)
      {
      const uchar *line = grey.constScanLine (y);

      for (int x = area.left (); x <= area.right (); x++)
         if (std::abs (line [x] - paper) > INK_LEVEL)
            count [rows ? y : x]++;
      }
   return count;
   }


/* find how far scanner background reaches in from each end of a row of
   counts. first and last come back as the first and last lines of paper,
   and are unchanged at an end which has no background */
static void find_bands (const std::vector<int> &count, int across, int &first,
                        int &last, bool &band_first, bool &band_last)
   {
   int limit = int (count.size () * BAND_MAX);
   int full = int (across * BAND_FILL);
   int n = int (count.size ());
   int upto;

   for (upto = 0; upto < limit && count [upto] > full; upto++)
      ;
   band_first = upto > 0 && upto < limit;
   if (band_first)
      first = upto;

   for (upto = 0; upto < limit && count [n - 1 - upto] > full; upto++)
      ;
   band_last = upto > 0 && upto < limit;
   if (band_last)
      last = n - 1 - upto;
   }


/* find the first and last lines with something on them. A line only
   counts if the next one does as well, which leaves out dust */
static bool find_content (const std::vector<int> &count, int across, int lo,
                          int hi, int &first, int &last)
   {
   int need = std::max (2, int (across * SPECK_FILL));
   int y;

   for (y = lo; y < hi; y++)
      if (count [y] >= need && count [y + 1] >= need)
         break;
   if (y >= hi)
      return false;
   first = y;

   for (y = hi; y > lo; y--)
      if (count [y] >= need && count [y - 1] >= need)
         break;
   last = y;
   return true;
   }


QRect Imagetrim::contentBounds (const QImage &image)
   {
   if (image.isNull ())
      return QRect ();

   QImage grey = image.convertToFormat (QImage::Format_Grayscale8);
   int longest = std::max (grey.width (), grey.height ());

   /* smooth scaling keeps thin print visible and softens noise, as well
      as saving time on a large page */
   if (longest > WORK_SIZE)
      grey = grey.scaled (std::max (1, grey.width () * WORK_SIZE / longest),
                          std::max (1, grey.height () * WORK_SIZE / longest),
                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat (QImage::Format_Grayscale8);  // Qt 5 widens it

   int width = grey.width (), height = grey.height ();
   int paper = paper_shade (grey);
   int left = 0, right = width - 1, top = 0, bottom = height - 1;
   bool band_left, band_right, band_top, band_bottom;

   std::vector<int> cols = count_ink (grey, paper, grey.rect (), false);

   find_bands (cols, height, left, right, band_left, band_right);

   // bands down the sides should not count towards those along the ends
   QRect paper_area (QPoint (left, 0), QPoint (right, height - 1));
   std::vector<int> rows = count_ink (grey, paper, paper_area, true);

   find_bands (rows, right - left + 1, top, bottom, band_top, band_bottom);
   paper_area = QRect (QPoint (left, top), QPoint (right, bottom));

   // look for the content on the paper alone
   cols = count_ink (grey, paper, paper_area, false);
   rows = count_ink (grey, paper, paper_area, true);

   int first_col, last_col, first_row, last_row;
   bool found = find_content (cols, paper_area.height (), left, right,
                              first_col, last_col)
             && find_content (rows, paper_area.width (), top, bottom,
                              first_row, last_row);
   bool banded = band_left || band_right || band_top || band_bottom;

   if (!found && !banded)
      return QRect ();

   if (found)
      {
      int pad = std::max (1, int (std::max (width, height) * PADDING));

      if (!band_left)
         left = std::max (left, first_col - pad);
      if (!band_right)
         right = std::min (right, last_col + pad);
      if (!band_top)
         top = std::max (top, first_row - pad);
      if (!band_bottom)
         bottom = std::min (bottom, last_row + pad);
      }

   // back to the page's own pixels, rounding outwards
   double xscale = double (image.width ()) / width;
   double yscale = double (image.height ()) / height;
   QRect rect (QPoint (int (std::floor (left * xscale)),
                       int (std::floor (top * yscale))),
               QPoint (int (std::ceil ((right + 1) * xscale)) - 1,
                       int (std::ceil ((bottom + 1) * yscale)) - 1));

   return rect & image.rect ();
   }


QRect Imagetrim::areaToRect (const QSize &size, const QRectF &area)
   {
   int left = int (std::floor (area.left () * size.width () + 1e-6));
   int top = int (std::floor (area.top () * size.height () + 1e-6));
   int right = int (std::ceil (area.right () * size.width () - 1e-6));
   int bottom = int (std::ceil (area.bottom () * size.height () - 1e-6));

   // keep at least one pixel, however thin the area is
   left = std::min (std::max (left, 0), size.width () - 1);
   top = std::min (std::max (top, 0), size.height () - 1);
   right = std::max (std::min (right, size.width ()), left + 1);
   bottom = std::max (std::min (bottom, size.height ()), top + 1);
   return QRect (left, top, right - left, bottom - top);
   }


QRectF Imagetrim::rectToArea (const QSize &size, const QRect &rect)
   {
   return QRectF (double (rect.x ()) / size.width (),
                  double (rect.y ()) / size.height (),
                  double (rect.width ()) / size.width (),
                  double (rect.height ()) / size.height ());
   }


bool Imagetrim::areaValid (const QRectF &area)
   {
   return area.width () > 0 && area.height () > 0 && area.left () >= 0
       && area.top () >= 0 && area.right () <= 1 && area.bottom () <= 1;
   }


QRect Imagetrim::cropRect (const QImage &image, const QRectF &area)
   {
   if (image.isNull ())
      return QRect ();

   QRect rect = area.isNull () ? contentBounds (image)
                               : areaToRect (image.size (), area);

   // no change is the same as nothing to do
   return rect == image.rect () ? QRect () : rect;
   }
