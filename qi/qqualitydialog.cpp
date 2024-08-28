/***************************************************************************
                          qqualitydialog.cpp  -  description
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

#include "qqualitydialog.h"
#include "qxmlconfig.h"

#include <QGroupBox>

#include <qcombobox.h>
#include <QHBoxLayout>
#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qstring.h>
#include <qwidget.h>
#include <QGridLayout>


QQualityDialog::QQualityDialog(ImageType t,QWidget *parent,
                               const char *name,bool modal)
               : QDialog(parent)
{
    setModal(modal);
    setObjectName(name);
  mCompressionType = "COMPRESSION_NONE";
  mpTiff8BitCombo = 0;
  mpTiffLineartCombo = 0;
  mImageType = t;
	setWindowTitle(tr("Image settings"));
	initDialog();
}

QQualityDialog::~QQualityDialog()
{
}
/**  */

void QQualityDialog::initDialog()
{
  QGridLayout* mainlayout = new QGridLayout(this);
  QGroupBox* qgb;
  int qual;
//  int type = 0;
//the appearance depends on the image type
  switch(mImageType)
  {
    case ImageType_PNG:
    {
      qgb = new QGroupBox(tr("PNG compression"),this);
      mpQualityHBox = new QHBoxLayout(qgb);
      mpQualityHBox->setSpacing(6);
      mpQualityHBox->addWidget(new QLabel(tr("low")));
      mpQualitySlider = new QSlider(Qt::Horizontal);
      mpQualitySlider->setMinimum(0);
      mpQualitySlider->setMaximum(9);
      mpQualitySlider->setSingleStep(1);
      mpQualitySlider->setPageStep(6);
      mpQualityHBox->addWidget(mpQualitySlider);
      mpQualityHBox->addWidget(new QLabel(tr("high")));
      mpQualityLabel = new QLabel("");
      mpQualityHBox->addWidget(mpQualityLabel);
      mpQualityLabel->setText("6");
      mQuality = 100-6*91/9;
      mpQualitySlider->setMinimumWidth(80);
    	connect(mpQualitySlider,SIGNAL(valueChanged(int)),
              this,SLOT(slotQualityChanged(int)));
      qual = xmlConfig->intValue("QUALITY_PNG_COMPRESSION");
      mpQualitySlider->setValue(qual);
      slotQualityChanged(qual);
      break;
    }
    case ImageType_JPEG:
    {
      qgb = new QGroupBox(tr("JPEG quality"), this);
      mpQualityHBox = new QHBoxLayout(qgb);
      mpQualityHBox->setSpacing(6);
      mpQualityHBox->addWidget(new QLabel(tr("low")));
      mpQualitySlider = new QSlider(Qt::Horizontal);
      mpQualitySlider->setMinimum(0);
      mpQualitySlider->setMaximum(100);
      mpQualitySlider->setSingleStep(10);
      mpQualitySlider->setPageStep(80);
      mpQualityHBox->addWidget(new QLabel(tr("high")));
      mpQualityLabel = new QLabel("");
      mpQualityHBox->addWidget(mpQualityLabel);
      mpQualityLabel->setText("80");
      mQuality = 80;
      mpQualitySlider->setMinimumWidth(80);
    	connect(mpQualitySlider,SIGNAL(valueChanged(int)),
              this,SLOT(slotQualityChanged(int)));
      qual = xmlConfig->intValue("QUALITY_JPEG_QUALITY");
      mpQualitySlider->setValue(qual);
      slotQualityChanged(qual);
      break;
    }
#if 0
    case ImageType_TIFF8BIT:
    {
      qgb = new QGroupBox(1,Qt::Horizontal,
                          tr("TIFF compression/quality"),this);
      QHBoxLayout* hb1 = new QHBoxLayout(qgb);
      hb1->setSpacing(6);
      new QLabel(tr("Compression type:"),hb1);
      mpTiff8BitCombo = new QComboBox(false,hb1);
      mpTiff8BitCombo->insertItem(tr("none"),0);
      mpTiff8BitCombo->insertItem(tr("JPEG DCT"),1);
      mpTiff8BitCombo->insertItem(tr("packed bits"),2);
      mpQualityHBox = new QHBoxLayout(qgb);
      mpQualityHBox->setSpacing(6);
    	new QLabel(tr("low"),mpQualityHBox);
      mpQualitySlider = new QSlider(0,100,10,80,Qt::Horizontal,
                                  mpQualityHBox,0);
    	new QLabel(tr("high"),mpQualityHBox);
      mpQualityLabel = new QLabel("",mpQualityHBox);
      mpQualityLabel->setText("80");
      mQuality = 80;
      mpQualityHBox->setEnabled(false);
      connect(mpTiff8BitCombo,SIGNAL(activated(int)),
              this,SLOT(slotEnableQuality(int)));
      mpQualitySlider->setMinimumWidth(80);
    	connect(mpQualitySlider,SIGNAL(valueChanged(int)),
              this,SLOT(slotQualityChanged(int)));
      type = xmlConfig->intValue("QUALITY_TIFF_8BIT_MODE");
      mpTiff8BitCombo->setCurrentItem(type);
      slotEnableQuality(type);
      qual = xmlConfig->intValue("QUALITY_TIFF_JPEG_QUALITY");
      mpQualitySlider->setValue(qual);
      slotQualityChanged(qual);
      break;
    }
    case ImageType_TIFFLINEART:
    {
      qgb = new QGroupBox(1,Qt::Horizontal,
                          tr("TIFF compression"),this);
      QHBoxLayout* hb1 = new QHBoxLayout(qgb);
      hb1->setSpacing(6);
      new QLabel(tr("Compression type:"),hb1);
      mpTiffLineartCombo = new QComboBox(false,hb1);
      mpTiffLineartCombo->insertItem(tr("none"),0);
      mpTiffLineartCombo->insertItem(tr("packed bits"),1);
      mpTiffLineartCombo->insertItem(tr("CCITT 1D Huffman"),2);
      mpTiffLineartCombo->insertItem(tr("CCITT group3 fax"),3);
      mpTiffLineartCombo->insertItem(tr("CCITT group4 fax"),4);
      connect(mpTiffLineartCombo,SIGNAL(activated(int)),
              this,SLOT(slotEnableQuality(int)));
      type = xmlConfig->intValue("QUALITY_TIFF_LINEART_MODE");
      mpTiffLineartCombo->setCurrentItem(type);
      slotEnableQuality(type);
      break;
    }
#endif
    default:;
  }

	QPushButton* button1 = new QPushButton(tr("OK"),this);
  button1->setDefault(true);

  mainlayout->addWidget(qgb,0,0,0,2);
  mainlayout->addWidget(button1,1,1);
  mainlayout->setMargin(4);
	mainlayout->setSpacing(3);

	connect(button1,SIGNAL(clicked()),this,SLOT(accept()));

  mainlayout->activate();
}
/**  */
int QQualityDialog::quality()
{
  if((mImageType == ImageType_TIFF8BIT) || (mImageType == ImageType_TIFFLINEART))
  {
    if(mCompressionType != "COMPRESSION_JPEG") return -1;
  }
	return mQuality;
}
/**  */
void QQualityDialog::slotQualityChanged(int value)
{
  switch(mImageType)
  {
    case ImageType_PNG:
    {
      //the quality value has to be in in a range from 0 to 100
      //for internal qt reasons
      //if we use the PNG format we must map the values from 0 to 9
      //to values between 0 and 100
      //0 means a quality of 100 - lowest compression
      //9 means a quality of 0   - highest compression
      mQuality= 100-value*91/9;   // map [9,0] -> [0,100]
      xmlConfig->setIntValue("QUALITY_PNG_COMPRESSION",value);
      break;
    }
    case ImageType_JPEG:
    {
      xmlConfig->setIntValue("QUALITY_JPEG_QUALITY",value);
  	  mQuality = value;
      break;
    }
    case ImageType_TIFF8BIT:
    {
      xmlConfig->setIntValue("QUALITY_TIFF_JPEG_QUALITY",value);
  	  mQuality = value;
      break;
    }
    default:;
  }
	QString qs;
    qs.asprintf("%i",value);
	mpQualityLabel->setText(qs);
}
/**  */
void QQualityDialog::slotEnableQuality(int index)
{

  if(mImageType == ImageType_TIFF8BIT)
  {
    if(index == 1)
      mpQualityHBox->setEnabled(true);
    else
      mpQualityHBox->setEnabled(false);
   switch(index)
    {
      case 0:
        mCompressionType = "COMPRESSION_NONE";
        break;
      case 1:
        mCompressionType = "COMPRESSION_JPEG";
        break;
      case 2:
        mCompressionType = "COMPRESSION_PACKBITS";
        break;
      default:
        mCompressionType = "COMPRESSION_NONE";
    }
    xmlConfig->setIntValue("QUALITY_TIFF_8BIT_MODE",index);
  }
  else if(mImageType == ImageType_TIFFLINEART)
  {
    switch(index)
    {
      case 0:
        mCompressionType = "COMPRESSION_NONE";
        break;
      case 1:
        mCompressionType = "COMPRESSION_PACKBITS";
        break;
      case 2:
        mCompressionType = "COMPRESSION_CCITTRLE";
        break;
      case 3:
        mCompressionType = "COMPRESSION_CCITTFAX3";
        break;
      case 4:
        mCompressionType = "COMPRESSION_CCITTFAX4";
        break;
      default:
        mCompressionType = "COMPRESSION_NONE";
    }
    xmlConfig->setIntValue("QUALITY_TIFF_LINEART_MODE",index);
  }
}
/**  */
QString QQualityDialog::compressionType()
{
  return mCompressionType;
}
