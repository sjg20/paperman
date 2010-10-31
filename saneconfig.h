/***************************************************************************
                          saneconfig.h  -  description
                             -------------------
    begin                : Sat Apr 26 2003
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

#ifndef SANECONFIG_H
#define SANECONFIG_H

#include "resource.h"
#include <qobject.h>
#include <qstring.h>

/**
  *@author Michael Herder
  */

#ifndef USE_QT3
class QProcessBackport;
#else
class Q3Process;
#endif

class SaneConfig : public QObject
{
Q_OBJECT
public:
  SaneConfig(QObject* parent,const char* name=0);
  /** No descriptions */
  QString& prefix();
  ~SaneConfig();
private:
  bool mRun;
  QString mPrefix;
#ifndef USE_QT3
  QProcessBackport* mpProcess;
#else
  Q3Process* mpProcess;
#endif
private slots: // Private slots
  /** No descriptions */
  void slotReadStdout();
  /** No descriptions */
  void slotExited();
  /** No descriptions */
  void slotForceStop();
private: // Private methods
  /** No descriptions */
  void run();
};

#endif
