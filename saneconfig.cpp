/***************************************************************************
                          saneconfig.cpp  -  description
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

#include "saneconfig.h"

#include <qapplication.h>
#ifndef USE_QT3
#include "3rdparty/qtbackport/q3process.h"
#else
#include <q3process.h>
#endif
#include <qtimer.h>

SaneConfig::SaneConfig(QObject* parent,const char* name)
           :QObject(parent,name)
{
  mRun = false;
#ifndef USE_QT3
  mpProcess = new QProcessBackport(parent);
#else
  mpProcess = new Q3Process(parent);
#endif
  connect(mpProcess,SIGNAL(processExited()),
          this,SLOT(slotExited()));
  connect(mpProcess,SIGNAL(readyReadStdout()),
          this,SLOT(slotReadStdout()));
}
SaneConfig::~SaneConfig()
{
}
/** No descriptions */
void SaneConfig::run()
{
  if(mRun == true)
    return;
  mpProcess->addArgument("sane-config");
  mpProcess->addArgument("--prefix");
  if(!mpProcess->start())
  {
    mRun = true;
    return;
  }
  //try to make sure, that our process doesn't block forever
  QTimer::singleShot(5000,this,SLOT(slotForceStop()));
}
/** No descriptions */
void SaneConfig::slotExited()
{
  mRun = true;
}
/** No descriptions */
void SaneConfig::slotForceStop()
{
  if(mpProcess->isRunning())
    mpProcess->kill();
  mRun = true;
}
/** No descriptions */
void SaneConfig::slotReadStdout()
{
  QString qs(mpProcess->readStdout());
  mPrefix = qs;
  mRun = true;
}
/** No descriptions */
QString& SaneConfig::prefix()
{
  if(mRun == false)
    run();
  while(mpProcess->isRunning() || (mRun == false))
  {
    qApp->processEvents();
  }
  return mPrefix;
}
