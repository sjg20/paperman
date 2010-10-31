/***************************************************************************
                          imagehistorybrowser description
                             -------------------
    begin                : Wed Jul 11 2001
    copyright            : (C) 2001 by Michael Herder
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

//renamed from QImageHistoryWidget to ImageHistoryBrowser on Fri Feb 01 2002

#ifndef IMAGEHISTORYBROWSER_H
#define IMAGEHISTORYBROWSER_H

#include <qdatetime.h>
#include <qwidget.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qmap.h>
#include <qstring.h>
#include <qstringlist.h>
/**
  *@author M. Herder
  */

class DirectoryListView;

class DragIconView;
class QButtonGroup;
class QComboBox;
class QDragListBox;
class QHBox;
class QIconViewItem;
class QLabel;
class QLineEdit;
class QListViewItem;
class QListViewItemExt;
class QPushButton;
class QRadioButton;
class QResizeEvent;
class QSplitter;
class QToolButton;
class QWidgetStack;

class ImageHistoryBrowser : public QWidget
{
Q_OBJECT
public:
  enum Mode
  {
    Mode_History      = 0,
    Mode_ImageBrowser = 1
  };
  enum ViewMode
  {
    ViewMode_List = 0,
    ViewMode_Icon = 1
  };
  enum PreviewMode
  {
    PreviewMode_Temporary      = 0,
    PreviewMode_DefaultMissing = 1,
    PreviewMode_CreateMissing  = 2
  };
  ImageHistoryBrowser(QWidget* parent=0,const char* name=0,WFlags f=WStyle_Customize |
                      WStyle_NormalBorder | WStyle_SysMenu | WStyle_MinMax | WStyle_ContextHelp);
  ~ImageHistoryBrowser();
  /**  */
  ImageHistoryBrowser::PreviewMode previewMode();
  /** */
  bool loadHistory();
  /**  */
  void setPreviewMode(ImageHistoryBrowser::PreviewMode pm);
  /**  */
  QString historyFilename();
  /**  */
  void setHistoryFilename(QString filename);
  /**  */
  void addHistoryItem(QString filename);
  /**  */
  bool saveHistory();
private: // Private methods
  /**  */
  void initWidget();
  /**  */
  void createContents();
  /**  */
  void createListContents();
  /**  */
  void createIconViewContents();
  /** */
  bool createPreviewImage(QString& absfilename);
  /**  */
  void setPreviewPixmap(QListViewItemExt* li,QString& filename);
  /**  */
  bool checkHistoryItemNumber();
  /**  */
  /** No descriptions */
  void addHistoryPath(QString& path);
  void createWhatsThisHelp();
  /** No descriptions */
  void checkUpButton();
private: // Private attributes
  /** */
  QStringList mBookmarkList;
  /** */
  QStringList mHistoryList;
  /** */
  int mHistoryIndex;
  /** */
  QMapIterator <QString,QDateTime> mHistoryMapIterator;
  /** */
  bool mHistoryValid;
  /** */
  bool mListContentsValid;
  /** */
  bool mIconContentsValid;
  /** */
  bool mRunning;
  /** */
  QMap <QString,QDateTime> mHistoryMap;
  /** */
  QString mHistoryFilename;
  /** */
  QHBox* mpModeHBox;
  /** */
  QLabel* mpModeLabel;
  /** */
  QLineEdit* mpModeLineEdit;
  /** */
  ViewMode mViewMode;
  /** */
  Mode mMode;
  /** */
  PreviewMode mPreviewMode;
  /**  */
  QWidgetStack* mpWidgetStack;
  /**  */
  QSplitter* mpSplitter;
  /**  */
  DragIconView* mpIconView;
  /**  */
  DirectoryListView* mpDirectoryListView;
  /**  */
  QDragListBox* mpListView;
  /**  */
  QPushButton* mpCloseButton;
  /**  */
  QButtonGroup* mpModeButtonGroup;
  /**  */
  QToolButton* mpIconModeButton;
  /**  */
  QToolButton* mpListModeButton;
  /**  */
  QToolButton* mpHistoryModeButton;
  /**  */
  QToolButton* mpBrowserModeButton;
  /**  */
  QToolButton* mpDeleteHistoryButton;
  /**  */
  QToolButton* mpCreatePreviewsButton;
  /**  */
  QToolButton* mpHomeButton;
  /**  */
  QToolButton* mpUpButton;
  /**  */
  QToolButton* mpBackButton;
  /**  */
  QToolButton* mpForwardButton;
  /**  */
  QToolButton* mpAddButton;
  /**  */
  QToolButton* mpSubButton;
  /**  */
  QToolButton* mpDelBookmarksButton;
  /**  */
  QComboBox* mpBookmarkCombo;
  /**  */
  QHBox* mpNavigationHBox;
  /**  */
  QString mDirPath;
  /** */
  QDir mDir;
public slots: // Public slots
  /**  */
  void show();
  /**  */
  void slotUpdateHistory();
private slots: // Private slots
  /**  */
  void slotStartDrag(QListViewItem*);
  /**  */
  void slotStartIconDrag(DragIconView* iconview);
  /**  */
  void slotChangeMode(int i);
  /**  */
  void slotChangeViewMode(int i);
  /**  */
  void slotDirectoryChanged(QString path);
  /**  */
  void slotCreatePreview();
  /**  */
  void slotItemDoubleClicked(QListViewItem* li);
  /**  */
  void slotDeleteHistory();
  /** No descriptions */
  void slotIconDoubleClicked(QIconViewItem* item);
  /** No descriptions */
  void slotDirForward();
  /** No descriptions */
  void slotDirBack();
  /** No descriptions */
  void slotDirUp();
  /** No descriptions */
  void slotClearBookmarks();
  /** No descriptions */
  void slotDeleteBookmark();
  /** No descriptions */
  void slotAddBookmark();
  /** No descriptions */
  void slotHome();
  /** No descriptions */
  void slotBookmarkSelected(const QString& bm);
protected:
  /**  */
  void resizeEvent(QResizeEvent* e);
  /**  */
  void closeEvent(QCloseEvent* e);
  /**  */
  void moveEvent(QMoveEvent* e);
signals: // Signals
  /**  */
  void signalItemDoubleClicked(QString text);
};

#endif
