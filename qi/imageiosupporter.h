/***************************************************************************
                          imageiosupporter.h  -  description
                             -------------------
    begin                : Thu Feb 14 2002
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

#ifndef IMAGEIOSUPPORTER_H
#define IMAGEIOSUPPORTER_H

#include <qimage.h>
#include <qmap.h>
#include <qstring.h>
#include <qstringlist.h>

/**
  *@author Michael Herder
  */
class QWidget;

class ImageIOSupporter
{
public: 
  ImageIOSupporter();
  ~ImageIOSupporter();
  /** No descriptions */
  QString getFormatByFilename(QString filepath);
  /** No descriptions */
  QString getExtensionByFormat(QString format);
  /** No descriptions */
  QString getFormatByFilter(QString filter);
  /** No descriptions */
  QStringList getOrderedOutFilterList(QString first_filter);
  /** No descriptions */
  QString getDataFromInFilter(QString filter);
  /** No descriptions */
  QString getDataFromOutFilter(QString filter);
  /** No descriptions */
  QStringList getOrderedInFilterList(QString first_filter);
  /** No descriptions */
  QString getInFilterString();
  /** No descriptions */
  bool saveImageInteractive(QString filename,QImage& image,QString iformat,QWidget* parent=0);
  /** No descriptions */
  bool saveImage(QString filename,QImage& image,QString iformat,QWidget* parent=0);
  /** No descriptions */
  bool loadImage(QImage& image,const QString& filename);
  /** No descriptions */
  QString validateExtension(QString file_path,QString format=QString());
  /** No descriptions */
  QString lastErrorString();
private: // Private methods
  /** No descriptions */
  QString mErrorString;
  /** No descriptions */
  void insertInMap(QMap<QString,QString> & map,QString key,QString value);
  /** No descriptions */
  QMap <QString,QString> mExtensionToFormatMap;
  /** No descriptions */
  QMap <QString,QString> mFormatToExtensionMap;
  /** No descriptions */
  QMap <QString,QString> mFilterMap;
  /** No descriptions */
  QMap <QString,QString> mInFilterMap;
};

#endif
