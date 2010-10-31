/***************************************************************************
                          unknownprogressdialog.cpp  -  description
                             -------------------
    begin                : Sat Mar 16 2002
    copyright            : (C) 2002 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "unknownprogressdialog.h"
#include "qunknownprogresswidget.h"

#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>

UnknownProgressDialog::UnknownProgressDialog(QString label,QWidget* parent,const char* name)
                      :QDialog(parent,name,true)
{
  mLabelText = label;
  initDialog();
}
UnknownProgressDialog::~UnknownProgressDialog()
{
}
/** No descriptions */
void UnknownProgressDialog::initDialog()
{
  QGridLayout* mainlayout = new QGridLayout(this,3,3);
  mainlayout->setRowStretch(1,1);
  mainlayout->setColStretch(0,1);
  mainlayout->setColStretch(2,1);
  mpLabel = new QLabel(mLabelText,this);
  mpUnknownProgress = new QUnknownProgressWidget(this);
  mpUnknownProgress->setFixedHeight(20);
  QPushButton* stop_button = new QPushButton(tr("&Cancel"),this);
  mainlayout->addMultiCellWidget(mpLabel,0,0,0,2);
  mainlayout->addMultiCellWidget(mpUnknownProgress,1,1,0,2);
  mainlayout->addWidget(stop_button,2,1);
}
