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
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "backgrounddialog.h"
#include "backgroundwork.h"
#include "ocrreader.h"

//! the columns of the list
enum
   {
   COL_stack,
   COL_folder,
   COL_left,

   COL_count
   };


Backgrounddialog::Backgrounddialog (OcrReader *reader, QWidget *parent)
   : QDialog (parent), _reader (reader)
   {
   setObjectName ("backgroundWork");
   setWindowTitle (tr ("Background work"));

   QVBoxLayout *layout = new QVBoxLayout (this);

   // the other work going on, which comes and goes
   _jobs = new QTableWidget (0, 2, this);
   _jobs->setObjectName ("jobs");
   _jobs->setHorizontalHeaderLabels (QStringList () << tr ("Work")
                                                    << tr ("Progress"));
   _jobs->setSelectionBehavior (QAbstractItemView::SelectRows);
   _jobs->setSelectionMode (QAbstractItemView::SingleSelection);
   _jobs->setEditTriggers (QAbstractItemView::NoEditTriggers);
   _jobs->verticalHeader ()->hide ();
   _jobs->horizontalHeader ()->setSectionResizeMode (0, QHeaderView::Stretch);
   _jobs->horizontalHeader ()->setSectionResizeMode (1,
                                             QHeaderView::ResizeToContents);
   _jobs->setTextElideMode (Qt::ElideMiddle);
   _jobs->setMaximumHeight (120);

   QHBoxLayout *jobs_row = new QHBoxLayout;

   _stop_job = new QPushButton (tr ("S&top"), this);
   _stop_job->setObjectName ("stopJob");
   _stop_job->setToolTip (tr ("Stop the work selected, if it can be "
                              "stopped"));
   jobs_row->addWidget (_jobs, 1);
   jobs_row->addWidget (_stop_job, 0, Qt::AlignTop);
   layout->addLayout (jobs_row);
   connect (_stop_job, &QPushButton::clicked, this,
            &Backgrounddialog::stopJob);
   connect (_jobs, &QTableWidget::itemSelectionChanged, this,
            &Backgrounddialog::refreshJobs);

   /* work says how it is going very often, so show it a few times a
      second and no more */
   connect (Backgroundwork::instance (), &Backgroundwork::changed, this,
            [this] () {
      if (_jobs_due)
         return;
      _jobs_due = true;
      QTimer::singleShot (jobs_refresh_ms, this,
                          &Backgrounddialog::refreshJobs);
   });

   _summary = new QLabel (this);
   _summary->setObjectName ("summary");
   _summary->setWordWrap (true);
   layout->addWidget (_summary);

   _table = new QTableWidget (0, COL_count, this);
   _table->setObjectName ("stacks");
   _table->setHorizontalHeaderLabels (QStringList () << tr ("Stack")
      << tr ("Folder") << tr ("Pages left"));
   _table->setSelectionBehavior (QAbstractItemView::SelectRows);
   _table->setSelectionMode (QAbstractItemView::ExtendedSelection);
   _table->setEditTriggers (QAbstractItemView::NoEditTriggers);
   _table->verticalHeader ()->hide ();
   _table->horizontalHeader ()->setSectionResizeMode (COL_stack,
                                                    QHeaderView::Stretch);
   _table->horizontalHeader ()->setSectionResizeMode (COL_folder,
                                                    QHeaderView::Stretch);

   /* what is left is short and is what the window is for, so it always
      gets the room it needs; a long folder loses its middle instead */
   _table->horizontalHeader ()->setSectionResizeMode (COL_left,
                                             QHeaderView::ResizeToContents);
   _table->setTextElideMode (Qt::ElideMiddle);
   layout->addWidget (_table);

   QHBoxLayout *buttons = new QHBoxLayout;

   _pause = new QPushButton (this);
   _pause->setObjectName ("pause");
   _remove = new QPushButton (tr ("&Remove from list"), this);
   _remove->setObjectName ("remove");
   _remove->setToolTip (tr ("Do not read the stacks selected. The pages of "
      "them already read keep their text"));
   _stop = new QPushButton (tr ("&Stop reading"), this);
   _stop->setObjectName ("stop");
   _stop->setToolTip (tr ("Do not read any of these stacks. The pages "
      "already read keep their text, and Read text (OCR) reads the rest "
      "another time"));
   buttons->addWidget (_pause);
   buttons->addWidget (_remove);
   buttons->addWidget (_stop);
   buttons->addStretch ();

   QDialogButtonBox *box = new QDialogButtonBox (QDialogButtonBox::Close,
                                                 this);

   buttons->addWidget (box);
   layout->addLayout (buttons);

   connect (_pause, &QPushButton::clicked, this,
            &Backgrounddialog::togglePaused);
   connect (_remove, &QPushButton::clicked, this,
            &Backgrounddialog::removeSelected);
   connect (_stop, &QPushButton::clicked, this, &Backgrounddialog::stopAll);
   connect (box, &QDialogButtonBox::rejected, this, &QDialog::close);
   connect (_table, &QTableWidget::itemSelectionChanged, this, [this] () {
      _remove->setEnabled (!_table->selectedItems ().isEmpty ());
   });
   connect (_reader, &OcrReader::progress, this, &Backgrounddialog::refresh);
   resize (640, 420);
   refresh ();
   refreshJobs ();
   }


