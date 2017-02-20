/***************************************************************************
                          qdoublespinbox.h  -  description
                             -------------------
    begin                : Tue Dec 12 2000
    copyright            : (C) 2000 by M. Herder
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

#ifndef QDOUBLESPINBOX_H
#define QDOUBLESPINBOX_H

#include <qspinbox.h>
#include <qstring.h>
/**
  *@author M. Herder
  */

class QDouble100SpinBox : public QSpinBox
{
Q_OBJECT
public:
	QDouble100SpinBox(QWidget * parent = 0, const char * name = 0);
  /**  */
  void selectAll();
protected: // Protected methods
  /**  */
  virtual int mapTextToValue(bool* ok);
protected: // Protected methods
  /**  */
  QValidator::State validate(QString &input, int &pos) const;
  virtual QString mapValueToText(int value);
	~QDouble100SpinBox();
public:
  void setValidator(QValidator *validator);
private:
  QValidator *mValidator;
};

#endif
