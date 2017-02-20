   /***************************************************************************
                          qwordarrayoption.cpp  -  description
                             -------------------
    begin                : Mon Oct 30 2000
    copyright            : (C) 2000 by M. Herder
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

#include "qdoublespinbox.h"
#include "qwordarrayoption.h"
#include "qcurvewidget.h"
//Added by qt3to4:
#include <Q3GridLayout>

#include <math.h>
#include <qcombobox.h>
#include <qdir.h>
#include <q3filedialog.h>
#include <q3hbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <q3pointarray.h>
#include <qpushbutton.h>
#include <qpixmap.h>
#include <qvalidator.h>
#include <qmatrix.h>
#include <sane/saneopts.h>

QWordArrayOption::QWordArrayOption(QString title,QWidget *parent,
                                   SANE_Value_Type type,const char *name )
                 :QSaneOption(title,parent,name)
{
  mOldGamma = -1.0;//invalid gamma value
  mValueType = type;
  initWidget();
  createCurveWidget();
}
QWordArrayOption::~QWordArrayOption(){
}
/**  */
void QWordArrayOption::slotShowOption()
{
  if(mpArrayWidget->isMinimized())
    mpArrayWidget->hide();
  if(mpArrayWidget->isHidden())
    mpArrayWidget->show();
  else
    mpArrayWidget->raise();
  mpCurveCombo->setFocus();
}
/**  */
void QWordArrayOption::initWidget()
{
	Q3GridLayout* qgl = new Q3GridLayout(this,1,3);
  qgl->setSpacing(4);
  qgl->setMargin(4);
  mpTitleLabel = new QLabel(optionTitle(),this);
	mpShowButton = new QPushButton(tr("Adjust..."),this);
  connect(mpShowButton,SIGNAL(clicked()),this,SLOT(slotShowOption()));
//create pixmap
  assignPixmap();
	qgl->addWidget(pixmapWidget(),0,0);
	qgl->addWidget(mpTitleLabel,0,1);
	qgl->addWidget(mpShowButton,0,2);
	qgl->setColStretch(1,1);
	qgl->activate();
}
/**  */
QVector<SANE_Word> QWordArrayOption::getValue()
{
  return mDataArray;
}
void QWordArrayOption::setRange(int min, int max)
{
  mMaxVal = max;
  mMinVal = min;
}
/**  */
void QWordArrayOption::setQuant(int min)
{
  mQuant = min;
}
/**  */
void QWordArrayOption::setValue(QVector<SANE_Word> array)
{
  mDataArray.resize(0);
  mDataArray = QVector<SANE_Word> (array);  // copy it
  setCurve();
}
/**  */
void QWordArrayOption::setCurve()
{
//Here we do the following:
//- copy the data in a QPointArray
//- map the QPointArray to 256 * 256
//- create a QPointArray with a size of 256
  QMatrix matrix;
  QMatrix inv_matrix;
  double m11;
  double m22;
  int z;
  int x;
  int i;

  qDebug("mDataArray.size(): %u",mDataArray.size());
  Q3PointArray qpa(mDataArray.size());
  qDebug("qpa.size(): %u",qpa.size());
  Q3PointArray qpa2;

  for(z=0;z<qpa.size();z++)
    qpa.setPoint(z,z,mDataArray[z]);
  m11 = 256.0/double(mDataArray.size());
  m22 = 256.0/double(mMaxVal-mMinVal);

  matrix.setMatrix(m11,0.0,0.0,m22,0.0,0.0);
  inv_matrix = matrix.invert();
  qpa2.resize(256);
  qDebug("qpa2.size(): %u",qpa2.size());

  x=0;
  for(i=0;i<256;i++)
  {
    int mx,my;
    inv_matrix.map(i,x,&mx,&my);
    if(mx < 0)
      mx = 0;
    if(mx > (int(qpa.size()) - 1))
      mx = int(qpa.size()) - 1;
    matrix.map(mx,qpa.point(mx).y(),&x,&my);
    if(my < 0)
      my = 0;
    if(my > 255)
      my = 255;
    qpa2.setPoint(i,i,my);
  }
  mOldGamma = -1.0;
  mpCurveWidget->setDataArray(qpa2);
}
/**  */
void QWordArrayOption::calcDataArray()
{
  double topval;
  double val;
  double s;
  int gval;
  int i;
  QString qs;

//Here we do the following:
//- map the data from a 256*256 QmPointArray
//to a QVector
  QMatrix matrix;
  double m11;
  double m22;
  int z;

  if(mpCurveCombo->currentItem() == 0)//gamma curve
  {
    if(mValueType == SANE_TYPE_INT)
      topval = double(mMaxVal);
    else
      topval = SANE_UNFIX(mMaxVal);
    s = topval/pow(topval,1.0/mGamma);
    for(i=0;i<mDataArray.size();i++)
    {
      val = double(i)*topval/double(mDataArray.size());
      if(val>topval) val = topval;
      if(mValueType == SANE_TYPE_INT)
        gval = int(pow(val,1.0/mGamma)*s);
      else
        gval = SANE_FIX(pow(val,1.0/mGamma)*s);
      mDataArray[i] = gval;
    }
  }
  else
  {
  //get the actual values stored in QWordArrays member
  //mPointArray
    QPolygon qpa;
    qpa = QPolygon(mpCurveWidget->pointArray());

    m11 = double(mDataArray.size())/255.0;
    m22 = double(mMaxVal-mMinVal)/255.0;

    matrix.setMatrix(m11,0.0,0.0,m22,0.0,0.0);
    //map to the real values
    qpa = matrix.map(qpa);

    for(i=0;i<255;i++)
    {
      for(z=qpa.point(i).x();z<qpa.point(i+1).x();z++)
      {
        mDataArray[z] = qpa.point(i).y();
      }
    }
  }
}
/**  */
void QWordArrayOption::createCurveWidget()
{
  mpArrayWidget = new QWidget(this,"",Qt::WType_TopLevel | Qt::WStyle_Title |
                                    Qt::WStyle_DialogBorder |
                                    Qt::WStyle_SysMenu | Qt::WStyle_Customize);
  mpArrayWidget->setCaption(optionTitle());
  Q3GridLayout* mainlayout = new Q3GridLayout(mpArrayWidget,4,2);
  QLabel* label = new QLabel(mpArrayWidget);
  label->setText(mpTitleLabel->text());
  mpCurveWidget = new QCurveWidget(mpArrayWidget);
  mpCurveWidget->setFixedHeight(266);
  mpCurveWidget->setFixedWidth(266);
  mpCurveWidget->setBackgroundColor(QColor(255,255,255));

  Q3HBox* qhb1 = new Q3HBox(mpArrayWidget);
  new QLabel(tr("Curve type:"),qhb1);
  mpCurveCombo = new QComboBox(qhb1);
  mpCurveCombo->insertItem(tr("Gamma"));
  mpCurveCombo->insertItem(tr("Free"));
  mpCurveCombo->insertItem(tr("Line segments"));
  mpCurveCombo->insertItem(tr("Interpolated"));

  Q3HBox* qhb2 = new Q3HBox(mpArrayWidget);
  new QLabel(tr("Gamma:"),qhb2);
  mpGammaSpin = new QDouble100SpinBox(qhb2);
  QDoubleValidator* vali = new QDoubleValidator(0.01,4.0,2,mpGammaSpin);
#warning "need to port this"
//  mpGammaSpin->setValidator(vali);
  mpGammaSpin->setMaxValue(400);
  mpGammaSpin->setMinValue(1);
  mpGammaSpin->setValue(100);
  mGamma = 1.0;

  Q3HBox* qhb3 = new Q3HBox(mpArrayWidget);
  mpSetButton = new QPushButton(tr("&Set"),qhb3);
  mpResetButton = new QPushButton(tr("&Reset"),qhb3);
  mpCloseButton = new QPushButton(tr("&Close"),qhb3);

  mainlayout->setSpacing(5);
  mainlayout->setMargin(5);
  mainlayout->addWidget(label,0,0);
  mainlayout->addWidget(mpCurveWidget,1,0);
  mainlayout->addWidget(qhb1,2,0);
  mainlayout->addWidget(qhb2,3,0);
  mainlayout->addWidget(qhb3,4,0);
  mainlayout->activate();
  mpArrayWidget->setFixedSize(mpArrayWidget->sizeHint());
  connect(mpCurveCombo,SIGNAL(activated(int)),
          mpCurveWidget,SLOT(slotChangeCurveType(int)));
  connect(mpCurveCombo,SIGNAL(activated(int)),
          this,SLOT(slotCurveCombo(int)));
  connect(mpGammaSpin,SIGNAL(valueChanged(int)),
          this,SLOT(slotGammaValue(int)));
  connect(mpCloseButton,SIGNAL(clicked()),
          mpArrayWidget,SLOT(close()));
  connect(mpResetButton,SIGNAL(clicked()),this,SLOT(slotReset()));
  connect(mpSetButton,SIGNAL(clicked()),this,SLOT(slotSet()));
  mpCurveCombo->setCurrentItem(1);
  mpCurveWidget->slotChangeCurveType(1);
  mpGammaSpin->setEnabled(false);
}
/**  */
void QWordArrayOption::slotReset()
{
  mpCurveWidget->reset();
  if(mOldGamma > -1.0)//gamma
  {
    mpCurveCombo->setCurrentItem(0);
    slotCurveCombo(0);
    mGamma = mOldGamma;
    mpGammaSpin->setValue(int(mGamma*100.0));
  }
  else
  {
    mpCurveCombo->setCurrentItem(1);
    slotCurveCombo(1);
  }
}
/**  */
void QWordArrayOption::slotSet()
{
  mpCurveWidget->set();
  calcDataArray();
  if(mpCurveCombo->currentItem() == 0)//gamma curve
  {
    mOldGamma = mGamma;
  }
  else
  {
    mOldGamma = -1.0;//invalid gamma
  }
  slotEmitOptionChanged();
}
Q3PointArray QWordArrayOption::pointArray()
{
  return mPointArray;
}/**  */
void QWordArrayOption::closeCurveWidget()
{
  mpArrayWidget->close();
}
/**  */
void QWordArrayOption::slotGammaValue(int value)
{
  mGamma = double(value)/100.0;
  mpCurveWidget->setGamma(mGamma);
}
/**  */
void QWordArrayOption::slotCurveCombo(int index)
{
  if(index == 0)
  {
    mpGammaSpin->setEnabled(true);
    mpGammaSpin->setFocus();
    mpGammaSpin->selectAll();
  }
  else
  {
    mpGammaSpin->setEnabled(false);
  }
}
