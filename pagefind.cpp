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

#include <QLineF>
#include <QTransform>
#include <QVector>

#include "pagefind.h"

/* How the page is found

   The picture is shrunk to a grey copy about SearchSize across, which is
   plenty to see a page by and quick to go through. The page is light
   and the mat is dark, so the level which best divides the copy's
   pixels into two kinds (Otsu's method) divides page from mat. The
   largest patch of light pixels is taken to be the page, or the book.

   Each of its four sides is then found on its own. The left side, say,
   starts from the leftmost light pixel of each row. Most of those lie
   along the page's left edge, so the line which the most of them agree
   on is that edge, whatever a thumb, a shadow or the rounded corner of
   a book adds or takes away. That line is then moved out until nearly
   all of the patch is inside it: where an edge is not straight, as with
   a curved page, it is better to keep some mat than to lose some page.
   Where the page runs off the picture there is no edge to find, and the
   picture's own edge is used.

   The four lines meet at the four corners. Because each side is found
   on its own, the shape is whatever the page appears as: turned, and
   narrower at the far end */

//! the least difference in brightness between page and mat, of 255
static const int MIN_CONTRAST = 40;

//! how much of the picture a page must cover, at the least
static const double MIN_AREA = 0.03;

//! how much of the shape found must be page for it to be believed
static const double MIN_CONFIDENCE = 0.6;

//! how far a point may be from a line and still agree with it, in pixels
static const double LINE_TOLERANCE = 2.0;

//! how many lines to try for each side
static const int LINE_TRIES = 400;

//! the most a side may slope from upright or level: tan of about 35 deg
static const double MAX_SLOPE = 0.7;

/** how far a side may have to move out from the line along its edge
    to hold the patch, as a proportion of the picture's size, for the
    edge to count as straight */
static const double STRAIGHT = 0.015;

//! how much of a straight side's length has points which agree on it
static const double ALONG = 0.6;

//! how much of the patch's edge a box round it must have inside it
static const double INSIDE = 0.995;

/** and for a straight side, which leaves out what sticks out past the
    edge for a little of its length, such as a finger holding the page */
static const double INSIDE_FLAT = 0.9;

//! how far to wear the patch away and build it back, to drop specks
static const int OPEN_RADIUS = 2;

//! how far to build it out and wear it back, to join a book's pages
static const int CLOSE_RADIUS = 6;

//! the size of the upright copy of the page the gutter is looked for in
static const int GUTTER_W = 600, GUTTER_H = 400;

//! how many bands down the page the gutter is looked for in
static const int GUTTER_BANDS = 10;

//! how closely the bands must agree on where it is, of the page's width
static const double GUTTER_AGREE = 0.012;

//! the most the gutter may lean: sideways movement over the page's height
static const double GUTTER_SLOPE = 0.2;

//! the part of the page's width the gutter may be in
static const double GUTTER_FROM = 0.3, GUTTER_TO = 0.7;

//! how much darker than the page beside it a gutter is, at the least
static const double GUTTER_DEPTH = 0.04;

//! and for it to be clear enough to split a page nobody said was two
static const double GUTTER_CLEAR = 0.1;

//! the largest picture rectify() makes, along either side
static const int MAX_RECTIFIED = 16000;


//! a small grey copy of a picture
struct Grey
   {
   int w = 0, h = 0;
   int factor = 1;         //!< how many of the picture's pixels to one
   QVector<uchar> px;

   uchar at (int x, int y) const { return px [y * w + x]; }
   };


//! one side of the page
struct Side
   {
   QPointF at;             //!< a point on the line
   QPointF dir;            //!< the way it runs, of length 1
   bool real = false;      //!< true if it is an edge seen in the picture
   int agree = 0;          //!< how many points agreed on the line
   };


//! the picture in a format whose pixels can be read here
static QImage readable (const QImage &picture)
   {
   if (picture.format () != QImage::Format_Grayscale8
       && picture.format () != QImage::Format_RGB32
       && picture.format () != QImage::Format_ARGB32)
      return picture.convertToFormat (QImage::Format_RGB32);
   return picture;
   }


/** make the small grey copy

   \param image  the picture, as readable() gives it */
static Grey reduce (const QImage &image)
   {
   Grey grey;

   int longest = qMax (image.width (), image.height ());
   int factor = qMax (1, (longest + Pagefind::SearchSize - 1)
                         / Pagefind::SearchSize);
   bool colour = image.format () != QImage::Format_Grayscale8;

   // each block need not be read in full to know how bright it is
   int step = factor >= 4 ? 2 : 1;
   int count = ((factor + step - 1) / step) * ((factor + step - 1) / step);

   grey.factor = factor;
   grey.w = image.width () / factor;
   grey.h = image.height () / factor;
   if (grey.w < 8 || grey.h < 8)
      return Grey ();
   grey.px.resize (grey.w * grey.h);
   for (int y = 0; y < grey.h; y++)
      {
      QVector<int> sum (grey.w, 0);

      for (int dy = 0; dy < factor; dy += step)
         {
         const uchar *line = image.constScanLine (y * factor + dy);

         if (colour)
            {
            const QRgb *rgb = (const QRgb *)line;

            for (int x = 0; x < grey.w; x++)
               for (int dx = 0; dx < factor; dx += step)
                  {
                  QRgb pix = rgb [x * factor + dx];

                  sum [x] += (qRed (pix) * 77 + qGreen (pix) * 150
                              + qBlue (pix) * 29) >> 8;
                  }
            }
         else
            for (int x = 0; x < grey.w; x++)
               for (int dx = 0; dx < factor; dx += step)
                  sum [x] += line [x * factor + dx];
         }
      for (int x = 0; x < grey.w; x++)
         grey.px [y * grey.w + x] = sum [x] / count;
      }
   return grey;
   }


/** find the level which best divides light from dark

   \param grey      the copy
   \param contrast  returns how far apart the two kinds are
   \returns the level: a pixel above it is light */
static int divide (const Grey &grey, int &contrast)
   {
   double hist [256] = {0};
   double total = grey.px.size (), sum = 0;

   for (uchar val : grey.px)
      hist [val]++;
   for (int i = 0; i < 256; i++)
      sum += i * hist [i];

   double below = 0, sum_below = 0, best = -1;
   int level = 127;

   contrast = 0;
   for (int i = 0; i < 255; i++)
      {
      below += hist [i];
      sum_below += i * hist [i];
      if (below == 0 || below == total)
         continue;

      double mean_below = sum_below / below;
      double mean_above = (sum - sum_below) / (total - below);
      double apart = below * (total - below)
            * (mean_above - mean_below) * (mean_above - mean_below);

      if (apart > best)
         {
         best = apart;
         level = i;
         contrast = (int)(mean_above - mean_below);
         }
      }
   return level;
   }


//! wear a patch away by a pixel all round, or build it out by one
static void wear (QVector<uchar> &mask, int w, int h, bool away)
   {
   QVector<uchar> result (mask.size ());
   const uchar *in = mask.constData ();
   uchar *out = result.data ();

   for (int y = 0; y < h; y++)
      {
      // beyond the picture counts as more of the same
      const uchar *row = in + y * w;
      const uchar *up = y ? row - w : row;
      const uchar *down = y < h - 1 ? row + w : row;
      uchar *dest = out + y * w;

      for (int x = 0; x < w; x++)
         {
         uchar left = row [x ? x - 1 : x];
         uchar right = row [x < w - 1 ? x + 1 : x];

         dest [x] = away ? (row [x] & left & right & up [x] & down [x])
                         : (row [x] | left | right | up [x] | down [x]);
         }
      }
   mask = result;
   }


