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

#ifndef __pagecover_h
#define __pagecover_h

#include <QMap>
#include <QRectF>
#include <QString>

#include "file.h"

/** which part of a page is a stack's front cover

A book scanned with its dust jacket laid flat makes one wide page, of
which only a part is the front cover. A page holding the cover keeps it
as its 'cover' property: "page" when the whole page is the cover, or the
box as four proportions of the page, "x,y,w,h", each to four decimal
places.

The box is held in units of 1/10000 of the page, so that turning or
mirroring a page, and then doing the opposite, gives back exactly the
box there was */
class Pagecover
   {
public:
   //! the name of the property which holds the cover
   static const char *const key;

   //! no cover
   Pagecover ();

   //! the whole page is the cover
   static Pagecover wholePage ();

   /** the cover is a box on the page

      The box is kept to the page and rounded to four decimal places. One
      covering the whole page is the whole page

      \param box  the box, as proportions of the page
      \returns the cover, or no cover if nothing of the box is left */
   static Pagecover fromBox (const QRectF &box);

   /** read a cover from the text a page keeps for it

      \param str    "page", or "x,y,w,h" with each a proportion of the page
      \param cover  returns the cover, or no cover if @p str is not one
      \returns true if @p str is a cover */
   static bool parse (const QString &str, Pagecover &cover);

   /** the cover in a page's properties

      \param props  the properties
      \returns the cover, or no cover if they hold none, or one which
               cannot be read */
   static Pagecover fromProps (const QMap<QString, QString> &props);

   /** put the cover in a page's properties, or take it out for no cover

      \param props  the properties to change */
   void toProps (QMap<QString, QString> &props) const;

   //! the text kept for the cover, or an empty string for no cover
   QString toString () const;

   //! true if there is no cover
   bool isNull () const { return _kind == Kind_none; }

   //! true if the whole page is the cover
   bool isWholePage () const { return _kind == Kind_page; }

   //! true if a box on the page is the cover
   bool isBox () const { return _kind == Kind_box; }

   /** the part of the page which is the cover, as proportions of the
       page: all of it for the whole page, nothing for no cover */
   QRectF box () const;

   /** the cover once the page is turned or mirrored

      \param op  the change to the page
      \returns the cover, in the same place on what the page shows */
   Pagecover transformed (File::e_transform op) const;

   /** the cover once the page is cropped

      \param area  the part of the page kept, as proportions of it
      \returns the part of the cover which is kept, as proportions of
               the new page, or no cover if none of it is */
   Pagecover cropped (const QRectF &area) const;

   /** the cover once a crop is undone: the opposite of cropped(), though
       what the crop cut away cannot come back

      \param area  the part of the page the crop kept, as proportions of
                   the page as it was
      \returns the cover, as proportions of the page as it was */
   Pagecover uncropped (const QRectF &area) const;

   bool operator== (const Pagecover &other) const;
   bool operator!= (const Pagecover &other) const
      { return !(*this == other); }

   /** units of the page in each part of a box: a box's values are kept
       in these, as four decimal places of a proportion */
   enum { Units = 10000 };

private:
   enum e_kind
      {
      Kind_none,     //!< no cover
      Kind_page,     //!< the whole page
      Kind_box       //!< the box _x, _y, _w, _h
      };

   //! a cover which is a box, in Units, which must be on the page
   Pagecover (int x, int y, int w, int h);

   e_kind _kind;
   int _x, _y, _w, _h;   //!< the box, in Units of the page
   };

#endif
