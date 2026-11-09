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

#ifndef TEST_IMAGETRIM_H
#define TEST_IMAGETRIM_H

#include "suite.h"

class TestImagetrim : public Suite
{
   Q_OBJECT

public:
   using Suite::Suite;

private slots:
   //! a page with white margins is cut just outside its print
   void testMargins();

   //! scanner background around a sheet is cut at the paper's edge
   void testBackground();

   //! a page mostly covered in colour still has its margins found
   void testColourPage();

   //! a blank page gives nothing to go by
   void testBlank();

   //! dust in a margin does not count as content
   void testDust();

   //! a small margin survives the working image being scaled down
   void testLargePage();

   //! a two-level page can be measured
   void testMono();

   //! areas convert to pixels, rounding outwards and staying inside
   void testAreaToRect();

   //! out-of-range areas are refused
   void testAreaValid();

   //! cropRect() leaves alone a page it would not change
   void testCropRectNoChange();
};

#endif