//! keep only the largest patch of a mask
static void largest_patch (QVector<uchar> &mask, int w, int h)
   {
   QVector<int> label (mask.size (), 0);
   QVector<int> todo;
   int best = 0, best_size = 0, next = 0;

   for (int start = 0; start < mask.size (); start++)
      {
      if (!mask [start] || label [start])
         continue;

      int size = 0;

      next++;
      label [start] = next;
      todo.clear ();
      todo << start;
      while (!todo.isEmpty ())
         {
         int at = todo.takeLast ();
         int x = at % w, y = at / w;

         size++;
         if (x && mask [at - 1] && !label [at - 1])
            { label [at - 1] = next; todo << at - 1; }
         if (x < w - 1 && mask [at + 1] && !label [at + 1])
            { label [at + 1] = next; todo << at + 1; }
         if (y && mask [at - w] && !label [at - w])
            { label [at - w] = next; todo << at - w; }
         if (y < h - 1 && mask [at + w] && !label [at + w])
            { label [at + w] = next; todo << at + w; }
         }
      if (size > best_size)
         {
         best_size = size;
         best = next;
         }
      }
   for (int i = 0; i < mask.size (); i++)
      mask [i] = best && label [i] == best;
   }


/** make everything a patch surrounds part of it

   \returns the number of pixels in it then */
static int fill_holes (QVector<uchar> &mask, int w, int h)
   {
   // whatever can be reached from the picture's edge is outside
   QVector<uchar> outside (mask.size (), 0);
   QVector<int> todo;
   int area = 0;

   auto reach = [&] (int at)
      {
      if (!mask [at] && !outside [at])
         {
         outside [at] = 1;
         todo << at;
         }
      };

   for (int x = 0; x < w; x++)
      {
      reach (x);
      reach ((h - 1) * w + x);
      }
   for (int y = 0; y < h; y++)
      {
      reach (y * w);
      reach (y * w + w - 1);
      }
   while (!todo.isEmpty ())
      {
      int at = todo.takeLast ();
      int x = at % w, y = at / w;

      if (x)
         reach (at - 1);
      if (x < w - 1)
         reach (at + 1);
      if (y)
         reach (at - w);
      if (y < h - 1)
         reach (at + w);
      }
   for (int i = 0; i < mask.size (); i++)
      {
      mask [i] = !outside [i];
      area += mask [i];
      }
   return area;
   }


/** find one side of the page

   \param edge      the patch's outermost point on this side, for each
                    row or column which has one
   \param on_edge   for each, true if it is at the picture's own edge
   \param upright   true for the left and right sides
   \param border    the picture's edge on this side, to use if the page
                    runs off it
   \returns the line most of the points agree on, which support() then
            puts where it holds the patch */
static Side find_side (const QVector<QPointF> &edge,
                       const QVector<bool> &on_edge, bool upright,
                       const Side &border)
   {
   QVector<QPointF> seen;

   for (int i = 0; i < edge.size (); i++)
      if (!on_edge [i])
         seen << edge [i];

   // with little of it seen, the page runs off the picture here
   int count = seen.size ();

   if (count < 12 || count * 5 < edge.size ())
      return border;

   /* try lines through pairs of points, some way apart, and keep the
      one most points agree with. The pairs are chosen the same way each
      time, so that the same picture always gives the same answer */
   quint32 rand = 12345;
   int best = -1;
   QPointF best_at, best_dir;

   for (int i = 0; i < LINE_TRIES; i++)
      {
      rand = rand * 1664525u + 1013904223u;
      int a = (rand >> 8) % count;
      rand = rand * 1664525u + 1013904223u;
      int b = (rand >> 8) % count;

      if (qAbs (a - b) < count / 10 + 1)
         continue;

      QPointF dir = seen [b] - seen [a];
      double len = std::hypot (dir.x (), dir.y ());

      if (len < 1)
         continue;
      dir /= len;
      if (upright ? qAbs (dir.x ()) > MAX_SLOPE * qAbs (dir.y ())
                  : qAbs (dir.y ()) > MAX_SLOPE * qAbs (dir.x ()))
         continue;

      int agree = 0;

      for (const QPointF &pt : seen)
         {
         QPointF rel = pt - seen [a];

         if (qAbs (rel.x () * dir.y () - rel.y () * dir.x ())
             <= LINE_TOLERANCE)
            agree++;
         }
      if (agree > best)
         {
         best = agree;
         best_at = seen [a];
         best_dir = dir;
         }
      }
   if (best < 12)
      return border;

   // the line which best fits the points which agreed
   double mx = 0, my = 0, sxx = 0, sxy = 0, syy = 0;
   int n = 0;

   for (const QPointF &pt : seen)
      {
      QPointF rel = pt - best_at;

      if (qAbs (rel.x () * best_dir.y () - rel.y () * best_dir.x ())
          > LINE_TOLERANCE)
         continue;
      mx += pt.x ();
      my += pt.y ();
      n++;
      }
   mx /= n;
   my /= n;
   for (const QPointF &pt : seen)
      {
      QPointF rel = pt - best_at;

      if (qAbs (rel.x () * best_dir.y () - rel.y () * best_dir.x ())
          > LINE_TOLERANCE)
         continue;
      sxx += (pt.x () - mx) * (pt.x () - mx);
      sxy += (pt.x () - mx) * (pt.y () - my);
      syy += (pt.y () - my) * (pt.y () - my);
      }

   double angle = 0.5 * std::atan2 (2 * sxy, sxx - syy);
   Side side;

   side.at = QPointF (mx, my);
   side.dir = QPointF (std::cos (angle), std::sin (angle));
   side.real = true;
   side.agree = n;
   return side;
   }


/** put a side where nearly all of the patch is inside it

   \param side   the side, of which only the way it runs is used
   \param edge   the patch's outermost points on this side, which
                 include where the page runs off the picture
   \param out    the way out of the page from this side
   \param part   how much of the patch's edge to have inside it, as a
                 proportion
   \returns how far the side had to move out from the line through the
            points which agreed on it: little for a straight edge */
static double support (Side &side, const QVector<QPointF> &edge,
                       const QPointF &out, double part)
   {
   QPointF normal (side.dir.y (), -side.dir.x ());

   if (normal.x () * out.x () + normal.y () * out.y () < 0)
      normal = -normal;

   QVector<double> beyond;

   for (const QPointF &pt : edge)
      {
      QPointF rel = pt - side.at;

      beyond << rel.x () * normal.x () + rel.y () * normal.y ();
      }
   if (beyond.isEmpty ())
      return 0;
   std::sort (beyond.begin (), beyond.end ());

   double shift = beyond [qMin (beyond.size () - 1,
                                (int)(beyond.size () * part))];

   side.at += normal * shift;
   return shift;
   }


//! where two sides meet
static bool meet (const Side &a, const Side &b, QPointF &pt)
   {
   QLineF la (a.at, a.at + a.dir), lb (b.at, b.at + b.dir);

   return la.intersects (lb, &pt) != QLineF::NoIntersection;
   }


//! true if four corners in order make a shape which bulges nowhere inwards
static bool convex (const QPolygonF &quad)
   {
   int sign = 0;

   for (int i = 0; i < 4; i++)
      {
      QPointF a = quad [(i + 1) % 4] - quad [i];
      QPointF b = quad [(i + 2) % 4] - quad [(i + 1) % 4];
      double cross = a.x () * b.y () - a.y () * b.x ();

      if (qAbs (cross) < 1e-6)
         return false;
      if (sign && (cross > 0) != (sign > 0))
         return false;
      sign = cross > 0 ? 1 : -1;
      }
   return true;
   }


