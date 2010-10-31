/***************************************************************************
                          qsanestatusmessage.cpp  -  description
                             -------------------
    begin                : Thu Jun 7 2001
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

#include "qsanestatusmessage.h"

#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>

QSaneStatusMessage::QSaneStatusMessage(SANE_Status status, QWidget*parent)
                   :QMessageBox(parent)
{
  mSaneStatus = status;
  setCaption(tr("SANE Message"));
  setMessage();
}

QSaneStatusMessage::~QSaneStatusMessage()
{
}

void QSaneStatusMessage::setMessage()
{
  setIcon(QMessageBox::Warning);
  switch(mSaneStatus)
  {
    case SANE_STATUS_ACCESS_DENIED:
      setText(tr("The access has been denied\n"
                 "due to insufficient or\n"
                 "wrong authentification."));
      break;
    case SANE_STATUS_CANCELLED:
      setText(tr("The operation was cancelled."));
      break;
    case SANE_STATUS_DEVICE_BUSY:
      setText(tr("The device is busy.\n\n"
                 "Please retry later."));
      break;
    case SANE_STATUS_JAMMED:
      setText(tr("The document feeder is jammed.\n\n"
                 "Please remove the problem and start\n"
                 "a new scan afterwards."));
      break;
    case SANE_STATUS_NO_DOCS:
      setText(tr("The document feeder is out of documents.\n\n"
                 "Please insert new documents and start a\n"
                 "new scan afterwards."));
      break;
    case SANE_STATUS_COVER_OPEN:
      setText(tr("The scanner cover is open.\n\n"
                 "Please close the scanner cover and\n"
                 "start a new scan afterwards."));
      break;
    case SANE_STATUS_IO_ERROR:
      setText(tr("IO error while communicating with the device."));
      break;
    case SANE_STATUS_NO_MEM:
      setText(tr("Insufficent memory."));
      break;
    case SANE_STATUS_EOF:
      setText(tr("The device returned an unexpected "
                 "SANE_STATUS_EOF."));
      break;
    case SANE_STATUS_UNSUPPORTED:
      setText(tr("The requested operation is not supported."));
      break;
    default:
      setText(tr("Unknown error."));
  }
}
