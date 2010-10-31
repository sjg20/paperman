/***************************************************************************
                          qimageioext.h  -  description
                             -------------------
    begin                : Mon Mar 26 2001
    copyright            : (C) 2001 by M. Herder
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

#ifndef QIMAGEIOEXT_H
#define QIMAGEIOEXT_H
/**
  *@author M. Herder
  */

#include "resource.h"
#include <qstring.h>
#include <qimage.h>

class QImageWriter;
class QImageReader;

#ifdef HAVE_LIBTIFF
  void initTiffIO();
#endif
  void initPnmIO();
  bool qis_write_pbm_image( QImageWriter *iio, QImage &image );
  QImage *qis_read_pbm_image(QImageReader* iio);
#endif

