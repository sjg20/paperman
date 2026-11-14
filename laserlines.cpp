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

#include <QStringList>

#include "laserlines.h"

//! how much redder than what is above and below it a line must be
static const int LINE_LEAST = 10;

/** how far above and below a line to look for what it is redder than,
    and how far apart two lines must be, as a proportion of the height */
static const double LINE_BESIDE = 0.0125, LINE_APART = 0.03;

//! the most things in a column which may be lines of light
static const int PEAKS = 8;

/** how many columns on each side of one to look in for more of a line,
    and how far up or down it may be there: this much, and this much
    more for each column away */
static const int JOIN_REACH = 2;
static const double JOIN_CLOSE = 1.0, JOIN_SLOPE = 1.5;

//! how far a line must run, as a proportion of the picture's width
static const double LINE_RUN = 0.04;

//! how far from its middle a line's light is weighed, in pixels
static const int LINE_HALF = 3;

//! how much of the picture's width the lines on a mat must be seen over
static const double MAT_SEEN = 0.4;

/** how far a line on the mat may be from straight, as a proportion of
    the height, and how much of what was seen of it must be that close */
static const double MAT_STRAIGHT = 0.004, MAT_AGREE = 0.85;

//! how far a line on the mat may slope, as a proportion of the height
static const double MAT_SLOPE = 0.05;

//! how much of a page a line must cross to be of use
static const double RISE_SEEN = 0.35;

/** how much a line must move across a page for its measure to be told,
    as a proportion of the height of its picture */
static const double RISE_MOVES = 0.008;

/** how far from the rest a point may be and still be used to find a
    line's measure, as a proportion of how tall the page looks */
static const double RISE_FITS = 0.004;

//! the most a page can rise, as a proportion of the camera's height
static const double RISE_MOST = 0.6;


QString Laserlines::Mat::toString (void) const
   {
   QStringList parts;

   if (!valid)
      return QString ();
   for (int i = 0; i < Count; i++)
      parts << QString::number (at [i], 'g', 8)
            << QString::number (slope [i], 'g', 8);
   return parts.join (',');
   }


Laserlines::Mat Laserlines::Mat::fromString (const QString &str)
   {
   QStringList parts = str.split (',');
   Mat mat;

   if (parts.size () != 2 * Count)
      return mat;
   for (int i = 0; i < Count; i++)
      {
      bool ok_at, ok_slope;

      mat.at [i] = parts [i * 2].toDouble (&ok_at);
      mat.slope [i] = parts [i * 2 + 1].toDouble (&ok_slope);
      if (!ok_at || !ok_slope || mat.at [i] < 0 || mat.at [i] > 1
          || (i && mat.at [i] <= mat.at [i - 1]))
         return Mat ();
      }
   mat.valid = true;
   return mat;
   }


//! something in a column of a picture which may be a line of light
struct Peak
   {
   double at;     //!< how far down the picture its middle is
   int strength;  //!< how much redder it is than what is beside it
   int left;      //!< how many columns it runs on for to the left
   int right;     //!< and to the right
   };


/** find what may be lines of light in a column of a picture

   \param red     how much redder than anything else each pixel down
                  the column is
   \param sharp   somewhere to work, as large as red
   \param beside  how far above and below a line to look for what it
                  is redder than
   \param apart   how far apart two lines must be
   \param peaks   returns them, the strongest first */
