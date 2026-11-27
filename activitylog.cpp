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

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QSettings>

#include "activitylog.h"
#include "err.h"

const char *const ActivityLog::file_name = ".paperlog";

static bool log_enabled = true;

// the word each action is kept as, which must never change once used
static const char *const action_name [Activity::A_count] = {
   "", "group", "scan", "scan-page", "import", "rename", "move", "trash",
   "duplicate", "stack", "unstack", "split", "remove-pages", "reorder",
   "turn", "crop", "cover", "rename-page", "facts", "details", "read",
   "unfold", "folder-new", "folder-move", "other", "undo", "redo",
};

static const char *const who_name [Activity::W_count] = {
   "user", "assistant", "reader",
};


QString Activity::actionName (e_action action)
   {
   return action >= 0 && action < A_count ? action_name [action] : "";
   }


QJsonObject Activity::toJson () const
   {
   QJsonObject obj;

   // the keys are short, as there is one of each on every line
   obj ["id"] = id;
   obj ["t"] = when.toUTC ().toString (Qt::ISODateWithMs);
   obj ["who"] = who_name [who];
   obj ["do"] = actionName (action);
   if (group)
      obj ["in"] = group;
   if (ref)
      obj ["ref"] = ref;
   if (!path.isEmpty ())
      obj ["path"] = path;
   if (!from.isEmpty ())
      obj ["from"] = from;
   if (page >= 0)
      obj ["page"] = page;
   if (count)
      obj ["n"] = count;
   if (!note.isEmpty ())
      obj ["note"] = note;
   return obj;
   }


bool Activity::fromJson (const QJsonObject &obj, Activity &act)
   {
   QString name = obj.value ("do").toString ();
   QString by = obj.value ("who").toString ();

   act = Activity ();
   act.id = qint64 (obj.value ("id").toDouble ());
   for (int i = A_none + 1; i < A_count; i++)
      if (name == action_name [i])
         act.action = e_action (i);
   if (!act.id || act.action == A_none)
      return false;

   // someone not known of is taken as the user, as the entry is still true
   for (int i = 0; i < W_count; i++)
      if (by == who_name [i])
         act.who = e_who (i);
   act.when = QDateTime::fromString (obj.value ("t").toString (),
                                     Qt::ISODateWithMs);
   act.group = qint64 (obj.value ("in").toDouble ());
   act.ref = qint64 (obj.value ("ref").toDouble ());
   act.path = obj.value ("path").toString ();
   act.from = obj.value ("from").toString ();
   act.page = obj.value ("page").toInt (-1);
   act.count = obj.value ("n").toInt ();
   act.note = obj.value ("note").toString ();
   return true;
   }


bool Activity::operator== (const Activity &other) const
   {
   return id == other.id && group == other.group && ref == other.ref
      && when == other.when && who == other.who && action == other.action
      && path == other.path && from == other.from && page == other.page
      && count == other.count && note == other.note;
   }


QString Activity::displayName (const QString &path)
   {
   QString name = path.section ('/', -1);

   // Paperman's own stacks are shown by name alone, as on the desk
   if (name.endsWith (".max", Qt::CaseInsensitive))
      name.chop (4);
   return name;
   }


// a stack's name as the user knows it
static QString leaf (const QString &path)
   {
   return Activity::displayName (path);
   }


// the folder a stack is in, as the user knows it: "the top folder" if none
static QString folder (const QString &path)
   {
   QString dir = path.contains ('/') ? path.section ('/', 0, -2) : QString ();

   return dir.isEmpty () ? QCoreApplication::translate ("Activity",
                                                        "the top folder") : dir;
   }


