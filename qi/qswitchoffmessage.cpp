/***************************************************************************
                          qswitchoffmessage.cpp  -  description
                             -------------------
    begin                : Mon Mar 12 2001
    copyright            : (C) 2001 by Michael Herder
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

#include "qswitchoffmessage.h"
#include "qxmlconfig.h"

#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qstring.h>


QSwitchOffMessage::QSwitchOffMessage(Type t,QWidget* parent,
                                     const char* name)
                  :QDialog(parent,name,true)
{
  mType = t;
  initDlg();
}
QSwitchOffMessage::~QSwitchOffMessage()
{
}

void QSwitchOffMessage::initDlg()
{
  QPushButton* button1;
  QPushButton* button2;
  QPushButton* button3;

  button1 = 0;
  button2 = 0;
  button3 = 0;

  QGridLayout* mainlayout = new QGridLayout(this,3,1);
  mainlayout->setMargin(10);
  mainlayout->setSpacing(8);
  QLabel* label = new QLabel(this);
  QWidget* buttonwidget = new QWidget(this);
  QHBoxLayout* hbl = new QHBoxLayout(buttonwidget);
  setCaption(tr("Warning"));
  if(mType == Type_AdfWarning)
  {
    label->setText(
       tr("<html>You are scanning in ADF mode. This means, that scanning "
          "will continue until there are no more documents left in "
          "the automatic document feeder.<br><br>You can also use this "
          "mode if your scanner doesn't have an automatic document "
          "feeder. In this case, you should also select the "
          "<b>Confirm</b> checkbox. This gives you the possibilty to "
          "replace the document before QuiteInsane proceeds "
          "with the next scan.<br>Otherwise, the same document will be "
          "scanned repeatedly.<br><br>Do you want to enable the "
          "<b>Confirm</b> checkbox now?</html>"));
    button1 = new QPushButton(tr("C&onfirm"),buttonwidget);
    hbl->addWidget(button1);
    hbl->addStretch(1);
    button2 = new QPushButton(tr("&Don't confirm"),buttonwidget);
    hbl->addWidget(button2);
    hbl->addStretch(1);
    button3 = new QPushButton(tr("&Cancel"),buttonwidget);
    hbl->addWidget(button3);
  }
  else if(mType == Type_MultiWarning)
  {
    label->setText(
       tr("<html>You are going to scan multiple documents.<br>"
          "If your scanner doesn't have an automatic document feeder, "
          "you should also select the <b>Confirm</b> checkbox. This gives "
          "you the possibilty to replace the document before QuiteInsane  "
          "proceeds with the next scan.<br>Otherwise, the same document "
          "will be scanned repeatedly.<br><br>Do you want to enable the "
          "<b>Confirm</b> checkbox now?</html>"));
    button1 = new QPushButton(tr("C&onfirm"),buttonwidget);
    hbl->addWidget(button1);
    hbl->addStretch(1);
    button2 = new QPushButton(tr("&Don't confirm"),buttonwidget);
    hbl->addWidget(button2);
    hbl->addStretch(1);
    button3 = new QPushButton(tr("&Cancel"),buttonwidget);
    hbl->addWidget(button3);
  }
  else if(mType == Type_PrinterSetupWarning)
  {
    label->setText(
       tr("<html>You have to setup your printer, "
          "before you can print multiple documents. "
          "Please click on the <b>Setup printer...</b> button "
          "in the dialog that will appear next.</html>"));
    button1 = new QPushButton(tr("&OK"),buttonwidget);
    hbl->addStretch(1);
    hbl->addWidget(button1);
    hbl->addStretch(1);
  }
  if(button1 != 0)
    connect(button1,SIGNAL(clicked()),this,SLOT(accept()));
  if(button2 != 0)
    connect(button2,SIGNAL(clicked()),this,SLOT(slotButton2()));
  if(button3 != 0)
    connect(button3,SIGNAL(clicked()),this,SLOT(reject()));
  mpDontShowCheckBox = new QCheckBox(tr("Don't &show this message again."),this);
  connect(mpDontShowCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotDontShow(bool)));

  mainlayout->addWidget(label,0,0);
  mainlayout->addWidget(mpDontShowCheckBox,1,0);
  mainlayout->addWidget(buttonwidget,2,0);
  mainlayout->activate();
}
/**  */
void QSwitchOffMessage::slotButton2()
{
  done(3);
}
/**  */
bool QSwitchOffMessage::dontShow()
{
  return mpDontShowCheckBox->isChecked();
}

void QSwitchOffMessage::slotDontShow(bool b)
{
  QString qs;
  if(mType == Type_AdfWarning)
    xmlConfig->setBoolValue("WARNING_ADF",!b);
  if(mType == Type_MultiWarning)
    xmlConfig->setBoolValue("WARNING_MULTI",!b);
  if(mType == Type_PrinterSetupWarning)
    xmlConfig->setBoolValue("WARNING_PRINTERSETUP",!b);
}
