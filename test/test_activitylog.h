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

#pragma once

#include "suite.h"

class TestActivityLog : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! an entry goes to JSON and back, with what is not set left out
   void testJson();

   //! entries are added at the end of the file and read back in order
   void testAddRead();

   //! a line which is damaged, or not understood, is passed over
   void testDamaged();

   //! each kind of entry says what was done in a line
   void testDescribe();

   //! a stack is followed through renames, moves, undo and redo
   void testWhereNow();

   //! a stack is followed when a folder above it is renamed or moved
   void testWhereNowFolders();

   //! with the log turned off, nothing is written
   void testDisabled();
};
