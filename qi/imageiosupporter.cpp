/***************************************************************************
                          imageiosupporter.cpp  -  description
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

#include "err.h"

#include <QImageWriter>
#include <QImageReader>

#include "imageiosupporter.h"
#include "qimageioext.h"
#include "qqualitydialog.h"
#include "qxmlconfig.h"
//Added by qt3to4:
#include <Q3StrList>

#include <math.h>

#include <qdir.h>
#include <qfile.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qregexp.h>

ImageIOSupporter::ImageIOSupporter()
{
  mErrorString = QString::null;
//initialize extension to format map;
  mFilterMap.clear();
  mExtensionToFormatMap.clear();
  mFormatToExtensionMap.clear();
  Q3StrList lout = QImageWriter::supportedImageFormats();
  if(lout.find("JPEG") != -1)
  {
    //test whether JPEG is supported
    insertInMap(mFilterMap,"JPEG (*.jpg *.jpeg)",".jpg");
    insertInMap(mExtensionToFormatMap,".jpg","JPEG");
    insertInMap(mExtensionToFormatMap,".jpeg","JPEG");
    insertInMap(mFormatToExtensionMap,"JPEG",".jpg");
  }
  if(lout.find("TIF") != -1)
  {
    //test whether TIFF is supported
    insertInMap(mFilterMap,"TIF (*.tif *.tiff)",".tif");
    insertInMap(mExtensionToFormatMap,".tif","TIF");
    insertInMap(mExtensionToFormatMap,".tiff","TIF");
    insertInMap(mFormatToExtensionMap,"TIF",".tif");
  }
  insertInMap(mFilterMap,"PNG (*.png)",".png");
  insertInMap(mFilterMap,"BMP (*.bmp)",".bmp");
  insertInMap(mFilterMap,"XBM (*.xbm)",".xbm");
  insertInMap(mFilterMap,"XPM (*.xpm)",".xpm");
  insertInMap(mFilterMap,"PBM (*.pbm)",".pbm");
  insertInMap(mFilterMap,"PGM (*.pgm)",".pgm");
  insertInMap(mFilterMap,"PPM (*.ppm)",".ppm");
  insertInMap(mFilterMap,"PNM (*.pnm)",".pnm");
  insertInMap(mFilterMap,QObject::tr("All files (*)"),"ALL_FILES");

  insertInMap(mExtensionToFormatMap,".png","PNG");
  insertInMap(mExtensionToFormatMap,".bmp","BMP");
  insertInMap(mExtensionToFormatMap,".xbm","XBM");
  insertInMap(mExtensionToFormatMap,".xpm","XPM");
  insertInMap(mExtensionToFormatMap,".pnm","PNM");
  insertInMap(mExtensionToFormatMap,".ppm","PPM");
  insertInMap(mExtensionToFormatMap,".pbm","PBM");
  insertInMap(mExtensionToFormatMap,".pgm","PGM");

  insertInMap(mFormatToExtensionMap,"PNG",".png");
  insertInMap(mFormatToExtensionMap,"BMP",".bmp");
  insertInMap(mFormatToExtensionMap,"XBM",".xbm");
  insertInMap(mFormatToExtensionMap,"XPM",".xpm");
  insertInMap(mFormatToExtensionMap,"PNM",".pnm");
  insertInMap(mFormatToExtensionMap,"PBM",".pbm");
  insertInMap(mFormatToExtensionMap,"PGM",".pgm");
  insertInMap(mFormatToExtensionMap,"PPM",".ppm");
//input formats
  Q3StrList lin = QImageReader::supportedImageFormats();
  if(lin.find("JPEG") != -1)
  {
    //test whether JPEG is supported
    insertInMap(mInFilterMap,"JPEG (*.jpg *.jpeg)"," *.jpg *.jpeg");
  }
  if(lin.find("TIF") != -1)
  {
    //test whether TIFF is supported
    insertInMap(mInFilterMap,"TIF (*.tif *.tiff)"," *.tif *.tiff");
  }
  if(lin.find("MNG") != -1)
  {
    insertInMap(mInFilterMap,"MNG (*.mng)"," *.mng");
  }
  if(lin.find("GIF") != -1)
  {
    insertInMap(mInFilterMap,"GIF (*.gif)"," *.gif");
  }
  insertInMap(mInFilterMap,"PNG (*.png)"," *.png");
  insertInMap(mInFilterMap,"BMP (*.bmp)"," *.bmp");
  insertInMap(mInFilterMap,"XBM (*.xbm)"," *.xbm");
  insertInMap(mInFilterMap,"XPM (*.xpm)"," *.xpm");
  insertInMap(mInFilterMap,"PBM (*.pbm)"," *.pbm");
  insertInMap(mInFilterMap,"PGM (*.pgm)"," *.pgm");
  insertInMap(mInFilterMap,"PPM (*.ppm)"," *.ppm");
  insertInMap(mInFilterMap,"PNM (*.pnm)"," *.pnm");
  QString all_images = QObject::tr("All image files (");
  QMap <QString,QString>::Iterator it;
  for(it=mInFilterMap.begin();it!=mInFilterMap.end();++it)
  {
     all_images += *it;
  }
  all_images += ")";
  insertInMap(mInFilterMap,all_images,"ALL_IMAGES");
  insertInMap(mInFilterMap,QObject::tr("All files (*)"),"ALL_FILES");
}
ImageIOSupporter::~ImageIOSupporter()
{
}
/** No descriptions */
QString ImageIOSupporter::getFormatByFilename(QString filepath)
{
  QString ext;
  QFileInfo fi(filepath);
  if(fi.isDir()) return QString::null;
  ext = "." + fi.extension(false);
  if(ext == ".") ext = QString::null;
  if(mExtensionToFormatMap.contains(ext))
    return mExtensionToFormatMap[ext];
  return QString::null;
}
/** Workaround some gcc problems with QMap */
void ImageIOSupporter::insertInMap(QMap <QString,QString> & map,QString key,QString value)
{
  map.insert(key,value);
}
/** No descriptions */
QString ImageIOSupporter::getFormatByFilter(QString filter)
{
  QString format;
  QString ext;
  if(mFilterMap.contains(filter))
  {
    ext = mFilterMap[filter];
    if(mExtensionToFormatMap.contains(ext))
    {
      format = mExtensionToFormatMap[ext];
      return format;
    }
  }
  return QString::null;
}
/** No descriptions */
QString ImageIOSupporter::getExtensionByFormat(QString format)
{
  QString ext;
  if(mFormatToExtensionMap.contains(format))
  {
    ext = mFormatToExtensionMap[format];
    return ext;
  }
  return QString::null;
}
/** No descriptions */
QStringList ImageIOSupporter::getOrderedOutFilterList(QString first_filter)
{
  QMap <QString,QString>::Iterator it;
  QStringList filters;
  //find first filter
  for(it=mFilterMap.begin();it!=mFilterMap.end();++it)
  {
    if(*it == first_filter)
    {
      filters.append(it.key());
      break;
    }
  }
  for(it=mFilterMap.begin();it!=mFilterMap.end();++it)
  {
    if(*it != first_filter)
      filters.append(it.key());
  }
  return filters;
}
/** No descriptions */
QStringList ImageIOSupporter::getOrderedInFilterList(QString first_filter)
{
  QMap <QString,QString>::Iterator it;
  QStringList filters;
  //find first filter
  for(it=mInFilterMap.begin();it!=mInFilterMap.end();++it)
  {
    if(*it == first_filter)
    {
      filters.append(it.key());
      break;
    }
  }
  for(it=mInFilterMap.begin();it!=mInFilterMap.end();++it)
  {
    if(*it != first_filter)
      filters.append(it.key());
  }
  return filters;
}
/** No descriptions */
QString ImageIOSupporter::getInFilterString()
{
  QMap <QString,QString>::Iterator it;
  QString filters;
  for(it=mInFilterMap.begin();it!=mInFilterMap.end();++it)
  {
    filters += it.key();
  }
  return filters;
}
/** No descriptions */
QString ImageIOSupporter::getDataFromInFilter(QString filter)
{
  QMap <QString,QString>::Iterator it;
  //find first filter
  for(it=mInFilterMap.begin();it!=mInFilterMap.end();++it)
  {
    if(it.key() == filter)
    {
      return *it;
    }
  }
  return QString::null;
}

