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

#include <QDialog>
#include <QStringList>

class QPushButton;
class QTableWidget;

/** Shows the folder suggested for each of several stacks, for the user to
    check before anything is moved.

    Each stack is a row with a tick, its name and the folder suggested,
    which can be changed to any other folder there is: the folders are
    offered as the name is typed. A stack with no suggestion says why and
    starts unticked. Only the stacks ticked, with a folder which exists and
    is not the one they are in, are to be moved */
class Foldersdialog : public QDialog
   {
   Q_OBJECT

public:
   explicit Foldersdialog (QWidget *parent = nullptr);

   //! the columns of the table
   enum
      {
      col_stack,     //!< the stack's name, with the tick
      col_folder,    //!< the folder suggested

      col_count
      };

   /** set the folders a stack can be moved to, and the one the stacks are
       in, which is no move at all

      \param folders  each folder, as its path below the top of the
                      repository
      \param current  the folder the stacks are in, in the same form;
                      empty for the top of the repository */
   void setFolders (const QStringList &folders, const QString &current);

   /** set the stacks to show

      \param stacks     the name of each stack
      \param suggested  the folder suggested for each, empty if none
      \param notes      why a stack has no suggestion, empty if it has one */
   void setStacks (const QStringList &stacks, const QStringList &suggested,
                   const QStringList &notes);

   /** the folder to move each stack to, as the user leaves things

      \returns a folder for each stack, empty for one which is to stay
               where it is */
   QStringList folders () const;

   //! how many stacks are to be moved
   int count () const;

   //! tick a stack, or not; from 0
   void setTicked (int row, bool ticked);
   bool ticked (int row) const;

   //! change the folder suggested for a stack
   void setFolder (int row, const QString &folder);

   //! what the button which moves the stacks says
   QString moveText () const;

private slots:
   //! keep the button in step with what is ticked
   void changed ();

private:
   QTableWidget *_table;
   QPushButton *_move;
   QStringList _folders;   //!< the folders there are
   QString _current;       //!< the folder the stacks are in
   };
