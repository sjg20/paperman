/***************************************************************************
                          qcombooption.cpp  -  description
                             -------------------
    begin                : Tue Jul 4 2000
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

#include "qcombooption.h"

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfontmetrics.h>
#include <q3hbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <q3listbox.h>
#include <qstring.h>
#include <qsizepolicy.h>
#include <qtooltip.h>
//Added by qt3to4:
#include <Q3GridLayout>

#include <stdio.h>

QComboOption::QComboOption(QString title,QWidget * parent,const char* name)
             :QSaneOption(title,parent,name)
{
  mTitleText = title;
  mAutomatic = false;
  initWidget();
}
QComboOption::~QComboOption()
{
}
/**  */
void QComboOption::initWidget()
{
  Q3GridLayout* qgl = new Q3GridLayout(this,2,2);
  mpTitleLabel = new QLabel(mTitleText,this);
  Q3HBox* hbox1 = new Q3HBox(this);
  mpSelectionCombo = new QComboBox(false,hbox1);
  mpSelectionCombo->setFocusPolicy(Qt::StrongFocus);//should get focus after clicking
  connect(mpSelectionCombo,SIGNAL(activated(int)),
          this,SLOT(slotSelectionChanged(int)));
  connect(mpSelectionCombo,SIGNAL(activated(int)),
          this,SLOT(slotChangeTooltip(int)));
  hbox1->setStretchFactor(mpSelectionCombo,1);
  mpAutoCheckBox = new QCheckBox(tr("Automatic"),hbox1);
  hbox1->setSpacing(6);
  mpAutoCheckBox->hide();
//create pixmap
  assignPixmap();
	qgl->addMultiCellWidget(pixmapWidget(),0,1,0,0);

	qgl->addWidget(mpTitleLabel,0,1);
	qgl->addWidget(hbox1,1,1);
  qgl->setSpacing(5);
	qgl->setColStretch(0,0);
	qgl->setColStretch(1,1);
	qgl->activate();
  connect(mpAutoCheckBox,SIGNAL(toggled(bool)),this,SLOT(slotAutoMode(bool)));
}
/**  */
void QComboOption::appendItem(const char* item)
{
	mpSelectionCombo->insertItem(item,-1);
}
/**  */
void QComboOption::setCurrentValue(const char* item)
{
	QString qs(item);
  QString qs2;
	int cnt;
	for(cnt=0;cnt<mpSelectionCombo->count();cnt++)
	{
    qs2 = mStringList[cnt];
		if(qs2 == qs)
		{
			mpSelectionCombo->setCurrentItem(cnt);
      slotChangeTooltip(cnt);
			break;
		}
	}
}
/**  */
QString QComboOption::getCurrentText()
{
  mCurrentText = mStringList[mpSelectionCombo->currentItem()];
	return mCurrentText;
}
/**  */
void QComboOption::enableAutomatic(bool b)
{
  mAutomatic = b;
  if(b)
    mpAutoCheckBox->show();
  else
    mpAutoCheckBox->hide();
}
/**  */
bool QComboOption::automatic()
{
  return mAutomatic;
}
/**Turn automatic mode on or off.  */
void QComboOption::slotAutoMode(bool automode)
{
  if(automode)
    mpSelectionCombo->setEnabled(false);
  else
    mpSelectionCombo->setEnabled(true);
  emit signalAutomatic(optionNumber(),automode);
}
/**  */
void QComboOption::slotSelectionChanged(int)
{
  slotEmitOptionChanged();
}
/** No descriptions */
void QComboOption::setStringList(const QStringList& slist)
{
  QStringList templist = slist;
  mStringList = slist;

  QStringList::Iterator it;
  for( it = templist.begin(); it != templist.end(); ++it )
  {
    QString qs = *it;
    if(qs.length() > 50)
    {
      qs = qs.left(47) + "...";
      *it = qs;
    }
  }
  mpSelectionCombo->clear();
  mpSelectionCombo->insertStringList(templist);
}
/** No descriptions */
void QComboOption::slotChangeTooltip(int index)
{
  QToolTip::remove(mpSelectionCombo);
  if(mpSelectionCombo->text(index).right(3) == "...")
    QToolTip::add(mpSelectionCombo,mStringList[index]);
}
