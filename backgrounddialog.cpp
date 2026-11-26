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
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "backgrounddialog.h"
#include "ocrreader.h"

//! the columns of the list
enum
   {
   COL_stack,
   COL_folder,
   COL_left,

   COL_count
   };


Backgrounddialog::Backgrounddialog (OcrReader *reader, QWidget *parent)
   : QDialog (parent), _reader (reader)
   {
   setObjectName ("backgroundWork");
   setWindowTitle (tr ("Background work"));

   QVBoxLayout *layout = new QVBoxLayout (this);

   _summary = new QLabel (this);
   _summary->setObjectName ("summary");
   _summary->setWordWrap (true);
   layout->addWidget (_summary);

   _table = new QTableWidget (0, COL_count, this);
   _table->setObjectName ("stacks");
   _table->setHorizontalHeaderLabels (QStringList () << tr ("Stack")
      << tr ("Folder") << tr ("Pages left"));
   _table->setSelectionBehavior (QAbstractItemView::SelectRows);
   _table->setSelectionMode (QAbstractItemView::ExtendedSelection);
   _table->setEditTriggers (QAbstractItemView::NoEditTriggers);
   _table->verticalHeader ()->hide ();
   _table->horizontalHeader ()->setSectionResizeMode (COL_stack,
                                                    QHeaderView::Stretch);
   _table->horizontalHeader ()->setSectionResizeMode (COL_folder,
                                                    QHeaderView::Stretch);

   /* what is left is short and is what the window is for, so it always
      gets the room it needs; a long folder loses its middle instead */
   _table->horizontalHeader ()->setSectionResizeMode (COL_left,
                                             QHeaderView::ResizeToContents);
   _table->setTextElideMode (Qt::ElideMiddle);
   layout->addWidget (_table);

   QHBoxLayout *buttons = new QHBoxLayout;

   _pause = new QPushButton (this);
   _pause->setObjectName ("pause");
   _remove = new QPushButton (tr ("&Remove from list"), this);
   _remove->setObjectName ("remove");
   _remove->setToolTip (tr ("Do not read the stacks selected. The pages of "
      "them already read keep their text"));
   _stop = new QPushButton (tr ("&Stop reading"), this);
   _stop->setObjectName ("stop");
   _stop->setToolTip (tr ("Do not read any of these stacks. The pages "
      "already read keep their text, and Read text (OCR) reads the rest "
      "another time"));
   buttons->addWidget (_pause);
   buttons->addWidget (_remove);
   buttons->addWidget (_stop);
   buttons->addStretch ();

   QDialogButtonBox *box = new QDialogButtonBox (QDialogButtonBox::Close,
                                                 this);

   buttons->addWidget (box);
   layout->addLayout (buttons);

   connect (_pause, &QPushButton::clicked, this,
            &Backgrounddialog::togglePaused);
   connect (_remove, &QPushButton::clicked, this,
            &Backgrounddialog::removeSelected);
   connect (_stop, &QPushButton::clicked, this, &Backgrounddialog::stopAll);
   connect (box, &QDialogButtonBox::rejected, this, &QDialog::close);
   connect (_table, &QTableWidget::itemSelectionChanged, this, [this] () {
      _remove->setEnabled (!_table->selectedItems ().isEmpty ());
   });
   connect (_reader, &OcrReader::progress, this, &Backgrounddialog::refresh);
   resize (640, 360);
   refresh ();
   }


QString Backgrounddialog::leftText (const OcrReader *reader)
   {
   int stacks = reader->stacksLeft (), pages = reader->pagesLeft ();
   QString in = stacks == 1 ? tr ("1 stack")
                            : tr ("%1 stacks").arg (QLocale ().toString (stacks));

   // a stack not yet opened has pages too, but how many is not known
   if (!pages)
      return in;
   return tr ("%1 in %2")
      .arg (pages == 1 ? tr ("1 page")
                       : tr ("%1 pages").arg (QLocale ().toString (pages)))
      .arg (in);
   }


QStringList Backgrounddialog::listed () const
   {
   QStringList paths;

   for (int row = 0; row < _table->rowCount (); row++)
      paths << _table->item (row, COL_stack)->data (Qt::UserRole).toString ();
   return paths;
   }


QString Backgrounddialog::summary () const
   {
   return _summary->text ();
   }


void Backgrounddialog::selectRow (int row)
   {
   _table->selectRow (row);
   }


void Backgrounddialog::refresh ()
   {
   const QList<OcrReader::Waiting> waiting = _reader->waiting ();
   bool any = !waiting.isEmpty ();

   if (!any)
      _summary->setText (tr ("Paperman reads the text of scanned pages in the "
         "background. Nothing is waiting to be read."));
   else if (_reader->isPaused ())
      _summary->setText (tr ("Reading the text of scanned pages is paused: "
                             "%1 left.").arg (leftText (_reader)));
   else
      _summary->setText (tr ("Reading the text of scanned pages: %1 left.")
                            .arg (leftText (_reader)));

   // keep the stacks selected so, as the list changes under them
   QStringList selected;

   for (const QModelIndex &ind : _table->selectionModel ()->selectedRows ())
      selected << _table->item (ind.row (), COL_stack)->data (Qt::UserRole)
                     .toString ();

   QSignalBlocker blocker (_table);

   _table->clearSelection ();
   _table->setRowCount (waiting.size ());
   for (int row = 0; row < waiting.size (); row++)
      {
      const OcrReader::Waiting &w = waiting [row];
      QFileInfo fi (w.path);
      QString left = w.pages < 0 ? tr ("not known yet")
                                 : QLocale ().toString (w.left);

      if (w.reading)
         left = tr ("%1, reading now").arg (left);

      QTableWidgetItem *stack = new QTableWidgetItem (fi.completeBaseName ());

      stack->setData (Qt::UserRole, w.path);
      stack->setToolTip (w.path);
      _table->setItem (row, COL_stack, stack);
      QTableWidgetItem *folder = new QTableWidgetItem (fi.path ());

      folder->setToolTip (fi.path ());
      _table->setItem (row, COL_folder, folder);
      _table->setItem (row, COL_left, new QTableWidgetItem (left));
      if (selected.contains (w.path))
         for (int col = 0; col < COL_count; col++)
            _table->item (row, col)->setSelected (true);
      }

   _pause->setText (_reader->isPaused () ? tr ("&Carry on") : tr ("&Pause"));
   _pause->setEnabled (any || _reader->isPaused ());
   _remove->setEnabled (!_table->selectedItems ().isEmpty ());
   _stop->setEnabled (any);
   }


void Backgrounddialog::togglePaused ()
   {
   _reader->setPaused (!_reader->isPaused ());
   refresh ();
   }


void Backgrounddialog::removeSelected ()
   {
   QStringList paths;

   for (const QModelIndex &ind : _table->selectionModel ()->selectedRows ())
      paths << _table->item (ind.row (), COL_stack)->data (Qt::UserRole)
                  .toString ();
   for (const QString &path : paths)
      _reader->remove (path);
   refresh ();
   }


void Backgrounddialog::stopAll ()
   {
   _reader->stop ();
   refresh ();
   }
