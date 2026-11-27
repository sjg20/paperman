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

#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QSet>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "activitydialog.h"
#include "err.h"

enum
   {
   COL_what,
   COL_who,
   COL_when,

   COL_count
   };

// where an entry is in _acts, kept with its line
static const int AtRole = Qt::UserRole;

// true if the entry of a line has since been undone
static const int UndoneRole = Qt::UserRole + 1;


Activitydialog::Activitydialog (QWidget *parent)
      : QDialog (parent)
   {
   setWindowTitle (tr ("Activity"));

   QVBoxLayout *layout = new QVBoxLayout (this);
   QLabel *label = new QLabel (tr ("What has been done to the papers here, "
      "the newest first. Choose a line to see its stack, where it is now."),
      this);

   label->setWordWrap (true);
   layout->addWidget (label);

   _none = new QLabel (this);
   _none->setWordWrap (true);
   layout->addWidget (_none);

   _tree = new QTreeWidget (this);
   _tree->setObjectName ("activity");
   _tree->setColumnCount (COL_count);
   _tree->setHeaderLabels ({tr ("What"), tr ("Who"), tr ("When")});
   _tree->setRootIsDecorated (true);
   _tree->setUniformRowHeights (true);
   _tree->setSelectionMode (QAbstractItemView::SingleSelection);
   _tree->header ()->setSectionResizeMode (COL_what, QHeaderView::Stretch);
   _tree->header ()->setSectionResizeMode (COL_who,
                                         QHeaderView::ResizeToContents);
   _tree->header ()->setSectionResizeMode (COL_when,
                                         QHeaderView::ResizeToContents);
   _tree->header ()->setStretchLastSection (false);
   layout->addWidget (_tree, 1);

   _where = new QLabel (this);
   _where->setObjectName ("where");
   _where->setWordWrap (true);
   layout->addWidget (_where);

   _timer = new QTimer (this);
   _timer->setSingleShot (true);
   _timer->setInterval (150);
   connect (_timer, &QTimer::timeout, this, &Activitydialog::reload);
   connect (_tree, &QTreeWidget::currentItemChanged, this,
            [this] () { lineChosen (); });
   resize (640, 520);
   reload ();
   }


void Activitydialog::setRoot (const QString &root)
   {
   QString top = root.isEmpty () ? QString () : QDir (root).absolutePath ();

   if (top == _root)
      return;
   _root = top;
   reload ();
   }


void Activitydialog::refresh ()
   {
   if (isVisible ())
      _timer->start ();
   else
      reload ();
   }


// who did something, as the list says it
static QString whoText (Activity::e_who who)
   {
   switch (who)
      {
      case Activity::W_assistant :
         return Activitydialog::tr ("Assistant");
      case Activity::W_reader :
         return Activitydialog::tr ("Paperman");
      default :
         break;
      }
   return Activitydialog::tr ("You");
   }


QTreeWidgetItem *Activitydialog::lineFor (int at,
                                         const QSet<qint64> &undone) const
   {
   const Activity &act = _acts [at];
   QTreeWidgetItem *line = new QTreeWidgetItem;
   bool gone = undone.contains (act.id) || undone.contains (act.group);
   QString what = act.describe ();

   if (gone)
      what = tr ("%1 (undone)").arg (what);
   line->setText (COL_what, what);
   line->setText (COL_who, whoText (act.who));
   line->setText (COL_when, QLocale ().toString (act.when.toLocalTime (),
                                                 QLocale::ShortFormat));
   line->setToolTip (COL_when, QLocale ().toString (act.when.toLocalTime (),
                                                    QLocale::LongFormat));
   line->setData (COL_what, AtRole, at);
   line->setData (COL_what, UndoneRole, gone);
   if (gone)
      for (int col = 0; col < COL_count; col++)
         line->setForeground (col, palette ().brush (QPalette::Disabled,
                                                     QPalette::Text));
   return line;
   }


