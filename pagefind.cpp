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


//! make the small grey copy
static Grey reduce (const QImage &picture)
   {
   Grey grey;
   QImage image = picture;

   if (image.format () != QImage::Format_Grayscale8
       && image.format () != QImage::Format_RGB32
       && image.format () != QImage::Format_ARGB32)
      image = image.convertToFormat (QImage::Format_RGB32);

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
   \returns true if found */
static bool find_outline (const Grey &grey, QPolygonF &quad,
                          double &confidence, bool &squared)
   {
   int w = grey.w, h = grey.h;
   int contrast;
   int level = divide (grey, contrast);

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
   for (int i = 0; i < CLOSE_RADIUS; i++)
      wear (mask, w, h, false);
   for (int i = 0; i < CLOSE_RADIUS; i++)
      wear (mask, w, h, true);

   largest_patch (mask, w, h);

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


Pagefind::Result Pagefind::find (const QImage &picture, Mode mode)
   {
   Result result;

   if (mode == Mode_whole || picture.isNull ())
      return result;

   Grey grey = reduce (picture);
   QPolygonF quad;

   bool squared = false;

   if (!grey.w || !find_outline (grey, quad, result.confidence, squared))
      return result;
   result.found = true;
   result.flat = !squared;

   double top, bottom;
   double depth = find_gutter (grey, quad, top, bottom);

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
      return result;
      }

   // told there are two with no gutter to be seen, cut down the middle
   QPointF gt = quad [0] + (quad [1] - quad [0]) * result.gutter_top;
   QPointF gb = quad [3] + (quad [2] - quad [3]) * result.gutter_bottom;

   result.pages << QPolygonF ({quad [0], gt, gb, quad [3]});
   result.pages << QPolygonF ({gt, quad [1], quad [2], gb});
   return result;
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
         int ix = (int)std::floor (px), iy = (int)std::floor (py);

         sx += map.m11 ();
         sy += map.m12 ();
         sw3 += map.m13 ();
         if (ix < 0 || iy < 0 || ix >= sw - 1 || iy >= sh - 1)
            {
            // on the last row or column there is no next pixel to mix
            if (ix >= -1 && iy >= -1 && ix < sw && iy < sh)
               {
               const uchar *p = bits + qBound (0, iy, sh - 1) * stride
                     + qBound (0, ix, sw - 1) * bytes;

               for (int c = 0; c < bytes; c++)
                  line [x * bytes + c] = p [c];
               }
            else
               for (int c = 0; c < bytes; c++)
                  line [x * bytes + c] = 255;
            continue;
            }

         int wx = (int)((px - ix) * 256), wy = (int)((py - iy) * 256);
         const uchar *p = bits + iy * stride + ix * bytes;
         const uchar *q = p + stride;

         for (int c = 0; c < bytes; c++)
            {
            int upper = p [c] * (256 - wx) + p [c + bytes] * wx;
            int lower = q [c] * (256 - wx) + q [c + bytes] * wx;

            line [x * bytes + c] = (upper * (256 - wy) + lower * wy
                                    + 32768) >> 16;
            }
         }
      }
   return out;
   }


QList<QImage> Pagefind::pages (const QImage &picture, Mode mode,
                               Result *result)
   {
   Result found = find (picture, mode);
   QList<QImage> out;

   if (result)
      *result = found;
   for (const QPolygonF &quad : found.pages)
      {
      QImage page = rectify (picture, quad);

      if (!page.isNull ())
         out << page;
      }
   return out;
   }
