/***************************************************************************
                          qsaneoption.cpp  -  description
                             -------------------
    begin                : Sat Mar 17 2001
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

#include "./images/brx_option.xpm"
#include "./images/bry_option.xpm"
#include "./images/tlx_option.xpm"
#include "./images/tly_option.xpm"
#include "./images/gamma_option.xpm"
#include "./images/gamma_red_option.xpm"
#include "./images/gamma_green_option.xpm"
#include "./images/gamma_blue_option.xpm"
#include "./images/brightness_option.xpm"
#include "./images/contrast_option.xpm"
#include "./images/color_option.xpm"
#include "./images/resolution_option.xpm"

#include "qsaneoption.h"

#include <qlabel.h>
#include <qpixmap.h>
#include <qstring.h>

QSaneOption::QSaneOption(QString title,QWidget *parent,const char *name )
            :QWidget(parent)
{
  setObjectName(name);
  mTitleText = title;
  mOptionName = name;
  mSaneConstraintType = SANE_CONSTRAINT_NONE;
  mSaneValueType = SANE_TYPE_INT;
  mOptionNumber = -1;
}

QSaneOption::~QSaneOption()
{
}
/**  */
void QSaneOption::assignPixmap()
{
  QPixmap pix;
  mpPixmapWidget = new QLabel(this);
	mpPixmapWidget->setFixedSize(31,31);
  if(mOptionName==QString(SANE_NAME_SCAN_MODE))
		pix = QPixmap((const char **)color_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_TL_X))
		pix = QPixmap((const char **)tlx_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_TL_Y))
		pix = QPixmap((const char **)tly_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_BR_X))
		pix = QPixmap((const char **)brx_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_BR_Y))
		pix = QPixmap((const char **)bry_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_RESOLUTION))
		pix = QPixmap((const char **)resolution_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_X_RESOLUTION))
		pix = QPixmap((const char **)resolution_option_xpm);
  else if(mOptionName==QString(SANE_NAME_SCAN_Y_RESOLUTION))
		pix = QPixmap((const char **)resolution_option_xpm);
  else if(mOptionName==QString(SANE_NAME_BRIGHTNESS))
		pix = QPixmap((const char **)brightness_option_xpm);
  else if(mOptionName==QString(SANE_NAME_CONTRAST))
		pix = QPixmap((const char **)contrast_option_xpm);
  else if(mOptionName==QString(SANE_NAME_GAMMA_VECTOR))
		pix = QPixmap((const char **)gamma_option_xpm);
  else if(mOptionName==QString(SANE_NAME_GAMMA_VECTOR_R))
		pix = QPixmap((const char **)gamma_red_option_xpm);
  else if(mOptionName==QString(SANE_NAME_GAMMA_VECTOR_G))
		pix = QPixmap((const char **)gamma_green_option_xpm);
  else if(mOptionName==QString(SANE_NAME_GAMMA_VECTOR_B))
		pix = QPixmap((const char **)gamma_blue_option_xpm);
  else if(mOptionName==QString(SANE_NAME_ANALOG_GAMMA))
		pix = QPixmap((const char **)gamma_option_xpm);
  else if(mOptionName==QString(SANE_NAME_ANALOG_GAMMA_R))
		pix = QPixmap((const char **)gamma_red_option_xpm);
  else if(mOptionName==QString(SANE_NAME_ANALOG_GAMMA_G))
		pix = QPixmap((const char **)gamma_green_option_xpm);
  else if(mOptionName==QString(SANE_NAME_ANALOG_GAMMA_B))
		pix = QPixmap((const char **)gamma_blue_option_xpm);
  if(!pix.isNull())
    mpPixmapWidget->setPixmap(pix);
}
/**  */
void QSaneOption::setOptionNumber(int number)
{
  mOptionNumber = number;
}
/**  */
int QSaneOption::optionNumber()
{
  return mOptionNumber;
}
/**  */
void QSaneOption::setSaneConstraintType(SANE_Constraint_Type type)
{
  mSaneConstraintType = type;
}
/**  */
SANE_Constraint_Type QSaneOption::saneConstraintType()
{
  return mSaneConstraintType;
}
/**  */
void QSaneOption::setSaneValueType(SANE_Value_Type type)
{
  mSaneValueType = type;
}
/**  */
SANE_Value_Type QSaneOption::saneValueType()
{
  return mSaneValueType;
}
/**  */
void QSaneOption::setOptionDescription(QString desc)
{
  mOptionDescription = desc;
}
/**  */
QString QSaneOption::optionDescription()
{
  return mOptionDescription;
}
/**  */
QString QSaneOption::optionName()
{
  return mOptionName;
}
/**  */
void QSaneOption::setSaneOptionNumber(int num)
{
  mSaneOptionNumber = num;
}
/**  */
int QSaneOption::saneOptionNumber()
{
  return mSaneOptionNumber;
}
/**  */
QWidget* QSaneOption::pixmapWidget()
{
  return (QWidget*) mpPixmapWidget;
}
/**  */
QString QSaneOption::optionTitle()
{
  return mTitleText;
}
void QSaneOption::slotEmitOptionChanged()
{
//printf ("slotEmitOptionChanged %p, %d\n", this, optionNumber());
  emit signalOptionChanged(optionNumber());
}
/**  */
void QSaneOption::setOptionSize(int size)
{
//the byte size
  mOptionSize = size;
}
/**  */
int QSaneOption::optionSize()
{
//the byte size
  return mOptionSize;
}