//! true if a point is inside a convex shape with its corners in order
static bool inside (const QPolygonF &quad, double x, double y)
   {
   for (int i = 0; i < 4; i++)
      {
      QPointF a = quad [i], b = quad [(i + 1) % 4];

      if ((b.x () - a.x ()) * (y - a.y ()) - (b.y () - a.y ()) * (x - a.x ())
          < 0)
         return false;
      }
   return true;
   }


/** find the corners of the shape four sides make

   \param side   the sides: left, right, top, bottom
   \param quad   returns the corners
   \returns true if they make a usable shape */
static bool corners (const Side side [4], QPolygonF &quad)
   {
   QPointF tl, tr, br, bl;

   if (!meet (side [2], side [0], tl) || !meet (side [2], side [1], tr)
       || !meet (side [3], side [1], br) || !meet (side [3], side [0], bl))
      return false;
   quad = QPolygonF ({tl, tr, br, bl});
   return convex (quad);
   }


/** find the outline of the page in the small copy

   \param grey        the copy
   \param quad        returns the outline, in the copy's pixels
   \param confidence  returns how much of the outline is page
   \param squared     returns true if the outline is a box square to the
                      picture, as what was found is not flat
   \param sheet       returns the pixels of the copy which are page, with
                      nothing done to join a book's two pages
   \param level       returns the level which divides page from mat
   \returns true if found */
static bool find_outline (const Grey &grey, QPolygonF &quad,
                          double &confidence, bool &squared,
                          QVector<uchar> &sheet, int &level)
   {
   int w = grey.w, h = grey.h;
   int contrast;

   level = divide (grey, contrast);

   // all mat, or all page
   if (contrast < MIN_CONTRAST)
      return false;

   QVector<uchar> mask (w * h);

   for (int i = 0; i < mask.size (); i++)
      mask [i] = grey.px [i] > level;
   for (int i = 0; i < OPEN_RADIUS; i++)
      wear (mask, w, h, true);
   for (int i = 0; i < OPEN_RADIUS; i++)
      wear (mask, w, h, false);

   /* the gutter of a book can be as dark as the mat, which would make
      two patches of its two pages: build the patch out far enough to
      join them and wear it back again, which leaves its edges as they
      were */
   sheet = mask;
   for (int i = 0; i < CLOSE_RADIUS; i++)
      wear (mask, w, h, false);
   for (int i = 0; i < CLOSE_RADIUS; i++)
      wear (mask, w, h, true);

   largest_patch (mask, w, h);
   for (int i = 0; i < mask.size (); i++)
      sheet [i] &= mask [i];

   // what is printed on the page is page too
   int area = fill_holes (mask, w, h);

   if (area < MIN_AREA * w * h)
      return false;

   /* the patch's outermost points, each given at the outer edge of its
      pixel so that the sides enclose the pixels */
   QVector<QPointF> left, right, top, bottom;
   QVector<bool> left_on, right_on, top_on, bottom_on;

   for (int y = 0; y < h; y++)
      {
      int first = -1, last = -1;

      for (int x = 0; x < w; x++)
         if (mask [y * w + x])
            {
            if (first < 0)
               first = x;
            last = x;
            }
      if (first < 0)
         continue;
      left << QPointF (first, y + 0.5);
      left_on << (first == 0);
      right << QPointF (last + 1, y + 0.5);
      right_on << (last == w - 1);
      }
   for (int x = 0; x < w; x++)
      {
      int first = -1, last = -1;

      for (int y = 0; y < h; y++)
         if (mask [y * w + x])
            {
            if (first < 0)
               first = y;
            last = y;
            }
      if (first < 0)
         continue;
      top << QPointF (x + 0.5, first);
      top_on << (first == 0);
      bottom << QPointF (x + 0.5, last + 1);
      bottom_on << (last == h - 1);
      }

   Side border [4];

   border [0].at = QPointF (0, 0);      // left
   border [0].dir = QPointF (0, 1);
   border [1].at = QPointF (w, 0);      // right
   border [1].dir = QPointF (0, 1);
   border [2].at = QPointF (0, 0);      // top
   border [2].dir = QPointF (1, 0);
   border [3].at = QPointF (0, h);      // bottom
   border [3].dir = QPointF (1, 0);

   // left, right, top, bottom
   const QVector<QPointF> *edge [4] = {&left, &right, &top, &bottom};
   const QVector<bool> *on_edge [4] = {&left_on, &right_on, &top_on,
                                       &bottom_on};
   const QPointF out [4] = {QPointF (-1, 0), QPointF (1, 0),
                            QPointF (0, -1), QPointF (0, 1)};
   Side side [4];
   bool flat = true;
   int real = 0;

   for (int i = 0; i < 4; i++)
      {
      side [i] = find_side (*edge [i], *on_edge [i], i < 2, border [i]);
      if (!side [i].real)
         continue;
      real++;

      /* An edge which is straight has nearly all of the patch inside the
         line along it. One which is not, such as the edge of
         a page which curves up from a book, has the line along one part
         of it and much of the patch beyond */
      if (support (side [i], *edge [i], out [i], INSIDE_FLAT)
          > STRAIGHT * qMax (w, h))
         flat = false;
      }

   // a page which fills the picture has no edges to go by
   if (real < 2)
      return false;

   if (flat)
      flat = corners (side, quad);

   /* and the points which agree on a straight edge lie along most of
      its length, where the line along part of a curved one touches it
      only here and there */
   for (int i = 0; flat && i < 4; i++)
      {
      static const int from [4] = {0, 1, 0, 3}, to [4] = {3, 2, 1, 2};
      double length = QLineF (quad [from [i]], quad [to [i]]).length ();

      if (side [i].real && side [i].agree < ALONG * length)
         flat = false;
      }

   /* What is not flat has no four corners to find, and the slope of an
      edge which is not straight says nothing about how the page lies:
      going by it would pull the page out of shape. So such a thing, an
      open book for one, is given a box square to the picture which
      holds the patch. Better to keep some mat than to lose some page */
   if (!flat)
      {
      for (int i = 0; i < 4; i++)
         {
         side [i] = border [i];
         support (side [i], *edge [i], out [i], INSIDE);
         }
      if (!corners (side, quad))
         return false;
      }
   squared = !flat;

   // the corners may be off the picture, but not by much
   QRectF bounds (-w * 0.25, -h * 0.25, w * 1.5, h * 1.5);

   for (const QPointF &pt : quad)
      if (!bounds.contains (pt))
         return false;
   if (QLineF (quad [0], quad [1]).length () < w * 0.05
       || QLineF (quad [3], quad [2]).length () < w * 0.05
       || QLineF (quad [0], quad [3]).length () < h * 0.05
       || QLineF (quad [1], quad [2]).length () < h * 0.05)
      return false;

   // how much of what the outline holds, within the picture, is page
   QRect box = quad.boundingRect ().toAlignedRect ()
         .intersected (QRect (0, 0, w, h));
   int within = 0, page = 0;

   for (int y = box.top (); y <= box.bottom (); y++)
      for (int x = box.left (); x <= box.right (); x++)
         if (inside (quad, x + 0.5, y + 0.5))
            {
            within++;
            page += mask [y * w + x];
            }
   if (within < MIN_AREA * w * h)
      return false;
   confidence = (double)page / within;
   return confidence >= MIN_CONFIDENCE;
   }


/** look for a gutter across one band of the page

   A gutter is a narrow dark line down the page: darker than the paper
   close by on both sides of it

   \param page    the page made upright, GUTTER_W by GUTTER_H, see
                  find_gutter()
   \param from    the first row of the band
   \param to      the row after its last
   \param where   returns where the gutter is, as a proportion of the
                  page's width
   \returns how much darker the gutter is than the page beside it, as a
            proportion, or 0 if there is none */
