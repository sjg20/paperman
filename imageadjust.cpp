/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
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

#include <QThread>
#include <QtConcurrent>

#include "imageadjust.h"


void ImageAdjust::apply (QImage &image, e_adjust type)
   {
   switch (type)
      {
      case Adjust_whiten:
         whitenBackground (image);
         break;
      default:
         break;
      }
   }


QString ImageAdjust::name (e_adjust type)
   {
   switch (type)
      {
      case Adjust_whiten:
         return "Whiten background";
      default:
         return "Unknown";
      }
   }


QString ImageAdjust::suffix (e_adjust type)
   {
   switch (type)
      {
      case Adjust_whiten:
         return "_white";
      default:
         return "_adj";
      }
   }


/** a row range for parallel processing */
struct RowRange
   {
   int start;
   int end;
   };


/** split rows into ranges, one per thread */
static QVector<RowRange> splitRows (int height, int nthreads)
   {
   QVector<RowRange> ranges;
   int rows_per = height / nthreads;
   int extra = height % nthreads;
   int y = 0;

   for (int t = 0; t < nthreads && y < height; t++)
      {
      RowRange r;
      r.start = y;
      r.end = y + rows_per + (t < extra ? 1 : 0);
      ranges.append (r);
      y = r.end;
      }
   return ranges;
   }


