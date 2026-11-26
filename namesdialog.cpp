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
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "namesdialog.h"

//! the columns of the table
enum
   {
   COL_current,   //!< with the tick
   COL_new,

   COL_count
   };


Namesdialog::Namesdialog (QWidget *parent)
   : QDialog (parent)
   {
   setObjectName ("suggestedNames");
   setWindowTitle (tr ("Suggested names"));

   QVBoxLayout *layout = new QVBoxLayout (this);

   _intro = new QLabel (tr ("Check the names suggested. A name can be "
      "changed, and a stack which is not ticked keeps the name it has."),
      this);
   _intro->setWordWrap (true);
   layout->addWidget (_intro);

   _table = new QTableWidget (0, COL_count, this);
   _table->setObjectName ("names");
   _table->setHorizontalHeaderLabels (QStringList () << tr ("Stack")
                                                     << tr ("New name"));
   _table->verticalHeader ()->hide ();
   _table->setSelectionMode (QAbstractItemView::NoSelection);
   _table->horizontalHeader ()->setSectionResizeMode (COL_current,
                                                    QHeaderView::Stretch);
   _table->horizontalHeader ()->setSectionResizeMode (COL_new,
                                                    QHeaderView::Stretch);
   _table->setTextElideMode (Qt::ElideMiddle);
   layout->addWidget (_table);

   QDialogButtonBox *box = new QDialogButtonBox (QDialogButtonBox::Cancel,
                                                 this);

   _rename = box->addButton (QString (), QDialogButtonBox::AcceptRole);
   _rename->setObjectName ("rename");
   _rename->setDefault (true);
   layout->addWidget (box);
   connect (box, &QDialogButtonBox::accepted, this, &QDialog::accept);
   connect (box, &QDialogButtonBox::rejected, this, &QDialog::reject);
   connect (_table, &QTableWidget::itemChanged, this, &Namesdialog::changed);
   resize (640, 400);
   changed ();
   }


void Namesdialog::setNames (const QStringList &current,
                            const QStringList &suggested,
                            const QStringList &notes)
   {
   QSignalBlocker blocker (_table);

   _current = current;
   _table->setRowCount (current.size ());
   for (int row = 0; row < current.size (); row++)
      {
      QString name = suggested.value (row);
      QTableWidgetItem *stack = new QTableWidgetItem (current [row]);
      QTableWidgetItem *next = new QTableWidgetItem (name);

      // the name a stack has is not for changing here, only the new one
      stack->setFlags (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      stack->setCheckState (name.isEmpty () ? Qt::Unchecked : Qt::Checked);
      stack->setToolTip (current [row]);
      if (name.isEmpty ())
         next->setToolTip (notes.value (row));
      _table->setItem (row, COL_current, stack);
      _table->setItem (row, COL_new, next);

      // say why there is nothing suggested, where a name would be
      if (name.isEmpty () && !notes.value (row).isEmpty ())
         {
         next->setText (QString ());
         stack->setText (tr ("%1 (%2)").arg (current [row], notes.value (row)));
         }
      }
   blocker.unblock ();
   changed ();
   }


QStringList Namesdialog::names () const
   {
   QStringList list;

   for (int row = 0; row < _table->rowCount (); row++)
      {
      QString name = _table->item (row, COL_new)->text ().trimmed ();

      if (!ticked (row) || name == _current.value (row))
         name.clear ();
      list << name;
      }
   return list;
   }


int Namesdialog::count () const
   {
   int count = 0;

   for (const QString &name : names ())
      count += !name.isEmpty ();
   return count;
   }


void Namesdialog::setTicked (int row, bool ticked)
   {
   _table->item (row, COL_current)->setCheckState (ticked ? Qt::Checked
                                                           : Qt::Unchecked);
   }


bool Namesdialog::ticked (int row) const
   {
   return _table->item (row, COL_current)->checkState () == Qt::Checked;
   }


void Namesdialog::setName (int row, const QString &name)
   {
   _table->item (row, COL_new)->setText (name);
   }


QString Namesdialog::renameText () const
   {
   return _rename->text ();
   }


void Namesdialog::changed ()
   {
   int num = count ();

   _rename->setText (num == 1 ? tr ("&Rename 1 stack")
                              : tr ("&Rename %1 stacks").arg (num));
   _rename->setEnabled (num > 0);
   }
