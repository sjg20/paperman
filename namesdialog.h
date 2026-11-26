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
#include <QStringList>

class QLabel;
class QPushButton;
class QTableWidget;

/** Shows the names suggested for several stacks, for the user to check
    before anything is renamed.

    Each stack is a row with a tick, the name it has and the name
    suggested, which can be changed. A stack with no suggestion says why
    and starts unticked. Only the stacks ticked, with a name which differs
    from the one they have, are to be renamed */
class Namesdialog : public QDialog
   {
   Q_OBJECT

public:
   explicit Namesdialog (QWidget *parent = nullptr);

   /** set the stacks to show

      \param current    the name each stack has
      \param suggested  the name suggested for each, empty if none
      \param notes      why a stack has no suggestion, empty if it has one */
   void setNames (const QStringList &current, const QStringList &suggested,
                  const QStringList &notes);

   /** the name to give each stack, as the user leaves things

      \returns a name for each stack, empty for one which is to be left
               as it is */
   QStringList names () const;

   //! how many stacks are to be renamed
   int count () const;

   //! tick a stack, or not; from 0
   void setTicked (int row, bool ticked);
   bool ticked (int row) const;

   //! change the name suggested for a stack
   void setName (int row, const QString &name);

   //! what the button which renames the stacks says
   QString renameText () const;

private slots:
   //! keep the button in step with what is ticked
   void changed ();

private:
   QStringList _current;
   QTableWidget *_table;
   QLabel *_intro;
   QPushButton *_rename;
   };
