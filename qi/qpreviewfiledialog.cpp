/***************************************************************************
                          qpreviewfiledialog.cpp  -  description
                             -------------------
    begin                : Tue Nov 14 2000
    copyright            : (C) 2000 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *  
 *                                                                         *  
***************************************************************************/

#include "imageiosupporter.h"
#include "qimageioext.h"
#include "qpreviewfiledialog.h"
#include "qqualitydialog.h"
#include "qxmlconfig.h"

#include <qcheckbox.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qdialog.h>
#include <qevent.h>
#include <qhbox.h>
#include <qimage.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qwidget.h>

QPreviewFileDialog::QPreviewFileDialog(bool has_preview,bool cancel_warning,QWidget* parent,
                                       const char* name,bool modal)
                   :QFileDialog(parent,name,modal)
{
  mPreviewMode = has_preview;
  mWarnOnCancel = cancel_warning;
  mpImage = 0;
  mMustDeleteImage = false;
  setCaption(tr("Save image as..."));
  initDlg();
}
QPreviewFileDialog::~QPreviewFileDialog()
{
}
/**  */
void QPreviewFileDialog::initDlg()
{
  ImageIOSupporter iisup;
  QStringList filters;
  QString qs;
  if(mPreviewMode)
  {
    QWidget* widget = new QWidget(this);
    QVBoxLayout* qvbl = new QVBoxLayout(widget);
    mpPreviewCheckBox = new QCheckBox(tr("Show preview"),widget);
    connect(mpPreviewCheckBox,SIGNAL(toggled(bool)),
            this,SLOT(slotShowPreview(bool)));
    mpPixWidget = new QLabel(widget);
    mpPixWidget->setMinimumWidth(200);
    qvbl->setMargin(5);
    qvbl->addWidget(mpPreviewCheckBox);
    qvbl->addWidget(mpPixWidget);
    qvbl->setStretchFactor (mpPixWidget,1);
    mpPixWidget->setPalette(QColor(lightGray));
    addLeftWidget(widget);
    resize(550,300);
  }
  mImageFormat =xmlConfig->stringValue("VIEWER_IMAGE_TYPE","ALL_FILES");
  filters = iisup.getOrderedOutFilterList(mImageFormat);
  setDir(xmlConfig->stringValue("SINGLEFILE_SAVE_PATH"));
  setFilters(filters);
  setMode(QFileDialog::AnyFile);
  setSizeGripEnabled(false);
  setViewMode((QFileDialog::ViewMode)xmlConfig->intValue("SINGLEFILE_VIEW_MODE"));
}
/**  */
void QPreviewFileDialog::loadImage(QString path)
{
  ImageIOSupporter iisup;
  if(mpImage && mMustDeleteImage) delete mpImage;
  mpImage = new QImage();
  iisup.loadImage(*mpImage,path);
  mFilePath = path;
  mMustDeleteImage = true;
}
/**  */
void QPreviewFileDialog::slotShowPreview(bool b)
{
  if(!mPreviewMode) return;
  QString qs;
  double xfac;
  double yfac;
  QImage qi;
  QPixmap* pix;
//  showExtension(b);
  if(b)
  {
    xfac = double(mpImage->width())/double(mpPixWidget->width());
    yfac = double(mpImage->height())/double(mpPixWidget->height());
    if(xfac > yfac)
      qi = mpImage->smoothScale(mpImage->width()/xfac,mpImage->height()/xfac);
    else
      qi = mpImage->smoothScale(mpImage->width()/yfac,mpImage->height()/yfac);
    pix = new QPixmap;
    pix->convertFromImage(qi);
    mpPixWidget->setPixmap(*pix);
  }
  else
  {
    if(mpPixWidget->pixmap())
      mpPixWidget->pixmap()->resize(0,0);
    mpPixWidget->repaint();
  }
  xmlConfig->setBoolValue("PREVIEW_EXTENSION",b);
}
/**  */
void QPreviewFileDialog::showEvent(QShowEvent* e)
{
  QString qs;
  bool b;
  if(mPreviewMode)
  {
    b = xmlConfig->boolValue("PREVIEW_EXTENSION");
    mpPreviewCheckBox->setChecked(b);
  }
  QWidget::showEvent(e);
  int w = xmlConfig->intValue("PREVIEW_FILE_DIALOG_WIDTH",0);
  int h = xmlConfig->intValue("PREVIEW_FILE_DIALOG_HEIGHT",0);
  if(w < width())
    w = width();
  if(h < height())
    h = height();
  resize(w,h);
}

