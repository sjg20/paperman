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
#include <QList>
#include <QString>

#include "activitylog.h"

class QLabel;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

/** Shows what has been done to the papers in a repository, from its record
    of activity, and goes to a stack from what was done to it.

    The newest thing is at the top. Things done as one step, such as the
    pages of a scan or the stacks named together, are under a line for the
    whole, which opens to show them. Something which has since been undone
    is greyed.

    Choosing a line, with the mouse or by moving through the list with the
    keys, asks for the stack it is about to be shown where it is now, which
    may not be where it was then: it is followed through whatever was done
    to it afterwards. The window stays open while Paperman is used, and
    follows the record as it grows */
class Activitydialog : public QDialog
   {
   Q_OBJECT

public:
   explicit Activitydialog (QWidget *parent = nullptr);

   /** set the repository whose record is shown

      \param root  the top of the repository; empty for none, as when the
                   folder shown is on a server */
   void setRoot (const QString &root);

   //! the top of the repository whose record is shown
   QString root () const { return _root; }

   //! what each line at the top level says, newest first
   QStringList listed () const;

   //! what the lines under a line at the top level say, in order
   QStringList listedUnder (int row) const;

   //! true if a line at the top level is for something since undone
   bool isUndone (int row) const;

   /** choose a line, as clicking it does

      \param row    the line at the top level, from 0
      \param child  the line under it, from 0; -1 for the line itself */
   void choose (int row, int child = -1);

   //! what the line below the list says about the stack chosen
   QString status () const;

   //! the most entries shown, counted from the newest
   static constexpr int max_entries = 5000;

public slots:
   //! read the record again, soon; several calls close together read once
   void refresh ();

   //! read the record again now
   void reload ();

signals:
   /** a line has been chosen whose stack is still there

      \param pathname  the stack's full path, as it is now
      \param page      the page the line is about, from 0; -1 if none */
   void stackChosen (const QString &pathname, int page);

   /** a line about a folder has been chosen, and the folder is still there

      \param pathname  the folder's full path, as it is now */
   void folderChosen (const QString &pathname);

private slots:
   //! show where the stack of the line chosen is now, and ask for it
   void lineChosen ();

private:
   //! the line for an entry
   QTreeWidgetItem *lineFor (int at, const QSet<qint64> &undone) const;

   QString _root;
   QList<Activity> _acts;        //!< the entries shown, oldest first
   QTreeWidget *_tree;
   QLabel *_where;               //!< says where the stack chosen is now
   QLabel *_none;                //!< says why there is nothing to show
   QTimer *_timer;               //!< puts off a refresh a little
   };