static double find_valley (const QVector<uchar> &page, int from, int to,
                           double &where)
   {
   QVector<double> level (GUTTER_W, 0);
   QVector<uchar> col;

   /* how bright the paper is down each column: the brighter pixels, so
      that the print on it is not counted */
   for (int x = 0; x < GUTTER_W; x++)
      {
      col.clear ();
      for (int y = from; y < to; y++)
         col << page [y * GUTTER_W + x];
      std::sort (col.begin (), col.end ());
      level [x] = col [col.size () * 7 / 10];
      }

   int lowest = -1;

   for (int x = (int)(GUTTER_W * GUTTER_FROM);
        x < (int)(GUTTER_W * GUTTER_TO); x++)
      if (lowest < 0 || level [x] < level [lowest])
         lowest = x;

   // the paper close by on each side of it
   double beside [2];
   int inner = GUTTER_W * 2 / 100, outer = GUTTER_W * 8 / 100;

   for (int i = 0; i < 2; i++)
      {
      QVector<double> near_by;

      for (int dx = inner; dx <= outer; dx++)
         {
         int x = i ? lowest + dx : lowest - dx;

         if (x >= 0 && x < GUTTER_W)
            near_by << level [x];
         }
      if (near_by.isEmpty ())
         return 0;
      std::sort (near_by.begin (), near_by.end ());
      beside [i] = near_by [near_by.size () / 2];
      }

   double paper = qMin (beside [0], beside [1]);

   where = (lowest + 0.5) / GUTTER_W;
   if (paper < 1)
      return 0;
   return qMax (0.0, 1 - level [lowest] / paper);
   }


/** look for the gutter of an open book

   \param grey     the small copy of the picture
   \param quad     the book's outline in the copy
   \param top      returns where the gutter meets the top of the outline,
                   as a proportion of the way along
   \param bottom   returns the same for the bottom
   \returns how clear the gutter is, 0 if there is none */
static double find_gutter (const Grey &grey, const QPolygonF &quad,
                           double &top, double &bottom)
   {
   QTransform map;
   QPolygonF upright ({QPointF (0, 0), QPointF (GUTTER_W, 0),
                       QPointF (GUTTER_W, GUTTER_H),
                       QPointF (0, GUTTER_H)});

   top = bottom = 0.5;
   if (!QTransform::quadToQuad (upright, quad, map))
      return 0;

   // a coarse upright copy of the page, to look through
   QVector<uchar> page (GUTTER_W * GUTTER_H, 0);

   for (int y = 0; y < GUTTER_H; y++)
      {
      double fy = y + 0.5;
      double sx = map.m21 () * fy + map.m31 () + map.m11 () * 0.5;
      double sy = map.m22 () * fy + map.m32 () + map.m12 () * 0.5;
      double sw = map.m23 () * fy + map.m33 () + map.m13 () * 0.5;

      for (int x = 0; x < GUTTER_W; x++)
         {
         int px = (int)(sx / sw), py = (int)(sy / sw);

         if (px >= 0 && py >= 0 && px < grey.w && py < grey.h)
            page [y * GUTTER_W + x] = grey.at (px, py);
         sx += map.m11 ();
         sy += map.m12 ();
         sw += map.m13 ();
         }
      }

   /* The gutter need not run straight down the page as it is laid out
      here, so look for it in each of a number of bands down the page.
      Where it is found, the bands agree on a line; what else is dark in
      one band or another, such as a rule or a picture, does not */
   double where [GUTTER_BANDS], depth [GUTTER_BANDS], mid [GUTTER_BANDS];
   int band = GUTTER_H * 8 / 10 / GUTTER_BANDS;

   for (int i = 0; i < GUTTER_BANDS; i++)
      {
      int from = GUTTER_H / 10 + i * band;

      depth [i] = find_valley (page, from, from + band, where [i]);
      mid [i] = (from + band / 2.0) / GUTTER_H;
      }

   int best = 0;
   double best_depth = 0;

   for (int i = 0; i < GUTTER_BANDS; i++)
      for (int j = i + 1; j < GUTTER_BANDS; j++)
         {
         if (depth [i] < GUTTER_DEPTH || depth [j] < GUTTER_DEPTH)
            continue;

         double slope = (where [j] - where [i]) / (mid [j] - mid [i]);
         double at_top = where [i] - slope * mid [i];
         QVector<double> agreed;

         if (qAbs (slope) > GUTTER_SLOPE)
            continue;
         for (int k = 0; k < GUTTER_BANDS; k++)
            if (depth [k] >= GUTTER_DEPTH
                && qAbs (at_top + slope * mid [k] - where [k])
                   <= GUTTER_AGREE)
               agreed << depth [k];
         std::sort (agreed.begin (), agreed.end ());

         double typical = agreed [agreed.size () / 2];

         if (agreed.size () > best
             || (agreed.size () == best && typical > best_depth))
            {
            best = agreed.size ();
            best_depth = typical;
            top = at_top;
            bottom = at_top + slope;
            }
         }

   // most of the bands must see it
   if (best * 10 < GUTTER_BANDS * 6)
      {
      top = bottom = 0.5;
      return 0;
      }
   top = qBound (0.05, top, 0.95);
   bottom = qBound (0.05, bottom, 0.95);
   return best_depth;
   }


//! the size of the finer copy of the page a thin gutter is looked for in
static const int THIN_W = 3000, THIN_H = 800;

//! how many bands down the page it is looked for in
static const int THIN_BANDS = 40;

//! how many columns either side of a line count as on it
static const int THIN_SPREAD = 2;

//! how far from it the paper beside it is looked at, of the page's width
static const double THIN_INNER = 0.004, THIN_OUTER = 0.02;

//! how much darker than the paper beside it a thin gutter is, at the least
static const double THIN_DEPTH = 0.06;

/** look for a gutter which is no more than a thin line

   A book held open and flat has no shadow between its pages: only a line
   where they meet, a few pixels wide and broken here and there. That is
   lost in the small copy find_gutter() looks through, so this looks
   through the middle of the picture itself, in more and smaller bands.
   Print has strokes as dark and as thin, but they do not line up down
   the page: the gutter is the straight line which most of the bands see
   something dark on.

   \param image    the picture, in a format reduce() can read
   \param quad     the outline, in the picture's pixels
   \param top      returns where the gutter meets the top of the outline,
                   as for find_gutter()
   \param bottom   returns where it meets the bottom
   \returns how clear the gutter is, 0 if there is none */
