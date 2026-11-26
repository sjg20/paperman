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

class TestDocfacts : public Suite
{
   Q_OBJECT

public:
   using Suite::Suite;

private slots:
   //! each fact is made fit to keep, and one in the wrong form is dropped
   void testClean();

   //! facts go in and out of a stack's properties, leaving the others
   void testProps();

   //! facts are read from JSON, as a model gives them, and written to it
   void testJson();

   //! the schema asks for each fact, and insists on none
   void testSchema();

   //! facts are kept in a stack's file, beside its other properties
   void testFile();
};