QString Activity::describe () const
   {
   auto tr = [] (const char *text) {
      return QCoreApplication::translate ("Activity", text);
   };
   QString name = leaf (path);
   QString pagestr = QString::number (page + 1);

   switch (action)
      {
      case A_group :
         return note.isEmpty () ? tr ("Several things") : note;
      case A_scan :
         return count == 1 ? tr ("Scanned '%1' (1 page)").arg (name)
            : tr ("Scanned '%1' (%2 pages)").arg (name).arg (count);
      case A_scan_page :
         return note.isEmpty () ? tr ("Page %1").arg (pagestr)
            : tr ("Page %1: %2").arg (pagestr).arg (note);
      case A_import :
         return tr ("Brought in '%1'").arg (name);
      case A_rename :
         return tr ("Renamed '%1' to '%2'").arg (leaf (from)).arg (name);
      case A_move :
         return tr ("Moved '%1' from %2 to %3").arg (name)
            .arg (folder (from)).arg (folder (path));
      case A_trash :
         return tr ("Deleted '%1'").arg (name);
      case A_duplicate :
         return tr ("Copied '%1' as '%2'").arg (leaf (from)).arg (name);
      case A_stack :
         return tr ("Stacked '%1' onto '%2'").arg (leaf (from)).arg (name);
      case A_unstack :
         return tr ("Took '%1' out of '%2'").arg (name).arg (leaf (from));
      case A_split :
         return count ? tr ("Split '%1' into %2 stacks").arg (name).arg (count)
            : tr ("Split '%1'").arg (name);
      case A_remove_pages :
         return count == 1 ? tr ("Removed a page from '%1'").arg (name)
            : tr ("Removed %1 pages from '%2'").arg (count).arg (name);
      case A_reorder :
         return tr ("Put the pages of '%1' in another order").arg (name);
      case A_turn :
         return page < 0 ? tr ("Turned the pages of '%1'").arg (name)
            : tr ("Turned page %1 of '%2'").arg (pagestr).arg (name);
      case A_crop :
         return page < 0 ? tr ("Cropped the pages of '%1'").arg (name)
            : tr ("Cropped page %1 of '%2'").arg (pagestr).arg (name);
      case A_cover :
         return tr ("Changed the cover of '%1'").arg (name);
      case A_rename_page :
         return tr ("Named page %1 of '%2' '%3'").arg (pagestr).arg (name)
            .arg (note);
      case A_facts :
         return tr ("Changed the facts about '%1'").arg (name);
      case A_details :
         return tr ("Changed the details of '%1'").arg (name);
      case A_read :
         if (page >= 0)
            return tr ("Read the text of page %1 of '%2'").arg (pagestr)
               .arg (name);
         return count == 1 ? tr ("Read the text of a page of '%1'").arg (name)
            : tr ("Read the text of %1 pages of '%2'").arg (count).arg (name);
      case A_unfold :
         return tr ("Unfolded '%1'").arg (name);
      case A_folder_new :
         return tr ("Made the folder %1").arg (path);
      case A_folder_move :
         // in the same place it has a new name; elsewhere, it has moved
         if (folder (from) == folder (path))
            return tr ("Renamed the folder %1 to '%2'").arg (from)
               .arg (path.section ('/', -1));
         return tr ("Moved the folder %1 to %2").arg (from).arg (path);
      case A_other :
         return name.isEmpty () ? note : QString ("%1: '%2'").arg (note, name);
      case A_undo :
         return tr ("Undid: %1").arg (note);
      case A_redo :
         return tr ("Did again: %1").arg (note);
      default :
         break;
      }
   return QString ();
   }


ActivityLog::ActivityLog (const QString &root)
   {
   _pathname = QDir (root).absoluteFilePath (file_name);
   }


void ActivityLog::setEnabled (bool enable)
   {
   log_enabled = enable;
   }


bool ActivityLog::enabled ()
   {
   return log_enabled;
   }


//! the setting which says whether the user wants a record kept
#define KEPT_KEY "activity/record"

bool ActivityLog::kept ()
   {
   return QSettings ().value (KEPT_KEY, true).toBool ();
   }


void ActivityLog::setKept (bool keep)
   {
   QSettings ().setValue (KEPT_KEY, keep);
   }


