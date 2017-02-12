/***************************************************************************
                          previewwidget.cpp  -  description
                             -------------------
    begin                : Fri Jun 23 2000
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

#include <QTextStream>

#include "err.h"
#include "checklistitemext.h"
#include "imagebuffer.h"
#include "imagedetection.h"
#include "imageiosupporter.h"
#include "previewwidget.h"
#include "previewupdatewidget.h"
#include "qextensionwidget.h"
#include "qscandialog.h"
#include "quiteinsane.h"
#include "qxmlconfig.h"
//Added by qt3to4:
#include <Q3GridLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include "ruler.h"
#include "scanareacanvas.h"
#include "scanarea.h"
#include "scanareatemplate.h"
#include "images/add.xpm"
#include "images/autoselection.xpm"
#include "images/autoselection_setup.xpm"
#include "images/delete_bookmarks.xpm"
#include "images/multiselection.xpm"
#include "images/sub.xpm"
#include "images/zoom.xpm"
#include "images/zoom_undo.xpm"
#include "images/zoom_redo.xpm"
#include "images/zoom_off.xpm"

#include <q3groupbox.h>
#include <qapplication.h>
#include <qcolor.h>
#include <qcolordialog.h>
#include <qcombobox.h>
#include <qdom.h>
#include <qfile.h>
#include <q3hbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <q3listview.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <q3picture.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <q3popupmenu.h>
#include <qpushbutton.h>
#include <qrect.h>
#include <qsizepolicy.h>
#include <qstring.h>
#include <q3textstream.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <q3valuelist.h>
#include <q3whatsthis.h>
#include <q3widgetstack.h>

#include <math.h>
#include <unistd.h>

PreviewWidget::PreviewWidget(QWidget *parent, const char *name,Qt::WFlags f)
              :QWidget(parent,name,f)
{
  setCaption(tr("MaxView - Preview"));
  mImageVectorIndex = -1;
  mTemplateVectorIndex = -1;
  mImageVector.setAutoDelete(true);
//  mTemplateVector.setAutoDelete(true);
  mpCurrentItem = 0;
  mMinRangeX = -1.0;
  mMaxRangeX = -1.0;
  mMinRangeY = -1.0;
  mMaxRangeY = -1.0;
  mWidth = mHeight = -1;
  mCancelled = false;

#define SIZES  33

#define SIZE_A4 6   // ID of A4 size

   static PredefinedSizes predefs[SIZES] =
  {
    {tr("Full size"),-1.0,-1.0},
    {tr("User size"),-1.0,-1.0},
    {"A0 (841x1189 mm)",841.0,1189.0},
    {"A1 (594x841 mm)",594.0,841.0},
    {"A2 (420x594 mm)",420.0,594.0},
    {"A3 (297x420 mm)",297.0,420.0},
    {"A4 (210x297 mm)",210.0,297.0},
    {"A4L (297x210 mm)",297.0,210.0},
    {"A5 (148x210 mm)",148.5,210.0},
    {"A6 (105x148 mm)",105.0,148.0},
    {"A7 (74x105 mm)",74.0,105.0},
    {"A8 (52x74 mm)",52.0,74.0},
    {"A9 (37x52 mm)",37.0,52.0},
    {"B0 (1030x1456 mm)",1030.0,1456.0},
    {"B1 (728x1030 mm)",728.0,1030.0},
    {"B10 (32x45 mm)",32.0,45.0},
    {"B2 (515x728 mm)",515.0,728.0},
    {"B3 (364x515 mm)",364.0,515.0},
    {"B4 (257x364 mm)",257.0,364.0},
    {"B5 (182x257 mm)",182.0,257.0},
    {"B6 (128x182 mm)",128.0,182.0},
    {"B7 (91x128 mm)",91.0,128.0},
    {"B8 (64x91 mm)",64.0,91.0},
    {"B9 (45x64 mm)",45.0,64.0},
    {"C5E (163x229 mm)",163.0,229.0},
    {"Comm10E (105x241 mm)",105.0,241.0},
    {"DLE (110x220 mm)",110.0,220.0},
    {"Executive (7.5x10)",191.0,254.0},
    {"Folio (210x330 mm)",210.0,330.0},
    {"Ledger (432x279 mm)",432.0,279.0},
    {"Legal (8.5x14 inches)",216.0,356.0},
    {"Letter (8.5x11 inches)",216.0,279.0},
    {"Tabloid (279x432 mm)",279.0,432.0}
  };
  /**  */

  mPreDefs = predefs;

  initWidget();
  createWhatsThisHelp();
  loadTemplates();
}

PreviewWidget::~PreviewWidget()
{
  mImageVector.clear();
  mTemplateVector.clear();
}