static void find_peaks (const QVector<int> &red, QVector<int> &sharp,
                        int beside, int apart, QVector<Peak> &peaks)
   {
   int h = red.size ();

   /* A line is thin, so it is redder than what lies a little above it
      and a little below. A hand is red too, but all over */
   for (int y = 0; y < h; y++)
      {
      int sum = red [y] + (y ? red [y - 1] : red [y])
            + (y < h - 1 ? red [y + 1] : red [y]);
      int above = red [qMax (0, y - beside)];
      int below = red [qMin (h - 1, y + beside)];

      sharp [y] = sum / 3 - qMax (above, below);
      }

   // take the strongest, then the next which is clear of it
   peaks.clear ();
   while (peaks.size () < PEAKS)
      {
      int best = -1;

      for (int y = 0; y < h; y++)
         {
         if (sharp [y] >= LINE_LEAST
             && (best < 0 || sharp [y] > sharp [best]))
            best = y;
         }
      if (best < 0)
         break;

      // its middle is where its light balances
      double sum = 0, weight = 0;

      for (int y = qMax (0, best - LINE_HALF);
           y <= qMin (h - 1, best + LINE_HALF); y++)
         {
         int val = qMax (0, sharp [y]);

         sum += val * (y + 0.5);
         weight += val;
         }
      peaks << Peak {sum / weight, sharp [best], 0, 0};
      for (int y = qMax (0, best - apart);
           y <= qMin (h - 1, best + apart); y++)
         sharp [y] = 0;
      }
   }


/** find how far a line which a peak is part of runs to one side of it

   \param peaks   the peaks in the column to that side which is nearest,
                  then in the next, and so on: as many as JOIN_REACH
   \param at      how far down the picture the peak is
   \param left    true to look to the left, false to the right
   \returns how many columns it runs for, 0 if it stops here */
static int runs_on (const QVector<Peak> *peaks [], double at, bool left)
   {
   int most = 0;

   for (int dx = 1; dx <= JOIN_REACH; dx++)
      {
      if (!peaks [dx - 1])
         break;
      for (const Peak &peak : *peaks [dx - 1])
         {
         if (qAbs (peak.at - at) <= JOIN_CLOSE + JOIN_SLOPE * dx)
            most = qMax (most, dx + (left ? peak.left : peak.right));
         }
      }
   return most;
   }


Laserlines::Lines Laserlines::trace (const QImage &frame)
   {
   Lines lines;
   QImage image = frame;

   if (image.isNull ())
      return lines;
   if (image.format () != QImage::Format_RGB32)
      image = image.convertToFormat (QImage::Format_RGB32);

   int w = image.width (), h = image.height ();
   int beside = qMax (2, qRound (h * LINE_BESIDE));
   int apart = qMax (beside + 1, qRound (h * LINE_APART));

   if (h < 4 * apart * Count)
      return lines;

   lines.size = image.size ();
   for (int i = 0; i < Count; i++)
      lines.row [i].fill (std::nan (""), w);

   QVector<QVector<Peak>> peaks (w);
   QVector<int> red (h), sharp (h);

   for (int x = 0; x < w; x++)
      {
      // how much redder than anything else each pixel down the column is
      for (int y = 0; y < h; y++)
         {
         QRgb pix = ((const QRgb *)image.constScanLine (y)) [x];

         red [y] = qRed (pix) - qMax (qGreen (pix), qBlue (pix));
         }
      find_peaks (red, sharp, beside, apart, peaks [x]);
      }

   /* A dim picture is grainy, and the grain is as red in places as a
      line is. Unlike the grain, a line runs on through the columns on
      either side, so see how far each of these does */
   for (int pass = 0; pass < 2; pass++)
      {
      for (int i = 0; i < w; i++)
         {
         int x = pass ? w - 1 - i : i;
         const QVector<Peak> *beside [JOIN_REACH];

         for (int dx = 1; dx <= JOIN_REACH; dx++)
            {
            int col = pass ? x + dx : x - dx;

            beside [dx - 1] = col >= 0 && col < w ? &peaks [col] : nullptr;
            }
         for (Peak &peak : peaks [x])
            {
            int run = runs_on (beside, peak.at, !pass);

            if (pass)
               peak.right = run;
            else
               peak.left = run;
            }
         }
      }

   int run = qMax (4, qRound (w * LINE_RUN));

   for (int x = 0; x < w; x++)
      {
      double at [Count];
      int got = 0;

      // they are in order of strength
      for (const Peak &peak : peaks [x])
         {
         if (peak.left + peak.right >= run && got < Count)
            at [got++] = peak.at;
         }

      // without them all there is no telling which is which
      if (got < Count)
         continue;
      std::sort (at, at + Count);
      for (int i = 0; i < Count; i++)
         lines.row [i] [x] = at [i];
      lines.found = true;
      }
   return lines;
   }