static double find_thin_gutter (const QImage &image, const QPolygonF &quad,
                                double &top, double &bottom)
   {
   QSize size = Pagefind::rectifiedSize (quad);
   int w = qMin ((int)THIN_W, size.width ());
   int h = qMin ((int)THIN_H, size.height ());
   int rows = h / THIN_BANDS;
   QTransform map;
   QPolygonF upright ({QPointF (0, 0), QPointF (w, 0), QPointF (w, h),
                       QPointF (0, h)});

   top = bottom = 0.5;
   if (rows < 8 || w < 400 || !QTransform::quadToQuad (upright, quad, map))
      return 0;

   int inner = qMax (2, (int)(w * THIN_INNER));
   int outer = qMax (inner + 4, (int)(w * THIN_OUTER));
   int first = (int)(w * GUTTER_FROM), last = (int)(w * GUTTER_TO);
   int x0 = first - outer, cols = last + outer - x0;
   bool colour = image.format () != QImage::Format_Grayscale8;
   int skip = (h - rows * THIN_BANDS) / 2;

   if (cols <= 2 * outer)
      return 0;

   QVector<uchar> band (cols * rows);
   QVector<double> level (cols), near_by;
   QVector<float> depth (THIN_BANDS * cols, 0), widened (cols);

   for (int i = 0; i < THIN_BANDS; i++)
      {
      for (int y = 0; y < rows; y++)
         for (int x = 0; x < cols; x++)
            {
            QPointF pt = map.map (QPointF (x0 + x + 0.5,
                                           skip + i * rows + y + 0.5));
            int px = (int)pt.x (), py = (int)pt.y ();
            uchar value = 0;

            if (px >= 0 && py >= 0 && px < image.width ()
                && py < image.height ())
               {
               const uchar *line = image.constScanLine (py);

               if (colour)
                  {
                  QRgb pix = ((const QRgb *)line) [px];

                  value = (qRed (pix) * 77 + qGreen (pix) * 150
                           + qBlue (pix) * 29) >> 8;
                  }
               else
                  value = line [px];
               }
            band [y * cols + x] = value;
            }

      // how bright each column is
      for (int x = 0; x < cols; x++)
         {
         int sum = 0;

         for (int y = 0; y < rows; y++)
            sum += band [y * cols + x];
         level [x] = (double)sum / rows;
         }

      // and how much darker than what is on both sides of it
      for (int x = outer; x < cols - outer; x++)
         {
         double paper = 255;

         for (int side = -1; side <= 1; side += 2)
            {
            near_by.clear ();
            for (int dx = inner; dx <= outer; dx++)
               near_by << level [x + side * dx];
            std::nth_element (near_by.begin (),
                              near_by.begin () + near_by.size () / 2,
                              near_by.end ());
            paper = qMin (paper, near_by [near_by.size () / 2]);
            }
         if (paper >= 1)
            depth [i * cols + x] = qMax (0.0, 1 - level [x] / paper);
         }

      // the line leans, so let it be a column or two either way
      widened.fill (0);
      for (int x = THIN_SPREAD; x < cols - THIN_SPREAD; x++)
         for (int dx = -THIN_SPREAD; dx <= THIN_SPREAD; dx++)
            widened [x] = qMax (widened [x], depth [i * cols + x + dx]);
      for (int x = 0; x < cols; x++)
         depth [i * cols + x] = widened [x];
      }

   /* Try each line down the page: where it meets the top band, and how
      far it has moved sideways by the bottom one */
   int lean = (int)(w * GUTTER_SLOPE * (THIN_BANDS - 1) * rows / h);
   int best_seen = 0, best_at = 0, best_move = 0;
   double best_sum = 0;

   for (int move = -lean; move <= lean; move += THIN_SPREAD)
      for (int at = outer; at < cols - outer; at++)
         {
         int end = at + move;
         int seen = 0;
         double sum = 0;

         if (end < outer || end >= cols - outer)
            continue;
         for (int i = 0; i < THIN_BANDS; i++)
            {
            int x = at + move * i / (THIN_BANDS - 1);
            float here = depth [i * cols + x];

            if (here >= THIN_DEPTH)
               seen++;
            sum += here;
            }
         if (seen > best_seen || (seen == best_seen && sum > best_sum))
            {
            best_seen = seen;
            best_sum = sum;
            best_at = at;
            best_move = move;
            }
         }

   // most of the bands must see it
   if (best_seen * 10 < THIN_BANDS * 6)
      return 0;

   // from the middle of the top band to the top of the page, and so on
   double slope = (double)best_move / w * h / ((THIN_BANDS - 1) * rows);
   double first_mid = (skip + 0.5 * rows) / h;

   top = (x0 + best_at + 0.5) / w - slope * first_mid;
   bottom = top + slope;
   top = qBound (0.05, top, 0.95);
   bottom = qBound (0.05, bottom, 0.95);
   return best_sum / THIN_BANDS;
   }

/* How the edges of a curved page are traced

   Looking down each column of the patch, the first light pixel is on the
   top edge of whatever is there and the last is on the bottom edge.
   Followed outwards from the gutter, those run along the page's edges
   until the page ends. There the edge stops, or steps to the edge of
   something else which is light and lies beside the page, or turns
   sharply to run down the side of the pages beneath: the flap of a
   book's jacket, say, or the block of its pages. Any of those ends the
   page.

   A line up the page which is level with the gutter looks, in the
   picture, much as the gutter does, so the two edges should end about
   as far from it as each other. If they do not, something beside the
   page has been followed along one of them, and the nearer end is taken
   for both.

   Where a page runs off the picture one of its edges is missing for
   some of its length. The part of a page which is nearer the camera
   looks larger, which moves its top edge up the picture and its bottom
   edge down it in step: where the one edge is to be seen, the other can
   be worked out from it, given how the two go together where both are
   to be seen */

//! how far an edge may step and still be the same edge, of the height
static const double EDGE_JUMP = 0.02;

//! how many columns apart the step is measured over
static const int EDGE_GAP = 4;

//! how many columns each way an edge's turn is measured over
static const int EDGE_TURN_GAP = 6;

//! how much an edge's slope may change there and still be the same edge
static const double EDGE_TURN = 0.25;

//! how far from the gutter an edge must be to end by turning, of the width
static const double EDGE_TURN_FROM = 0.15;

//! how far from the gutter to start following an edge, of the width
static const double EDGE_START = 0.02;

//! how far apart the two edges may end, of the width
static const double EDGE_ENDS = 0.05;

//! how much of an edge must be seen for the rest to be worked out
static const double EDGE_SEEN = 0.25;

/** how far the edge seen must move up or down, of the height, for how
    the other moves with it to be found */
static const double EDGE_MOVES = 0.02;

/** where between the mat's brightness and the level which divides it
    from the page an edge is taken to be */
static const double EDGE_LEVEL = 0.4;

//! how far along an edge to average it over, in thousandths of the width
static const int EDGE_SMOOTH = 4;

//! the least width of a page whose edges are worth tracing, of the width
static const double EDGE_LEAST = 0.08;


//! one edge of the patch, top or bottom, column by column
struct Edge
   {
   QVector<int> at;        //!< the row it is in, -1 where there is none
   QVector<double> exact;  //!< the same, to a part of a pixel
   int limit = 0;          //!< the row of the picture's own edge
   int inward = 1;         //!< which way is into the page: 1 is down
   int from = 0, to = 0;   //!< the columns of the page's edge
   };


/** follow an edge until it ends

   \param edge   the edge
   \param from   the column to start from, which has an edge
   \param dir    -1 to go left, 1 to go right
   \param jump   how far it may step
   \param turn   how many columns to go before a turn ends it
   \returns the last column of the edge */
static int follow (const Edge &edge, int from, int dir, int jump, int turn)
   {
   const QVector<int> &prof = edge.at;
   int last = from;

   for (int x = from + dir; x >= 0 && x < prof.size (); x += dir)
      {
      int back = x - EDGE_GAP * dir;

      if ((back - from) * dir < 0)
         back = from;
      if (prof [x] < 0)
         break;
      if (qAbs (prof [x] - prof [back]) > jump)
         {
         // the page ends at the largest step on the way
         int most = 0;

         last = back;
         for (int at = back; at != x; at += dir)
            if (qAbs (prof [at + dir] - prof [at]) > most)
               {
               most = qAbs (prof [at + dir] - prof [at]);
               last = at;
               }
         return last;
         }

      /* where the edge turns in towards the page, the page has ended.
         A step is not a turn, and nor is where the edge leaves the
         picture */
      int before = x - EDGE_TURN_GAP * dir, after = x + EDGE_TURN_GAP * dir;

      if ((x - from) * dir >= turn && after >= 0 && after < prof.size ()
          && prof [after] >= 0 && qAbs (prof [after] - prof [x]) <= jump
          && prof [x] != edge.limit && prof [after] != edge.limit
          && prof [before] != edge.limit)
         {
         double bend = (edge.exact [after] - edge.exact [x])
               - (edge.exact [x] - edge.exact [before]);

         if (bend * edge.inward > EDGE_TURN * EDGE_TURN_GAP)
            return x;
         }
      last = x;
      }
   return last;
   }


