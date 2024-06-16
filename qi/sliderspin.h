/***************************************************************************
                          sliderspin.h  -  description
                             -------------------
    begin                : Fri Mar 8 2002
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

#ifndef SLIDERSPIN_H
#define SLIDERSPIN_H

#include <QLabel>

/**A combination of a QLabel, a QSlider and
a QSpinBox, arranged in a QVBox.
  *@author Michael Herder
  */

class QLabel;
class QSlider;
class QSpinBox;

class SliderSpin : public QWidget
{
Q_OBJECT
public:
  SliderSpin(QWidget* parent=0,const char* name=0,Qt::WindowFlags f=Qt::WindowFlags(),bool allowLines=true);
  SliderSpin(int minval,int maxval,int val, QString title,QWidget* parent=0,
             const char* name=0,Qt::WindowFlags f=Qt::WindowFlags(),bool allowLines=true);
  ~SliderSpin();
  /** No descriptions */
  void setRange(int min,int max);
  /** No descriptions */
  void setTitle(QString title);
  /** No descriptions */
  int value();
  /** No descriptions */
  void setValue(int value);
  void setEnabled ( bool enable);
private: // Private methods
  /** No descriptions */
  void initWidget();
private:
  /** */
  QLabel* mpTitleLabel;
  /** */
  QSlider* mpSlider;
  /** */
  QSpinBox* mpSpinBox;
private slots: // Private slots
  /** No descriptions */
  void slotValueChanged(int value);
  /** No descriptions */
  void slotSliderMoved(int value);
  /** No descriptions */
  void slotSpinValueChanged(int value);
signals:
  void signalValueChanged(int value);
};

#endif