//! \returns the middle value of some, which are put in order
static double middle (QVector<double> &val)
   {
   std::sort (val.begin (), val.end ());
   return val [val.size () / 2];
   }


bool Laserlines::mat (const Lines &lines, Mat &mat)
   {
   int w = lines.size.width (), h = lines.size.height ();
   Mat out;

   if (!lines.found || w < 8)
      return false;
   for (int i = 0; i < Count; i++)
      {
      const QVector<double> &row = lines.row [i];
      QVector<double> left, right, off;
      double left_x = 0, right_x = 0;
      int seen = 0;

      /* the slope is that between the middle of its left third and the
         middle of its right one, which a few stray points do not move */
      for (int x = 0; x < w; x++)
         {
         if (std::isnan (row [x]))
            continue;
         seen++;
         if (x < w / 3)
            {
            left << row [x];
            left_x += x;
            }
         else if (x >= w - w / 3)
            {
            right << row [x];
            right_x += x;
            }
         }
      if (seen < w * MAT_SEEN || left.size () < w / 8
          || right.size () < w / 8)
         return false;
      left_x /= left.size ();
      right_x /= right.size ();

      double slope = (middle (right) - middle (left)) / (right_x - left_x);

      if (qAbs (slope * w) > MAT_SLOPE * h)
         return false;
      for (int x = 0; x < w; x++)
         {
         if (!std::isnan (row [x]))
            off << row [x] - slope * x;
         }

      double base = middle (off);
      int close = 0;

      for (double val : off)
         {
         if (qAbs (val - base) <= MAT_STRAIGHT * h)
            close++;
         }
      if (close < seen * MAT_AGREE)
         return false;
      out.at [i] = base / h;
      out.slope [i] = slope * w / h;
      }
   out.valid = true;
   mat = out;
   return true;
   }


/** find how far a line of light is moved where a line up the page
    crosses it

   \param row     the line of light, for each column of its picture
   \param mat     where the lines fall on the mat
   \param n       which line this is
   \param up      the line up the page, in the pixels of that picture
   \param size    the size of that picture
   \returns how far up the picture it is moved, in pixels; NaN if the
            line of light is not seen there */
static double moved (const QVector<double> &row, const Laserlines::Mat &mat,
                     int n, const QLineF &up, const QSize &size)
   {
   int w = size.width (), h = size.height ();
   double x = (up.x1 () + up.x2 ()) / 2;
   double y = mat.down (n, x, w, h);
   double len = up.y2 () - up.y1 ();

   if (qAbs (len) < 1)
      return std::nan ("");

   // where the two cross depends on each, so go back and forth
   for (int pass = 0; pass < 3; pass++)
      {
      x = up.x1 () + (up.x2 () - up.x1 ()) * (y - up.y1 ()) / len;

      int col = (int)std::floor (x);

      if (col < 0 || col >= w || std::isnan (row [col]))
         return std::nan ("");
      y = row [col];
      }

   // it must be on the page
   if ((y - up.y1 ()) * (y - up.y2 ()) > 0)
      return std::nan ("");
   return mat.down (n, x, w, h) - y;
   }


/** find the measure of a line of light: how far it would be moved by
    something as high as the camera is, were that linear

   How tall the page looks goes up with its height in the same way as
   how far the line is moved, so the one is a straight line against the
   other, and where that line meets no movement at all is how tall the
   page would look lying on the mat.

   \param moved   how far the line is moved at each point across the
                  page, NaN where it is not seen
   \param tall    how tall the page looks at each
   \param measure returns the measure, in the units of moved
   \returns true if it could be found */
