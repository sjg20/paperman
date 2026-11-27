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

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

struct err_info;

/** one thing which was done to the papers in a repository

Each has when it was done, who did it and what it was, with the stack it
was done to. A stack is given by its path below the top of the repository,
as it is once the thing is done, so that the entry still says where the
stack is after a rename or a move. Things done together, such as the pages
of a scan or the stacks an operation worked on, are entries of their own
under one entry for the whole, which they name as their group */
struct Activity
   {
   //! what was done. Each is kept in the log as the word given here
   enum e_action
      {
      A_none,
      A_group,       //!< several things done together, in the entries under it
      A_scan,        //!< a stack was scanned
      A_scan_page,   //!< a page of a scan
      A_import,      //!< files were brought in as stacks
      A_rename,      //!< a stack was given a new name
      A_move,        //!< a stack was moved to another folder
      A_trash,       //!< a stack was moved to the trash; its path is where
                     //!< it was, and 'kept' its name in the trash
      A_duplicate,   //!< a copy of a stack was made
      A_stack,       //!< stacks were put together into one
      A_unstack,     //!< pages were taken out into stacks of their own
      A_split,       //!< a stack was divided into several
      A_remove_pages,   //!< pages were removed from a stack
      A_reorder,     //!< a stack's pages were put in another order
      A_turn,        //!< a page was turned or flipped
      A_crop,        //!< a page was cropped
      A_cover,       //!< a stack's cover was set or cleared
      A_rename_page, //!< a page was given a new name
      A_facts,       //!< the facts kept about a document were changed
      A_details,     //!< a stack's title, author, keywords or notes changed
      A_read,        //!< the text of pages was read (OCR)
      A_unfold,      //!< a booklet was unfolded
      A_folder_new,  //!< a folder was made; its path is the folder
      A_folder_move, //!< a folder was renamed, or moved into another; its
                     //!< path and where it was before are folders
      A_other,       //!< something else, said in the note
      A_restore,     //!< a deleted stack was put back from the trash; its
                     //!< path is where it now is, and its ref the entry
                     //!< which deleted it
      A_undo,        //!< an earlier entry was undone
      A_redo,        //!< an entry which was undone was done again

      A_count
      };

   //! who did it
   enum e_who
      {
      W_user,        //!< the person using Paperman
      W_assistant,   //!< the AI assistant, with what it suggested accepted
      W_reader,      //!< Paperman itself, reading text in the background

      W_count
      };

   qint64 id = 0;       //!< its number in the log, set when it is added
   qint64 group = 0;    //!< the entry it belongs under, 0 if none
   qint64 ref = 0;      //!< the entry which an undo or redo is about
   QDateTime when;      //!< when it was done; set when added, if not given
   e_who who = W_user;
   e_action action = A_none;

   //! the stack, or for a group perhaps the folder: its path below the
   //! top of the repository, with no slash first, as it is afterwards
   QString path;

   //! where the stack was before, for a rename or a move; or what it was
   //! made from, for a copy or a stack taken out of another
   QString from;

   int page = -1;       //!< the page it was done to, from 0; -1 if not one
   int count = 0;       //!< how many pages or stacks it covers, 0 if not said
   QString note;        //!< anything more, in words: a new page name, say

   //! for a stack moved to the trash, the name it has there, by which it
   //! can be brought back
   QString kept;

   //! the entry in the form it is kept in: one JSON object
   QJsonObject toJson () const;

   /** read an entry from the form it is kept in

      \param obj  the object
      \param act  returns the entry
      \returns true if it is one, false if it has no number or its action
               is not known */
   static bool fromJson (const QJsonObject &obj, Activity &act);

   //! the word an action is kept as
   static QString actionName (e_action action);

   //! true if this is about a folder, and its path is one, not a stack
   bool isFolder () const
      { return action == A_folder_new || action == A_folder_move; }

   //! what was done, in a line for the user: "Renamed 'a' to 'b'"
   QString describe () const;

   //! a stack's name as the desk shows it, from its path: no folder, and
   //! no extension for one of Paperman's own stacks
   static QString displayName (const QString &path);

   bool operator== (const Activity &other) const;
   };


/** a stack to put back: where it is now, and where it goes, both as full
paths. One which was deleted is in its repository's trash, and has the
entry which deleted it */
struct PutBack
   {
   PutBack () {}
   PutBack (const QString &now, const QString &was, qint64 deleted = 0)
      : now (now), was (was), deleted (deleted) {}
   bool operator== (const PutBack &other) const
      {
      return now == other.now && was == other.was
         && deleted == other.deleted;
      }

   QString now;         //!< where the stack is
   QString was;         //!< where it is to go
   qint64 deleted = 0;  //!< the entry which deleted it; 0 if it was not
   };


/** the record of what has been done to the papers in a repository

It is a file at the top of the repository, so that it goes where the papers
go and is the same from whichever computer they are used. Each entry is
one line of JSON, added at the end; nothing is ever changed, so two programs
can add to it, and a line which is damaged or not understood is passed over.

It records what Paperman does, and is not what makes anything happen: it
can be deleted, and all that is lost is the history */
class ActivityLog
   {
public:
   //! the name of the file, at the top of the repository
   static const char *const file_name;

   /** \param root  the top of the repository, with or without a slash */
   explicit ActivityLog (const QString &root);

   //! the full path of the file
   QString pathname () const { return _pathname; }

   /** add an entry at the end

      \param act  the entry; its number is set, and when it was done if
                  that is not given
      \returns error, or NULL if ok */
   err_info *add (Activity &act);

   /** read the entries, oldest first

      \param acts  returns the entries
      \param max   the most to return, counted from the newest; 0 for all
      \returns error, or NULL if ok. A log which is not there has none */
   err_info *read (QList<Activity> &acts, int max = 0) const;

   /** find where a stack is now, by following what the entries after one
       say happened to it

      \param acts  the entries, oldest first
      \param at    which entry the stack is the subject of
      \param gone  returns true if it has since gone: to the trash, or
                   into another stack
      \returns its path below the top of the repository, as the last entry
               about it leaves it. For an entry about a folder this is the
               folder, which is followed in the same way */
   static QString whereNow (const QList<Activity> &acts, int at, bool &gone);

   /** the folder, at the top of a repository, which holds its deleted
       stacks */
   static QString trashName () { return QStringLiteral (".maxview-trash"); }

   /** find where a folder is now, by following the moves of it, and of
       the folders above it, which come after an entry

      \param acts  the entries, oldest first
      \param at    the entry to start after
      \param dir   the folder's path below the top of the repository, as
                   it was at that entry; empty for the top itself
      \returns its path now */
   static QString folderNow (const QList<Activity> &acts, int at,
                             const QString &dir);

   /** say whether entries are recorded at all, which they are unless this
       is turned off, as tests which count the files in a folder do */
   static void setEnabled (bool enable);
   static bool enabled ();

   /** whether the user wants a record kept, from the settings: it is kept
       unless they have said not. With it off nothing more is written; a
       record which is there already is left, and can still be read */
   static bool kept ();
   static void setKept (bool keep);

private:
   QString _pathname;
   };
