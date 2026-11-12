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

#ifndef TEST_PAGECOVER_H
#define TEST_PAGECOVER_H

#include "suite.h"

class TestPagecover : public Suite
{
   Q_OBJECT

public:
   using Suite::Suite;

private slots:
   //! the whole page and boxes are read, and anything else is refused
   void testParse();

   //! a cover is written as "page" or a box to four decimal places
   void testFormat();

   //! a cover goes in and out of a page's properties
   void testProps();

   //! a box follows the page as it is turned or mirrored
   void testTransform();

   //! a box stays where it was on the page when the page is turned
   void testTransformImage();

   //! a box keeps the part of it which a crop keeps
   void testCrop();

   //! undoing a crop puts what is left of the box back where it was
   void testUncrop();
};

#endif