/**  */
void PreviewWidget::initWidget()
{
//add a grid layout to this widget
  mpMainLayout = new Q3GridLayout(this,5,2);
  mpMainLayout->setSpacing(3);
//create a widget with a layout that holds size options
  mpSizeHBox = new Q3HBox(this);
  mpScanSizeLabel = new QLabel(tr("Scan size"),mpSizeHBox);
  mpComboStack = new Q3WidgetStack(mpSizeHBox);
  mpScanSizeCombo = new QComboBox(false,mpComboStack,"");

  Q3HBox* sthbox = new Q3HBox(mpComboStack);
  sthbox->setSpacing(4);

  mpAddButton = new QToolButton(sthbox);
  mpAddButton->setPixmap(QPixmap((const char **)add_xpm));

  mpSubButton = new QToolButton(sthbox);
  mpSubButton->setPixmap(QPixmap((const char **)sub_xpm));

  mpDelTemplatesButton = new QToolButton(sthbox);
  mpDelTemplatesButton->setPixmap(QPixmap((const char **)delete_bookmarks_xpm));

  mpScanTemplateCombo = new QComboBox(false,sthbox,"");
  mpScanTemplateCombo->insertItem(tr("None"));
  mpScanTemplateCombo->setCurrentItem(0);
  mpComboStack->addWidget(mpScanSizeCombo,0);
  mpComboStack->addWidget(sthbox,1);
  mpComboStack->raiseWidget(0);
  sthbox->setStretchFactor(mpScanTemplateCombo,1);

  connect(mpScanSizeCombo,SIGNAL(activated(int)),
          this,SLOT(slotSizeComboChanged(int)));
  connect(mpScanTemplateCombo,SIGNAL(activated(int)),
          this,SLOT(slotTemplateSelected(int)));
  mpSizeHBox->setStretchFactor(mpScanSizeLabel,1);
  mpSizeHBox->setStretchFactor(mpComboStack,1);

  mpMainLayout->addMultiCellWidget(mpSizeHBox,0,0,0,1);
//toolbuttons
  mpToolHBox = new Q3HBox(this);
  mpZoomHBox = new Q3HBox(mpToolHBox);
  mpZoomButton = new QToolButton(mpZoomHBox);
  mpZoomButton->setPixmap(QPixmap((const char **)zoom_xpm));
  mpZoomUndoButton = new QToolButton(mpZoomHBox);
  mpZoomUndoButton->setPixmap(QPixmap((const char **)zoom_undo_xpm));
  mpZoomRedoButton = new QToolButton(mpZoomHBox);
  mpZoomRedoButton->setPixmap(QPixmap((const char **)zoom_redo_xpm));
  mpZoomOffButton = new QToolButton(mpZoomHBox);
  mpZoomOffButton->setPixmap(QPixmap((const char **)zoom_off_xpm));
  QWidget* dummy = new QWidget(mpToolHBox);
  mpAutoSelectionButton = new QToolButton(mpToolHBox);
  mpAutoSelectionButton->setPixmap(QPixmap((const char **)autoselection_xpm));
  mpAutoSetupButton = new QToolButton(mpToolHBox);
  mpAutoSetupButton->setPixmap(QPixmap((const char **)autoselection_setup_xpm));
  mpMultiSelectionButton = new QToolButton(mpToolHBox);
  mpMultiSelectionButton->setPixmap(QPixmap((const char **)multiselection_xpm));
  mpMultiSelectionButton->setToggleButton(true);
  mpToolHBox->setStretchFactor(dummy,1);
  mpToolHBox->setSpacing(3);
  mpMainLayout->addMultiCellWidget(mpToolHBox,1,1,0,1);
  connect(mpAutoSelectionButton,SIGNAL(clicked()),
          this,SLOT(slotAutoSelection()));
  connect(mpAutoSetupButton,SIGNAL(clicked()),
          this,SLOT(slotAutoSelectionSetup()));
  connect(mpMultiSelectionButton,SIGNAL(toggled(bool)),
          this,SLOT(slotShowListView(bool)));
  connect(mpZoomButton,SIGNAL(clicked()),
          this,SLOT(slotZoom()));
  connect(mpZoomUndoButton,SIGNAL(clicked()),
          this,SLOT(slotZoomUndo()));
  connect(mpZoomRedoButton,SIGNAL(clicked()),
          this,SLOT(slotZoomRedo()));
  connect(mpZoomOffButton,SIGNAL(clicked()),
          this,SLOT(slotZoomOff()));
  connect(mpAddButton,SIGNAL(clicked()),
          this,SLOT(slotAddTemplate()));
  connect(mpSubButton,SIGNAL(clicked()),
          this,SLOT(slotDeleteTemplate()));
  connect(mpDelTemplatesButton,SIGNAL(clicked()),
          this,SLOT(slotDeleteAllTemplates()));
////////////////////////////////////////////////////////
//scan area and rulers
  //metric label
  dummy = new QWidget(this);
  Q3GridLayout* sublayout = new Q3GridLayout(dummy,2,2);
  sublayout->setColStretch(1,1);
  sublayout->setRowStretch(1,1);
  mpMetricLabel = new QLabel("mm",dummy);
  mpMetricLabel->setMinimumSize(30,30);
  //horizontal ruler
  mpHRuler = new Ruler(dummy,"",Qt::Horizontal);
  mpHRuler->setMinimumHeight(30);
  //Widgetstack for the scanarea canvas and a widget, which is used to
  //continously display the progress during a preview scan
  mpPreviewStack = new Q3WidgetStack(dummy);
  mpScanAreaWidget=new ScanAreaCanvas(mpPreviewStack);
  mpUpdateWidget=new PreviewUpdateWidget(mpPreviewStack);
  mpPreviewStack->addWidget(mpScanAreaWidget,0);
  mpPreviewStack->addWidget(mpUpdateWidget,1);
  mpPreviewStack->raiseWidget(0);
  //vertical ruler
  mpVRuler = new Ruler(dummy,"",Qt::Vertical);
  mpVRuler->setMinimumWidth(30);
  //add to mpMainLayout
  sublayout->addWidget(mpMetricLabel,0,0);
  sublayout->addWidget(mpHRuler,0,1);
  sublayout->addWidget(mpVRuler,1,0);
  sublayout->addWidget(mpPreviewStack,1,1);
  sublayout->activate();
  mpMainLayout->addWidget(dummy,2,0);
  connect(mpScanAreaWidget,SIGNAL(signalNewActiveRect(int)),
          this,SLOT(slotNewActiveRect(int)));
//listview
  mpListView = new Q3ListView(this);
  mpListView->addColumn("");
  mpListView->addColumn(tr("Type"));
  mpListView->setAllColumnsShowFocus(true);
  mpMainLayout->addWidget(mpListView,2,1);
//load/define rect colors
  unsigned int fg_colors [20] = {16777215,16777215,16776960,16761024,49344,
                                 12648384,16777215,12632319,12632064,16776960,
                                 16777215,65535,12648447,16777087,16761087,
                                 16744448,16768168,16729293,65433,16763955};
  unsigned int bg_colors [20] = {0,11141120,11163007,0,0,0,255,0,0,0,43520,0,0,
                                 11141375,0,0,0,0,0,0};
  Q3ValueList <unsigned int> fg_list_def;
  Q3ValueList <unsigned int> bg_list_def;

  for(int i=0;i<20;i++)
  {
    fg_list_def.append(fg_colors[i]);
    bg_list_def.append(bg_colors[i]);
  }
  mFgColorList = xmlConfig->uintValueList("PREVIEW_FG_COLOR",fg_list_def);
  mBgColorList = xmlConfig->uintValueList("PREVIEW_BG_COLOR",bg_list_def);
  for(int i=0;i<20;i++)
  {
    CheckListItemExt* ci = new CheckListItemExt(mpListView,"",CheckListItemExt::CheckBox);
    ci->setBgColor(mBgColorList[19-i]);
    ci->setFgColor(mFgColorList[19-i]);
    mpScanAreaWidget->setRectFgColor(i,mFgColorList[i]);
    mpScanAreaWidget->setRectBgColor(i,mBgColorList[i]);
    if(i == 19)
    {
      ci->setOn(true);
      mpListView->setSelected(ci,true);
    }
    ci->setText(1,tr("Image"));
    ci->setNumber(19 - i);
  }
  connect(mpListView,SIGNAL(rightButtonClicked(Q3ListViewItem*,const QPoint&,int)),
          this,SLOT(slotColorPopup(Q3ListViewItem*,const QPoint&,int)));
  connect(mpListView,SIGNAL(clicked(Q3ListViewItem*,const QPoint&,int)),
          this,SLOT(slotListItem(Q3ListViewItem*,const QPoint&,int)));
  mpListView->hide();
//buttons
  Q3HBox* qhb = new Q3HBox(this);
//s  mpPreviewButton = new QPushButton(tr("Sca&n preview"),qhb);
  dummy = new QWidget(qhb);
  qhb->setStretchFactor(dummy,1);
  qhb->setSpacing(5);
  mpButtonStack = new Q3WidgetStack(qhb);
  mpCloseButton = new QPushButton(tr("&Close"),mpButtonStack);
  mpCancelButton = new QPushButton(tr("&Cancel"),mpButtonStack);
  mpButtonStack->addWidget(mpCloseButton,0);
  mpButtonStack->addWidget(mpCancelButton,1);
  mpButtonStack->raiseWidget(0);
  if(mpCloseButton->sizeHint().width() < mpCancelButton->sizeHint().width())
    mpButtonStack->setMinimumWidth(mpCancelButton->sizeHint().width());
  else
    mpButtonStack->setMinimumWidth(mpCloseButton->sizeHint().width());
  mpMainLayout->addMultiCellWidget(qhb,4,4,0,1);
//connect signal
  connect(mpCloseButton,SIGNAL(clicked()),this,SIGNAL(signalHidePreview()));
  connect(mpCancelButton,SIGNAL(clicked()),this,SLOT(slotCancel()));
  connect(this,SIGNAL(signalHidePreview()),this,SLOT(hide()));

//s  connect(mpPreviewButton,SIGNAL(clicked()),this,SLOT(slotPreviewRequest()));

  mpMainLayout->setColStretch(0,1);
  mpMainLayout->setRowStretch(2,1);
  mpMainLayout->activate();
//connect signals
  connect(mpScanAreaWidget,SIGNAL(signalTlxPercent(double)),
          this,SLOT(slotTlxPercent(double)));
  connect(mpScanAreaWidget,SIGNAL(signalTlyPercent(double)),
          this,SLOT(slotTlyPercent(double)));
  connect(mpScanAreaWidget,SIGNAL(signalBrxPercent(double)),
          this,SLOT(slotBrxPercent(double)));
  connect(mpScanAreaWidget,SIGNAL(signalBryPercent(double)),
          this,SLOT(slotBryPercent(double)));
  connect(mpScanAreaWidget,SIGNAL(signalUserSetSize()),
          this,SLOT(slotUserSize()));
  if(mImageVectorIndex < 0)
  {
    QImage im(250,300,1,2,QImage::LittleEndian);
    im.setColor(1,qRgb(255,255,255));
    im.fill(1);
    addImageToQueue(&im);
    if(!im.isNull())
      mpScanAreaWidget->setImage(&im);
  }
  resize(250,300);
}
/**  */
void PreviewWidget::createWhatsThisHelp()
{
//auto-selection button
  Q3WhatsThis::add(mpAutoSelectionButton,tr("Click this button to start the "
                               "automatic image selection."));
//auto-selection setup button
  Q3WhatsThis::add(mpAutoSetupButton,tr("Click this button to adjust the parameters "
                               "for the automatic image selection."));
//scan size combo
  Q3WhatsThis::add(mpScanSizeCombo,tr("Select a scan size."));
//zoom button
  Q3WhatsThis::add(mpZoomButton,tr("Zoom in to the size of the selection rectangle."));
//zoom off button
  Q3WhatsThis::add(mpZoomOffButton,tr("Turn off the zoomfunction. The image buffer is not "
                                  "deleted."));
//zoom undo button
  Q3WhatsThis::add(mpZoomUndoButton,tr("Go back to previous zoom-level."));
//zoom redo button
  Q3WhatsThis::add(mpZoomRedoButton,tr("Go forward to next zoom-level."));
//multi selection button
  Q3WhatsThis::add(mpMultiSelectionButton,tr("Switch to multi-selection mode, which allows you to "
                              "select up to 20 scan areas and scan them in a single pass."));
//multi selection listview
  Q3WhatsThis::add(mpListView,tr("You can hide/show selection rectangles by "
                              "enabling/disabling the checkboxes. Clicking on an item with "
                              "the right mouse button opens a menu, that you can use to "
                              "change the back- or foreground color of a selection rectangle."));
//add button
  Q3WhatsThis::add(mpAddButton,tr("Click this button to add a template. "
                               "You will be prompted for a name."));
  QToolTip::add(mpAddButton,tr("Add template"));
//del button
  Q3WhatsThis::add(mpSubButton,tr("Click this button to delete the selected template. "));
  QToolTip::add(mpSubButton,tr("Delete template"));
//del all button
  Q3WhatsThis::add(mpDelTemplatesButton,tr("Click this button to delete all templates. "));
  QToolTip::add(mpDelTemplatesButton,tr("Delete all templates"));
}
/**  */
void PreviewWidget::setMetrics(QIN::MetricSystem ms,SANE_Unit unit)
{
  mSaneUnit = unit;
  if(unit == SANE_UNIT_PIXEL)
  {
    mpSizeHBox->hide();
    mpToolHBox->show();
    mpScanAreaWidget->enableSelection(true);
  }
  else if(unit == SANE_UNIT_MM)
  {
    mpSizeHBox->show();
    mpToolHBox->show();
  	slotChangeMetricSystem(ms);
    createPredefinedSizes();
    mpScanAreaWidget->enableSelection(true);
  }
  else
  {
    mpSizeHBox->hide();
    mpToolHBox->hide();
    mpMultiSelectionButton->setOn(false);
    slotZoomOff();
    mpScanAreaWidget->enableSelection(false);
  }
  redrawRulers();
}
/**  */
void PreviewWidget::loadPreviewPixmap(QString path)
{
  ImageIOSupporter iisup;
  QImage im;
  iisup.loadImage(im,path);
  addImageToQueue(&im,true);
  if(!im.isNull())
    mpScanAreaWidget->setImage(&im);
  if(mSaneUnit == SANE_UNIT_NONE)
    setAspectRatio(double(im.width())/double(im.height()));
  redrawRulers();
}


