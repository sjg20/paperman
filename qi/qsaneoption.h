/***************************************************************************
                          qsaneoption.h  -  description
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

#ifndef QSANEOPTION_H
#define QSANEOPTION_H

#include <qwidget.h>
#include <QLabel>
#include <sane/sane.h>
#include <sane/saneopts.h>

/**The base class for all
option widgets.
  *@author Michael Herder
  */
class QLabel;
class QString;

class QSaneOption : public QWidget
{
   Q_OBJECT
public: 
  QSaneOption(QString title,QWidget *parent,const char *name=0);
  ~QSaneOption();
  /**  */
  void setOptionNumber(int number);
  /**  */
  int optionNumber();
  /**  */
  void setOptionDescription(QString desc);
  /**  */
  QString optionDescription();
  /**  */
  QString optionName();
  /**  */
  void setSaneOptionNumber(int num);
  /**  */
  int saneOptionNumber();
  /**  */
  void setSaneConstraintType(SANE_Constraint_Type type);
  /**  */
  SANE_Constraint_Type saneConstraintType();
  /**  */
  void setSaneValueType(SANE_Value_Type type);
  /**  */
  SANE_Value_Type saneValueType();
  /**  */
  QString optionTitle();
  /**  */
  void setOptionSize(int size);
  /**  */
  int optionSize();
protected:
  /**  */
  void assignPixmap();
  /**  */
  QWidget* pixmapWidget();
private:
  int mOptionSize;
  /**  */
  int mOptionNumber;
  /**  */
  QString mOptionName;
  /**  */
  int mSaneOptionNumber;
  /**  */
  QString mOptionDescription;
  /**  */
  QLabel* mpPixmapWidget;
  /**  */
  QString mTitleText;
  /**  */
  SANE_Constraint_Type mSaneConstraintType;
  /**  */
  SANE_Value_Type mSaneValueType;
public slots:
  /**  */
  void slotEmitOptionChanged();
signals:
  /**  */
  void signalOptionChanged(int);
};

#endif