//! smooth a line of values, keeping its ends
static void smooth (QVector<double> &val, int radius)
   {
   int count = val.size ();

   if (count < 1)
      return;

   QVector<double> out (count);

   // first drop the odd one which is far from those beside it
   for (int i = 0; i < count; i++)
      {
      QVector<double> near_by;
      int r = qMin (2, qMin (i, count - 1 - i));

      for (int j = i - r; j <= i + r; j++)
         near_by << val [j];
      std::sort (near_by.begin (), near_by.end ());
      out [i] = near_by [near_by.size () / 2];
      }
   val = out;

   // then average, over less near the ends so that a slope is kept
   for (int pass = 0; pass < 2; pass++)
      {
      QVector<double> sum (count + 1, 0);

      for (int i = 0; i < count; i++)
         sum [i + 1] = sum [i] + val [i];
      for (int i = 0; i < count; i++)
         {
         int r = qMin (radius, qMin (i, count - 1 - i));

         out [i] = (sum [i + r + 1] - sum [i - r]) / (2 * r + 1);
         }
      val = out;
      }
   }


/** find the columns in which an edge is the page's

   \param edge    the edge: its from and to are set
   \param h       the height of the copy
   \param kind    0 for the left page of two, 1 for the right, 2 for a
                  page on its own
   \param gutter  the column in which the gutter meets the edge; for a
                  page on its own, one in the middle of it
   \returns true if found */
static bool edge_range (Edge &edge, int h, int kind, int gutter)
   {
   int w = edge.at.size ();
   int jump = qMax (3, (int)(h * EDGE_JUMP));
   int start = qMax (2, (int)(w * EDGE_START));
   int turn = (int)(w * EDGE_TURN_FROM);

   if (kind == 0)
      {
      edge.to = gutter;
      edge.from = gutter - start;
      if (edge.from < 0 || edge.at [edge.from] < 0)
         return false;
      edge.from = follow (edge, edge.from, -1, jump, turn);
      }
   else if (kind == 1)
      {
      edge.from = gutter;
      edge.to = gutter + start;
      if (edge.to >= w || edge.at [edge.to] < 0)
         return false;
      edge.to = follow (edge, edge.to, 1, jump, turn);
      }
   else
      {
      if (edge.at [gutter] < 0)
         return false;
      edge.from = follow (edge, gutter, -1, jump, turn);
      edge.to = follow (edge, gutter, 1, jump, turn);
      }
   return true;
   }


/** give the points along an edge

   \param edge    the edge
   \param points  returns CurvePoints points along it, left to right
   \param seen    returns for each whether the edge is seen there, rather
                  than being off the picture */
static void edge_points (const Edge &edge, QVector<QPointF> &points,
                         QVector<bool> &seen)
   {
   int from = edge.from, to = edge.to;
   int w = edge.at.size ();
   QVector<double> val (to - from + 1);
   double last = -1;

   // the edge in each column, with any gap filled from beside it
   for (int x = from; x <= to; x++)
      {
      if (edge.at [x] >= 0)
         last = edge.exact [x];
      val [x - from] = last;
      }
   for (int x = to; x >= from; x--)
      {
      if (val [x - from] >= 0)
         last = val [x - from];
      else
         val [x - from] = last;
      }
   smooth (val, qMax (2, w * EDGE_SMOOTH / 1000));

   points.clear ();
   seen.clear ();
   for (int i = 0; i < Pagefind::CurvePoints; i++)
      {
      double at = (double)i * (to - from) / (Pagefind::CurvePoints - 1);
      int n = qMin ((int)at, to - from - 1);
      double y = val [n] + (val [n + 1] - val [n]) * (at - n);
      bool on = true;

      // what is close to a part which is off the picture is in doubt too
      for (int x = from + n - 4; x <= from + n + 5; x++)
         if (x >= from && x <= to && edge.at [x] == edge.limit)
            on = false;
      points << QPointF (from + at + 0.5, y);
      seen << on;
      }
   }


/** work out the part of an edge which is off the picture from the other

   \param part    the edge with a part missing
   \param seen    where it is seen
   \param whole   the other edge
   \param h       the height of the copy
   \returns true if done */
static bool fill_edge (QVector<QPointF> &part, const QVector<bool> &seen,
                       const QVector<QPointF> &whole, int h)
   {
   double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
   double low = h, high = 0;

   for (int i = 0; i < part.size (); i++)
      {
      if (!seen [i])
         continue;

      double x = whole [i].y (), y = part [i].y ();

      n++;
      sx += x;
      sy += y;
      sxx += x * x;
      sxy += x * y;
      low = qMin (low, x);
      high = qMax (high, x);
      }
   if (n < part.size () * EDGE_SEEN)
      return false;

   /* The two edges move apart in step: this one's row is a fixed part
      of the other's, counted from the row the camera is right above.
      Where the other moves enough to show both, find them. Where it
      does not, take the camera to be above the picture's middle */
   double slope, base;

   if (high - low >= h * EDGE_MOVES && n * sxx - sx * sx > 0)
      {
      slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
      base = (sy - slope * sx) / n;
      }
   else
      {
      double mid = h / 2.0;

      if (qAbs (sx / n - mid) < 1)
         return false;
      slope = (sy / n - mid) / (sx / n - mid);
      base = mid - slope * mid;
      }

   // an edge moves the other way from the one across the page
   if (slope > 0)
      return false;
   for (int i = 0; i < part.size (); i++)
      if (!seen [i])
         part [i].setY (base + slope * whole [i].y ());
   return true;
   }


/** trace the top and bottom edges of a page which is not flat

   \param grey    the copy
   \param level   the level which divides page from mat in it
   \param sheet   the pixels of the copy which are page
   \param kind    0 for the left page of two, 1 for the right, 2 for a
                  page on its own
   \param gt      the gutter's top end, for a page of two; for one on
                  its own, a point in the middle of it
   \param gb      the gutter's bottom end
   \returns the edges, in the copy's pixels */
