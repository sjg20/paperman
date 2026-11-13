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

#ifndef TEST_CAMERA_H
#define TEST_CAMERA_H

#include "suite.h"

class TestCamera : public Suite
{
   Q_OBJECT

public:
   using Suite::Suite;

   /** take the Huffman tables out of a JPEG, as a camera may send it
      \param jpeg   the JPEG
      \returns the JPEG without them */
   static QByteArray withoutTables(const QByteArray &jpeg);

private slots:
   void init();
   void cleanup();

   //! a JPEG's size and depth are read, and a damaged one is refused
   void testJpegInfo();

   //! a picture with its tables is kept as it is, less what follows it
   void testStillAsSent();

   //! a picture without Huffman tables gets the standard ones
   void testStillTables();

   //! the small pictures are the smallest of the still's shape
   void testPreviewSize();

   //! the fake cameras are the ones listed, and can be made
   void testList();

   //! a fake camera sends small pictures until it is stopped
   void testPreview();

   //! a fake camera sends the stills it is given, as they are
   void testCapture();

   //! a camera which fails says so
   void testFailures();
};

#endif