/** No descriptions */
QString ImageIOSupporter::getDataFromOutFilter(QString filter)
{
  QMap <QString,QString>::Iterator it;
  //find first filter
  for(it=mFilterMap.begin();it!=mFilterMap.end();++it)
  {
    if(it.key() == filter)
    {
      return *it;
    }
  }
  return QString::null;
}
/** Stupid hack collection, mainly due to the fact, that it seems impossible to
overwrite existing ImageIO handlers in Qt. */
bool ImageIOSupporter::saveImageInteractive(QString filename,QImage& image,
                                            QString iformat,QWidget* parent)
{
  QImage im;
  QFile f;
  QImageWriter iio;
  int quality;
  int i;

  quality = -1;

  if(image.isNull())
    return false;
  im = image;


  if(QFile::exists(filename))
  {
    i = QMessageBox::warning(parent,QObject::tr("Save image"),
        QObject::tr("This file already exists.\n"
  	  	"Do you want to overwrite it ?\n") ,
        QObject::tr("&Overwrite"),QObject::tr("&Cancel"));
    if(i == 1)
    {
      return false;
    }
  }
  if((iformat == "PGM") || (iformat == "PBM") || (iformat == "PPM") ||
     (iformat == "PNM"))
  {
    f.setName(filename);
    if(!f.open(QIODevice::WriteOnly))
    {
      QMessageBox::warning(parent,QObject::tr("Warning"),
                           QObject::tr("The image could not be saved."),QObject::tr("&OK"));
      return false;
    }
    iio.setDevice(&f);
  }
  else
  {
    iio.setFileName(filename);
  }

  //let the user select the compression/quality for formats
  //that support it
  if(quality == -1)
  {
    if(iformat == "JPEG")
    {
    	QQualityDialog quali(QQualityDialog::ImageType_JPEG,parent);
     	quali.exec();
      quality = quali.quality();
   	}
    if(iformat == "PNG")
    {
    	QQualityDialog quali(QQualityDialog::ImageType_PNG,parent);
     	quali.exec();
      quality = quali.quality();
   	}
    if((iformat == "TIF") && (im.depth() >= 8))
    {
    	QQualityDialog quali(QQualityDialog::ImageType_TIFF8BIT,parent);
     	quali.exec();
      quality = quali.quality();
      iio.setCompression(xmlConfig->intValue("QUALITY_TIFF_8BIT_MODE"));
   	}
    if((iformat == "TIF") && (im.depth() == 1))
    {
    	QQualityDialog quali(QQualityDialog::ImageType_TIFFLINEART,parent);
     	quali.exec();
      quality = -1;
      iio.setCompression(xmlConfig->intValue("QUALITY_TIFF_LINEART_MODE"));
   	}
  }
  //Qt supports only up to 4096 colors for the XPM format
  //actually, we transform images with a depth > 8
  //to images with a depth of 8 bit
  //Only warns if called with warnings==true
  if((iformat == "XPM") && (im.depth()>8))
  {
    i = QMessageBox::warning(parent,QObject::tr("Warning"),
                     QObject::tr("Saving the image in XPM format will change "
                     "the depth to 8 bit. This means a loss of "
                     "color information."),QObject::tr("&Save"),QObject::tr("&Cancel"));
    if(i == 1)
    {
      return false;
    }
    im.detach();
    im = im.convertDepth(8);
  }
  iio.setFormat(iformat.latin1 ());
  if((quality >= 0) && (quality <= 100))
    iio.setQuality(quality);
//  iio.setImage(im);

  bool ok;

  if((iformat == "PNM") || (iformat == "PNM") || (iformat == "PNM") ||
     (iformat == "PNM"))
  {
    ok = qis_write_pbm_image(&iio, im);
  }
  else
    ok = iio.write(im);
  if(f.isOpen())
    f.close();
  if(!ok)
  {
    QMessageBox::warning(parent,QObject::tr("Warning"),
                         QObject::tr("The image could not be saved."),QObject::tr("&OK"));
    return false;
  }
  return true;
}
/** No descriptions */
bool ImageIOSupporter::saveImage(QString filename,QImage& image,QString iformat,QWidget* parent)
{
  QImage im;
  QFile f;
  QImageWriter iio;
  int quality;

  UNUSED(parent);
  if(image.isNull())
    return false;
  im = image;

  if((iformat == "PGM") || (iformat == "PBM") || (iformat == "PPM") ||
     (iformat == "PNM"))
  {
    f.setName(filename);
    if(!f.open(QIODevice::WriteOnly))
    {
      return false;
    }
    iio.setDevice(&f);
  }
  else
  {
    iio.setFileName(filename);
  }
  //read compression/quality for formats that support it
  quality = -1;
  if(iformat == "JPEG")
  {
    quality = xmlConfig->intValue("JPEG_QUALITY");
  }
  else if(iformat == "PNG")
  {
    quality = xmlConfig->intValue("PNG_COMPRESSION");
    quality= 100-quality*91/9;   // map [9,0] -> [0,100]
    if(quality>100) quality = 100;
    if(quality<0) quality = 0;
  }
  else if((iformat == "TIF") && (im.depth() >= 8))
  {
    int mode = xmlConfig->intValue("TIFF_8BIT_MODE");
    switch(mode)
    {
      case 0: //no compression
        iio.setCompression(0);
        break;
      case 1:
        quality = xmlConfig->intValue("TIFF_JPEG_QUALITY");
        break;
      case 2:
        iio.setCompression(1);
        break;
      default:
        iio.setCompression(0);
    }
  }
  else if((iformat == "TIF") && (im.depth() == 1))
  {
    int mode = xmlConfig->intValue("TIFF_LINEART_MODE");
    switch(mode)
    {
      case 0: //no compression
        iio.setCompression(0);
        break;
      case 1:
        iio.setCompression(1);
//s        iio.setParameters("COMPRESSION_PACKBITS");
        break;
      case 2:
//s        iio.setParameters("COMPRESSION_CCITTRLE");
        break;
      case 3:
//s        iio.setParameters("COMPRESSION_CCITTFAX3");
        break;
      case 4:
//s        iio.setParameters("COMPRESSION_CCITTFAX4");
        break;
      default:
        iio.setCompression(0);
//s        iio.setParameters("COMPRESSION_NONE");
    }
  }
  //Qt supports only up to 4096 colors for the XPM format
  //actually, we transform images with a depth > 8
  //to images with a depth of 8 bit
  //Only warns if called with warnings==true
  if((iformat == "XPM") && (im.depth()>8))
  {
    im.detach();
    im = im.convertDepth(8);
    if(im.isNull())
    {
      return false;
    }
  }

  iio.setFormat(iformat.latin1 ());
  if((quality >= 0) && (quality <= 100))
    iio.setQuality(quality);

  bool ok;

  if((iformat == "PGM") || (iformat == "PBM") || (iformat == "PPM") ||
     (iformat == "PNM"))
  {
    ok = qis_write_pbm_image(&iio, im);
  }
  else
    ok = iio.write(im);
  if(f.isOpen())
    f.close();
  return ok;
}
/** No descriptions */
bool ImageIOSupporter::loadImage(QImage& imager,const QString& filename)
{
  QString iformat;

  QImageReader reader("image.png");
 // reader.format() == "png"  iformat = QImage::imageFormat(filename);
  iformat = reader.format ().left(3);
  if((iformat == "PGM") || (iformat == "PBM") ||
     (iformat == "PPM") || (iformat == "PNM"))
  {
    QFile f(filename);
    if(!f.open(QIODevice::ReadOnly))
      return false;
    QImageReader iio((QIODevice*)&f,0);
    QImage *image = qis_read_pbm_image(&iio);
    f.close();
    if(!image)
      return false;
    imager = *image;
    return true;
  }
  return imager.load(filename);
}
/** No descriptions */
QString ImageIOSupporter::validateExtension(QString file_path,QString format)
{
  QString new_filepath;
  QString ext;
  QFileInfo fi(file_path);
  mErrorString = QString::null;
  if(!(fi.extension(false).isEmpty()))
  {
    ext = "." + fi.extension(false);
    if(mFormatToExtensionMap.contains(format))
    {
      if(mFormatToExtensionMap[format] != ext)
        ext = QString::null;
    }
  }
qDebug("ImageIOSupporter: ext %s",ext.latin1());

  if(mExtensionToFormatMap.contains(ext)) //valid filename extension ?
  {
    new_filepath = file_path;//valid
  }
  else
  {
    if(format.isEmpty())
    {
      mErrorString = QObject::tr("No valid filename extension.") + "\n";
      return QString::null;//no extension, no format -> can't create filename
    }
    if(mFormatToExtensionMap.contains(format))
    {
      ext = mFormatToExtensionMap[format];
      new_filepath = file_path + ext;
    }
    else
    {
      mErrorString = QObject::tr("No valid filename extension.") + "\n";
      return QString::null;
    }
  }
qDebug("ImageIOSupporter:return new_filepath %s",new_filepath.latin1());
  return new_filepath;
}
/** No descriptions */
QString ImageIOSupporter::lastErrorString()
{
  return mErrorString;
}
