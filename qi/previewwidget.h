/***************************************************************************
                          previewwidget.h  -  description
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

#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

//s #include <qarray.h>
#include <q3valuelist.h>
#include <q3ptrvector.h>
#include <qwidget.h>

#include "quiteinsanenamespace.h"
//Added by qt3to4:
#include <QShowEvent>
#include <QResizeEvent>
#include <Q3GridLayout>
#include <QLabel>
extern "C"
{
	#include <sane/sane.h>
}
/**
  *@author M. Herder
  */
//forward declarations
class CheckListItemExt;
class ImageBuffer;
class QComboBox;
class Q3GridLayout;
class Q3HBox;
class QLabel;
class Q3ListView;
class Q3ListViewItem;
class QPushButton;
class QRect;
class QString;
class Q3WidgetStack;
class QToolButton;
class PreviewUpdateWidget;
class Ruler;
class ScanArea;
class ScanAreaTemplate;
class ScanAreaCanvas;

struct PredefinedSizes
{
  QString name;
  double width;
  double height;
};

class PreviewWidget : public QWidget
{
   Q_OBJECT
public:
  PreviewWidget(QWidget *parent=0, const char *name=0,Qt::WFlags f=0);
  ~PreviewWidget();
  /**  */
  void setMetrics(QIN::MetricSystem ms,SANE_Unit unit);
  /**  */
  void loadPreviewPixmap(QString path);
  /** Set the size of the rectangle */
  void setRectSize(double tlx,double tly,double brx,double bry);
  /**  */
  void changeLayout(bool toplevel);
  /** No descriptions */
  const QVector <double> selectedRects();
  /** No descriptions */
  void setAspectRatio(double aspect);
  /** No descriptions */
  void setSaneUnit(SANE_Unit unit);

  /** set the limits of the scan area, and also the page width/height. If the latter are -1 then it is
     not possible to change them. The latter are normally used with ADF scanners which support changing
     the ADF paper size. In this case the 'page size limits' can effectively be changed */
  void setRange(double minx,double maxx,double miny,double maxy, double width = -1, double height = -1);
  /** No descriptions */
  void enableMultiSelection(bool state);
  /** No descriptions */
  void clearPreview();
  /** No descriptions */
  void setData(QByteArray& byte_array);
  /** No descriptions */
  void initPixmap(int rw,int rh);
  /** No descriptions */
  void enablePreviewMode(bool state);
  /** No descriptions */
  bool wasCancelled();

  /** get name of n'th predefined size, returns NULL if none */
  const char *getSizeName (unsigned id);

  /** get the n'th predefined size, returns NULL if none */
  ScanArea *getSize (unsigned id);

  /** set the size to the n'th predefined size */
  void setSize (unsigned id);

  /** get the id of the A4 paper size */
  int getPreDefA4 (void) { return mPreDefA4; }

private:
  /** */
  Q3ValueList <unsigned int> mFgColorList;
  /** */
  Q3ValueList <unsigned int> mBgColorList;
  /** */
  Q3PtrVector <ImageBuffer> mImageVector;
  /** */
  int mImageVectorIndex;
  /** */
  QVector <ScanAreaTemplate> mTemplateVector;
  /** */
  int mTemplateVectorIndex;
  /** */
  CheckListItemExt* mpCurrentItem;
  /**  */
  bool mCancelled;

  /** these variables define the scan area within the page size. It is set in QScanDialog::setPreviewRange()
    by calling setRange(). These are in units of mm */
  double mMinRangeX;
  /**  */
  double mMaxRangeX;
  /**  */
  double mMinRangeY;
  /**  */
  double mMaxRangeY;

  /**  */
  double mFactorX;
  /**  */
  double mFactorY;

  /** page width and height (-1 if not known) */
  double mHeight;
  /**  */
  double mWidth;
  /**  */

