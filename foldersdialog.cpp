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

#include <QCompleter>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include "foldersdialog.h"

namespace {

/** edits a folder, offering those there are as it is typed, wherever in
    their paths what is typed comes */
class FolderDelegate : public QStyledItemDelegate
   {
public:
   FolderDelegate (const QStringList *folders, QObject *parent)
         : QStyledItemDelegate (parent), _folders (folders)
      {
      }

   QWidget *createEditor (QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
      {
      QLineEdit *edit = new QLineEdit (parent);
      QCompleter *completer = new QCompleter (*_folders, edit);

      completer->setCaseSensitivity (Qt::CaseInsensitive);
      completer->setFilterMode (Qt::MatchContains);
      edit->setCompleter (completer);
      return edit;
      }

private:
   const QStringList *_folders;
   };

}


Foldersdialog::Foldersdialog (QWidget *parent)
      : QDialog (parent)
   {
   setWindowTitle (tr ("Suggested folders"));

   QVBoxLayout *layout = new QVBoxLayout (this);
   QLabel *intro = new QLabel (tr ("Check the folder suggested for each "
      "stack. A folder can be changed to any other, and a stack which is "
      "not ticked stays where it is. Nothing moves until you press Move, "
      "and it can be undone."), this);

   intro->setWordWrap (true);
   layout->addWidget (intro);

   _table = new QTableWidget (0, col_count, this);
   _table->setObjectName ("folders");
   _table->setHorizontalHeaderLabels (QStringList () << tr ("Stack")
                                                     << tr ("Move to"));
   _table->verticalHeader ()->hide ();
   _table->setSelectionMode (QAbstractItemView::NoSelection);
   _table->horizontalHeader ()->setSectionResizeMode (QHeaderView::Stretch);
   _table->setTextElideMode (Qt::ElideMiddle);
   _table->setItemDelegateForColumn (col_folder,
                                     new FolderDelegate (&_folders, this));
   layout->addWidget (_table);

   QDialogButtonBox *box = new QDialogButtonBox (QDialogButtonBox::Cancel,
                                                 this);

   _move = box->addButton (QString (), QDialogButtonBox::AcceptRole);
   _move->setObjectName ("move");
   _move->setDefault (true);
   layout->addWidget (box);
   connect (box, &QDialogButtonBox::accepted, this, &QDialog::accept);
   connect (box, &QDialogButtonBox::rejected, this, &QDialog::reject);
   connect (_table, &QTableWidget::itemChanged, this,
            &Foldersdialog::changed);
   resize (640, 400);
   changed ();
   }


void Foldersdialog::setFolders (const QStringList &folders,
                                const QString &current)
   {
   _folders = folders;
   _current = current;
   changed ();
   }


void Foldersdialog::setStacks (const QStringList &stacks,
                               const QStringList &suggested,
                               const QStringList &notes)
   {
   QSignalBlocker blocker (_table);

   _table->setRowCount (stacks.size ());
   for (int row = 0; row < stacks.size (); row++)
      {
      QString folder = suggested.value (row);
      QString note = notes.value (row);
      QTableWidgetItem *stack = new QTableWidgetItem (stacks [row]);
      QTableWidgetItem *where = new QTableWidgetItem (folder);

      // the stack's name is not for changing here, only where it goes
      stack->setFlags (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      stack->setCheckState (folder.isEmpty () ? Qt::Unchecked : Qt::Checked);
      stack->setToolTip (stacks [row]);

      // say why there is nothing suggested, beside the stack's name
      if (folder.isEmpty () && !note.isEmpty ())
         {
         stack->setText (tr ("%1 (%2)").arg (stacks [row], note));
         where->setToolTip (note);
         }
      _table->setItem (row, col_stack, stack);
      _table->setItem (row, col_folder, where);
      }
   blocker.unblock ();
   changed ();
   }


QStringList Foldersdialog::folders () const
   {
   QStringList list;

   for (int row = 0; row < _table->rowCount (); row++)
      {
      QString folder = _table->item (row, col_folder)->text ().trimmed ();

      while (folder.endsWith ('/'))
         folder.chop (1);

      // only to a folder which is there, and which is somewhere else
      if (!ticked (row) || !_folders.contains (folder) || folder == _current)
         folder.clear ();
      list << folder;
      }
   return list;
   }


int Foldersdialog::count () const
   {
   int count = 0;

   for (const QString &folder : folders ())
      count += !folder.isEmpty ();
   return count;
   }


void Foldersdialog::setTicked (int row, bool ticked)
   {
   _table->item (row, col_stack)->setCheckState (ticked ? Qt::Checked
                                                         : Qt::Unchecked);
   }


bool Foldersdialog::ticked (int row) const
   {
   return _table->item (row, col_stack)->checkState () == Qt::Checked;
   }


void Foldersdialog::setFolder (int row, const QString &folder)
   {
   _table->item (row, col_folder)->setText (folder);
   }


QString Foldersdialog::moveText () const
   {
   return _move->text ();
   }


void Foldersdialog::changed ()
   {
   int num = count ();

   _move->setText (num == 1 ? tr ("&Move 1 stack")
                            : tr ("&Move %1 stacks").arg (num));
   _move->setEnabled (num > 0);
   }