QString Backgrounddialog::leftText (const OcrReader *reader)
   {
   int stacks = reader->stacksLeft (), pages = reader->pagesLeft ();
   QString in = stacks == 1 ? tr ("1 stack")
                            : tr ("%1 stacks").arg (QLocale ().toString (stacks));

   // a stack not yet opened has pages too, but how many is not known
   if (!pages)
      return in;
   return tr ("%1 in %2")
      .arg (pages == 1 ? tr ("1 page")
                       : tr ("%1 pages").arg (QLocale ().toString (pages)))
      .arg (in);
   }


QStringList Backgrounddialog::listed () const
   {
   QStringList paths;

   for (int row = 0; row < _table->rowCount (); row++)
      paths << _table->item (row, COL_stack)->data (Qt::UserRole).toString ();
   return paths;
   }


QString Backgrounddialog::summary () const
   {
   return _summary->text ();
   }


void Backgrounddialog::selectRow (int row)
   {
   _table->selectRow (row);
   }


void Backgrounddialog::refresh ()
   {
   const QList<OcrReader::Waiting> waiting = _reader->waiting ();
   bool any = !waiting.isEmpty ();

   if (!any)
      _summary->setText (tr ("Paperman reads the text of scanned pages in the "
         "background. Nothing is waiting to be read."));
   else if (_reader->isPaused ())
      _summary->setText (tr ("Reading the text of scanned pages is paused: "
                             "%1 left.").arg (leftText (_reader)));
   else
      _summary->setText (tr ("Reading the text of scanned pages: %1 left.")
                            .arg (leftText (_reader)));

   // keep the stacks selected so, as the list changes under them
   QStringList selected;

   for (const QModelIndex &ind : _table->selectionModel ()->selectedRows ())
      selected << _table->item (ind.row (), COL_stack)->data (Qt::UserRole)
                     .toString ();

   QSignalBlocker blocker (_table);

   _table->clearSelection ();
   _table->setRowCount (waiting.size ());
   for (int row = 0; row < waiting.size (); row++)
      {
      const OcrReader::Waiting &w = waiting [row];
      QFileInfo fi (w.path);
      QString left = w.pages < 0 ? tr ("not known yet")
                                 : QLocale ().toString (w.left);

      if (w.reading)
         left = tr ("%1, reading now").arg (left);

      QTableWidgetItem *stack = new QTableWidgetItem (fi.completeBaseName ());

      stack->setData (Qt::UserRole, w.path);
      stack->setToolTip (w.path);
      _table->setItem (row, COL_stack, stack);
      QTableWidgetItem *folder = new QTableWidgetItem (fi.path ());

      folder->setToolTip (fi.path ());
      _table->setItem (row, COL_folder, folder);
      _table->setItem (row, COL_left, new QTableWidgetItem (left));
      if (selected.contains (w.path))
         for (int col = 0; col < COL_count; col++)
            _table->item (row, col)->setSelected (true);
      }

   _pause->setText (_reader->isPaused () ? tr ("&Carry on") : tr ("&Pause"));
   _pause->setEnabled (any || _reader->isPaused ());
   _remove->setEnabled (!_table->selectedItems ().isEmpty ());
   _stop->setEnabled (any);
   }


