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

#ifndef __camerascan_h
#define __camerascan_h

#include <functional>

#include <QByteArray>
#include <QModelIndex>
#include <QString>

#include "err.h"

class Desktopmodel;
class File;

/** files the pictures a camera takes as the pages of one stack

The stack is made in a folder as the first page arrives and each picture
after that is added to it, until finish() keeps the stack or cancel()
throws it away. Each picture is a JPEG, which is stored as the page just
as it is.

This uses what a scan uses to make its stack, Desktopmodel::beginScan()
and the calls which follow it, so the stack is shown as it grows and is
uploaded if its folder is on a server. As with a scan, only one of these
can be going at a time, and not during a scan */
class CameraScan
   {
public:
   /** \param model       the model for the stacks
       \param parent      the folder to put the stack in
       \param stack_name  what to call the stack, empty for the date
       \param page_name   what to call its pages: the first has this
                          name and the rest are numbered, as the pages
                          of a scan are
       \param dpi         the resolution to record for each page, which
                          a camera does not know */
   CameraScan (Desktopmodel *model, const QModelIndex &parent,
               const QString &stack_name, const QString &page_name,
               int dpi);

   /** add a picture to the stack, making the stack if this is the first

      \param jpeg   the picture, a whole JPEG file
      \returns error, or NULL if none */
   err_info *addPage (const QByteArray &jpeg);

   /** keep the stack. With no pages there is nothing to keep, and no
       stack is left behind

      \param fname     returns the stack's filename, empty if none
      \param filep     returns the stack, NULL if none
      \param uploaded  called when a stack in a folder on a server has
                       been uploaded, or could not be
      \returns error, or NULL if none */
   err_info *finish (QString *fname = nullptr, File **filep = nullptr,
                     std::function<void (err_info *err)> uploaded
                        = nullptr);

   /** throw the stack away, with its pages
      \returns error, or NULL if none */
   err_info *cancel (void);

   //! the number of pages added so far
   int pageCount (void) const { return _pages; }

   //! true if the stack has been made and neither kept nor thrown away
   bool isOpen (void) const { return _open; }

private:
   Desktopmodel *_model;
   QPersistentModelIndex _parent;
   QString _stack_name;
   QString _page_name;    //!< the name the next page is filed under
   int _dpi;
   bool _open = false;
   int _pages = 0;
   };

#endif
