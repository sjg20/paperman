/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
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
//
// C++ Implementation: senddialogue
//
// Description:
//
//
// Author: Simon Glass <sglass@bluewatersys.com>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//


#include <QDebug>
#include <QPushButton>

#include "desktopdelegate.h"
#include "desktopmodel.h"
#include "senddialog.h"
#include "transfer.h"


Senddialog::Senddialog (QWidget *parent)
   : QDialog (parent)
   {
   setupUi (this);
   toList->setSelectionMode (QAbstractItemView::ExtendedSelection);

   connect (toList, SIGNAL (itemSelectionChanged ()), this, SLOT (itemSelectionChanged ()));
   itemSelectionChanged ();
   _transfer = 0;
   }


err_info *Senddialog::setup (Desktopmodel *contents, QModelIndex parent,
                QModelIndexList &slist)
   {
   QPersistentModelIndex pparent = parent;
   QList<QPersistentModelIndex> pslist;

   // get a list of the row numbers
   int parent_row = parent.row ();
   int row_count = contents->rowCount (parent);
   QList<int> rows;

   int size = 0;
   foreach (const QModelIndex ind, slist)
      {
      rows << ind.row ();
      size += contents->imageSize (ind);
      pslist << ind;
      }

   char sizestr [30];

   util_bytes_to_user (sizestr, size);

   itemCount->setText (tr ("%n item(s) to send, total %1", 0, slist.size ())
         .arg (sizestr));

   _model = new Desktopmodel (this);
   _model->cloneModel (contents, parent);

   _proxy = new Desktopproxy (this);
   _proxy->setSourceModel (_model);
   _proxy->setRows (parent_row, row_count, rows);

   // set up the model converter
   _modelconv = new Desktopmodelconv (_model, _proxy);

   // setup another one for Desktopmodel, which only allows assertions
   _modelconv_assert = new Desktopmodelconv (_model, _proxy, false);

   view->setFreeForm ();
   view->setModelConv (_modelconv);

   _model->setModelConv (_modelconv_assert);
   view->setModel (_proxy);

   _delegate = new Desktopdelegate (_modelconv, this);
   view->setItemDelegate (_delegate);

   QModelIndex proxy_parent = _model->index (parent_row, 0, QModelIndex ());
   _modelconv->indexToProxy (_model, proxy_parent);
   view->setRootIndex (proxy_parent);

   // now create the list of recipients
   _transfer = new Transfer (contents, parent, pslist);

   CALL (_transfer->init ());

   from->setText (_transfer->me ()->name ());

   QListWidgetItem *w = new QListWidgetItem ();
   w->setText ("<newuser@company.com>");
   w->setFlags (w->flags () | Qt::ItemIsEditable);
   toList->insertItem (0, w);

   for (int i = 0; i < _transfer->userCount (); i++)
      {
      User &user = _transfer->user (i);

      if (&user != _transfer->me ())
         {
         QListWidgetItem *w = new QListWidgetItem (user.name () + "  <" + user.email () + ">", toList);
         w->setData (Qt::UserRole, i);
         }
      }
   return NULL;
   }


err_info *Senddialog::doSend (void)
   {
   QStringList env;
   QString to;

   env << _transfer->me ()->username ();

   QList <QListWidgetItem *> sel = toList->selectedItems ();
   if (!sel.size ())
      return NULL;

   foreach (QListWidgetItem *w, sel)
      {
      int usernum = w->data (Qt::UserRole).toInt ();
      to += _transfer->user (usernum).username () + ",";
      }
   Q_ASSERT (!to.isEmpty ());
   to.truncate (to.length () - 1);  // remove final ,
   env << to;
   env << subject->text ();
   env << notes->toPlainText();

   return _transfer->send (env);
   }


void Senddialog::itemSelectionChanged (void)
   {
   buttonBox->button (QDialogButtonBox::Ok)->setEnabled (toList->selectedItems ().size ());
   }

