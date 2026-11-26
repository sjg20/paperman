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

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/

#pragma once

#include <QDialog>
#include <QList>
#include <QMap>
#include <QPixmap>

#include "assistant.h"

class QPushButton;
class QTableWidget;

/** Shows how a stack would be divided into documents, for the user to
    check before anything is done.

    Each document is a row with its first page as a picture, its pages and
    its name, and the facts found about it. A name can be changed, and a
    document can be joined to the one before it, where the stack is divided
    in a place it should not be: the one before keeps its own facts */
class Splitdialog : public QDialog
   {
   Q_OBJECT

public:
   //! the columns of the table
   enum
      {
      col_name,
      col_pages,
      col_kind,
      col_from,
      col_date,
      col_amount,

      col_count
      };

   explicit Splitdialog (QWidget *parent = nullptr);

   /** set the documents to show

      \param docs       the documents, in order, the first starting at
                        page 0
      \param pagecount  number of pages in the stack */
   void setDocs (const QList<Assistant::Doc> &docs, int pagecount);

   //! the documents as the user leaves them
   QList<Assistant::Doc> docs () const { return _docs; }

   /** set the picture shown for a page, which is the first of a document

      \param page    the page, from 0
      \param pixmap  its picture */
   void setThumbnail (int page, const QPixmap &pixmap);

   //! select a document, from 0
   void selectDoc (int row);

   //! the text shown for a document's pages, numbered from 1
   QString pagesText (int row) const;

   //! the size of the picture of each document's first page
   static constexpr int thumb_width = 60, thumb_height = 80;

public slots:
   //! join the document selected to the one before it
   void joinWithPrevious ();

private slots:
   //! take a name the user has changed
   void nameChanged (int row, int col);

   //! allow joining only when there is a document before the one selected
   void selectionChanged ();

private:
   //! show the documents
   void fill ();

   QTableWidget *_table;
   QPushButton *_join;
   QList<Assistant::Doc> _docs;
   int _pagecount;
   QMap<int, QPixmap> _thumbs;   //!< the picture of a page, by its number
   bool _filling;                //!< true while fill() is changing cells
   };
