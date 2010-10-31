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
// C++ Interface: senddialog
//
// Description:
//
//
// Author: Simon Glass <sglass@bluewatersys.com>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//


#include <QModelIndex>


#include "ui_send.h"


class Desktopdelegate;
class Desktopmodel;
class Desktopproxy;
class Transfer;
struct err_info;


class Senddialog : public QDialog, Ui::send
   {
   Q_OBJECT

public:
   Senddialog (QWidget *parent = 0);

   err_info *setup (Desktopmodel *contents, QModelIndex parent, QModelIndexList &slist);

   err_info *doSend (void);

private slots:
   void itemSelectionChanged (void);

private:
   Desktopmodel *_model;      //!< the model for our list of items to send
   Desktopmodelconv *_modelconv;
   Desktopmodelconv *_modelconv_assert;
   Desktopdelegate *_delegate;
   Desktopproxy *_proxy;
   Transfer *_transfer;
   };
