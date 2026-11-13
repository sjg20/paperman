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

#include <QModelIndex>

#include "suite.h"

class Camerapanel;
class Desktopmodel;
class File;
class Mainwindow;

class TestCamera : public Suite
{
   Q_OBJECT

public:
   using Suite::Suite;

   /** take the Huffman tables out of a JPEG, as a camera may send it
      \param jpeg   the JPEG
      \returns the JPEG without them */
   static QByteArray withoutTables(const QByteArray &jpeg);

private:
   /** set up a repository in a window, for filing into
      \param me       the window
      \param model    returns the model for its stacks
      \param parent   returns the folder which is shown */
   void setupDesk(Mainwindow *me, Desktopmodel *&model, QModelIndex &parent);

   /** show the window and open the camera panel from its toolbar
      \param me      the window
      \returns the panel, nullptr if it did not open */
   Camerapanel *openPanel(Mainwindow *me);

   /** find the stack with a number of pages
      \returns the stack, nullptr if there is not exactly one */
   File *stackWithPages(Desktopmodel *model, const QModelIndex &parent,
                        int pages);

   /** a picture of an open book on a mat, as a JPEG
      \param gutter  where its gutter is, as a proportion of its width */
   static QByteArray bookJpeg(double gutter);

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

   //! a still the camera sends unasked is passed on, and not shown
   void testPush();

   //! each picture filed is a page of one stack, stored as it was given
   void testFile();

   //! a stack which is cancelled, or never has a page, is not left behind
   void testFileNone();

   //! the main widget keeps or throws away the stack of pages it is given
   void testSession();

   //! the panel shows what the camera sees and files each still taken
   void testPanel();

   //! Cancel leaves no stack, and closing the panel keeps the pages
   void testPanelCancel();

   //! with no camera the button is greyed out and the panel says so
   void testPanelNoCamera();

   //! the camera and size chosen are used, and remembered
   void testPanelChoices();

   //! a still which fails is reported, and another can be taken
   void testPanelFailure();

   //! a still the camera sends unasked is filed as a page
   void testPanelPush();

   //! pages are made of pictures in the background, in order
   void testPages();

   //! a book whose pages curve has them made flat
   void testPagesCurved();

   //! the camera's own pictures are kept in a stack written as they come
   void testKeep();

   //! the main widget files the pages of each picture, and its pictures
   void testSessionPages();
};

#endif