err_info *ActivityLog::add (Activity &act)
   {
   /* numbers go up with the time, so that entries sort as they happened
      even when two programs add to the log: the time to a millisecond,
      then a count for those added within one */
   static qint64 last;
   QDateTime now = QDateTime::currentDateTimeUtc ();
   qint64 id = now.toMSecsSinceEpoch () * 1000;

   if (id <= last)
      id = last + 1;
   last = id;
   act.id = id;
   if (!act.when.isValid ())
      act.when = now;
   if (!log_enabled || !kept ())
      return NULL;

   QFile file (_pathname);

   if (!file.open (QIODevice::WriteOnly | QIODevice::Append))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (_pathname));

   QByteArray line = QJsonDocument (act.toJson ()).toJson (
                        QJsonDocument::Compact) + "\n";

   if (file.write (line) != line.size ())
      return err_make (ERRFN, ERR_failed_to_write_bytes1, int (line.size ()));
   return NULL;
   }


err_info *ActivityLog::read (QList<Activity> &acts, int max) const
   {
   QFile file (_pathname);

   acts.clear ();
   if (!file.exists ())
      return NULL;
   if (!file.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (_pathname));
   while (!file.atEnd ())
      {
      QJsonDocument doc = QJsonDocument::fromJson (file.readLine ());
      Activity act;

      // a line cut short, or written by something newer, is passed over
      if (doc.isObject () && Activity::fromJson (doc.object (), act))
         acts << act;
      }
   if (max > 0 && acts.size () > max)
      acts = acts.mid (acts.size () - max);
   return NULL;
   }


QString ActivityLog::whereNow (const QList<Activity> &acts, int at,
                               bool &gone)
   {
   gone = false;
   if (at < 0 || at >= acts.size ())
      return QString ();

   /* an undo puts back what the entry it names did, and a redo does it
      again, so each is followed by working that entry backwards or
      forwards */
   QHash<qint64, int> by_id;

   for (int i = 0; i < acts.size (); i++)
      by_id [acts [i].id] = i;

   QString path = acts [at].path;

   // what one entry does to where the stack is, forwards or backwards
   auto apply = [&] (const Activity &act, bool forwards) {
      switch (act.action)
         {
         case Activity::A_rename :
         case Activity::A_move :
            if (forwards && act.from == path)
               path = act.path;
            else if (!forwards && act.path == path)
               path = act.from;
            break;
         case Activity::A_folder_move :
            {
            /* everything in the folder goes with it, and the folder
               itself if that is what is being followed */
            QString was = forwards ? act.from : act.path;
            QString now = forwards ? act.path : act.from;

            if (path == was)
               path = now;
            else if (path.startsWith (was + "/"))
               path = now + path.mid (was.size ());
            break;
            }
         case Activity::A_trash :
            if (act.path == path)
               gone = forwards;
            break;
         case Activity::A_stack :
            // the stack put onto another is no more; the other stays
            if (act.from == path)
               gone = forwards;
            break;
         default :
            break;
         }
   };

   if (acts [at].action == Activity::A_trash)
      gone = true;

   for (int i = at + 1; i < acts.size (); i++)
      {
      const Activity &act = acts [i];

      if (act.action == Activity::A_undo || act.action == Activity::A_redo)
         {
         bool forwards = act.action == Activity::A_redo;
         int target = by_id.value (act.ref, -1);

         if (target < 0)
            continue;

         /* a group is undone as a whole: each of its entries, the last
            first */
         QList<int> which;

         if (acts [target].action == Activity::A_group)
            {
            for (int j = target + 1; j < acts.size (); j++)
               if (acts [j].group == acts [target].id)
                  which << j;
            }
         else
            which << target;
         if (!forwards)
            std::reverse (which.begin (), which.end ());
         for (int j : which)
            apply (acts [j], forwards);
         }
      else
         apply (act, true);
      }
   return path;
   }
