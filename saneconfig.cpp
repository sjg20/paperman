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

#include <QProcess>
#include <QVariant>

#include "saneconfig.h"

#include <qapplication.h>
#include <qtimer.h>

SaneConfig::SaneConfig(QObject* parent,const char* name)
           :QObject(parent)
{
  setProperty ("name", QVariant(name));
  mRun = false;
#ifndef USE_QT3
  mpProcess = new QProcessBackport(parent);
#else
  mpProcess = new QProcess(parent);
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
  QStringList arguments;

  arguments << "--prefix";
  mpProcess->start("sane-config", arguments);
  if (mpProcess->waitForStarted (1000))
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
  if(mpProcess->state() == QProcess::Running)
    mpProcess->kill();
  mRun = true;
}
/** No descriptions */
void SaneConfig::slotReadStdout()
{
  QString qs(mpProcess->readAllStandardOutput());
  mPrefix = qs;
  mRun = true;
}
/** No descriptions */
QString& SaneConfig::prefix()
{
  if(mRun == false)
    run();
  while (mpProcess->state() == QProcess::Running || (mRun == false))
  {
    qApp->processEvents();
  }
  return mPrefix;
}
