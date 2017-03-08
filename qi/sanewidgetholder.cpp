/***************************************************************************
                          sanewidgetholder.cpp  -  description
                             -------------------
    begin                : Wed Feb 26 2003
    copyright            : (C) 2003 by Michael Herder
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

#include "sanewidgetholder.h"
#include "qsaneoption.h"

#include <qlayout.h>
#include <QGridLayout>

SaneWidgetHolder::SaneWidgetHolder(QWidget* parent,const char* name,Qt::WindowFlags f)
                 :QWidget(parent,f)
{
  setObjectName(name);
  mpSaneOption = 0L;
  mPending = false;
  mpGridLayout = new QGridLayout(this);
}
SaneWidgetHolder::~SaneWidgetHolder()
{
}
/** No descriptions */
void SaneWidgetHolder::addWidget(QSaneOption* widget)
{
  if(!widget)
    return;
  if(mpSaneOption)
    return;
  mpGridLayout->addWidget(widget,0,0);
  mpSaneOption = widget;
}
/** No descriptions */
void SaneWidgetHolder::replaceWidget(QSaneOption* widget)
{
  if(!widget)
    return;
  if(mpSaneOption != 0)
    delete mpSaneOption;
  mpGridLayout->addWidget(widget,0,0);
  mpSaneOption = widget;
}
/** No descriptions */
QSaneOption* SaneWidgetHolder::saneOption()
{
  return mpSaneOption;
}

bool SaneWidgetHolder::getPending (void)
{
  return mPending;
}


void SaneWidgetHolder::setPending (bool pending)
{
  mPending = pending;
}