void QPreviewFileDialog::reject()
{
  int exit;
  //if there's no parent, we ask the user whether he really
  //wants to cancel saving; otherwise we assume, that the
  //parent will warn the user
  if(mWarnOnCancel)
  {
    exit=QMessageBox::warning(this, tr("Cancel..."),
                              tr("The image has not been saved.\n"
                              "Do you really want to cancel?"),
                              tr("&Yes"),tr("&No"));
    if(exit == 1)
    {
      return;
    }
  }
  if(mpImage && mMustDeleteImage) delete mpImage;
  int view_mode = int(viewMode());
  //What the hell is this? Not enough, that viewMode() only works, as
  //long as the dialog is visible, it also returns the wrong values
  //(0 when it should return 1 and vice versa)
  //At least, this stupid behaviour is consistent in all versions
  //of Qt2.2.x. If this is fixed in Qt3, the following lines must
  //be removed.
  if(view_mode == 0)
    view_mode = 1;
  else if(view_mode == 1)
    view_mode = 0;
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",view_mode);
  QDialog::reject();
}

void QPreviewFileDialog::accept()
{
  ImageIOSupporter iisup;
  QString qs;
  mFilePath = selectedFile();
  format();
  if(mFilePath == xmlConfig->absConfDirPath()+".scantemp.pnm")
  {
    QMessageBox::information(this,tr("Information"),
                 tr("You can not save the image under the name\n"
                    "of the temporary image file, because it\n"
                    "will be overwritten with the next scan."),tr("OK"));	
    return;
  }
  if(mImageFormat == "")
  {
    QMessageBox::information(this,tr("Information"),
                 tr("You did not specify a valid image format.\n"
                    "Either append a valid extension to the filename,\n"
                    "or select a file filter."),tr("OK"));	
    return;
  }
  if(!iisup.saveImageInteractive(mFilePath,*mpImage,mImageFormat,this))
  {
    return;
  }
  xmlConfig->setStringValue("SINGLEFILE_SAVE_PATH",dir()->absPath());
  int view_mode = int(viewMode());
  //What the hell is this? Not enough, that viewMode() only works, as
  //long as the dialog is visible, it also returns the wrong values
  //(0 when it should return 1 and vice versa)
  //At least, this stupid behaviour is consistent in all versions
  //of Qt2.2.x. If this is fixed in Qt3, the following lines must
  //be removed.
  if(view_mode == 0)
    view_mode = 1;
  else if(view_mode == 1)
    view_mode = 0;
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",view_mode);
  xmlConfig->setStringValue("VIEWER_IMAGE_TYPE",
                            iisup.getDataFromOutFilter(selectedFilter()));
  if(mpImage && mMustDeleteImage) delete mpImage;
  QDialog::accept();
}

void QPreviewFileDialog::format()
{
  ImageIOSupporter iisup;
  QString ext;
//did the user type an extension ?
  mImageFormat = iisup.getFormatByFilename(mFilePath);
  if(mImageFormat.isEmpty())
  {
    //if there's no extension, try to get the format from the currently
    //selected filter
    mImageFormat = iisup.getFormatByFilter(selectedFilter());
    if(!mImageFormat.isEmpty())
    {
      ext = iisup.getExtensionByFormat(mImageFormat);
      mFilePath+= ext;
    }
  }
}
/**  */
bool QPreviewFileDialog::setImage(QImage* image)
{
  if(!image) return false;
  if(image->isNull()) return false;
  if(mpImage && mMustDeleteImage) delete mpImage;
  mpImage = image;
  mMustDeleteImage = false;
  return true;
}
/** No descriptions */
void QPreviewFileDialog::resizeEvent(QResizeEvent* e)
{
  if(isVisible())
  {
    xmlConfig->setIntValue("PREVIEW_FILE_DIALOG_WIDTH",width());
    xmlConfig->setIntValue("PREVIEW_FILE_DIALOG_HEIGHT",height());
  }
}
