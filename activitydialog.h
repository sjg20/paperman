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

#include <functional>

#include <QDialog>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>

#include "activitylog.h"

class QComboBox;
class QLabel;
class QPushButton;
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

   //! gives the entries of a record kept elsewhere, oldest first, or says
   //! why they cannot be had
   typedef std::function<void (const QList<Activity> &acts,
                               const QString &error)> Fetched;

   /** show the record of a repository on a server

      \param root   the top of the repository, as the folder tree names
                    it
      \param fetch  called to fetch the newest entries; it calls back
                    with them, which may be later. With none, the server
                    keeps no record, and the window says so */
   void setServer (const QString &root,
                   std::function<void (Fetched done)> fetch);

   //! true if the record shown is one kept on a server
   bool onServer () const { return _server; }

   //! what the window says in place of a list, when it has none to show
   QString explanation () const;

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

   //! a stack to put back
   typedef PutBack Move;

   /** what Put back would do for the line chosen: each stack which the
       line, or the lines under it, renamed, moved or deleted, and which is
       still there to put back. None if the line is not about such a
       thing, or has been undone */
   QList<Move> putBackMoves () const;

   //! show only what one person did: Activity::e_who, or -1 for everyone
   void setWho (int who);

   //! the most entries shown, counted from the newest
   static constexpr int max_entries = 5000;

   //! how far apart, in seconds, an entry for a deleted stack and the
   //! arrival of a stack in the trash may be for them to be the same thing
   static constexpr int trash_slack = 10;

public slots:
   //! read the record again, soon; several calls close together read once
   void refresh ();

   //! read the record again now
   void reload ();

   //! a server's record has grown: read it again if one is shown
   void serverGrown ();

signals:
   /** a line has been chosen whose stack is still there

      \param pathname  the stack's full path, as it is now
      \param page      the page the line is about, from 0; -1 if none */
   void stackChosen (const QString &pathname, int page);

   /** Put back has been pressed: each stack is to go from where it is to
       where it was before the line chosen was done, as one step

      \param moves  the stacks, in the order to do them */
   void putBackWanted (const QList<Activitydialog::Move> &moves);

   /** a line about a folder has been chosen, and the folder is still there

      \param pathname  the folder's full path, as it is now */
   void folderChosen (const QString &pathname);

private slots:
   //! show where the stack of the line chosen is now, and ask for it
   void lineChosen ();

private:
   //! the entry of the line chosen, as a place in _acts; -1 if none
   int chosenEntry () const;

   //! allow Put back when the line chosen has something to put back
   void updatePutBack ();

   //! the line for an entry
   QTreeWidgetItem *lineFor (int at, const QSet<qint64> &undone) const;

   /** find a deleted stack in the trash

      \param act  the entry which deleted it
      \returns the name it has there, empty if it is not there */
   QString keptName (const Activity &act) const;

   /** show entries: the tree, what has been undone, and why there is
       nothing if there is nothing

      \param error  why the record could not be had, empty if it was */
   void showEntries (const QString &error);

   QString _root;
   bool _server = false;      //!< the record is kept on a server
   std::function<void (Fetched done)> _fetch;   //!< fetches it from there
   int _asked = 0;            //!< counts fetches, to tell the latest reply
   QList<Activity> _acts;        //!< the entries shown, oldest first
   QTreeWidget *_tree;
   QLabel *_where;               //!< says where the stack chosen is now
   QLabel *_none;                //!< says why there is nothing to show
   QPushButton *_put_back;       //!< puts back what the line chosen did
   QComboBox *_show;             //!< chooses whose doings are shown
   QSet<qint64> _undone;         //!< the entries since undone
   int _who = -1;                //!< whose doings are shown; -1 for all
   QTimer *_timer;               //!< puts off a refresh a little
   };
