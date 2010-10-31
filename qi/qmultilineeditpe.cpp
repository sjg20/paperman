/***************************************************************************
                          qmultilineeditpe.cpp  -  description
                             -------------------
    begin                : Sat Feb 10 2001
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

#include "resource.h"

#include "qdoublespinbox.h"
#include "qmultilineeditpe.h"
#include "quiteinsanenamespace.h"
#include "qxmlconfig.h"

#include <qapplication.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qdragobject.h>
#include <qfile.h>
#include <qfontdialog.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>

#include <qpaintdevicemetrics.h>
#include <qpainter.h>
#ifdef KDEAPP
#include <kprinter.h>
#else
#include <qprinter.h>
#endif
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qsizepolicy.h>
#include <qtextstream.h>
#include <qtoolbutton.h>
#include <qwhatsthis.h>


QTextEditorSetup::QTextEditorSetup(QMultiLineEditPE* parent,
                                   const char* name,bool modal,WFlags f)
                 :QDialog(parent,name,modal,f)
{
  setCaption(tr("Editor options"));
  mFont = ((QMultiLineEditPE*)parent)->font();
  initDlg();
  createWhatsThisHelp();
}
QTextEditorSetup::~QTextEditorSetup()
{
}
/**Initialize the dialog.  */
void QTextEditorSetup::initDlg()
{
//the main layout
	QGridLayout* mainlayout = new QGridLayout(this,5,2);
//create mpWhatsThisButton in a HBox
  QHBox* qhb1 = new QHBox(this);
  QWidget* dummy = new QWidget(qhb1);
  mpWhatsThisButton = QWhatsThis::whatsThisButton(qhb1);
	mpWhatsThisButton->setAutoRaise(FALSE);	
  qhb1->setStretchFactor(dummy,1);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    mpWhatsThisButton->hide();

///////////////////////
//margins
///////////////////////
  QGroupBox* qgb1 = new QGroupBox(tr("Printer margins"),this);
  QGridLayout* sublayout1 = new QGridLayout(qgb1,4,2);
  QLabel* leftlabel = new QLabel(tr("Left"),qgb1);
  mpDoubleSpinLeft = new QDoubleSpinBox(qgb1);
  mpDoubleSpinLeft->setMaxValue(200000);
  QLabel* toplabel = new QLabel(tr("Top"),qgb1);
  mpDoubleSpinTop = new QDoubleSpinBox(qgb1);
  mpDoubleSpinTop->setMaxValue(200000);
  QLabel* rightlabel = new QLabel(tr("Right"),qgb1);
  mpDoubleSpinRight = new QDoubleSpinBox(qgb1);
  mpDoubleSpinRight->setMaxValue(200000);
  QLabel* bottomlabel = new QLabel(tr("Bottom"),qgb1);
  mpDoubleSpinBottom = new QDoubleSpinBox(qgb1);
  mpDoubleSpinBottom->setMaxValue(200000);

  sublayout1->addWidget(leftlabel,0,0);
  sublayout1->addWidget(toplabel,1,0);
  sublayout1->addWidget(rightlabel,2,0);
  sublayout1->addWidget(bottomlabel,3,0);

  sublayout1->addWidget(mpDoubleSpinLeft,0,1);
  sublayout1->addWidget(mpDoubleSpinTop,1,1);
  sublayout1->addWidget(mpDoubleSpinRight,2,1);
  sublayout1->addWidget(mpDoubleSpinBottom,3,1);
  sublayout1->setMargin(15);
  sublayout1->setSpacing(5);
///////////////////////
//print mode
///////////////////////
  QGroupBox* qgb2 = new QGroupBox(tr("Print mode"),this);
  QGridLayout* sublayout2 = new QGridLayout(qgb2,3,1);
  mpRadioKeepFormat = new QRadioButton(tr("&Keep format"),qgb2);
  mpRadioWordWrap = new QRadioButton(tr("&Automatic word wrap"),qgb2);
#ifdef USE_QT3
  mpRadioKeepFormat->hide();
  mpRadioWordWrap->hide();
#endif
  mpCheckSelected = new QCheckBox(tr("&Print selected text only"),qgb2);

  sublayout2->addWidget(mpRadioKeepFormat,0,0);
  sublayout2->addWidget(mpRadioWordWrap,1,0);
  sublayout2->addWidget(mpCheckSelected,2,0);

  sublayout2->setMargin(15);
  sublayout2->setSpacing(5);
  QButtonGroup* bg = new QButtonGroup(this);
  bg->hide();
  bg->insert(mpRadioKeepFormat);
  bg->insert(mpRadioWordWrap);

  mpRadioKeepFormat->setChecked(TRUE);
///////////////////////
//font
///////////////////////
  QGroupBox* qgb3 = new QGroupBox(tr("Font"),this);
  QGridLayout* sublayout3 = new QGridLayout(qgb3,2,2);
  QLabel* fontlabel = new QLabel(tr("Current font:"),qgb3);
	mpFontButton = new QPushButton(tr("Select &font..."),qgb3);
  mpLineEditFont = new QLineEdit(qgb3);
  mpLineEditFont->setReadOnly(true);
//  mpLineEditFont->setSizePolicy(QSizePolicy(QSizePolicy::Maximum,
//                                         QSizePolicy::Fixed));
//  mpLineEditFont->setFrameStyle(QFrame::StyledPanel|QFrame::Sunken);
  mpLineEditFont->setText(mFont.rawName());
  sublayout3->addWidget(fontlabel,0,0);
  sublayout3->addWidget(mpFontButton,0,1);
  sublayout3->addMultiCellWidget(mpLineEditFont,1,1,0,1);
	connect(mpFontButton,SIGNAL(clicked()),this,SLOT(slotChangeFont()));

  sublayout3->setColStretch(0,1);
  sublayout3->setMargin(15);
  sublayout3->setSpacing(5);

//create buttons
  QHBox* qhb2 = new QHBox(this);
	mpCancelButton = new QPushButton(tr("&Cancel"),qhb2);
  dummy = new QWidget(qhb2);
  qhb2->setStretchFactor(dummy,1);
	mpUseButton = new QPushButton(tr("&Use"),qhb2);
  dummy = new QWidget(qhb2);
  qhb2->setStretchFactor(dummy,1);
	mpSaveButton = new QPushButton(tr("&Save"),qhb2);
  mpSaveButton->setDefault(true);
  connect(mpCancelButton,SIGNAL(clicked()),this,SLOT(reject()));
  connect(mpUseButton,SIGNAL(clicked()),this,SLOT(slotUse()));
  connect(mpSaveButton,SIGNAL(clicked()),this,SLOT(slotSave()));
//add widgets to mainlayout
  mainlayout->setColStretch(0,1);
  mainlayout->setMargin(5);
  mainlayout->setSpacing(5);
	mainlayout->addMultiCellWidget(qhb1,0,0,0,1);	
  mainlayout->addWidget(qgb1,1,0);
	mainlayout->addWidget(qgb2,1,1);	
	mainlayout->addMultiCellWidget(qgb3,2,2,0,1);	
	mainlayout->addMultiCellWidget(qhb2,4,4,0,1);	
	mainlayout->setRowStretch(3,1);
  mainlayout->activate();
  QString qs;
  QIN::MetricSystem ms;
  ms = (QIN::MetricSystem)xmlConfig->intValue("METRIC_SYSTEM");
  switch(ms)
	{
		case QIN::Millimetre:
      mMetricFactor = 1.0;
      mpDoubleSpinRight->setSuffix( " mm" );
      mpDoubleSpinLeft->setSuffix( " mm" );
      mpDoubleSpinTop->setSuffix( " mm" );
      mpDoubleSpinBottom->setSuffix( " mm" );
			break;
		case QIN::Centimetre:
      mMetricFactor = 0.1;
      mpDoubleSpinRight->setSuffix( " cm" );
      mpDoubleSpinLeft->setSuffix( " cm" );
      mpDoubleSpinTop->setSuffix( " cm" );
      mpDoubleSpinBottom->setSuffix( " cm" );
			break;
		case  QIN::Inch:
      mMetricFactor = 1.0/25.4;
      mpDoubleSpinRight->setSuffix( " inch" );
      mpDoubleSpinLeft->setSuffix( " inch" );
      mpDoubleSpinTop->setSuffix( " inch" );
      mpDoubleSpinBottom->setSuffix( " inch" );
			break;
    default:;//do nothing--shouldn't happen
	}

  mpDoubleSpinLeft->setValue(int(((QMultiLineEditPE*)parent())->mLeftMargin*
                      mMetricFactor*100.0));
  mpDoubleSpinRight->setValue(int(((QMultiLineEditPE*)parent())->mRightMargin*
                      mMetricFactor*100.0));
  mpDoubleSpinTop->setValue(int(((QMultiLineEditPE*)parent())->mTopMargin*
                      mMetricFactor*100.0));
  mpDoubleSpinBottom->setValue(int(((QMultiLineEditPE*)parent())->mBottomMargin*
                      mMetricFactor*100.0));
  if(((QMultiLineEditPE*)parent())->mPrintMode == 0)
    mpRadioKeepFormat->setChecked(true);
  else
    mpRadioWordWrap->setChecked(true);
  mpCheckSelected->setChecked(((QMultiLineEditPE*)parent())->mPrintSelected);
  setMaximumWidth(3*QApplication::desktop()->width()/4);
}
/**  */
void QTextEditorSetup::slotUse()
{
  double d;
  d = (double(mpDoubleSpinRight->value())/100.0/mMetricFactor);
  ((QMultiLineEditPE*)parent())->mRightMargin = d;
  d = (double(mpDoubleSpinLeft->value())/100.0/mMetricFactor);
  ((QMultiLineEditPE*)parent())->mLeftMargin = d;
  d = (double(mpDoubleSpinTop->value())/100.0/mMetricFactor);
  ((QMultiLineEditPE*)parent())->mTopMargin = d;
  d = (double(mpDoubleSpinBottom->value())/100.0/mMetricFactor);
  ((QMultiLineEditPE*)parent())->mBottomMargin = d;
  if(mpRadioKeepFormat->isChecked())
    ((QMultiLineEditPE*)parent())->mPrintMode = 0;
  else
    ((QMultiLineEditPE*)parent())->mPrintMode = 1;
  ((QMultiLineEditPE*)parent())->mPrintSelected
                                 = mpCheckSelected->isChecked();
  ((QMultiLineEditPE*)parent())->setFont(mFont);
  accept();
}
/**  */
void QTextEditorSetup::slotSave()
{
  QString qs;
  QFont font = ((QMultiLineEditPE*)parent())->font();
  double d;
  d = (double(mpDoubleSpinRight->value())/mMetricFactor);
  xmlConfig->setIntValue("EDITOR_RIGHT_MARGIN",int(d));
  d = (double(mpDoubleSpinLeft->value())/mMetricFactor);
  xmlConfig->setIntValue("EDITOR_LEFT_MARGIN",int(d));
  d = (double(mpDoubleSpinTop->value())/mMetricFactor);
  xmlConfig->setIntValue("EDITOR_TOP_MARGIN",int(d));
  d = (double(mpDoubleSpinBottom->value())/mMetricFactor);
  xmlConfig->setIntValue("EDITOR_BOTTOM_MARGIN",int(d));
  xmlConfig->setStringValue("EDITOR_FONT_FAMILY",font.family());
  xmlConfig->setIntValue("EDITOR_FONT_POINTSIZE",font.pointSize());
  xmlConfig->setIntValue("EDITOR_FONT_WEIGHT",font.weight());
  xmlConfig->setBoolValue("EDITOR_FONT_ITALIC",font.italic());
#ifndef USE_QT3
  xmlConfig->setIntValue("EDITOR_FONT_CHARSET",int(font.charSet()));
  if(mpRadioKeepFormat->isChecked())
    xmlConfig->setIntValue("EDITOR_PRINT_MODE",0);
  else
    xmlConfig->setIntValue("EDITOR_PRINT_MODE",1);
#endif
  xmlConfig->setBoolValue("EDITOR_PRINT_SELECTED",mpCheckSelected->isChecked());
  slotUse();
}
/**  */
void QTextEditorSetup::slotChangeFont()
{
  QFont f;
  bool ok;
  f = QFontDialog::getFont(&ok,font(), this );
  if(ok)
  {
    if(f != mFont)
    {
      mFont = f;
      mpLineEditFont->setText(mFont.rawName());
    }
  }
}

