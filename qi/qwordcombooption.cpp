/***************************************************************************
                          qwordcombooption.cpp  -  description
                             -------------------
    begin                : Tue Nov 19 2000
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

#include "qwordcombooption.h"

#include <QGridLayout>
#include <QHBoxLayout>

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfontmetrics.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qsizepolicy.h>
#include <qstring.h>
#include <qwidget.h>

QWordComboOption::QWordComboOption(QString title,QWidget * parent,
                                   SANE_Value_Type type,const char * name)
                 :QSaneOption(title,parent,name)
{
  mAutomatic = false;
  mSaneValueType = type;
	initWidget();
}
QWordComboOption::~QWordComboOption()
{
}
/**  */
void QWordComboOption::initWidget()
{
    QGridLayout* qgl = new QGridLayout(this);
	mpTitleLabel = new QLabel(optionTitle(),this);
  QHBoxLayout* hbox1 = new QHBoxLayout(this);
    mpSelectionCombo = new QComboBox(this);
    hbox1->addWidget(mpSelectionCombo);
  mpSelectionCombo->setFocusPolicy(Qt::StrongFocus);//should get focus after clicking
  connect(mpSelectionCombo,SIGNAL(activated(int)),
          this,SLOT(slotValueChanged(int)));
  hbox1->setStretchFactor(mpSelectionCombo,1);
  mpAutoCheckBox = new QCheckBox(tr("Automatic"));
  hbox1->addWidget(mpAutoCheckBox);
  hbox1->setSpacing(6);
  mpAutoCheckBox->hide();
//create pixmap
  assignPixmap();
    qgl->addWidget(pixmapWidget(),0,0,2,0);

	qgl->addWidget(mpTitleLabel,0,1);
    qgl->addLayout(hbox1,1,1);
  qgl->setSpacing(5);
    qgl->setColumnStretch(0,0);
    qgl->setColumnStretch(1,1);
	qgl->activate();
  connect(mpAutoCheckBox,SIGNAL(toggled(bool)),this,SLOT(slotAutoMode(bool)));
}
/**  */
void QWordComboOption::appendArray(QVector <SANE_Word> qa)
{
  int i;
  int val;
  QString qs;
  mValueArray = qa;
  for(i=0;i<mValueArray.size();i++)
  {
    val = mValueArray[i];
    if(mSaneValueType == SANE_TYPE_FIXED)
      qs.asprintf("%.2f",SANE_UNFIX(val));
    else
      qs.asprintf("%d",int(val));
    mpSelectionCombo->addItem(qs);
  }
}
/**  */
SANE_Value_Type QWordComboOption::getSaneType()
{
	return mSaneValueType;
}
/**  */
SANE_Word QWordComboOption::getCurrentValue()
{
    return mValueArray[mpSelectionCombo->currentIndex()];
}
/**  */
void QWordComboOption::setValue(SANE_Word val)
{
  int i;
  for(i=0;i<mValueArray.size();i++)
  {
    if(val == mValueArray[i])
    {
      mpSelectionCombo->setCurrentIndex(i);
      return;
    }
  }
}
/**  */
void QWordComboOption::slotValueChanged(int)
{
  slotEmitOptionChanged();
}
/**  */
void QWordComboOption::enableAutomatic(bool b)
{
  mAutomatic =b;
  if(b)
    mpAutoCheckBox->show();
  else
    mpAutoCheckBox->hide();
}
/**  */
bool QWordComboOption::automatic()
{
  return mAutomatic;
}
/**Turn automatic mode on or off.  */
void QWordComboOption::slotAutoMode(bool automode)
{
  mAutomatic = automode;
  if(automode)
    mpSelectionCombo->setEnabled(false);
  else
    mpSelectionCombo->setEnabled(true);
  emit signalAutomatic(optionNumber(),automode);
}