static bool find_measure (const QVector<double> &moved,
                          const QVector<double> &tall, double least,
                          double &measure)
   {
   int count = moved.size ();
   QVector<bool> use (count);
   double flat = 0, per = 0;

   for (int i = 0; i < count; i++)
      use [i] = !std::isnan (moved [i]);

   // fit, drop what does not fit, as where a hand lies, and fit again
   for (int pass = 0; pass < 3; pass++)
      {
      double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
      double low = 0, high = 0;

      for (int i = 0; i < count; i++)
         {
         if (!use [i])
            continue;
         if (!n || moved [i] < low)
            low = moved [i];
         if (!n || moved [i] > high)
            high = moved [i];
         n++;
         sx += moved [i];
         sy += tall [i];
         sxx += moved [i] * moved [i];
         sxy += moved [i] * tall [i];
         }

      double det = n * sxx - sx * sx;

      if (n < count * RISE_SEEN || high - low < least || det <= 0)
         return false;
      per = (n * sxy - sx * sy) / det;
      flat = (sy - per * sx) / n;
      if (per <= 0 || flat <= 0)
         return false;
      for (int i = 0; i < count; i++)
         {
         if (use [i] && qAbs (flat + per * moved [i] - tall [i])
               > RISE_FITS * tall [i])
            use [i] = false;
         }
      }
   measure = flat / per;
   return true;
   }


QVector<double> Laserlines::rise (const Lines &lines, const Mat &mat,
                                  const QVector<QLineF> &ups,
                                  const QSize &size)
   {
   int count = ups.size ();
   QVector<double> out;

   if (!lines.found || !mat.valid || count < 4 || size.isEmpty ())
      return out;

   // the lines up the page, in the pixels of the picture of the lines
   double scale_x = (double)lines.size.width () / size.width ();
   double scale_y = (double)lines.size.height () / size.height ();
   QVector<QLineF> across (count);
   QVector<double> tall (count);

   for (int i = 0; i < count; i++)
      {
      across [i] = QLineF (ups [i].x1 () * scale_x, ups [i].y1 () * scale_y,
                           ups [i].x2 () * scale_x, ups [i].y2 () * scale_y);
      tall [i] = across [i].length ();
      }

   // what each line of light says the height is, where it says anything
   QVector<double> high [Count];
   int usable = 0;

   for (int n = 0; n < Count; n++)
      {
      QVector<double> shift (count);
      double measure;

      for (int i = 0; i < count; i++)
         shift [i] = moved (lines.row [n], mat, n, across [i], lines.size);
      if (!find_measure (shift, tall, RISE_MOVES * lines.size.height (),
                         measure))
         continue;
      high [usable].resize (count);
      for (int i = 0; i < count; i++)
         {
         /* the line is moved by measure * t / (1 - t) where the page is
            t of the way up to the camera */
         double val = shift [i] / (measure + shift [i]);

         high [usable] [i] = std::isnan (shift [i])
               || measure + shift [i] <= 0 ? std::nan ("")
               : qBound (0.0, val, RISE_MOST);
         }
      usable++;
      }
   if (!usable)
      return out;

   /* Take the middle of what they say. A hand or a page holder lies
      under one line at a time, and stands above the page, so of two
      which differ the lower is the page */
   out.fill (std::nan (""), count);
   for (int i = 0; i < count; i++)
      {
      QVector<double> val;

      for (int n = 0; n < usable; n++)
         {
         if (!std::isnan (high [n] [i]))
            val << high [n] [i];
         }
      if (val.isEmpty ())
         continue;
      std::sort (val.begin (), val.end ());
      out [i] = val.size () == 2 ? val [0] : val [val.size () / 2];
      }

   // fill in where none of them says, from what is on either side
   int last = -1;

   for (int i = 0; i < count; i++)
      {
      if (std::isnan (out [i]))
         continue;
      for (int j = last + 1; j < i; j++)
         {
         out [j] = last < 0 ? out [i]
               : out [last] + (out [i] - out [last]) * (j - last)
                  / (i - last);
         }
      last = i;
      }
   if (last < 0)
      return QVector<double> ();
   for (int j = last + 1; j < count; j++)
      out [j] = out [last];

   // even out the small steps a line's place is read in, keeping the ends
   for (int pass = 0; pass < 2; pass++)
      {
      QVector<double> was = out;

      for (int i = 1; i < count - 1; i++)
         out [i] = (was [i - 1] + 2 * was [i] + was [i + 1]) / 4;
      }
   return out;
   }