void ImageAdjust::whitenBackground (QImage &image)
   {
   int width = image.width ();
   int height = image.height ();

   if (width == 0 || height == 0)
      return;

   // 1bpp images are already black and white
   if (image.depth () == 1)
      return;

   // indexed images have a small colour table; just process it directly
   if (image.format () == QImage::Format_Indexed8)
      {
      QVector<QRgb> table = image.colorTable ();
      int counts[256] = {};
      int lum_hist[256] = {};

      for (int y = 0; y < height; y++)
         {
         const uchar *line = image.constScanLine (y);
         for (int x = 0; x < width; x++)
            counts[line[x]]++;
         }

      // build luminance histogram and per-bin RGB sums
      long long rsum[256] = {}, gsum[256] = {}, bsum[256] = {};
      for (int i = 0; i < table.size () && i < 256; i++)
         {
         int r = qRed (table[i]), g = qGreen (table[i]), b = qBlue (table[i]);
         int lum = (r * 299 + g * 587 + b * 114) / 1000;
         lum_hist[lum] += counts[i];
         rsum[lum] += (long long)r * counts[i];
         gsum[lum] += (long long)g * counts[i];
         bsum[lum] += (long long)b * counts[i];
         }

      // find background luminance peak in upper half
      int bg_peak = 128;
      int bg_count = 0;
      for (int i = 128; i < 256; i++)
         {
         if (lum_hist[i] > bg_count)
            {
            bg_count = lum_hist[i];
            bg_peak = i;
            }
         }

      // measure the actual RGB of background pixels (near the peak)
      long long tr = 0, tg = 0, tb = 0, tn = 0;
      for (int i = std::max (bg_peak - 5, 0);
           i <= std::min (bg_peak + 5, 255); i++)
         {
         tr += rsum[i];
         tg += gsum[i];
         tb += bsum[i];
         tn += lum_hist[i];
         }

      if (tn == 0)
         return;

      int white_pt[3];
      white_pt[0] = std::min ((int)(tr / tn) + 5, 255);
      white_pt[1] = std::min ((int)(tg / tn) + 5, 255);
      white_pt[2] = std::min ((int)(tb / tn) + 5, 255);

      // find the black point at the 0.5th percentile of luminance
      long total = (long)width * height;
      long threshold = total / 200;
      long cumul = 0;
      int black_point = 0;
      for (int i = 0; i < 256; i++)
         {
         cumul += lum_hist[i];
         if (cumul >= threshold)
            {
            black_point = i;
            break;
            }
         }

      // build per-channel LUTs
      uchar lut[3][256];
      for (int ch = 0; ch < 3; ch++)
         {
         if (white_pt[ch] - black_point < 20)
            {
            for (int i = 0; i < 256; i++)
               lut[ch][i] = i;
            continue;
            }
         for (int i = 0; i < 256; i++)
            {
            if (i <= black_point)
               lut[ch][i] = 0;
            else if (i >= white_pt[ch])
               lut[ch][i] = 255;
            else
               lut[ch][i] = (uchar)((i - black_point) * 255
                                     / (white_pt[ch] - black_point));
            }
         }

      for (int i = 0; i < table.size (); i++)
         {
         int r = lut[0][qRed (table[i])];
         int g = lut[1][qGreen (table[i])];
         int b = lut[2][qBlue (table[i])];
         int a = qAlpha (table[i]);
         table[i] = qRgba (r, g, b, a);
         }
      image.setColorTable (table);
      return;
      }

   // convert to 32-bit if needed for uniform pixel access
   if (image.format () != QImage::Format_RGB32 &&
       image.format () != QImage::Format_ARGB32)
      image = image.convertToFormat (QImage::Format_RGB32);

   int nthreads = QThread::idealThreadCount ();
   if (nthreads < 1)
      nthreads = 1;

   // skip colour pages: count pixels with high saturation (max-min > 30)
   // and bail out if more than 20% of the page is colourful
   QVector<RowRange> ranges = splitRows (height, nthreads);
   QAtomicInt colour_count (0);

   QtConcurrent::blockingMap (ranges,
      [&image, &colour_count, width] (const RowRange &range)
      {
      int local = 0;
      for (int y = range.start; y < range.end; y++)
         {
         const QRgb *line = (const QRgb *)image.constScanLine (y);
         for (int x = 0; x < width; x++)
            {
            int r = qRed (line[x]);
            int g = qGreen (line[x]);
            int b = qBlue (line[x]);
            int hi = std::max ({r, g, b});
            int lo = std::min ({r, g, b});
            if (hi - lo > 30)
               local++;
            }
         }
      colour_count.fetchAndAddRelaxed (local);
      });

   long npix_total = (long)width * height;
   if (colour_count.loadRelaxed () > npix_total / 5)
      return;

   // divide-by-background: blur the image to estimate local illumination,
   // then divide each pixel by its local background to normalise lighting.
   // This handles gradients, colour casts and scanner edge effects.

   // use a box blur radius large enough to smooth over text (~3% of image)
   int radius = std::max (width, height) / 30;
   if (radius < 2)
      radius = 2;

   // allocate per-channel sum buffers for the separable box blur
   int npix = width * height;
   QVector<uint> blur_r (npix), blur_g (npix), blur_b (npix);

   // horizontal pass: compute running sums along each row (sequential
   // prefix sums, but rows are independent so we parallelise across rows)

   QtConcurrent::blockingMap (ranges,
      [&image, &blur_r, &blur_g, &blur_b, width, radius]
      (const RowRange &range)
      {
      for (int y = range.start; y < range.end; y++)
         {
         const QRgb *line = (const QRgb *)image.constScanLine (y);
         int off = y * width;

         for (int x = 0; x < width; x++)
            {
            int x0 = std::max (x - radius, 0);
            int x1 = std::min (x + radius, width - 1);
            int n = x1 - x0 + 1;
            uint sr = 0, sg = 0, sb = 0;

            for (int xi = x0; xi <= x1; xi++)
               {
               sr += qRed (line[xi]);
               sg += qGreen (line[xi]);
               sb += qBlue (line[xi]);
               }
            blur_r[off + x] = sr / n;
            blur_g[off + x] = sg / n;
            blur_b[off + x] = sb / n;
            }
         }
      });

   // vertical pass: blur the horizontal averages along columns
   QVector<uint> bg_r (npix), bg_g (npix), bg_b (npix);

   // parallelise across column ranges
   QVector<RowRange> col_ranges = splitRows (width, nthreads);

   QtConcurrent::blockingMap (col_ranges,
      [&blur_r, &blur_g, &blur_b, &bg_r, &bg_g, &bg_b,
       width, height, radius]
      (const RowRange &range)
      {
      for (int x = range.start; x < range.end; x++)
         {
         for (int y = 0; y < height; y++)
            {
            int y0 = std::max (y - radius, 0);
            int y1 = std::min (y + radius, height - 1);
            int n = y1 - y0 + 1;
            uint sr = 0, sg = 0, sb = 0;

            for (int yi = y0; yi <= y1; yi++)
               {
               int off = yi * width + x;
               sr += blur_r[off];
               sg += blur_g[off];
               sb += blur_b[off];
               }
            int off = y * width + x;
            bg_r[off] = std::max (sr / n, 1u);
            bg_g[off] = std::max (sg / n, 1u);
            bg_b[off] = std::max (sb / n, 1u);
            }
         }
      });

   // free the intermediate buffers
   blur_r.clear ();
   blur_g.clear ();
   blur_b.clear ();

   // apply: divide each pixel by its local background, scaling to 255;
   // also build a luminance histogram for the contrast stretch below
   ranges = splitRows (height, nthreads);

   struct ThreadHist { int hist[256]; };
   QVector<ThreadHist> thread_hists (ranges.size ());
   for (int t = 0; t < ranges.size (); t++)
      memset (thread_hists[t].hist, 0, sizeof (thread_hists[t].hist));

   QtConcurrent::blockingMap (ranges,
      [&image, &bg_r, &bg_g, &bg_b, &thread_hists, &ranges, width]
      (const RowRange &range)
      {
      int idx = &range - ranges.constData ();
      int *hist = thread_hists[idx].hist;

      for (int y = range.start; y < range.end; y++)
         {
         QRgb *line = (QRgb *)image.scanLine (y);
         int off = y * width;

         for (int x = 0; x < width; x++)
            {
            int r = std::min (qRed (line[x]) * 255u / bg_r[off + x], 255u);
            int g = std::min (qGreen (line[x]) * 255u / bg_g[off + x], 255u);
            int b = std::min (qBlue (line[x]) * 255u / bg_b[off + x], 255u);
            line[x] = qRgba (r, g, b, qAlpha (line[x]));
            int lum = (r * 299 + g * 587 + b * 114) / 1000;
            hist[lum]++;
            }
         }
      });

   // free the background buffers
   bg_r.clear ();
   bg_g.clear ();
   bg_b.clear ();

   // merge luminance histograms
   int lum_hist[256] = {};
   for (int t = 0; t < ranges.size (); t++)
      for (int i = 0; i < 256; i++)
         lum_hist[i] += thread_hists[t].hist[i];

   // find the black point at the 0.5th percentile
   long total = (long)width * height;
   long threshold = total / 200;
   long cumul = 0;
   int black_point = 0;
   for (int i = 0; i < 256; i++)
      {
      cumul += lum_hist[i];
      if (cumul >= threshold)
         {
         black_point = i;
         break;
         }
      }

   // contrast stretch: map [black_point, 255] → [0, 255]
   if (black_point > 2)
      {
      uchar lut[256];
      for (int i = 0; i < 256; i++)
         {
         if (i <= black_point)
            lut[i] = 0;
         else
            lut[i] = std::min ((i - black_point) * 255
                               / (255 - black_point), 255);
         }

      QtConcurrent::blockingMap (ranges,
         [&image, &lut, width] (const RowRange &range)
         {
         for (int y = range.start; y < range.end; y++)
            {
            QRgb *line = (QRgb *)image.scanLine (y);
            for (int x = 0; x < width; x++)
               {
               int r = lut[qRed (line[x])];
               int g = lut[qGreen (line[x])];
               int b = lut[qBlue (line[x])];
               line[x] = qRgba (r, g, b, qAlpha (line[x]));
               }
            }
         });
      }
   }
