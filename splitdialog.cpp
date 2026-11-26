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

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "splitdialog.h"

enum
   {
   COL_name,
   COL_pages,

   COL_count
   };


Splitdialog::Splitdialog (QWidget *parent)
      : QDialog (parent)
   {
   _pagecount = 0;
   _filling = false;
   setWindowTitle (tr ("Split into documents"));

   QVBoxLayout *layout = new QVBoxLayout (this);
   QLabel *label = new QLabel (tr ("The assistant suggests dividing this "
      "stack as below. Change any name, or join a document to the one "
      "before it, then press Split. Nothing changes until you do, and it "
      "can be undone."), this);

   label->setWordWrap (true);
   layout->addWidget (label);

   _table = new QTableWidget (0, COL_count, this);
   _table->setHorizontalHeaderLabels ({tr ("Document"), tr ("Pages")});
   _table->setSelectionBehavior (QAbstractItemView::SelectRows);
   _table->setSelectionMode (QAbstractItemView::SingleSelection);
   _table->setIconSize (QSize (thumb_width, thumb_height));
   _table->verticalHeader ()->hide ();
   _table->horizontalHeader ()->setSectionResizeMode (COL_name,
                                                     QHeaderView::Stretch);
   _table->horizontalHeader ()->setSectionResizeMode (COL_pages,
                                             QHeaderView::ResizeToContents);
   layout->addWidget (_table);

   QDialogButtonBox *buttons = new QDialogButtonBox (
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

   buttons->button (QDialogButtonBox::Ok)->setText (tr ("&Split"));

   // joining is kept apart from the buttons which close the window
   QHBoxLayout *row = new QHBoxLayout;

   _join = new QPushButton (tr ("&Join with previous"), this);
   _join->setAutoDefault (false);
   row->addWidget (_join);
   row->addStretch ();
   row->addWidget (buttons);
   layout->addLayout (row);

   connect (buttons, SIGNAL (accepted ()), this, SLOT (accept ()));
   connect (buttons, SIGNAL (rejected ()), this, SLOT (reject ()));
   connect (_join, SIGNAL (clicked ()), this, SLOT (joinWithPrevious ()));
   connect (_table, SIGNAL (cellChanged (int, int)),
            this, SLOT (nameChanged (int, int)));
   connect (_table, SIGNAL (itemSelectionChanged ()),
            this, SLOT (selectionChanged ()));
   resize (520, 480);
   selectionChanged ();
   }


void Splitdialog::setDocs (const QList<Assistant::Doc> &docs, int pagecount)
   {
   _docs = docs;
   _pagecount = pagecount;
   fill ();
   }


void Splitdialog::setThumbnail (int page, const QPixmap &pixmap)
   {
   _thumbs [page] = pixmap.scaled (thumb_width, thumb_height,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
   fill ();
   }


void Splitdialog::selectDoc (int row)
   {
   _table->selectRow (row);
   }


QString Splitdialog::pagesText (int row) const
   {
   int first = _docs [row].first;
   int end = row + 1 < _docs.size () ? _docs [row + 1].first : _pagecount;

   if (end - first == 1)
      return tr ("%1 (1 page)").arg (first + 1);
   return tr ("%1-%2 (%3 pages)").arg (first + 1).arg (end).arg (end - first);
   }


void Splitdialog::fill ()
   {
   int row = _table->currentRow ();

   _filling = true;
   _table->setRowCount (_docs.size ());
   for (int i = 0; i < _docs.size (); i++)
      {
      QTableWidgetItem *name = new QTableWidgetItem (_docs [i].name);
      QTableWidgetItem *pages = new QTableWidgetItem (pagesText (i));

      if (_thumbs.contains (_docs [i].first))
         name->setIcon (QIcon (_thumbs [_docs [i].first]));
      pages->setFlags (pages->flags () & ~Qt::ItemIsEditable);
      _table->setItem (i, COL_name, name);
      _table->setItem (i, COL_pages, pages);
      _table->setRowHeight (i, thumb_height + 8);
      }
   _filling = false;
   if (row >= 0)
      _table->selectRow (qMin (row, (int)_docs.size () - 1));
   selectionChanged ();
   }


void Splitdialog::joinWithPrevious ()
   {
   int row = _table->currentRow ();

   // its pages go to the one before, whose name stands
   if (row < 1 || row >= _docs.size ())
      return;
   _docs.removeAt (row);
   fill ();
   _table->selectRow (row - 1);
   }


void Splitdialog::nameChanged (int row, int col)
   {
   if (_filling || col != COL_name || row < 0 || row >= _docs.size ())
      return;
   _docs [row].name = Assistant::cleanName (_table->item (row, col)->text ());
   }


void Splitdialog::selectionChanged ()
   {
   _join->setEnabled (_table->currentRow () > 0
                      && !_table->selectedItems ().isEmpty ());
   }
