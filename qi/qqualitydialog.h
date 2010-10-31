/***************************************************************************
                          qqualitydialog.h  -  description
                             -------------------
    begin                : Fri Sep 1 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#ifndef QQUALITYDIALOG_H
#define QQUALITYDIALOG_H

#include <qdialog.h>
//Added by qt3to4:
#include <QLabel>

/**
  *@author M. Herder
  */
class QComboBox;
class QSlider;
class QLabel;
class QString;
class Q3HBox;

class QQualityDialog : public QDialog
{
   Q_OBJECT
public:
  enum ImageType
  {
    ImageType_PNG         = 0,
    ImageType_JPEG        = 1,
    ImageType_TIFF8BIT    = 2,
    ImageType_TIFFLINEART = 3
  };
	QQualityDialog(ImageType t,QWidget *parent=0, const char *name=0,bool modal=true);
	~QQualityDialog();
private: // Private attributes
  /**  */
  QLabel* mpQualityLabel;
  /**  */
  QComboBox* mpTiff8BitCombo;
  /**  */
  QComboBox* mpTiffLineartCombo;
  /**  */
  int mQuality;
  /**  */
  QSlider* mpQualitySlider;
  /** */
  ImageType mImageType;
  /**  */
  QString mCompressionType;
  /**  */
  Q3HBox* mpQualityHBox;
private: //methods
  /**  */
  void initDialog();
public:
  /**  */
  int quality();
  /**  */
  QString compressionType();
private slots: // Private slots
  /**  */
  void slotQualityChanged(int value);
  /**  */
  void slotEnableQuality(int index);
};

#endif