static Pagefind::Curve trace_page (const Grey &grey, int level,
                                   const QVector<uchar> &sheet, int kind,
                                   QPointF gt, QPointF gb)
   {
   int w = grey.w, h = grey.h;
   Edge top, bottom;
   Pagefind::Curve curve;

   top.at = QVector<int> (w, -1);
   top.exact = QVector<double> (w, -1);
   bottom = top;
   bottom.limit = h - 1;
   bottom.inward = -1;

   /* Where a page goes down into the gutter it is in shadow, and can be
      nearer to the mat in brightness than to the rest of the page. So
      the edge is taken to be where the brightness falls most of the way
      to the mat's, going on out from the patch */
   double mat = 0;
   int count = 0;

   for (uchar val : grey.px)
      if (val <= level)
         {
         mat += val;
         count++;
         }
   if (count)
      mat /= count;
   level = qRound (mat + (level - mat) * EDGE_LEVEL);

   for (int x = 0; x < w; x++)
      {
      int first = -1, last = -1;

      for (int y = 0; y < h; y++)
         if (sheet [y * w + x])
            {
            if (first < 0)
               first = y;
            last = y;
            }
      if (first < 0)
         continue;
      while (first > 0 && grey.at (x, first - 1) > level)
         first--;
      while (last < h - 1 && grey.at (x, last + 1) > level)
         last++;

      /* The edge is where the brightness crosses the level, which is
         some way between the middle of the last pixel of mat and that
         of the first of page. That places it more finely than a pixel,
         which the slope of the page is worked out from */
      auto cross = [&] (int page, int mat)
         {
         if (mat < 0 || mat >= h)
            return page + (mat < page ? 0.0 : 1.0);

         double light = grey.at (x, page), dark = grey.at (x, mat);
         double part = light > dark
               ? qBound (0.0, (light - level) / (light - dark), 1.0) : 0.5;

         return page + 0.5 + (mat - page) * part;
         };

      top.at [x] = first;
      top.exact [x] = cross (first, first - 1);
      bottom.at [x] = last;
      bottom.exact [x] = cross (last, last + 1);
      }

   // where the gutter meets each edge, and how far it leans between
   double lean = 0;
   int meet [2];

   for (int i = 0; i < 2; i++)
      {
      const Edge &edge = i ? bottom : top;
      double gx = i ? gb.x () : gt.x ();

      if (kind != 2 && qAbs (gb.y () - gt.y ()) > 1)
         for (int j = 0; j < 3; j++)
            {
            int x = qBound (0, (int)gx, w - 1);

            if (edge.at [x] < 0)
               break;
            gx = gt.x () + (edge.at [x] - gt.y ()) * (gb.x () - gt.x ())
                  / (gb.y () - gt.y ());
            }
      meet [i] = qBound (0, (int)gx, w - 1);
      }
   lean = meet [1] - meet [0];

   if (!edge_range (top, h, kind, meet [0])
       || !edge_range (bottom, h, kind, meet [1]))
      return curve;

   /* the outer ends: one which is off the picture is not to be believed,
      and the two should be level with the gutter */
   for (int side = 0; side < 2; side++)
      {
      if (kind == side)
         continue;      // this side of the page is the gutter

      int &t = side ? top.to : top.from;
      int &b = side ? bottom.to : bottom.from;
      bool t_off = top.at [t] == top.limit;
      bool b_off = bottom.at [b] == bottom.limit;
      int inward = side ? -1 : 1;
      double apart = (b - lean) - t;

      if (t_off && b_off)
         return curve;
      if (t_off || (!b_off && apart * inward > w * EDGE_ENDS))
         t = qRound (b - lean);
      else if (b_off || apart * inward < -w * EDGE_ENDS)
         b = qRound (t + lean);

      // the last pixel or two of an edge are part page and part mat
      t += inward * 2;
      b += inward * 2;
      }
   top.from = qBound (0, top.from, w - 1);
   top.to = qBound (0, top.to, w - 1);
   bottom.from = qBound (0, bottom.from, w - 1);
   bottom.to = qBound (0, bottom.to, w - 1);
   if (top.to - top.from < w * EDGE_LEAST
       || bottom.to - bottom.from < w * EDGE_LEAST)
      return curve;

   QVector<QPointF> top_pts, bottom_pts;
   QVector<bool> top_seen, bottom_seen;

   edge_points (top, top_pts, top_seen);
   edge_points (bottom, bottom_pts, bottom_seen);

   bool top_part = top_seen.contains (false);
   bool bottom_part = bottom_seen.contains (false);

   if ((top_part && bottom_part)
       || (top_part && !fill_edge (top_pts, top_seen, bottom_pts, h))
       || (bottom_part && !fill_edge (bottom_pts, bottom_seen, top_pts, h)))
      return curve;

   curve.traced = true;
   curve.top = QPolygonF (top_pts);
   curve.bottom = QPolygonF (bottom_pts);
   return curve;
   }


Pagefind::Result Pagefind::find (const QImage &picture, Mode mode)
   {
   Result result;

   if (mode == Mode_whole || picture.isNull ())
      return result;

   QImage image = readable (picture);
   Grey grey = reduce (image);
   QPolygonF quad;

   QVector<uchar> sheet;
   bool squared = false;
   int level = 0;

   if (!grey.w
       || !find_outline (grey, quad, result.confidence, squared, sheet,
                         level))
      return result;
   result.found = true;
   result.flat = !squared;

   double top, bottom;
   double depth = find_gutter (grey, quad, top, bottom);

   // a book held flat shows no more of its gutter than a line
   if (depth < GUTTER_CLEAR)
      {
      QPolygonF full = quad;
      double thin_top, thin_bottom;

      for (QPointF &pt : full)
         pt *= grey.factor;

      double thin = find_thin_gutter (image, full, thin_top, thin_bottom);

      if (thin > depth)
         {
         depth = thin;
         top = thin_top;
         bottom = thin_bottom;
         }
      }

   // in the copy's pixels
   QPointF gutter_top = quad [0] + (quad [1] - quad [0]) * top;
   QPointF gutter_bottom = quad [3] + (quad [2] - quad [3]) * bottom;
   QPointF middle = (quad [0] + quad [2]) / 2;

   auto trace = [&] (int kind)
      {
      Curve curve;

      if (!squared)
         return curve;
      curve = trace_page (grey, level, sheet, kind,
                          kind == 2 ? middle : gutter_top,
                          kind == 2 ? middle : gutter_bottom);
      for (QPointF &pt : curve.top)
         pt *= grey.factor;
      for (QPointF &pt : curve.bottom)
         pt *= grey.factor;
      return curve;
      };

   for (QPointF &pt : quad)
      pt *= grey.factor;
   result.outline = quad;
   result.gutter = depth > 0;
   if (result.gutter)
      {
      result.gutter_top = top;
      result.gutter_bottom = bottom;
      }

   /* left to choose, split only a book which is clearly one: wider than
      it is tall, with a gutter which stands out */
   QSize size = rectifiedSize (quad);
   bool split = mode == Mode_two
         || (mode == Mode_auto && depth >= GUTTER_CLEAR
             && size.width () >= size.height ());

   if (!split)
      {
      result.pages << quad;
      result.curves << trace (2);
      return result;
      }

   // told there are two with no gutter to be seen, cut down the middle
   QPointF gt = quad [0] + (quad [1] - quad [0]) * result.gutter_top;
   QPointF gb = quad [3] + (quad [2] - quad [3]) * result.gutter_bottom;

   result.pages << QPolygonF ({quad [0], gt, gb, quad [3]});
   result.pages << QPolygonF ({gt, quad [1], quad [2], gb});
   result.curves << trace (0) << trace (1);
   return result;
   }


/** work out one pixel of a page from the picture's pixels around a point

   \param bits    the picture's pixels
   \param sw      its width
   \param sh      its height
   \param stride  the number of bytes in each of its lines
   \param bytes   the number of bytes in each of its pixels
   \param px      the point, where 0 is the middle of the first pixel
   \param py      the same, down the picture
   \param dest    where to put the pixel: white, if the point is outside
                  the picture */
static inline void mix (const uchar *bits, int sw, int sh, int stride,
                        int bytes, double px, double py, uchar *dest)
   {
   int ix = (int)std::floor (px), iy = (int)std::floor (py);

   if (ix < 0 || iy < 0 || ix >= sw - 1 || iy >= sh - 1)
      {
      // on the last row or column there is no next pixel to mix
      if (ix >= -1 && iy >= -1 && ix < sw && iy < sh)
         {
         const uchar *p = bits + qBound (0, iy, sh - 1) * stride
               + qBound (0, ix, sw - 1) * bytes;

         for (int c = 0; c < bytes; c++)
            dest [c] = p [c];
         }
      else
         for (int c = 0; c < bytes; c++)
            dest [c] = 255;
      return;
      }

   int wx = (int)((px - ix) * 256), wy = (int)((py - iy) * 256);
   const uchar *p = bits + iy * stride + ix * bytes;
   const uchar *q = p + stride;

   for (int c = 0; c < bytes; c++)
      {
      int upper = p [c] * (256 - wx) + p [c + bytes] * wx;
      int lower = q [c] * (256 - wx) + q [c + bytes] * wx;

      dest [c] = (upper * (256 - wy) + lower * wy + 32768) >> 16;
      }
   }