//! what a job says about how far it has got
static QString jobProgress (const Backgroundjob *job)
   {
   QString text = job->detail ();
   int percent = job->percent ();

   if (percent >= 0)
      text = text.isEmpty () ? Backgrounddialog::tr ("%1%").arg (percent)
         : Backgrounddialog::tr ("%1, %2%").arg (text).arg (percent);
   if (job->stopRequested ())
      text = text.isEmpty () ? Backgrounddialog::tr ("stopping")
         : Backgrounddialog::tr ("%1, stopping").arg (text);
   return text;
   }


QStringList Backgrounddialog::jobsListed () const
   {
   QStringList list;

   for (int row = 0; row < _jobs->rowCount (); row++)
      list << _jobs->item (row, 0)->text () + ": "
              + _jobs->item (row, 1)->text ();
   return list;
   }


void Backgrounddialog::selectJob (int row)
   {
   _jobs->selectRow (row);
   }


void Backgrounddialog::refreshJobs ()
   {
   const QList<Backgroundjob *> jobs = Backgroundwork::instance ()->jobs ();

   _jobs_due = false;

   // keep the job selected so, as others come and go around it
   quintptr selected = 0;
   const QModelIndexList rows = _jobs->selectionModel ()->selectedRows ();

   if (!rows.isEmpty ())
      selected = _jobs->item (rows [0].row (), 0)->data (Qt::UserRole)
                    .value<quintptr> ();

   QSignalBlocker blocker (_jobs);
   const Backgroundjob *chosen = nullptr;

   _jobs->clearSelection ();
   _jobs->setRowCount (jobs.size ());
   for (int row = 0; row < jobs.size (); row++)
      {
      const Backgroundjob *job = jobs [row];
      QTableWidgetItem *title = new QTableWidgetItem (job->title ());

      title->setData (Qt::UserRole, QVariant::fromValue (quintptr (job)));
      title->setToolTip (job->title ());
      _jobs->setItem (row, 0, title);
      _jobs->setItem (row, 1, new QTableWidgetItem (jobProgress (job)));
      if (quintptr (job) == selected)
         {
         chosen = job;
         _jobs->item (row, 0)->setSelected (true);
         _jobs->item (row, 1)->setSelected (true);
         }
      }

   // with only one job there is no doubt which is meant
   if (!chosen && jobs.size () == 1)
      chosen = jobs [0];
   _stop_job->setEnabled (chosen && chosen->stoppable ()
                          && !chosen->stopRequested ());
   }


void Backgrounddialog::stopJob ()
   {
   const QList<Backgroundjob *> jobs = Backgroundwork::instance ()->jobs ();
   const QModelIndexList rows = _jobs->selectionModel ()->selectedRows ();
   Backgroundjob *chosen = jobs.size () == 1 ? jobs [0] : nullptr;

   // the job may have finished since it was listed, so look for it
   if (!rows.isEmpty ())
      {
      quintptr selected = _jobs->item (rows [0].row (), 0)
                             ->data (Qt::UserRole).value<quintptr> ();

      for (Backgroundjob *job : jobs)
         if (quintptr (job) == selected)
            chosen = job;
      }
   if (chosen)
      chosen->requestStop ();
   refreshJobs ();
   }


void Backgrounddialog::togglePaused ()
   {
   _reader->setPaused (!_reader->isPaused ());
   refresh ();
   }


void Backgrounddialog::removeSelected ()
   {
   QStringList paths;

   for (const QModelIndex &ind : _table->selectionModel ()->selectedRows ())
      paths << _table->item (ind.row (), COL_stack)->data (Qt::UserRole)
                  .toString ();
   for (const QString &path : paths)
      _reader->remove (path);
   refresh ();
   }


void Backgrounddialog::stopAll ()
   {
   _reader->stop ();
   refresh ();
   }
