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

#include "docfacts.h"

class QLabel;
class QPushButton;
class QTableWidget;

/** Shows the names suggested for several stacks, for the user to check
    before anything is renamed.

    Each stack is a row with a tick, the name it has and the name
    suggested, which can be changed. A stack with no suggestion says why
    and starts unticked. Only the stacks ticked, with a name which differs
    from the one they have, are to be renamed.

    The facts which came with a name are shown beside it: what kind of
    document it is, who it is from, its date and its amount. They can be
    changed too, and are kept with each stack which is ticked */
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

   /** set the facts which came with each name, to be shown beside it and
       changed if they are wrong. Call this after setNames()

      \param facts  the facts for each stack, empty for one with none */
   void setFacts (const QList<Docfacts> &facts);

   /** the facts to keep about each stack, as the user leaves things

      \returns the facts for each stack, with what has been changed made
               fit to keep; empty for a stack which is not ticked */
   QList<Docfacts> facts () const;

   //! change one of the facts shown for a stack: its column, from col_kind
   void setFact (int row, int col, const QString &text);

   //! the columns of the table
   enum
      {
      col_current,   //!< the name the stack has, with the tick
      col_new,       //!< the name suggested
      col_kind,      //!< what kind of document it is
      col_from,      //!< who it is from
      col_date,      //!< its date
      col_amount,    //!< its main amount, with the currency

      col_count
      };

   /** the name to give each stack, as the user leaves things

      \returns a name for each stack, empty for one which is to be left
               as it is */
   QStringList names () const;

   //! how many stacks are to be given a name, or facts, or both
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
   QList<Docfacts> _facts;    //!< the facts given, before any change
   QTableWidget *_table;
   QLabel *_intro;
   QPushButton *_rename;
   };