QSize Pagefind::rectifiedSize (const QPolygonF &quad)
   {
   if (quad.size () != 4)
      return QSize ();

   double w = qMax (QLineF (quad [0], quad [1]).length (),
                    QLineF (quad [3], quad [2]).length ());
   double h = qMax (QLineF (quad [0], quad [3]).length (),
                    QLineF (quad [1], quad [2]).length ());

   return QSize (qRound (w), qRound (h));
   }


QImage Pagefind::rectify (const QImage &picture, const QPolygonF &quad)
   {
   QSize size = rectifiedSize (quad);
   QTransform map;

   if (picture.isNull () || size.width () < 2 || size.height () < 2
       || size.width () > MAX_RECTIFIED || size.height () > MAX_RECTIFIED
       || !convex (quad))
      return QImage ();

   QPolygonF upright ({QPointF (0, 0), QPointF (size.width (), 0),
                       QPointF (size.width (), size.height ()),
                       QPointF (0, size.height ())});

   if (!QTransform::quadToQuad (upright, quad, map))
      return QImage ();

   // grey stays grey, and anything else is done as colour
   QImage src = picture;
   bool grey = src.format () == QImage::Format_Grayscale8;

   if (!grey && src.format () != QImage::Format_RGB32)
      src = src.convertToFormat (QImage::Format_RGB32);

   QImage out (size, grey ? QImage::Format_Grayscale8
                          : QImage::Format_RGB32);

   if (out.isNull ())
      return QImage ();

   int bytes = grey ? 1 : 4;
   int sw = src.width (), sh = src.height ();
   int stride = src.bytesPerLine ();
   const uchar *bits = src.constBits ();

   /* For each pixel of the page, find where in the picture it is and
      mix the four pixels around that point. A point of the page goes to
      (X / W, Y / W), each of which changes steadily along a row */
   for (int y = 0; y < size.height (); y++)
      {
      uchar *line = out.scanLine (y);
      double fy = y + 0.5;
      double sx = map.m21 () * fy + map.m31 () + map.m11 () * 0.5;
      double sy = map.m22 () * fy + map.m32 () + map.m12 () * 0.5;
      double sw3 = map.m23 () * fy + map.m33 () + map.m13 () * 0.5;

      for (int x = 0; x < size.width (); x++)
         {
         double px = sx / sw3 - 0.5, py = sy / sw3 - 0.5;

         sx += map.m11 ();
         sy += map.m12 ();
         sw3 += map.m13 ();
         mix (bits, sw, sh, stride, bytes, px, py, line + x * bytes);
         }
      }
   return out;
   }


QPolygonF Pagefind::Result::shape (int page) const
   {
   if (page < 0 || page >= pages.size ())
      return QPolygonF ();
   if (page >= curves.size () || !curves [page].traced)
      return pages [page];

   QPolygonF out = curves [page].top;

   for (int i = curves [page].bottom.size () - 1; i >= 0; i--)
      out << curves [page].bottom [i];
   return out;
   }


QImage Pagefind::flatten (const QImage &picture, const Curve &curve,
                          double focal)
   {
   int count = curve.top.size ();

   if (picture.isNull () || !curve.traced || count < 4
       || curve.bottom.size () != count || focal <= 0)
      return QImage ();

   /* The part of the page nearer the camera looks larger, by Z / (Z - h)
      for a camera Z above the mat and a part h above it. A line up the
      page is as long all the way across, so how long it looks gives
      h / Z, taking the part which looks smallest to be the lowest */
   QVector<double> tall (count);

   for (int i = 0; i < count; i++)
      tall [i] = QLineF (curve.top [i], curve.bottom [i]).length ();
   smooth (tall, 1);

   double least = *std::min_element (tall.begin (), tall.end ());
   double most = *std::max_element (tall.begin (), tall.end ());

   if (least < 2)
      return QImage ();

   /* Going across the page, it moves sideways and it rises, and how far
      it goes along its own surface is made up of both. The rise is in
      units of Z, which the focal length turns into pixels: the camera
      is as many pixels above the mat as its focal length */
   double cx = picture.width () / 2.0;
   double f = focal * picture.width ();
   QVector<double> along (count, 0);
   double prev_x = 0, prev_z = 0;

   for (int i = 0; i < count; i++)
      {
      double rise = 1 - least / tall [i];
      double x = ((curve.top [i].x () + curve.bottom [i].x ()) / 2 - cx)
            * (1 - rise);
      double z = f * rise;

      if (i)
         along [i] = along [i - 1] + std::hypot (x - prev_x, z - prev_z);
      prev_x = x;
      prev_z = z;
      }

   /* make it at the size of its largest part, so that nothing in the
      picture is lost */
   double scale = most / least;
   QSize size (qRound (along [count - 1] * scale), qRound (most));

   if (size.width () < 2 || size.height () < 2
       || size.width () > MAX_RECTIFIED || size.height () > MAX_RECTIFIED)
      return QImage ();

   // grey stays grey, and anything else is done as colour
   QImage src = picture;
   bool grey = src.format () == QImage::Format_Grayscale8;

   if (!grey && src.format () != QImage::Format_RGB32)
      src = src.convertToFormat (QImage::Format_RGB32);

   QImage out (size, grey ? QImage::Format_Grayscale8
                          : QImage::Format_RGB32);

   if (out.isNull ())
      return QImage ();

   // the ends in the picture of the line up the page for each column
   QVector<QPointF> head (size.width ()), step (size.width ());
   int seg = 0;

   for (int x = 0; x < size.width (); x++)
      {
      double at = (x + 0.5) / scale;

      while (seg < count - 2 && along [seg + 1] < at)
         seg++;

      double len = along [seg + 1] - along [seg];
      double part = len > 0 ? qBound (0.0, (at - along [seg]) / len, 1.0)
                            : 0;
      QPointF t = curve.top [seg]
            + (curve.top [seg + 1] - curve.top [seg]) * part;
      QPointF b = curve.bottom [seg]
            + (curve.bottom [seg + 1] - curve.bottom [seg]) * part;

      head [x] = t - QPointF (0.5, 0.5);
      step [x] = (b - t) / size.height ();
      }

   int bytes = grey ? 1 : 4;
   int sw = src.width (), sh = src.height ();
   int stride = src.bytesPerLine ();
   const uchar *bits = src.constBits ();

   for (int y = 0; y < size.height (); y++)
      {
      uchar *line = out.scanLine (y);
      double fy = y + 0.5;

      for (int x = 0; x < size.width (); x++)
         mix (bits, sw, sh, stride, bytes,
              head [x].x () + step [x].x () * fy,
              head [x].y () + step [x].y () * fy, line + x * bytes);
      }
   return out;
   }


QList<QImage> Pagefind::pages (const QImage &picture, Mode mode,
                               Result *result, double focal)
   {
   Result found = find (picture, mode);
   QList<QImage> out;

   if (result)
      *result = found;
   for (int i = 0; i < found.pages.size (); i++)
      {
      QImage page;

      if (i < found.curves.size () && found.curves [i].traced)
         page = flatten (picture, found.curves [i], focal);
      if (page.isNull ())
         page = rectify (picture, found.pages [i]);
      if (!page.isNull ())
         out << page;
      }
   return out;
   }