  QIN::MetricSystem mMetricSystem;
  /**  */
  SANE_Unit mSaneUnit;
  /**  */
  Q3WidgetStack* mpPreviewStack;
  /**  */
  PreviewUpdateWidget* mpUpdateWidget;
  /**  */
  ScanAreaCanvas* mpScanAreaWidget;
  /**  */
  Q3GridLayout* mpMainLayout;
  /**  */
  int mInitialHeight;
  /**  */
  int mInitialWidth;
  /**  */
  Q3WidgetStack* mpButtonStack;
  /**  */
  Q3WidgetStack* mpComboStack;
  /**  */
  QPushButton* mpCancelButton;
  /**  */
  QComboBox* mpScanSizeCombo;
  /**  */
  QComboBox* mpScanTemplateCombo;
  /**  */
  QPushButton* mpCloseButton;
  /**  */
  QPushButton* mpPreviewButton;
  /**  */
  QVector <ScanArea*> mSizeArray;
  /**  */
  QVector <ScanArea*> mUserSizeArray;
  /**  */
  QVector <ScanAreaTemplate*> mUserSizeTemplateArray;
  /**  */
  QLabel* mpMetricLabel;
  /**  */
  QLabel* mpScanSizeLabel;
  /**  */
  Ruler* mpVRuler;
  /**  */
  Ruler* mpHRuler;
  /**  */
  Q3HBox* mpZoomHBox;
  /**  */
  Q3HBox* mpSizeHBox;
  /**  */
  Q3HBox* mpToolHBox;
  /**  */
  QToolButton* mpAutoSelectionButton;
  /**  */
  QToolButton* mpAutoSetupButton;
  /**  */
  QToolButton* mpZoomButton;
  /**  */
  QToolButton* mpZoomUndoButton;
  /**  */
  QToolButton* mpZoomRedoButton;
  /**  */
  QToolButton* mpZoomOffButton;
  /**  */
  QToolButton* mpMultiSelectionButton;
  /**  */
  QToolButton* mpAddButton;
  /**  */
  QToolButton* mpSubButton;
  /**  */
  QToolButton* mpDelTemplatesButton;
  /**  */
  Q3ListView* mpListView;
  /**  */
  QWidget* mpResizeWidget;

  // list of predefined paper sizes
  PredefinedSizes *mPreDefs;

  // id of the A4 size
  int mPreDefA4;

  /**  */
  void initWidget();
  /**  */
  void createPredefinedSizes();
  /**  */
  void redrawRulers();
  /** */
  void addImageToQueue(QImage* image,bool overwrite_current=false);
  /** */
  bool undoAvailable();
  /**  */
  bool redoAvailable();
  /** No descriptions */
  void redrawZoomButtons();
  /** No descriptions */
  void forceOptionUpdate();
  /** No descriptions */
  void createWhatsThisHelp();
  /** No descriptions */
  void saveTemplates();
  /** No descriptions */
  void loadTemplates();
protected:
  /** */
  void resizeEvent(QResizeEvent* e);
  /** */
  void showEvent(QShowEvent* se);
private slots:
  /**  */
  void slotAutoSelectionSetup();
  /**  */
  void slotTlxPercent(double pval);
  /**  */
  void slotTlyPercent(double pval);
  /**  */
  void slotBrxPercent(double pval);
  /**  */
  void slotBryPercent(double pval);
  /**  */
  void slotSizeComboChanged(int index);
  /**  */
  void slotUserSize();
  /** No descriptions */
  void slotColorPopup(Q3ListViewItem* li,const QPoint& p,int i);
  /** No descriptions */
  void slotShowListView(bool state);
  /** No descriptions */
  void slotZoom();
  /** No descriptions */
  void slotListItem(Q3ListViewItem* li,const QPoint& p,int c);
  /** No descriptions */
  void slotNewActiveRect(int num);
  /** No descriptions */
  void slotZoomOff();
  /** No descriptions */
  void slotZoomRedo();
  /** No descriptions */
  void slotZoomUndo();
  /** No descriptions */
  void slotPreviewRequest();
  /** No descriptions */
  void slotDeleteAllTemplates();
  /** No descriptions */
  void slotDeleteTemplate();
  /** No descriptions */
  void slotAddTemplate();
  /** No descriptions */
  void slotTemplateSelected(int i);
public slots:
  /**  */
  void slotAutoSelection();
  /**  */
  void slotChangeMetricSystem(QIN::MetricSystem ms);
  /**  */
  void slotSetTlxPercent(double pval);
  /**  */
  void slotSetTlyPercent(double pval);
  /** No descriptions */
  void slotCancel();
  /**  */
  void slotSetBrxPercent(double pval);
  /**  */
  void slotSetBryPercent(double pval);

  /** called when the range changes - this is mMinRangeX, etc. We recalculate the standard paper sizes */
  void slotRangeChange();

signals: // Signals
  /**  */
  void signalPreviewSize(QRect rect);
  /**  */
  void signalPreviewRequest(double tlx,double tly,double brx,double bry,int res);
  /**  */
  void signalPredefinedSize(ScanArea* sca);
  /**  */
  void signalUserSetSize();
  /**  */
  void signalHidePreview();
  /** */
  void signalTlxPercent(double val);
  /** */
  void signalTlyPercent(double val);
  /** */
  void signalBrxPercent(double val);
  /** */
  void signalBryPercent(double val);
  /** No descriptions */
  void signalMultiSelectionMode(bool on);
  /** No descriptions */
  void signalEnableScanAreaOptions(bool on);
};
#endif
