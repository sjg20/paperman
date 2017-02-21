/***************************************************************************
                          qstringoption.cpp  -  description
                             -------------------
    begin                : Fri Sep 15 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#include "qstringoption.h"

#include <qstring.h>
#include <qlineedit.h>
#include <qpixmap.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <QGridLayout>
#include <sane/saneopts.h>

QStringOption::QStringOption(QString title,QWidget *parent, const char *name )
              :QSaneOption(title,parent,name)
{
  initWidget();
}

QStringOption::~QStringOption()
{
}
/**  */
void QStringOption::initWidget()
{
  QGridLayout* qgl = new QGridLayout(this);
  QLabel* label = new QLabel(optionTitle(),this);
	mpOptionLineEdit = new QLineEdit(this);
  connect(mpOptionLineEdit,SIGNAL(textChanged(const QString&)),
          this,SLOT(slotTextChanged(const QString&)));
//create pixmap
  assignPixmap();
    qgl->addWidget(pixmapWidget(),0,0,2,0);
	qgl->addWidget(label,0,1);
	qgl->addWidget(mpOptionLineEdit,1,1);
  qgl->setSpacing(5);
    qgl->setColumnStretch(1,1);
	qgl->activate();
}
/**  */
void QStringOption::setText(const char* text)
{
  QString qs(text);
  mpOptionLineEdit->setText(qs);
}
/**  */
QString QStringOption::text()
{
  return mpOptionLineEdit->text();
}
/**  */
void QStringOption::slotTextChanged(const QString& qs)
{
  mCurrentText = qs;
  slotEmitOptionChanged();
}
/** No descriptions */
void QStringOption::setMaxLength(int length)
{
  mpOptionLineEdit->setMaxLength(length);
}
