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

#include "pagecover.h"

const char *const Pagecover::key = "cover";

//! the text kept for a cover which is the whole page
static const char *const whole_page = "page";

//! how far past the edge of the page a box read in may reach
static const double SLACK = 0.5 / Pagecover::Units;


Pagecover::Pagecover ()
      : _kind (Kind_none), _x (0), _y (0), _w (0), _h (0)
   {
   }


Pagecover::Pagecover (int x, int y, int w, int h)
      : _kind (Kind_box), _x (x), _y (y), _w (w), _h (h)
   {
   }


Pagecover Pagecover::wholePage ()
   {
   Pagecover cover;

   cover._kind = Kind_page;
   return cover;
   }


//! a proportion of the page in Units, kept to the page
static int to_units (double value)
   {
   long units = std::lround (value * Pagecover::Units);

   return int (std::min (std::max (units, 0L), long (Pagecover::Units)));
   }


Pagecover Pagecover::fromBox (const QRectF &box)
   {
   QRectF rect = box.normalized ();

   if (!std::isfinite (rect.left ()) || !std::isfinite (rect.top ())
       || !std::isfinite (rect.right ()) || !std::isfinite (rect.bottom ()))
      return Pagecover ();

   // round the edges, so that boxes side by side stay that way
   int left = to_units (rect.left ());
   int top = to_units (rect.top ());
   int right = to_units (rect.right ());
   int bottom = to_units (rect.bottom ());

   if (right <= left || bottom <= top)
      return Pagecover ();
   if (!left && !top && right == Units && bottom == Units)
      return wholePage ();
   return Pagecover (left, top, right - left, bottom - top);
   }


bool Pagecover::parse (const QString &str, Pagecover &cover)
   {
   cover = Pagecover ();
   if (str == whole_page)
      {
      cover = wholePage ();
      return true;
      }

   const QStringList parts = str.split (',');

   if (parts.size () != 4)
      return false;

   double value [4];

   for (int i = 0; i < 4; i++)
      {
      bool ok;

      value [i] = parts [i].trimmed ().toDouble (&ok);
      if (!ok || !std::isfinite (value [i]) || value [i] < 0
          || value [i] > 1 + SLACK)
         return false;
      }

   double x = value [0], y = value [1], w = value [2], h = value [3];

   if (x + w > 1 + SLACK || y + h > 1 + SLACK)
      return false;
   cover = fromBox (QRectF (x, y, w, h));
   return !cover.isNull ();
   }


Pagecover Pagecover::fromProps (const QMap<QString, QString> &props)
   {
   Pagecover cover;

   parse (props.value (key), cover);
   return cover;
   }


void Pagecover::toProps (QMap<QString, QString> &props) const
   {
   if (isNull ())
      props.remove (key);
   else
      props.insert (key, toString ());
   }


//! a value in Units as the proportion it is, to four decimal places
static QString units_str (int units)
   {
   return QString::number (double (units) / Pagecover::Units, 'f', 4);
   }


QString Pagecover::toString () const
   {
   switch (_kind)
      {
      case Kind_none :
         break;
      case Kind_page :
         return whole_page;
      case Kind_box :
         return QString ("%1,%2,%3,%4").arg (units_str (_x))
            .arg (units_str (_y)).arg (units_str (_w)).arg (units_str (_h));
      }
   return QString ();
   }


QRectF Pagecover::box () const
   {
   switch (_kind)
      {
      case Kind_none :
         break;
      case Kind_page :
         return QRectF (0, 0, 1, 1);
      case Kind_box :
         return QRectF (double (_x) / Units, double (_y) / Units,
                        double (_w) / Units, double (_h) / Units);
      }
   return QRectF ();
   }


Pagecover Pagecover::transformed (File::e_transform op) const
   {
   if (!isBox ())
      return *this;

   // where the box's top-left corner goes, and how big it is then
   switch (op)
      {
      case File::Transform_rotate90 :     // clockwise
         return Pagecover (Units - _y - _h, _x, _h, _w);
      case File::Transform_rotate180 :
         return Pagecover (Units - _x - _w, Units - _y - _h, _w, _h);
      case File::Transform_rotate270 :    // anticlockwise
         return Pagecover (_y, Units - _x - _w, _h, _w);
      case File::Transform_hflip :
         return Pagecover (Units - _x - _w, _y, _w, _h);
      case File::Transform_vflip :
         return Pagecover (_x, Units - _y - _h, _w, _h);
      }
   return *this;
   }


Pagecover Pagecover::cropped (const QRectF &area) const
   {
   QRectF kept = area.normalized ();

   if (!isBox () || kept.isEmpty ())
      return *this;

   QRectF part = box () & kept;

   if (part.isEmpty ())
      return Pagecover ();
   return fromBox (QRectF ((part.x () - kept.x ()) / kept.width (),
                           (part.y () - kept.y ()) / kept.height (),
                           part.width () / kept.width (),
                           part.height () / kept.height ()));
   }


Pagecover Pagecover::uncropped (const QRectF &area) const
   {
   QRectF kept = area.normalized ();

   if (!isBox () || kept.isEmpty ())
      return *this;

   QRectF rect = box ();

   return fromBox (QRectF (kept.x () + rect.x () * kept.width (),
                           kept.y () + rect.y () * kept.height (),
                           rect.width () * kept.width (),
                           rect.height () * kept.height ()));
   }


bool Pagecover::operator== (const Pagecover &other) const
   {
   if (_kind != other._kind)
      return false;
   return _kind != Kind_box || (_x == other._x && _y == other._y
                                && _w == other._w && _h == other._h);
   }