QMultiLineEditPE::QMultiLineEditPE(QWidget * parent, const char * name)
                 :QMultiLineEdit(parent,name)
{
  mLeftMargin = 10.0;
  mTopMargin = 10.0;
  mRightMargin = 10.0;
  mBottomMargin = 15.0;
  mPrintMode = 0;
//try to restore last font
  QFont font;
  QString qs;
#ifdef USE_QT3
  setWordWrap(QTextEdit::NoWrap);
#endif
  if(qs != "") font.setFamily(xmlConfig->stringValue("EDITOR_FONT_FAMILY"));
  {
    if(xmlConfig->intValue("EDITOR_FONT_POINTSIZE") > 0)
      font.setPointSize(xmlConfig->intValue("EDITOR_FONT_POINTSIZE"));
    if(xmlConfig->intValue("EDITOR_FONT_WEIGHT") > 0)
      font.setWeight(xmlConfig->intValue("EDITOR_FONT_WEIGHT"));
    font.setItalic(xmlConfig->boolValue("EDITOR_FONT_ITALIC"));
#ifndef USE_QT3
    if(xmlConfig->intValue("EDITOR_FONT_CHARSET") > -1)
      font.setCharSet((QFont::CharSet)xmlConfig->intValue("EDITOR_FONT_CHARSET"));
#endif
    if(font.exactMatch()) setFont(font);
  }
  mPrintSelected = xmlConfig->boolValue("EDITOR_PRINT_SELECTED");
#ifndef USE_QT3
  mPrintMode = xmlConfig->intValue("EDITOR_PRINT_MODE");
#else
  mPrintMode = 0;
#endif
  mLeftMargin = double(xmlConfig->intValue("EDITOR_LEFT_MARGIN"))/100.0;
  mRightMargin = double(xmlConfig->intValue("EDITOR_RIGHT_MARGIN"))/100.0;
  mTopMargin = double(xmlConfig->intValue("EDITOR_TOP_MARGIN"))/100.0;
  mBottomMargin = double(xmlConfig->intValue("EDITOR_BOTTOM_MARGIN"))/100.0;
}
QMultiLineEditPE::~QMultiLineEditPE()
{
}
/**Print the text. Note that there are two printmodes:
   1. Keep format
      This means, that the output is scaled down if neccessary.
   2. Auto word wrap
      This means, that linebreaks are inserted where neccessary.
   The default mode is mode 1. The mode can be changed through the setup
   dialog.
*/
bool QMultiLineEditPE::slotPrint()
{
  QMultiLineEdit dummyle;
  dummyle.setFont(font());
  int rightm,leftm,topm,bottomm;
  int pagenum;
  int pwidth;//printable width
  double sf; //scale factor

  leftm = int(double(mLeftMargin)*72.0/25.4);
  topm = int(double(mTopMargin)*72.0/25.4);
  rightm = int(double(mRightMargin)*72.0/25.4);
  bottomm = int(double(mBottomMargin)*72.0/25.4);

  //no scaling
  sf = 1.0;

#ifdef KDEAPP
  KPrinter printer;
#else
  QPrinter printer;
#endif
  QPainter painter;
  printer.setFullPage(true);
  if(!printer.setup()) return false;
  pagenum = printer.numCopies();
  painter.begin(&printer);
  QPaintDeviceMetrics qpdm(&printer);
  painter.setFont(dummyle.font());
//auto wrap mode
//The dummy QLineEdit does the wrapping for us
  if(mPrintMode == 1)
  {
#ifndef USE_QT3
    dummyle.setWordWrap(QMultiLineEdit::FixedPixelWidth);
    dummyle.setWrapColumnOrWidth(qpdm.width()-leftm-rightm);
    if(mPrintSelected && hasMarkedText())
      dummyle.setText(markedText());
    else
      dummyle.setText(text());
    painter.setFont(dummyle.font());
#else
    return false;
#endif
  }
  else
  {
    //We have to keep the format; first of all we check whether we
    //have to scale the text down.
    //This is neccessary if the width of the longest line is
    //greater than the printable width.
#ifdef USE_QT3
    dummyle.setWordWrap(QTextEdit::NoWrap);
#endif
    if(mPrintSelected && hasMarkedText())
      dummyle.setText(markedText());
    else
      dummyle.setText(text());
    //the printable width
    pwidth = qpdm.width()-leftm-rightm;
#ifdef USE_QT3
    if(lineWidthMax(&dummyle) > pwidth)
    {//we have to scale
      sf = double(dummyle.contentsWidth())/double(pwidth);
#else
    if(dummyle.maxLineWidth() > pwidth)
    {//we have to scale
      sf = double(dummyle.maxLineWidth())/double(pwidth);
#endif
      painter.setWindow(0,0,int(sf*double(qpdm.width())),
                            int(sf*double(qpdm.height())));
    }
  }
  QFontMetrics fm(dummyle.font());
  int ypos,xpos;
  int c;
  ypos = int(double(topm)*sf);
  xpos = int(double(leftm)*sf);
  painter.setPen(Qt::black);
  for(int p=0;p<pagenum;p++)
  {
    for(c=0;c<dummyle.numLines();c++)
    {
      if(ypos>int(sf*double(qpdm.height()-bottomm)))
      {
        printer.newPage();
        ypos = int(sf*double(topm));
      }
      painter.drawText(xpos,ypos,dummyle.textLine(c));
      ypos += fm.lineSpacing();
    }
    if(p<pagenum-1) printer.newPage();
  }
  painter.end();
  return true;
}
/** Set the margins used for printing. The unit is millimetre.
 */
void QMultiLineEditPE::setMarginsMM(double left,double top,double right,double bottom)
{
  mLeftMargin = left;
  mTopMargin = top;
  mRightMargin = right;
  mBottomMargin = bottom;
}
/** Set the left margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
void QMultiLineEditPE::setLeftMarginMM(double left)
{
  mLeftMargin = left;
}
/** Set the right margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
void QMultiLineEditPE::setRightMarginMM(double right)
{
  mRightMargin = right;
}
/** Set the top margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
void QMultiLineEditPE::setTopMarginMM(double top)
{
  mTopMargin = top;
}
/** Set the bottom margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
void QMultiLineEditPE::setBottomMarginMM(double bottom)
{
  mBottomMargin = bottom;
}
/**  */
void QMultiLineEditPE::slotSetup()
{
  QTextEditorSetup EditorSetup(this,0,true);
  EditorSetup.exec();
}
/**  */
void QTextEditorSetup::createWhatsThisHelp()
{
//check selected
  QWhatsThis::add(this,tr("Use this dialog to setup the editor."));
//check selected
  QWhatsThis::add(mpCheckSelected,tr("Activate this checkbox, if the "
															"selected (highlighted) text should be "
															"printed only. If no text is highlighted, "
                              "the contents is printed instead."));
//font label
  QWhatsThis::add(mpLineEditFont,tr("Displays the name of the currently "
															"selected font."));
//bottom margin
  QWhatsThis::add(mpDoubleSpinBottom,tr("Use this spinbox to adjust the bottom "
															"margin of the printed text. Please note "
                              "that normal printers can not print on the "
                              "full paper size. It is mandatory to set "
                              "a margin size according to the capabilities "
                              "of your printer." ));
//left margin
  QWhatsThis::add(mpDoubleSpinLeft,tr("Use this spinbox to adjust the left "
															"margin of the printed text. Please note "
                              "that normal printers can not print on the "
                              "full paper size. It is mandatory to set "
                              "a margin size according to the capabilities "
                              "of your printer." ));
//top margin
  QWhatsThis::add(mpDoubleSpinTop,tr("Use this spinbox to adjust the top "
															"margin of the printed text. Please note "
                              "that normal printers can not print on the "
                              "full paper size. It is mandatory to set "
                              "a margin size according to the capabilities "
                              "of your printer." ));
//right margin
  QWhatsThis::add(mpDoubleSpinRight,tr("Use this spinbox to adjust the right "
															"margin of the printed text. Please note "
                              "that normal printers can not print on the "
                              "full paper size. It is mandatory to set "
                              "a margin size according to the capabilities "
                              "of your printer." ));
//keep format
  QWhatsThis::add(mpRadioKeepFormat,tr("Activate this radiobutton to keep "
															"the format of the text when printing. "
                              "This means, that no linebreaks are added "
                              "and that the text is scaled to fit the "
                              "page width when necessary."));
//word wrap
  QWhatsThis::add(mpRadioWordWrap,tr("Activate this radiobutton for "
															"automatic word wrap when printing. "
                              "This means, that linebreaks are inserted "
                              "where necessary to make the text fit the "
                              "page width."));
//font button
  QWhatsThis::add(mpFontButton,tr("Click this button to open a font dialog. "
															"Please note, that the selected font is not "
                              "necessarily available for printing."));

//cancel button
  QWhatsThis::add(mpCancelButton,tr("Closes the dialog and discards "
														      "all changes."));
//use button
  QWhatsThis::add(mpUseButton,tr("Use the settings, but don't save them "
														   "to the configuration file."));
//save button
  QWhatsThis::add(mpSaveButton,tr("Use the settings and save them to the "
														    "configuration file."));
}
/** No descriptions */
void QMultiLineEditPE::dropEvent(QDropEvent* e)
{
  QStringList list;
  if(isReadOnly())
    return;
  if(QUriDrag::decodeLocalFiles(e,list))
  {
    for(QStringList::Iterator it=list.begin();it!=list.end();++it)
    {
      QFile f(*it);
      if(f.open(IO_ReadOnly))
      {
        QTextStream t( &f );
        QString s;
        while(!t.eof())
          s += t.readLine() + "\n";
        append(s);
        f.close();
      }
    }
  }
  else
    QMultiLineEdit::dropEvent(e);
}
/** No descriptions */
void QMultiLineEditPE::dragMoveEvent(QDragMoveEvent* e)
{
  if(isReadOnly())
    return;
  if(QUriDrag::canDecode(e))
  {
    e->accept();
  }
  else
  {
    QMultiLineEdit::dragMoveEvent(e);
  }
}
/** No descriptions */
int QMultiLineEditPE::lineWidthMax(QMultiLineEdit* le)
{
#ifndef USE_QT3
  return -1;
#else
  int w = -1;
  for(int i=0;i<le->paragraphs();i++)
  {
    QRect r = le->paragraphRect(i);
    if(r.width() > w)
      w = r.width();
  }
  return w;
#endif
}