/** Set the size of the rectangle */
void PreviewWidget::setRectSize(double tlx,double tly,double brx,double bry)
{
  mpScanAreaWidget->setRectSize(tlx,tly,brx,bry);
}


/**  */
void PreviewWidget::slotSizeComboChanged(int index)
{
  ScanArea* sca;

//printf ("slotSizeComboChanged %d\n", index);
  sca = mSizeArray[index];
  emit signalPredefinedSize(sca);
}

const char *PreviewWidget::getSizeName (unsigned id)
{
  return (int)id < mSizeArray.size ()
    ? mSizeArray[id]->getName ().latin1 ()
    : NULL;
}


ScanArea *PreviewWidget::getSize (unsigned id)
{
  return (int)id < mSizeArray.size ()
    ? mSizeArray[id]
    : 0;
}


void PreviewWidget::setSize (unsigned id)
{
//  printf ("setSize: id = %d\n", id);
  mpScanSizeCombo->setCurrentItem(id);
  emit slotSizeComboChanged (id);
}


/**  */
void PreviewWidget::slotUserSize()
{
  if(!mpListView->isVisible())
  {
    if((mpScanSizeCombo->currentItem() != 1) &&
       (mpScanSizeCombo->count() > 0))
      mpScanSizeCombo->setCurrentItem(1);
  }
  else
  {
    mpScanTemplateCombo->setCurrentItem(0);
  }
}
/**  */
void PreviewWidget::slotChangeMetricSystem(QIN::MetricSystem ms)
{
  mMetricSystem = ms;
  redrawRulers();
}


/**  */
void PreviewWidget::createPredefinedSizes()
{
  // calculate sizes
  slotRangeChange ();

  mpScanSizeCombo->setCurrentItem(1);
  //load templates
}


void PreviewWidget::slotRangeChange()
{
   ScanArea* sca;
   int a;
   bool valid, valid2;

//create Array which holds the predefined sizes;
//only predefined sizes that are <= the real
//scan size are added to the list
   mpScanSizeCombo->clear();
   mSizeArray.resize(0);
//  printf ("w %1.1lf to %1.1lf = %1.1lf\n", mMinRangeX, mMaxRangeX, mMaxRangeX - mMinRangeX);
//  printf ("h %1.1lf to %1.1lf = %1.1lf\n", mMinRangeY, mMaxRangeY, mMaxRangeY - mMinRangeY);
//  printf ("page %1.1lf, %1.1lf\n", mWidth, mHeight);
   mPreDefA4 = -1;

   for(a=0;a<SIZES;a++)
   {
//      printf ("w=%1.1lf  h=%1.1lf: %s\n", mPreDefs[a].width, mPreDefs[a].height, mPreDefs[a].name.latin1 ());
    // check if this size fits within the limit
    valid = (mPreDefs[a].width <= (mMaxRangeX -mMinRangeX)) &&
       (mPreDefs[a].height <= (mMaxRangeY -mMinRangeY));

    // if we can adjust the page size, allow sizes up to this limit also
    valid2 = mWidth != -1 &&
       (mPreDefs[a].width <= mWidth && mPreDefs[a].height <= mHeight);
    if (valid || valid2)
    {
      if (mPreDefA4 == -1 && a >= SIZE_A4)
      {
         mPreDefA4 = mSizeArray.size();
//         printf ("mPreDefA4 = %d\n", mPreDefA4);
      }
      sca = new ScanArea(mPreDefs[a].name);
      mSizeArray.resize(mSizeArray.size()+1);
      mSizeArray[mSizeArray.size()-1] = sca;
      mpScanSizeCombo->insertItem(sca->getName(),-1);

      double tlx,tly,brx,bry;
      if(a == 0) //max size
      {
         tlx = tly = 0.0;
         brx = bry = 1.0;
      }
      else if(a == 1) //user size
        tlx = tly = brx = bry = -1.0;
      else
      {
        tlx = tly = 0.0;
        brx = mPreDefs[a].width/(mMaxRangeX -mMinRangeX);
        bry = mPreDefs[a].height/(mMaxRangeY -mMinRangeY);
        if (sca->getName () == "A4")
         printf ("%s: %1.15lf, %1.15lf   %1.15lf   %1.15lf\n", sca->getName ().latin1 (), brx, bry,
            mMinRangeX, mMaxRangeX);
      }
      sca = mSizeArray [mSizeArray.size() - 1];
      sca->setRange (tlx,tly,brx,bry,mPreDefs[a].width, mPreDefs[a].height);
    }
   }
}


