/***************************************************************************
                          imagebuffer.h  -  description
                             -------------------
    begin                : Tue Jan 22 2002
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

#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <qstring.h>
/**
  *@author Michael Herder
  */
class QImage;

class ImageBuffer
{
public: 
	ImageBuffer();
	~ImageBuffer();
  /** No descriptions */
  double tlx();
  /** No descriptions */
  double tly();
  /** No descriptions */
  double brx();
  /** No descriptions */
  double bry();
  /** No descriptions */
  void setPercentSize(double tlx,double tly,double brx,double bry);
  /** No descriptions */
  void setTlx(double tlx);
  /** No descriptions */
  void setTly(double tly);
  /** No descriptions */
  void setBrx(double brx);
  /** No descriptions */
  void setBry(double bry);
  /** No descriptions */
  void setImage(QImage* image,bool saved=false);
  /** No descriptions */
  QImage* image();
  /** No descriptions */
  void setZoomString(QString string);
  /** No descriptions */
  QString zoomString();
  /** No descriptions */
  void setSaved(bool status);
  /** No descriptions */
  bool saved();
  /** No descriptions */
  void setTlxOrig(double value);
  /** No descriptions */
  void setTlyOrig(double value);
  /** No descriptions */
  void setBrxOrig(double value);
  /** No descriptions */
  void setBryOrig(double value);
  /** No descriptions */
  double tlxOrig();
  /** No descriptions */
  double tlyOrig();
  /** No descriptions */
  double brxOrig();
  /** No descriptions */
  double bryOrig();
  /** No descriptions */
  double tlxMapped();
  /** No descriptions */
  double tlyMapped();
  /** No descriptions */
  double brxMapped();
  /** No descriptions */
  double bryMapped();
  /** No descriptions */
  QString path();
  /** No descriptions */
  void setPath(QString path);
  /** No descriptions */
  QImage selectedImage();
  /** No descriptions */
  double aspectRatio();
  /** No descriptions */
  void setAspectRatio(double aspect);
private:
  bool mSaved;
  QString mZoomString;
  QString mPath;
  QImage* mpImage;
  double mTlxPercent;
  double mTlyPercent;
  double mBrxPercent;
  double mBryPercent;
  double mTlxOrig;
  double mTlyOrig;
  double mBrxOrig;
  double mBryOrig;
  double mAspectRatio;
};

#endif
