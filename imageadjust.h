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

#ifndef __imageadjust_h
#define __imageadjust_h

#include <QImage>

class ImageAdjust
   {
public:
   enum e_adjust
      {
      Adjust_whiten,      //!< whiten the background of scanned pages

      Adjust_count
      };

   /** apply an adjustment to an image

      \param image    the image to adjust (modified in place)
      \param type     which adjustment to apply */
   static void apply (QImage &image, e_adjust type);

   /** return a human-readable name for an adjustment type */
   static QString name (e_adjust type);

   /** return a suffix for filenames created by this adjustment */
   static QString suffix (e_adjust type);

   /** whiten the background of a scanned image by stretching levels
       so the background maps to white and dark text is preserved

      \param image   the image to process (modified in place) */
   static void whitenBackground (QImage &image);
   };

#endif
