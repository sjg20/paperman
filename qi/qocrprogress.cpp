/***************************************************************************
                          qocrprogress.cpp  -  description
                             -------------------
    begin                : Tue Jan 16 2001
    copyright            : (C) 2001 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#include "qocrprogress.h"
#include "imageiosupporter.h"

#ifndef USE_QT3
#include "./3rdparty/qtbackport/qprocess.h"
#else
#include <qprocess.h>
#endif

#include "qunknownprogresswidget.h"
#include "qxmlconfig.h"

#include <qfile.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qtextstream.h>


QOCRProgress::QOCRProgress(QWidget* parent,const char* name,bool modal)
             :QDialog(parent,name,modal)
{
  setCaption(tr("QuiteInsane - OCR"));
  mImagePath = QString::null;
  mOcrImagePath = QString::null;
#ifndef USE_QT3
  mpOcrProcess = new QProcessBackport(this);
#else
  mpOcrProcess = new QProcess(this);
#endif
  initDlg();
}
QOCRProgress::~QOCRProgress()
{
  if(mpOcrProcess != 0)
  {
    qDebug("deleting mpOcrProcess");
    delete mpOcrProcess;
  }
}
/**  */
void QOCRProgress::initDlg()
{
  mpMainLayout = new QGridLayout(this,3,3);
  mpInfoLabel = new QLabel(tr("Optical character recognition..."),this);
  mpUnknownProgress = new QUnknownProgressWidget(this);
  mpUnknownProgress->setFixedHeight(20);
  mpCancelButton = new QPushButton(tr("&Cancel"),this);
  mpCancelButton->setDefault(true);
  mpMainLayout->addMultiCellWidget(mpInfoLabel,0,0,0,2);
  mpMainLayout->addMultiCellWidget(mpUnknownProgress,1,1,0,2);
  mpMainLayout->addWidget(mpCancelButton,2,1);
  mpMainLayout->setSpacing(6);
  mpMainLayout->setMargin(6);
  mpMainLayout->activate();
  setMinimumWidth(150);
  connect(mpCancelButton,SIGNAL(clicked()),this,SLOT(slotStopOCR()));
}
/**  */
void QOCRProgress::startOCR()
{
  ImageIOSupporter iosup;
  if(mpOcrProcess == 0)
    reject();
  mpOcrProcess->clearArguments();
//We must check, whether the current image has a format which
//can be read by gocr.
//If not, we save it in pnm format first
  QImage tempimage;
  QString format;
  if(!mImagePath.isEmpty())
  {
    format = QImage::imageFormat(mImagePath);
    if((format.left(3) == "PPM") ||
       (format.left(3) == "PGM") ||
       (format.left(3) == "PPM"))
    {//correct format
      mOcrImagePath = mImagePath;
    }
    else
    {
      tempimage.load(mImagePath);
      iosup.saveImage(xmlConfig->absConfDirPath()+".ocrtemp.pnm",tempimage,"PPM");
      mOcrImagePath = xmlConfig->absConfDirPath()+".ocrtemp.pnm";
    }
  }
  else
  {
    if(mImage.isNull())
    {
      return;
    }
    else
    {
      iosup.saveImage(xmlConfig->absConfDirPath()+".ocrtemp.pnm",mImage,"PPM");
      mOcrImagePath = xmlConfig->absConfDirPath()+".ocrtemp.pnm";
    }
  }
  connect(mpOcrProcess,SIGNAL(processExited()),
          this,SLOT(slotStopOCR2()));
  connect(mpOcrProcess,SIGNAL(readyReadStdout()),
          this,SLOT(slotReceivedStdout()));
  connect(mpOcrProcess,SIGNAL(readyReadStderr()),
          this,SLOT(slotReceivedStderr()));

  QStringList al = QStringList::split(" ",
                                      xmlConfig->stringValue("GOCR_COMMAND"),
                                      false);
  for(QStringList::Iterator it = al.begin(); it != al.end(); ++it )
  {
    if((*it).left(2) != "-o")
    {
      mpOcrProcess->addArgument(*it);
    }
    else
      break;
  }
  mpOcrProcess->addArgument(mOcrImagePath);
  mpOcrProcess->addArgument("-o");
  mpOcrProcess->addArgument(xmlConfig->absConfDirPath()+"ocrtext.txt");
  if(mpOcrProcess->start())
  {
    mpUnknownProgress->start(40);
  }
}
/**  */
void QOCRProgress::slotStopOCR2()
{
  if(!mpOcrProcess) reject();
  int n;
  n = 1;
  QString qs;
  if(mpOcrProcess->normalExit())
  {
    if(!mpOcrProcess->exitStatus())
    {//we can read the result
      QFile f(xmlConfig->absConfDirPath()+"ocrtext.txt");
      if ( f.open(IO_ReadOnly) )
      {
        QTextStream t( &f );
        mOcrText = "";
        while ( !t.eof() )
        {
          mOcrText += t.readLine() + "\n";
        }
        f.close();
      }
    }
    mpUnknownProgress->stop();
    accept();
    return;
  }
  mpUnknownProgress->stop();
  done(0);
}
/**  */
void QOCRProgress::slotStopOCR()
{
  if(mpOcrProcess == 0)
    reject();
  QString qs;
  if(mpOcrProcess->isRunning())
  {
    mpOcrProcess->kill();
  }
  mpUnknownProgress->stop();
  done(2);
}
/**  */
void QOCRProgress::slotReceivedStdout()
{
  if(mpOcrProcess)
  {
    QString qs(mpOcrProcess->readStdout());
    qDebug(qs);
  }
}
/** */
void QOCRProgress::slotReceivedStderr()
{
  if(mpOcrProcess)
  {
    QString qs(mpOcrProcess->readStderr());
    qDebug(qs);
  }
}
/**  */
void QOCRProgress::showEvent(QShowEvent * e)
{
  setEnabled(true);
  QDialog::showEvent(e);
  startOCR();
}
/**  */
QString QOCRProgress::ocrText()
{
  return mOcrText;
}
/** No descriptions */
void QOCRProgress::setImage(QImage& image)
{
  mImagePath = QString::null;
  mImage = image;
}
/** No descriptions */
void QOCRProgress::setImagePath(QString path)
{
  mImagePath = path;
}