void Activitydialog::reload ()
   {
   // keep the line chosen, by its entry, if it is still there
   QTreeWidgetItem *current = _tree->currentItem ();
   qint64 chosen = current ? _acts.value (current->data (COL_what,
                                             AtRole).toInt ()).id : 0;
   err_info *err = nullptr;

   _timer->stop ();
   _acts.clear ();
   if (!_root.isEmpty ())
      err = ActivityLog (_root).read (_acts, max_entries);

   /* what has been undone, and not done again since: the last word on
      each entry is what counts */
   QSet<qint64> undone;

   for (const Activity &act : _acts)
      {
      if (act.action == Activity::A_undo)
         undone << act.ref;
      else if (act.action == Activity::A_redo)
         undone.remove (act.ref);
      }

   QHash<qint64, QTreeWidgetItem *> lines;
   QTreeWidgetItem *again = nullptr;

   _tree->blockSignals (true);
   _tree->clear ();
   for (int at = 0; at < _acts.size (); at++)
      {
      const Activity &act = _acts [at];
      QTreeWidgetItem *line = lineFor (at, undone);

      // under its group, in order; or at the top, the newest first
      QTreeWidgetItem *over = act.group ? lines.value (act.group) : nullptr;

      if (over)
         over->addChild (line);
      else
         _tree->insertTopLevelItem (0, line);
      lines [act.id] = line;
      if (act.id == chosen)
         again = line;
      }
   if (again)
      _tree->setCurrentItem (again);
   _tree->blockSignals (false);

   QString why;

   if (_root.isEmpty ())
      why = tr ("The folder shown is not in a repository on this computer, "
                "so there is no record of activity to show.");
   else if (err)
      why = tr ("The record of activity cannot be read: %1")
               .arg (err->errstr);
   else if (_acts.isEmpty ())
      why = tr ("Nothing has been recorded here yet. What is done to the "
                "stacks from now on is listed here.");
   _none->setText (why);
   _none->setVisible (!why.isEmpty ());
   if (!again)
      _where->clear ();
   }


void Activitydialog::lineChosen ()
   {
   QTreeWidgetItem *line = _tree->currentItem ();

   _where->clear ();
   if (!line)
      return;

   int at = line->data (COL_what, AtRole).toInt ();

   if (at < 0 || at >= _acts.size ())
      return;

   /* an undo or a redo is about the entry it names, and a group about
      the first stack under it */
   QHash<qint64, int> by_id;

   for (int i = 0; i < _acts.size (); i++)
      by_id [_acts [i].id] = i;
   if (_acts [at].action == Activity::A_undo
       || _acts [at].action == Activity::A_redo)
      at = by_id.value (_acts [at].ref, -1);
   if (at >= 0 && _acts [at].action == Activity::A_group)
      {
      int first = -1;

      for (int i = at + 1; i < _acts.size () && first < 0; i++)
         if (_acts [i].group == _acts [at].id)
            first = i;
      at = first;
      }
   if (at < 0 || _acts [at].path.isEmpty ())
      return;

   bool gone;
   QString path = ActivityLog::whereNow (_acts, at, gone);
   QString pathname = QDir (_root).absoluteFilePath (path);
   QString name = Activity::displayName (path);
   QString dir = QFileInfo (path).path ();

   if (dir == "." || dir.isEmpty ())
      dir = tr ("the top folder");

   // a line about a folder leads to the folder
   if (_acts [at].isFolder ())
      {
      if (!QFileInfo (pathname).isDir ())
         _where->setText (tr ("The folder %1 is no longer there.").arg (path));
      else
         {
         _where->setText (tr ("The folder is now %1.").arg (path));
         emit folderChosen (pathname);
         }
      return;
      }
   if (gone)
      _where->setText (tr ("'%1' has since been deleted, or put into "
                           "another stack.").arg (name));
   else if (!QFileInfo (pathname).isFile ())
      _where->setText (tr ("'%1' is no longer in %2: it may have been "
                           "changed outside Paperman.").arg (name, dir));
   else
      {
      _where->setText (tr ("'%1' is now in %2.").arg (name, dir));
      emit stackChosen (pathname, _acts [at].page);
      }
   }


QStringList Activitydialog::listed () const
   {
   QStringList list;

   for (int i = 0; i < _tree->topLevelItemCount (); i++)
      list << _tree->topLevelItem (i)->text (COL_what);
   return list;
   }


QStringList Activitydialog::listedUnder (int row) const
   {
   QTreeWidgetItem *line = _tree->topLevelItem (row);
   QStringList list;

   for (int i = 0; line && i < line->childCount (); i++)
      list << line->child (i)->text (COL_what);
   return list;
   }


bool Activitydialog::isUndone (int row) const
   {
   QTreeWidgetItem *line = _tree->topLevelItem (row);

   return line && line->data (COL_what, UndoneRole).toBool ();
   }


void Activitydialog::choose (int row, int child)
   {
   QTreeWidgetItem *line = _tree->topLevelItem (row);

   if (line && child >= 0)
      line = line->child (child);
   if (!line)
      return;

   // choosing the line already chosen asks again, as a click does
   if (line == _tree->currentItem ())
      lineChosen ();
   else
      _tree->setCurrentItem (line);
   }


QString Activitydialog::status () const
   {
   return _where->text ();
   }
