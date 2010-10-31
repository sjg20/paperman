/***************************************************************************
                          qpreviewfiledialog.h  -  description
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

#ifndef QPREVIEWFILEDIALOG_H
#define QPREVIEWFILEDIALOG_H

#include <qfiledialog.h>
#include <qimage.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QWidget;
/**
  *@author Michael Herder
  */

class QPreviewFileDialog : public QFileDialog
{
Q_OBJECT
public:
	QPreviewFileDialog(bool has_preview=false,bool cancel_warning=false,QWidget* parent=0,
                     const char* name = 0,bool modal = true);
	~QPreviewFileDialog();
  /**  */
  void loadImage(QString path);
  /**  */
  bool setImage(QImage* image);
private: // Private methods
  /**  */
  QString mFilePath;
  /** */
  QImage* mpImage;
  /** */
  QCheckBox* mpPreviewCheckBox;
  /**  */
  bool mPreviewMode;
  /**  */
  bool mWarnOnCancel;
  /** */
  QLabel* mpPixWidget;
  /**  */
  bool mMustDeleteImage;
  /**  */
  QString mImageFormat;
private: //methods
  /**  */
  void format();
private slots: // Private slots
  /**  */
  void slotShowPreview(bool b);
  /**  */
  void initDlg();
protected: // Protected methods
  /**  */
  virtual void showEvent(QShowEvent* e);
protected slots:
  virtual void reject();
  virtual void accept();
  /** No descriptions */
  void resizeEvent(QResizeEvent* e);
};

#endif