/**  */
void PreviewWidget::changeLayout(bool toplevel)
{
  if(!layout()) return;

  if(toplevel)
  {
    layout()->setMargin(5);
    layout()->activate();
  }
  else
    layout()->setMargin(0);
}
/** */
void PreviewWidget::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  redrawRulers();
  if(!isVisible())
    return;
  if(xmlConfig->boolValue("SEPARATE_PREVIEW"))
  {
    xmlConfig->setIntValue("SCANDIALOG_STANDALONE_PREVIEW_WIDTH",width());
    xmlConfig->setIntValue("SCANDIALOG_STANDALONE_PREVIEW_HEIGHT",height());
  }
}
/**  */
void PreviewWidget::redrawRulers()
{
  if(mImageVectorIndex < 0) return;
  qApp->processEvents();
  double min_h;
  double max_h;
  double min_v;
  double max_v;

  min_h = mImageVector[mImageVectorIndex]->tlxOrig() * (mMaxRangeX -mMinRangeX);
  max_h = mImageVector[mImageVectorIndex]->brxOrig() * (mMaxRangeX -mMinRangeX);
  min_v = mImageVector[mImageVectorIndex]->tlyOrig() * (mMaxRangeY -mMinRangeY);
  max_v = mImageVector[mImageVectorIndex]->bryOrig() * (mMaxRangeY -mMinRangeY);

  int w = mpScanAreaWidget->canvas()->width();
  int h = mpScanAreaWidget->canvas()->height();;
  double f1= double(mpScanAreaWidget->width())/double(w);
  double f2= double(mpScanAreaWidget->height())/double(h);

  if(mSaneUnit == SANE_UNIT_NONE)
  {
    max_h = double(mImageVector[mImageVectorIndex]->image()->width());
    max_v = double(mImageVector[mImageVectorIndex]->image()->height());
    mpMetricLabel->setText(tr("pixel"));
    mpHRuler->setRange(0.0,max_h*f1);
    mpVRuler->setRange(0.0,max_v*f2);
    return;
  }

  max_h = (max_h - min_h) * f1 + min_h;
  max_v = (max_v - min_v) * f2 + min_v;

  if(mSaneUnit == SANE_UNIT_PIXEL)
  {
    mpMetricLabel->setText(tr("pixel"));
    mpHRuler->setRange(min_h,max_h);
    mpVRuler->setRange(min_v,max_v);
    return;
  }
  switch(mMetricSystem)
  {
    case QIN::Millimetre:
      mpMetricLabel->setText(tr("mm"));
      mpHRuler->setRange(min_h,max_h);
      mpVRuler->setRange(min_v,max_v);
      break;
    case QIN::Centimetre:
      mpMetricLabel->setText(tr("cm"));
      mpHRuler->setRange(min_h/10.0,max_h/10.0);
      mpVRuler->setRange(min_v/10.0,max_v/10.0);
      break;
    case  QIN::Inch:
      mpMetricLabel->setText(tr("inch"));
      mpHRuler->setRange(min_h/25.4,max_h/25.4);
      mpVRuler->setRange(min_v/25.4,max_v/25.4);
      break;
    default:;//do nothing--shouldn't happen
  }
}
/**  */
void PreviewWidget::slotSetTlxPercent(double pval)
{
  //only accept call if preview isn't zoomed
  if(mpListView->isVisible() || (mImageVectorIndex != 0))
    return;
  mpScanAreaWidget->setTlx(pval);
}
/**  */
void PreviewWidget::slotSetTlyPercent(double pval)
{
  //only accept call if preview isn't zoomed
  if(mpListView->isVisible() || (mImageVectorIndex != 0))
    return;
  mpScanAreaWidget->setTly(pval);
}
/**  */
void PreviewWidget::slotSetBrxPercent(double pval)
{
  //only accept call if preview isn't zoomed
  if(mpListView->isVisible() || (mImageVectorIndex != 0))
    return;
  mpScanAreaWidget->setBrx(pval);
}
/**  */
void PreviewWidget::slotSetBryPercent(double pval)
{
  //only accept call if preview isn't zoomed
  if(mpListView->isVisible() || (mImageVectorIndex != 0))
    return;
  mpScanAreaWidget->setBry(pval);
}
/** No descriptions */
void PreviewWidget::slotTlxPercent(double pval)
{
  mImageVector[mImageVectorIndex]->setTlx(pval);
  //relay signal
  //map to device range
  double d = (mImageVector[mImageVectorIndex]->brxOrig() -
              mImageVector[mImageVectorIndex]->tlxOrig()) *
              pval + mImageVector[mImageVectorIndex]->tlxOrig();
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  emit signalTlxPercent(d);
}
/**  */
void PreviewWidget::slotTlyPercent(double pval)
{
  mImageVector[mImageVectorIndex]->setTly(pval);
  //relay signal
  //map to device range
  double d = (mImageVector[mImageVectorIndex]->bryOrig() -
              mImageVector[mImageVectorIndex]->tlyOrig()) *
              pval + mImageVector[mImageVectorIndex]->tlyOrig();
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  emit signalTlyPercent(d);
}
/**  */
void PreviewWidget::slotBrxPercent(double pval)
{
  mImageVector[mImageVectorIndex]->setBrx(pval);
  //relay signal
  //map to device range
  double d = (mImageVector[mImageVectorIndex]->brxOrig() -
              mImageVector[mImageVectorIndex]->tlxOrig()) *
              pval + mImageVector[mImageVectorIndex]->tlxOrig();
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  emit signalBrxPercent(d);
}
/**  */
void PreviewWidget::slotBryPercent(double pval)
{
  mImageVector[mImageVectorIndex]->setBry(pval);
  //relay signal
  //map to device range
  double d = (mImageVector[mImageVectorIndex]->bryOrig() -
              mImageVector[mImageVectorIndex]->tlyOrig()) *
              pval + mImageVector[mImageVectorIndex]->tlyOrig();
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  emit signalBryPercent(d);
}
/** No descriptions */
void PreviewWidget::slotColorPopup(Q3ListViewItem* li,const QPoint& p,int i)
{
  i = i;  //s unused
  if(!li)
    return;
  QRgb rgb;
  CheckListItemExt* ci;
  ci = (CheckListItemExt*) li;
  Q3PopupMenu pop(0);
  pop.insertItem(tr("Change background color..."),0);
  pop.insertItem(tr("Change foreground color..."),1);
  int n = pop.exec(p);
  if(n == 0)
  {
    rgb = QColorDialog::getRgba(ci->bgColor(),0,this);
    ci->setBgColor(rgb);
    mpScanAreaWidget->setRectBgColor(ci->number(),rgb);
    mBgColorList[ci->number()] = rgb;
    xmlConfig->setUintValueList("PREVIEW_BG_COLOR",mBgColorList);
  }
  else if(n == 1)
  {
    rgb = QColorDialog::getRgba(ci->fgColor(),0,this);
    ci->setFgColor(rgb);
    mpScanAreaWidget->setRectFgColor(ci->number(),rgb);
    mFgColorList[ci->number()] = rgb;
    xmlConfig->setUintValueList("PREVIEW_FG_COLOR",mFgColorList);
  }
}
/** No descriptions */
void PreviewWidget::slotShowListView(bool state)
{
  if(state)
  {
    if(undoAvailable())
      slotZoomOff();
    mpListView->show();
    mpComboStack->raiseWidget(1);
    mpScanSizeLabel->setText(tr("Template"));
  }
  else
  {
    mpListView->hide();
    mpComboStack->raiseWidget(0);
    mpScanSizeLabel->setText(tr("Scan size"));
  }
  mpZoomHBox->setEnabled(!state);
  mpScanAreaWidget->setMultiSelectionMode(state);
  mpListView->setCurrentItem(mpListView->firstChild());
  emit signalMultiSelectionMode(state);
  redrawRulers();
}
/**  */
void PreviewWidget::addImageToQueue(QImage* image,bool overwrite_current)
{
  ImageBuffer* imagebuffer = new ImageBuffer();
  if(!imagebuffer)
    return;
  QImage* new_image = new QImage();
  if(!new_image)
  {
    delete imagebuffer;
    return;
  }
  *new_image = image->copy();
  imagebuffer->setImage(new_image);
  if((mImageVectorIndex > - 1) &&
     (mImageVectorIndex  < int(mImageVector.size()) - 1))
  {
    //This indicates, that the user has chosen undo before.
    //We delete the images after the current index.
    for(int i=mImageVectorIndex+1;i<int(mImageVector.size())-1;i++)
      mImageVector.remove(i);
    if(!overwrite_current)
    {
      if(mImageVector.resize(mImageVectorIndex+2))
        mImageVector.insert(mImageVector.size()-1,imagebuffer);
    }
    else
    {
      //save values
      imagebuffer->setTlxOrig(mImageVector[mImageVectorIndex]->tlxOrig());
      imagebuffer->setTlyOrig(mImageVector[mImageVectorIndex]->tlyOrig());
      imagebuffer->setBrxOrig(mImageVector[mImageVectorIndex]->brxOrig());
      imagebuffer->setBryOrig(mImageVector[mImageVectorIndex]->bryOrig());
      imagebuffer->setAspectRatio(mImageVector[mImageVectorIndex]->aspectRatio());

      mImageVector.remove(mImageVectorIndex);
      if(mImageVector.resize(mImageVectorIndex+1))
        mImageVector.insert(mImageVectorIndex,imagebuffer);
    }
    mImageVectorIndex = mImageVector.size()-1;
  }
  else if(mImageVectorIndex  < 0)
  {
    if(mImageVector.resize(mImageVector.size()+1))
    {
      mImageVector.insert(mImageVector.size()-1,imagebuffer);
      mImageVectorIndex = mImageVector.size()-1;
    }
  }
  else if(mImageVectorIndex == int(mImageVector.size()) - 1)
  {
    if(!overwrite_current)
    {
      if(mImageVector.resize(mImageVector.size()+1))
      {
        mImageVector.insert(mImageVector.size()-1,imagebuffer);
        mImageVectorIndex = mImageVector.size()-1;
      }
    }
    else
    {
      //save values
      imagebuffer->setTlxOrig(mImageVector[mImageVectorIndex]->tlxOrig());
      imagebuffer->setTlyOrig(mImageVector[mImageVectorIndex]->tlyOrig());
      imagebuffer->setBrxOrig(mImageVector[mImageVectorIndex]->brxOrig());
      imagebuffer->setBryOrig(mImageVector[mImageVectorIndex]->bryOrig());
      imagebuffer->setAspectRatio(mImageVector[mImageVectorIndex]->aspectRatio());
      mImageVector.remove(mImageVectorIndex);
      mImageVector.insert(mImageVectorIndex,imagebuffer);
    }
  }
  else
    mImageVectorIndex = -1;
  redrawZoomButtons();
}
/**  */
bool PreviewWidget::undoAvailable()
{
  if(mImageVectorIndex > 0)
    return true;
  return false;
}
/**  */
bool PreviewWidget::redoAvailable()
{
  if((mImageVectorIndex == -1) ||
     (mImageVectorIndex >= int(mImageVector.size()-1)))
    return false;
  return true;
}
/** No descriptions */
void PreviewWidget::slotZoom()
{
  int x,y,b,r;
  double tlx_o,tly_o,brx_o,bry_o;

  //ensure that the scan area values are up to date
  mImageVector[mImageVectorIndex]->setTlx(mpScanAreaWidget->tlxPercent());
  mImageVector[mImageVectorIndex]->setTly(mpScanAreaWidget->tlyPercent());
  mImageVector[mImageVectorIndex]->setBrx(mpScanAreaWidget->brxPercent());
  mImageVector[mImageVectorIndex]->setBry(mpScanAreaWidget->bryPercent());

  tlx_o =  mImageVector[mImageVectorIndex]->tlxMapped();
  tly_o =  mImageVector[mImageVectorIndex]->tlyMapped();
  brx_o =  mImageVector[mImageVectorIndex]->brxMapped();
  bry_o =  mImageVector[mImageVectorIndex]->bryMapped();

  if((tlx_o > brx_o) || (tly_o > bry_o))
    return;
  double o_ar;
  o_ar = double(mImageVector[0]->aspectRatio());
  double ar = o_ar*(brx_o-tlx_o)/(bry_o-tly_o);

  x = int(double(mImageVector[mImageVectorIndex]->image()->width()) *
          mpScanAreaWidget->tlxPercent());
  y = int(double(mImageVector[mImageVectorIndex]->image()->height()) *
          mpScanAreaWidget->tlyPercent());
  r = int(double(mImageVector[mImageVectorIndex]->image()->width()) *
          mpScanAreaWidget->brxPercent());
  b = int(double(mImageVector[mImageVectorIndex]->image()->height()) *
          mpScanAreaWidget->bryPercent());

  QImage im = mImageVector[mImageVectorIndex]->image()->copy(x,y,r-x,b-y);
  addImageToQueue(&im);
  if(!im.isNull())
    mpScanAreaWidget->setImage(&im);

  mpScanSizeCombo->setEnabled(false);
  mpScanAreaWidget->setAspectRatio(ar);
  mImageVector[mImageVectorIndex]->setAspectRatio(ar);

  mImageVector[mImageVectorIndex]->setTlx(0.0);
  mImageVector[mImageVectorIndex]->setTly(0.0);
  mImageVector[mImageVectorIndex]->setBrx(1.0);
  mImageVector[mImageVectorIndex]->setBry(1.0);

  mImageVector[mImageVectorIndex]->setTlxOrig(tlx_o);
  mImageVector[mImageVectorIndex]->setTlyOrig(tly_o);
  mImageVector[mImageVectorIndex]->setBrxOrig(brx_o);
  mImageVector[mImageVectorIndex]->setBryOrig(bry_o);
  mpScanAreaWidget->setTlx(0.0);
  mpScanAreaWidget->setTly(0.0);
  mpScanAreaWidget->setBrx(1.0);
  mpScanAreaWidget->setBry(1.0);
  redrawRulers();
}
/** No descriptions */
void PreviewWidget::slotZoomUndo()
{
  if(mImageVectorIndex > 0)
  {
    mImageVectorIndex -= 1;
    mpScanAreaWidget->setImage(mImageVector[mImageVectorIndex]->image());
    mpScanAreaWidget->setAspectRatio(mImageVector[mImageVectorIndex]->aspectRatio());
    redrawRulers();
    redrawZoomButtons();
    mpScanAreaWidget->setTlx(mImageVector[mImageVectorIndex]->tlx());
    mpScanAreaWidget->setTly(mImageVector[mImageVectorIndex]->tly());
    mpScanAreaWidget->setBrx(mImageVector[mImageVectorIndex]->brx());
    mpScanAreaWidget->setBry(mImageVector[mImageVectorIndex]->bry());
    if(!undoAvailable())
      mpScanSizeCombo->setEnabled(true);
    forceOptionUpdate();
  }
}
/** No descriptions */
void PreviewWidget::slotZoomRedo()
{
  if(mImageVectorIndex < int(mImageVector.size())-1)
  {
    mImageVectorIndex += 1;
    mpScanAreaWidget->setImage(mImageVector[mImageVectorIndex]->image());
    mpScanAreaWidget->setAspectRatio(mImageVector[mImageVectorIndex]->aspectRatio());
    redrawRulers();
    redrawZoomButtons();
    mpScanAreaWidget->setTlx(mImageVector[mImageVectorIndex]->tlx());
    mpScanAreaWidget->setTly(mImageVector[mImageVectorIndex]->tly());
    mpScanAreaWidget->setBrx(mImageVector[mImageVectorIndex]->brx());
    mpScanAreaWidget->setBry(mImageVector[mImageVectorIndex]->bry());
    mpScanSizeCombo->setEnabled(false);
    forceOptionUpdate();
  }
}
/** No descriptions */
void PreviewWidget::slotZoomOff()
{
  mImageVectorIndex = 0;
  mpScanAreaWidget->setImage(mImageVector[mImageVectorIndex]->image());
  mpScanAreaWidget->setAspectRatio(mImageVector[mImageVectorIndex]->aspectRatio());

  mpScanAreaWidget->setTlx(mImageVector[mImageVectorIndex]->tlx());
  mpScanAreaWidget->setTly(mImageVector[mImageVectorIndex]->tly());
  mpScanAreaWidget->setBrx(mImageVector[mImageVectorIndex]->brx());
  mpScanAreaWidget->setBry(mImageVector[mImageVectorIndex]->bry());
  mpScanSizeCombo->setEnabled(true);
  forceOptionUpdate();
  redrawZoomButtons();
  redrawRulers();
}
/** No descriptions */
void PreviewWidget::slotListItem(Q3ListViewItem* li,const QPoint& p,int c)
{
  UNUSED (p);
  if(!li)
    return;
  CheckListItemExt* ci;
  ci = (CheckListItemExt *) li;
  if(c == 0)
  {
    if(ci->isOn())
    {
      mpScanAreaWidget->showRect(ci->number());
      mpScanAreaWidget->setUserSelected(ci->number(),true);
      mpScanAreaWidget->setActiveRect(ci->number());
      mpCurrentItem = ci;
      slotUserSize();
    }
    else
    {
      if(ci->number() != 0)
      {
        mpScanAreaWidget->hideRect(ci->number());
        mpScanAreaWidget->setUserSelected(ci->number(),false);
        mpCurrentItem = 0;
        slotUserSize();
      }
      else
      {
        //first rect always stays visible
        ci->setOn(true);
        mpListView->setCurrentItem(ci);
        mpCurrentItem = ci;
        mpScanAreaWidget->setActiveRect(ci->number());
      }
    }
  }
  else
  {
    if(ci->isOn())
    {
      mpScanAreaWidget->setActiveRect(ci->number());
      mpCurrentItem = ci;
    }
  }
  if(mpCurrentItem)
  {
    if(mpCurrentItem != ci)
      mpListView->setCurrentItem(mpCurrentItem);
  }
  else
  {
    ci = (CheckListItemExt*) mpListView->firstChild();
    if(ci)
    {
      ci->setOn(true);
      mpListView->setCurrentItem(ci);
      mpCurrentItem = ci;
      mpScanAreaWidget->setActiveRect(ci->number());
    }
  }
}
/** No descriptions */
void PreviewWidget::slotNewActiveRect(int num)
{
  CheckListItemExt* ci;
  ci = (CheckListItemExt*) mpListView->firstChild();
  while(ci)
  {
    if(ci->number() == num)
    {
      mpListView->setCurrentItem(ci);
      if(!ci->isOn())
        ci->setOn(true);
      mpCurrentItem = ci;
      break;
    }
    ci = (CheckListItemExt*) ci->nextSibling();
  }
}
/** No descriptions */
void PreviewWidget::redrawZoomButtons()
{
  mpZoomUndoButton->setEnabled(undoAvailable());
  mpZoomRedoButton->setEnabled(redoAvailable());
  mpZoomOffButton->setEnabled(undoAvailable());
  emit signalEnableScanAreaOptions(!undoAvailable());
}
/** No descriptions */
void PreviewWidget::forceOptionUpdate()
{
  emit signalTlxPercent(mImageVector[mImageVectorIndex]->tlxMapped());
  emit signalTlyPercent(mImageVector[mImageVectorIndex]->tlyMapped());
  emit signalBrxPercent(mImageVector[mImageVectorIndex]->brxMapped());
  emit signalBryPercent(mImageVector[mImageVectorIndex]->bryMapped());
}
/** No descriptions */
void PreviewWidget::slotPreviewRequest()
{
  double tlx,tly,brx,bry;
  double x,y;
  double w,h;
  int res;

  w = (mMaxRangeX -mMinRangeX);
  h = (mMaxRangeY -mMinRangeY);

  tlx = mImageVector[mImageVectorIndex]->tlxOrig();
  tly = mImageVector[mImageVectorIndex]->tlyOrig();
  brx = mImageVector[mImageVectorIndex]->brxOrig();
  bry = mImageVector[mImageVectorIndex]->bryOrig();
//find a reasonable resolution
  if(mSaneUnit == SANE_UNIT_MM)
  {
    x = double(mpScanAreaWidget->width())/(w*brx - w*tlx);
    y = double(mpScanAreaWidget->height())/(h*bry - h*tly);
    x = x*25.4;
    y = y*25.4;
  }
  else //
  {
    x = 75.0*double(mpScanAreaWidget->width())/(w*brx - w*tlx);
    y = 75.0*double(mpScanAreaWidget->height())/(h*bry - h*tly);
  }

  if(x > y)
    res = int(x);
  else
    res = int(y);
  emit signalPreviewRequest(tlx,tly,brx,bry,res);
}
/** No descriptions */
const QVector <double> PreviewWidget::selectedRects()
{
  return mpScanAreaWidget->selectedRects();
}
/** No descriptions */
void PreviewWidget::setRange(double minx,double maxx,double miny,double maxy, double width, double height)
{
  mMinRangeX = minx;
  mMaxRangeX = maxx;
  mMinRangeY = miny;
  mMaxRangeY = maxy;
  mWidth = width;
  mHeight = height;
  redrawRulers();
  slotRangeChange();
}
/** No descriptions */
void PreviewWidget::setSaneUnit(SANE_Unit unit)
{
  mSaneUnit = unit;
}
/** No descriptions */
void PreviewWidget::setAspectRatio(double aspect)
{
  if(mImageVectorIndex < 0)
    return;

  mImageVector[0]->setAspectRatio(aspect);
  if(mImageVectorIndex == 0)
    mpScanAreaWidget->setAspectRatio(aspect);
}
/** No descriptions */
void PreviewWidget::enableMultiSelection(bool state)
{
  mpMultiSelectionButton->setEnabled(state);
  if(!state)
    mpMultiSelectionButton->setOn(state);
}
/**  */
void PreviewWidget::showEvent(QShowEvent* se)
{
  QWidget::showEvent(se);
  int w = xmlConfig->intValue("SCANDIALOG_STANDALONE_PREVIEW_WIDTH",250);
  int h = xmlConfig->intValue("SCANDIALOG_STANDALONE_PREVIEW_HEIGHT",300);
  if(isTopLevel())
  {
    if(w < width())
      w = width();
    if(h < height())
      h = height();
    resize(w,h);
  }
}
/** No descriptions */
void PreviewWidget::clearPreview()
{
  mpScanAreaWidget->clearPreview();
  mpUpdateWidget->clearWidget();
}
/** No descriptions */
void PreviewWidget::setData(QByteArray& byte_array)
{
  mpUpdateWidget->setData(byte_array);
}
/** No descriptions */
void PreviewWidget::initPixmap(int rw,int rh)
{
  mpUpdateWidget->initPixmap(rw,rh);
}
/** No descriptions */
void PreviewWidget::enablePreviewMode(bool state)
{
  //In preview mode, i.e. after the user clicked the "Scan preview" button,
  //all GUI elements are disabled. We raise the mpUpdateWidget and the
  //"Cancel" button; these elements are not disabled.
  if(state)
  {
    mCancelled = false;
    mpPreviewStack->raiseWidget(1);
    mpButtonStack->raiseWidget(1);
    mpSizeHBox->setEnabled(false);
    mpToolHBox->setEnabled(false);
//s    mpPreviewButton->setEnabled(false);
    mpPreviewStack->setEnabled(true);
    mpCancelButton->setEnabled(true);
    mpListView->setEnabled(false);
    //don't allow resizing during preview scan
    setFixedSize(size());
  }
  else
  {
    mpPreviewStack->raiseWidget(0);
    mpButtonStack->raiseWidget(0);
    mpSizeHBox->setEnabled(true);
    mpToolHBox->setEnabled(true);
//s    mpPreviewButton->setEnabled(true);
    mpListView->setEnabled(true);
    setMinimumSize(minimumSizeHint().width(),minimumSizeHint().height());
    setMaximumSize(3000,3000);
  }
}
/** No descriptions */
bool PreviewWidget::wasCancelled()
{
  return mCancelled;
}
/** No descriptions */
void PreviewWidget::slotCancel()
{
  mCancelled = true;
}
/** No descriptions */
void PreviewWidget::slotAutoSelection()
{
  double tlx,tly,brx,bry;
  QVector <double> rects;
  //if we can't select anything, then there's no use in auto-selection
  if(!mpScanAreaWidget->selectionEnabled())
    return;
  QImage image = *(mImageVector[mImageVectorIndex]->image());
  if(image.isNull())
    return;
  double factor;
  double sizefactor;

  if(image.allGray())
    factor = double(xmlConfig->intValue("AUTOSELECT_GRAY_FACTOR",10))/100.0;
  else
    factor = double(xmlConfig->intValue("AUTOSELECT_COLOR_FACTOR",10))/100.0;

  sizefactor = double(xmlConfig->intValue("AUTOSELECT_SIZE",4))/100.0;

  bool ms = mpListView->isVisible();

  ImageDetection imagedetection(&image,ms,qRgb(0,0,0),factor,sizefactor);

  int bgtype = xmlConfig->intValue("AUTOSELECT_BG_TYPE",0);
  if(bgtype == 0)
    imagedetection.setGrayLimit(xmlConfig->intValue("AUTOSELECT_MAXIMAL_GRAY_VALUE",100),
                                true);
  else
    imagedetection.setGrayLimit(xmlConfig->intValue("AUTOSELECT_MINIMAL_GRAY_VALUE",155),
                                false);

  rects = imagedetection.autoSelect();
  tlx = 1.0;
  tly = 1.0;
  brx = 0.0;
  bry = 0.0;

  if(rects.size() >= 4)
  {
    int i = 0;
    if(mpListView->isVisible())
    {
      CheckListItemExt* ci;
      ci = (CheckListItemExt*) mpListView->firstChild();
      for(i=0;i < int(rects.size())-3;i+=4)
      {
        if(ci && (i < int(rects.size())-3) &&
           (sizefactor < rects[i+1] - rects[i]) &&
           (sizefactor < rects[i+3] - rects[i+2]))
        {
          mpListView->setCurrentItem(ci);
          if(!ci->isOn())
            ci->setOn(true);
          mpScanAreaWidget->showRect(ci->number());
          mpScanAreaWidget->setActiveRect(ci->number());
          mpScanAreaWidget->setUserSelected(ci->number(),true);
          mpScanAreaWidget->setRectSize(rects[i],rects[i+2],
                                        rects[i+1],rects[i+3]);
          mpCurrentItem = ci;
          ci = (CheckListItemExt*) ci->nextSibling();
        }
      }
      while(ci)
      {
        mpScanAreaWidget->hideRect(ci->number());
        mpScanAreaWidget->setUserSelected(ci->number(),false);
        ci->setOn(false);
        ci = (CheckListItemExt*) ci->nextSibling();
      }
      if(ci == (CheckListItemExt*) mpListView->firstChild())
      {
        if(!ci->isOn())
        {
          mpScanAreaWidget->showRect(ci->number());
          mpScanAreaWidget->setActiveRect(ci->number());
          mpScanAreaWidget->setUserSelected(ci->number(),true);
          mpScanAreaWidget->setRectSize(0.0,0.0,1.0,1.0);
          mpCurrentItem = ci;
          ci->setOn(true);
        }
      }
    }
    else
    {
      for(i=0;i < int(rects.size())-3;i+=4)
      {
        if((sizefactor < rects[i+1] - rects[i]) &&
           (sizefactor < rects[i+3] - rects[i+2]))
        {
          if(rects[i] < tlx)
            tlx = rects[i];
          if(rects[i+1] > brx)
            brx = rects[i+1];
          if(rects[i+2] < tly)
            tly = rects[i+2];
          if(rects[i+3] > bry)
            bry = rects[i+3];
        }
      }
      if(tlx > brx)
        tlx = 0.0;
      if(brx == tlx)
        brx = 1.0;
      if(tly > bry)
        tly = 0.0;
      if(bry == tly)
        bry = 1.0;
      setRectSize(tlx,tly,brx,bry);
    }
  }
  slotUserSize();
}
/** No descriptions */
void PreviewWidget::slotAutoSelectionSetup()
{
  QExtensionWidget ew(this);
  ew.setPage(8);
  ew.exec();
}
/** No descriptions */
void PreviewWidget::slotAddTemplate()
{
  QString qs;
  QDialog d(this,0,true);
  d.setCaption(tr("Template name"));
  Q3GridLayout* mainlayout = new Q3GridLayout(&d,4,3);
  mainlayout->setColStretch(1,1);
  mainlayout->setRowStretch(2,1);
  mainlayout->setSpacing(5);
  mainlayout->setMargin(5);
  QLabel* label = new QLabel(tr("Please enter a template name:"),&d);
  QLineEdit* le = new QLineEdit(&d);
  le->setMaxLength(20);
  QPushButton* pb1 = new QPushButton(tr("&OK"),&d);
  QPushButton* pb2 = new QPushButton(tr("&Cancel"),&d);
  mainlayout->addMultiCellWidget(label,0,0,0,1);
  mainlayout->addMultiCellWidget(le,1,1,0,2);
  mainlayout->addWidget(pb1,3,0);
  mainlayout->addWidget(pb2,3,2);
  connect(pb1,SIGNAL(clicked()),&d,SLOT(accept()));
  connect(pb2,SIGNAL(clicked()),&d,SLOT(reject()));
  if(d.exec())
  {
    qs = le->text();
    if(qs.isEmpty())
      return;
    //test, whether template exists already
    for(int u=0;u<mTemplateVector.size();u++)
    {
      if(mTemplateVector[u].name() == qs)
      {
        return;
      }
    }
    ScanAreaTemplate* st = new ScanAreaTemplate(qs);
    st->addRects(mpScanAreaWidget->scanAreas());
    mTemplateVector.append(*st);
    mpScanTemplateCombo->insertItem(qs);
    mpScanTemplateCombo->setCurrentItem(mpScanTemplateCombo->count() - 1);
    saveTemplates();
  }
}
/** No descriptions */
void PreviewWidget::slotDeleteTemplate()
{
  QString qs = mpScanTemplateCombo->currentText();
  if(mpScanTemplateCombo->currentItem() == 0)
    return;
  for(int u=0;u<mTemplateVector.size();u++)
  {
    if(mTemplateVector[u].name() == qs)
    {
      mTemplateVector.remove(u);
      mpScanTemplateCombo->removeItem(mpScanTemplateCombo->currentItem());
//      mTemplateVector.sort();
//      mTemplateVector.resize(mTemplateVector.size() - 1);
      saveTemplates();
      slotUserSize();
      return;
    }
  }
}
/** No descriptions */
void PreviewWidget::slotDeleteAllTemplates()
{
  int i = QMessageBox::warning(this,tr("Delete templates"),
                             tr("Do you really want to delete all templates?"),
                             tr("&Delete"),tr("&Cancel"),QString::null,1,1);
  if(i == 1)
    return;
  mTemplateVector.clear();
  mTemplateVector.resize(0);
  mpScanTemplateCombo->clear();
  mpScanTemplateCombo->insertItem(tr("None"));
  saveTemplates();
  slotUserSize();
}
/** No descriptions */
void PreviewWidget::slotTemplateSelected(int i)
{
  if(i == 0)
    return;
  QString qs = mpScanTemplateCombo->currentText();
  QVector <ScanArea*> vec;
  for(int u=0;u<mTemplateVector.size();u++)
  {
    if(mTemplateVector[u].name() == qs)
    {
      vec = mTemplateVector[u].rects();
      break;
    }
  }
  Q3ListViewItemIterator it(mpListView);
  ++it;
  for( ; it.current(); ++it)
  {
    CheckListItemExt* li = (CheckListItemExt*) it.current();
    li->setOn(false);
    mpScanAreaWidget->hideRect(li->number());
    mpScanAreaWidget->setUserSelected(li->number(),false);
  }
  for(int u=0;u<vec.size();u++)
  {
    ScanArea *sa = vec[u];

    Q3ListViewItemIterator it(mpListView);
    for( ; it.current(); ++it)
    {
      CheckListItemExt* li = (CheckListItemExt*) it.current();
      if(li->number() == vec[u]->number())
      {
        li->setOn(true);
        mpScanAreaWidget->setUserSelected(sa->number(),true);
        mpScanAreaWidget->showRect(sa->number());
        mpScanAreaWidget->setActiveRect(sa->number());
        mpScanAreaWidget->setRectSize(sa->tlx(),sa->tly(),sa->brx(),sa->bry());
        break;
      }
    }
  }
  mpListView->setCurrentItem(mpListView->firstChild());
  mpScanAreaWidget->setActiveRect(0);
}
/** No descriptions */
void PreviewWidget::saveTemplates()
{
  QString qs;
  QDomDocument doc("ScanAreaTemplates");

  // create the root element
  QDomElement root = doc.createElement(doc.doctype().name());
  root.setAttribute("version","1");

  QDomElement temp;
  for (int u = 0;u < mTemplateVector.size(); u++)
  {
    temp = doc.createElement("scan_area_template");
    temp.setAttribute("name",mTemplateVector[u].name());
    QVector <ScanArea*> sca = mTemplateVector[u].rects();
    for (int u2 = 0;u2 < sca.size(); u2++)
    {
      ScanArea *sa = sca[u2];
      QDomElement rec;
      rec = doc.createElement("scan_rect");
      rec.setAttribute("number", qs.setNum(sa->number()));
      rec.setAttribute("tlx", qs.setNum(sa->tlx()));
      rec.setAttribute("tly", qs.setNum(sa->tly()));
      rec.setAttribute("brx", qs.setNum(sa->brx()));
      rec.setAttribute("bry", qs.setNum(sa->bry()));
      temp.appendChild(rec);
    }
    root.appendChild(temp);
  }
  doc.appendChild(root);

  // open file
  QFile file(xmlConfig->absConfDirPath()+"scanareatemplates.xml");
  if (!file.open(QIODevice::WriteOnly))
  {
    // error opening file
    return;
  }
  // write it
  QTextStream textstream(&file);
  doc.save(textstream, 0);
  file.close();
}
/** No descriptions */
void PreviewWidget::loadTemplates()
{
  ScanArea* sca;
  QVector <ScanArea*> vec;
  QString tpath =  xmlConfig->absConfDirPath()+"scanareatemplates.xml";
  vec.resize(0);
  QFile tempfile(tpath);
  if (!tempfile.open(QIODevice::ReadOnly))
  {
    //If the template file doesn't exist
    //create an empty file and return true
    if(!QFile::exists(tpath))
    {
       saveTemplates();
    }
    return;
  }
  // open dom document
  QDomDocument doc("ScanAreaTemplates");
  if (!doc.setContent(&tempfile))
  {
    return;
  }
  tempfile.close();

  // check the doc type
  if (doc.doctype().name() != "ScanAreaTemplates")
  {
    // wrong file type
    return;
  }
  QDomElement root = doc.documentElement();
  // get list of items
  QDomNodeList nodes = root.elementsByTagName("scan_area_template");
  QDomElement ele;
  QString name;
  // iterate over the items
  for (int n=0; n<nodes.count(); ++n)
  {
    if (nodes.item(n).isElement())
    {
      ele = nodes.item(n).toElement();
      if(ele.hasAttribute("name"))
      {
        name = ele.attribute("name");
        ScanAreaTemplate* st = new ScanAreaTemplate(name);
        QDomNode cn = ele.firstChild();
        while( !cn.isNull() )
        {
          QDomElement ce = cn.toElement();
          if( !ce.isNull() && (ce.tagName() == "scan_rect") )
          {
             int num = -1;
             double tlx = -1.0;
             double tly = -1.0;
             double brx = -1.0;
             double bry = -1.0;
             if(ce.hasAttribute("number"))
               num = ce.attribute("number").toInt();
             if(ce.hasAttribute("tlx"))
               tlx = ce.attribute("tlx").toDouble();
             if(ce.hasAttribute("tly"))
               tly = ce.attribute("tly").toDouble();
             if(ce.hasAttribute("brx"))
               brx = ce.attribute("brx").toDouble();
             if(ce.hasAttribute("bry"))
               bry = ce.attribute("bry").toDouble();
             if((num > -1) && (tlx >= 0.0) && (tly >= 0.0) &&
                (brx >= 0.0) && (bry >= 0.0))
             {
               sca = new ScanArea(QString::null,tlx,tly,brx,bry);
               sca->setNumber(num);
               vec.append(sca);
             }
           }
           cn = cn.nextSibling();
        }
        st->addRects(vec);
        mTemplateVector.append(*st);
        mpScanTemplateCombo->insertItem(name);
      }
    }
  }
}
