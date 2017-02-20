/***************************************************************************
                          imagehistorybrowser.cpp  -  description
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

#include "directorylistview.h"
#include "dragiconview.h"
#include "qdraglistbox.h"
#include "iconviewitemext.h"
#include "imagehistorybrowser.h"
#include "imageiosupporter.h"
#include "qimageioext.h"
#include "qlistviewitemext.h"
#include "qxmlconfig.h"
#include "./pics/add.xpm"
#include "./pics/browser_mode.xpm"
#include "./pics/create_previews.xpm"
#include "./pics/delete_bookmarks.xpm"
#include "./pics/delete_history.xpm"
#include "./pics/dir_up.xpm"
#include "./pics/dir_back.xpm"
#include "./pics/dir_forward.xpm"
#include "./pics/history_mode.xpm"
#include "./pics/home.xpm"
#include "./pics/icon_mode.xpm"
#include "./pics/list_mode.xpm"
#include "./pics/sub.xpm"

#include <qapplication.h>
#include <qarray.h>
#include <qbuttongroup.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qcstring.h>
#include <qdatastream.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qdom.h>
#include <qdragobject.h>
#include <qevent.h>
#include <qfile.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qhbox.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpixmap.h>
#include <qprogressdialog.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qstring.h>
#include <qstrlist.h>
#include <qsplitter.h>
#include <qtextstream.h>
#include <qtimer.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qwidgetstack.h>

ImageHistoryBrowser::ImageHistoryBrowser(QWidget* parent,const char* name,WFlags f)
                    :QWidget(parent,name,f)
{
  mRunning = false;
  mListContentsValid = false;
  mIconContentsValid = false;
  mHistoryValid = false;
  mHistoryFilename = QString::null;
  mHistoryIndex = -1;
  mHistoryList.clear();
  mBookmarkList.clear();
  mMode = Mode_History;
  mViewMode = ViewMode_List;
  mPreviewMode = PreviewMode_DefaultMissing;
  setCaption(tr("QuiteInsane - History"));
  initWidget();
  createWhatsThisHelp();
}
ImageHistoryBrowser::~ImageHistoryBrowser()
{
}
/**  */
void ImageHistoryBrowser::initWidget()
{
  mDirPath = xmlConfig->stringValue("BROWSER_PATH",QDir::homePath());
  int browsemode = xmlConfig->intValue("BROWSER_MODE",0);
  int viewmode = xmlConfig->intValue("BROWSER_VIEW_MODE",0);
  mBookmarkList = xmlConfig->stringList("BROWSER_BOOKMARKS");

  QGridLayout* mainlayout = new QGridLayout(this,4,3);
  mainlayout->setSpacing(6);
  mainlayout->setMargin(6);
  mainlayout->setColStretch(0,1);
  mainlayout->setRowStretch(2,1);
//label and (tool-)buttons - 1st row
  mpModeHBox = new QHBox(this);
  mpModeLabel = new QLabel(tr("History"),mpModeHBox);
  mpModeLineEdit = new QLineEdit(mpModeHBox);
  mpModeLineEdit->hide();

  QHBox* modehb = new QHBox(mpModeHBox);
  modehb->setSpacing(3);
  mpHistoryModeButton = new QToolButton(modehb);
  mpHistoryModeButton->setPixmap(QPixmap((const char **)history_mode_xpm));
  mpHistoryModeButton->setToggleButton(true);

  mpBrowserModeButton = new QToolButton(modehb);
  mpBrowserModeButton->setPixmap(QPixmap((const char **)browser_mode_xpm));
  mpBrowserModeButton->setToggleButton(true);

  QHBox* viewhb = new QHBox(mpModeHBox);
  viewhb->setSpacing(3);

  mpListModeButton = new QToolButton(viewhb);
  mpListModeButton->setPixmap(QPixmap((const char **)list_mode_xpm));
  mpListModeButton->setToggleButton(true);

  mpIconModeButton = new QToolButton(viewhb);
  mpIconModeButton->setPixmap(QPixmap((const char **)icon_mode_xpm));
  mpIconModeButton->setToggleButton(true);

  mpCreatePreviewsButton = new QToolButton(mpModeHBox);
  mpCreatePreviewsButton->setPixmap(QPixmap((const char **)create_previews_xpm));
  connect(mpCreatePreviewsButton,SIGNAL(clicked()),
          this,SLOT(slotCreatePreview()));

  mpDeleteHistoryButton = new QToolButton(mpModeHBox);
  mpDeleteHistoryButton->setPixmap(QPixmap((const char **)delete_history_xpm));
  connect(mpDeleteHistoryButton,SIGNAL(clicked()),
          this,SLOT(slotDeleteHistory()));

  mpModeHBox->setStretchFactor(mpModeLabel,1);
  mpModeHBox->setSpacing(5);
  mainlayout->addMultiCellWidget(mpModeHBox,0,0,0,2);
  QButtonGroup* gbox1 = new QButtonGroup(1,Qt::Horizontal,QString::null,this);
  gbox1->hide();
  gbox1->insert(mpHistoryModeButton,0);
  gbox1->insert(mpBrowserModeButton,1);
  gbox1->setExclusive(true);
  gbox1->setButton(browsemode);
  connect(gbox1,SIGNAL(clicked(int)),this,SLOT(slotChangeMode(int)));

  QButtonGroup* gbox2 = new QButtonGroup(1,Qt::Horizontal,QString::null,this);
  gbox2->hide();
  gbox2->insert(mpListModeButton,0);
  gbox2->insert(mpIconModeButton,1);
  gbox2->setExclusive(true);
  gbox2->setButton(viewmode);
  connect(gbox2,SIGNAL(clicked(int)),this,SLOT(slotChangeViewMode(int)));

  //2nd row - bookmarks and navigation buttons
  mpNavigationHBox = new QHBox(this);
  mpNavigationHBox->setSpacing(5);
  new QLabel(tr("Bookmarks:"),mpNavigationHBox);

  mpBookmarkCombo = new QComboBox(mpNavigationHBox);
  mpNavigationHBox->setStretchFactor(mpBookmarkCombo,1);
  mainlayout->addMultiCellWidget(mpNavigationHBox,1,1,0,2);
  mpBookmarkCombo->insertStringList(mBookmarkList);

  mpAddButton = new QToolButton(mpNavigationHBox);
  mpAddButton->setPixmap(QPixmap((const char **)add_xpm));
  mpAddButton->setFixedHeight(mpBookmarkCombo->sizeHint().height());

  mpSubButton = new QToolButton(mpNavigationHBox);
  mpSubButton->setPixmap(QPixmap((const char **)sub_xpm));
  mpSubButton->setFixedHeight(mpBookmarkCombo->sizeHint().height());

  mpDelBookmarksButton = new QToolButton(mpNavigationHBox);
  mpDelBookmarksButton->setPixmap(QPixmap((const char **)delete_bookmarks_xpm));
  mpDelBookmarksButton->setFixedHeight(mpBookmarkCombo->sizeHint().height());

  QWidget* dummy = new QWidget(mpNavigationHBox);
  dummy->setFixedWidth(8);

  mpHomeButton = new QToolButton(mpNavigationHBox);
  mpHomeButton->setPixmap(QPixmap((const char **)home_xpm));

  mpUpButton = new QToolButton(mpNavigationHBox);
  mpUpButton->setPixmap(QPixmap((const char **)dir_up_xpm));

  mpBackButton = new QToolButton(mpNavigationHBox);
  mpBackButton->setPixmap(QPixmap((const char **)dir_back_xpm));

  mpForwardButton = new QToolButton(mpNavigationHBox);
  mpForwardButton->setPixmap(QPixmap((const char **)dir_forward_xpm));

  mpForwardButton->setEnabled(false);
  mpBackButton->setEnabled(false);

  mpNavigationHBox->hide();
  connect(mpBackButton,SIGNAL(clicked()),this,SLOT(slotDirBack()));
  connect(mpForwardButton,SIGNAL(clicked()),this,SLOT(slotDirForward()));
  connect(mpUpButton,SIGNAL(clicked()),this,SLOT(slotDirUp()));
  connect(mpHomeButton,SIGNAL(clicked()),this,SLOT(slotHome()));

  connect(mpAddButton,SIGNAL(clicked()),this,SLOT(slotAddBookmark()));
  connect(mpSubButton,SIGNAL(clicked()),this,SLOT(slotDeleteBookmark()));
  connect(mpDelBookmarksButton,SIGNAL(clicked()),this,SLOT(slotClearBookmarks()));

  connect(mpBookmarkCombo,SIGNAL(activated(const QString&)),
          this,SLOT(slotBookmarkSelected(const QString&)));
//splitter
  mpSplitter = new QSplitter(this);
//left side of splitter hold a widget stack wich display either
//an icon view or a list view
  mpWidgetStack = new QWidgetStack(mpSplitter);
  mpListView = new QDragListBox(mpWidgetStack);
  mpListView->setSelectionMode(QListView::Extended);
  mpListView->setAllColumnsShowFocus(true);
  mpListView->addColumn(tr("Preview"));
  mpListView->addColumn(tr("Filename"));
  mpListView->addColumn(tr("Size"));
  mpListView->addColumn(tr("Date of creation"));
  mpListView->setItemMargin(2);

  mpIconView = new DragIconView(mpWidgetStack);
  mpIconView->setSelectionMode(QIconView::Extended);
  mpIconView->setAutoArrange(true);
  mpIconView->setItemsMovable(false);
  mpIconView->setResizeMode(QIconView::Adjust);
  mpIconView->setGridX(90);
  mpWidgetStack->addWidget(mpListView,0);
  mpWidgetStack->addWidget(mpIconView,1);
  mpWidgetStack->raiseWidget(0);

  mainlayout->addMultiCellWidget(mpSplitter,2,2,0,2);
  connect(mpListView,SIGNAL(doubleClicked(QListViewItem*)),
          this,SLOT(slotItemDoubleClicked(QListViewItem*)));
  connect(mpListView,SIGNAL(signalDragStarted(QListViewItem*)),
          this,SLOT(slotStartDrag(QListViewItem*)));	
  connect(mpIconView,SIGNAL(signalDragStarted(DragIconView*)),
          this,SLOT(slotStartIconDrag(DragIconView*)));	
  connect(mpIconView,SIGNAL(doubleClicked(QIconViewItem*)),
          this,SLOT(slotIconDoubleClicked(QIconViewItem*)));

//right side of splitter holds a DirectoryListView, which is only
//visible in image browser mode
  mpDirectoryListView = new DirectoryListView(mpSplitter);
  mpDirectoryListView->hide();
  mpDirectoryListView->setDirectory(mDirPath);
  addHistoryPath(mDirPath);
  connect(mpDirectoryListView,SIGNAL(signalDirectoryChanged(QString)),
          this,SLOT(slotDirectoryChanged(QString)));
//close button
  mpCloseButton = new QPushButton(tr("&Close"),this);
  mainlayout->addWidget(mpCloseButton,3,2);
  connect(mpCloseButton,SIGNAL(clicked()),this,SLOT(close()));

  mainlayout->activate();
  slotChangeMode(browsemode);
  slotChangeViewMode(viewmode);
}


/**
Creates a preview under absfilename +"/.xvpics"
absfilename must be an existing and valid image file
*/
bool ImageHistoryBrowser::createPreviewImage(QString& absfilename)
{
  if(mMode == Mode_History)
  {
    if(!xmlConfig->boolValue("HISTORY_ENABLED") ||
       !xmlConfig->boolValue("HISTORY_CREATE_PREVIEWS"))
      return false;
  }

  QFileInfo fi(absfilename);
  QString xvfilename;
  QImage im;
  QPixmap pix;

  QDir dir(fi.dirPath(true)+"/.xvpics");
  //If the preview folder doesn't exist, try to create it.
  if(!dir.exists())
  {
    if(!dir.mkdir(fi.dirPath(true)+"/.xvpics")) return false;
  }
  xvfilename = fi.dirPath(true) + "/.xvpics/" + fi.fileName();
  im.reset();
  im.load(absfilename);
  if(im.isNull()) return false;
  int x,y;
  double f;
  x = im.width();
  y = im.height();
  if(x <= 80 && y <= 60)
  {
    f=1.0;
  }
  else
  {
    f = 80.0/double(x);
    if(60.0/double(y) < f) f = 60.0/double(y);
  }
  im = im.smoothScale(int(double(x)*f),int(double(y)*f));
  return (im.save(xvfilename,"PNM7"));
}

/**  */
void ImageHistoryBrowser::show()
{
  int x;
  int y;
  x = xmlConfig->intValue("BROWSER_SIZE_X",500);
  y = xmlConfig->intValue("BROWSER_SIZE_Y",400);
  resize(x,y);
  x = xmlConfig->intValue("BROWSER_POS_X",0);
  y = xmlConfig->intValue("BROWSER_POS_Y",0);
  move(x,y);
  QWidget::show();
  qApp->processEvents();
  createContents();
}
/**  */
void ImageHistoryBrowser::moveEvent(QMoveEvent* e)
{
  QWidget::moveEvent(e);
  xmlConfig->setIntValue("BROWSER_POS_X",x());
  xmlConfig->setIntValue("BROWSER_POS_Y",y());
}
/**  */
void ImageHistoryBrowser::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  //it's possible to get a resize-event, even if the widget
  //isn't visible; this would cause problems when we try to
  //restore the saved size/position
  if(isVisible())
  {
    xmlConfig->setIntValue("BROWSER_SIZE_X",width());
    xmlConfig->setIntValue("BROWSER_SIZE_Y",height());
  }
}
/**  */
void ImageHistoryBrowser::closeEvent(QCloseEvent* e)
{
  if(mRunning == true)
  {
    mRunning = false;
    if(mViewMode == ViewMode_List)
    {
      mListContentsValid = false;
    }
    else if(mViewMode == ViewMode_Icon)
    {
      mIconContentsValid = false;
    }
  }
  QWidget::closeEvent(e);
}
/**  */
void ImageHistoryBrowser::createContents()
{
  if(!isVisible())
    return;
  if(mViewMode == ViewMode_List)
  {
    createListContents();
  }
  else if(mViewMode == ViewMode_Icon)
  {
    createIconViewContents();
  }
}
/**  */
void ImageHistoryBrowser::createListContents()
{
  ImageIOSupporter iiosup;
  const QFileInfoList* info_list = 0;
  QFileInfoListIterator* info_list_iterator = 0;
  QListViewItemExt* li;
  QListViewItemExt* li_last;
  QFileInfo* fi = 0;
  QDateTime dt;
  QImage im;
  QString filename;
  QString xvfilename;
  QString qs;
  QString filter_string;
  int row=0;

  if((mViewMode == ViewMode_List) && mListContentsValid)
    return;
  if((mViewMode == ViewMode_Icon) && mIconContentsValid)
    return;
  if(xmlConfig->boolValue("IMAGEBROWSER_EXTENSION_ONLY",false))
    filter_string = iiosup.getInFilterString();
  mpListView->clear();
  li_last = 0;
  if(mMode == Mode_ImageBrowser)
  {
    mDir.setPath(mDirPath);
    mDir.setFilter( QDir::Files | QDir::Hidden | QDir::NoSymLinks );
    info_list = mDir.entryInfoList();
    //info_list == 0 means, that the directory isn't readable or
    //doesn't exist; in this case, we change to the users home dir
    if(info_list == 0)
    {
      mDirPath = QDir::homePath();
      mDir.setPath(mDirPath);
      if(xmlConfig->boolValue("IMAGEBROWSER_EXTENSION_ONLY",false))
        mDir.setNameFilter(filter_string);
      info_list = mDir.entryInfoList();
      //should not happen, but you never know
      if(info_list == 0)
        return;
      xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
      mpDirectoryListView->setDirectory(mDirPath);
    }
    info_list_iterator = new QFileInfoListIterator(*info_list);
    mpModeLabel->setText(tr("Directory:"));
    mpModeLineEdit->setText(QString("%1").arg(mDirPath));
  }
  else
  {
    if(!mHistoryValid) loadHistory();
    mpModeLabel->setText(tr("History"));
    mHistoryMapIterator = mHistoryMap.begin();
  }
  mRunning = true;
  mpListView->setSorting(-1);
  int proc_cnt = 0;
  while(mRunning)
  {
    if(mMode == Mode_ImageBrowser)
    {
      if(!info_list_iterator || !info_list || mListContentsValid)
      {
        mpListView->setSorting(0);
        mRunning = false;
        return;
      }
      if(!(fi=info_list_iterator->current()))
      {
        mpListView->setSorting(0);
        mListContentsValid = true;
        mRunning = false;
        return;
      }
    }
    else //mode history
    {
      if(mListContentsValid)
      {
        mpListView->setSorting(0);
        mRunning = false;
        return;
      }
      if(mHistoryMapIterator != mHistoryMap.end())
      {
        fi = new QFileInfo(mHistoryMapIterator.key());
        if(!fi)
        {
          mpListView->setSorting(0);
          return;
        }
      }
      else
      {
        mpListView->setSorting(0);
        mListContentsValid = true;
        mRunning = false;
        return;
      }
    }

    if(fi->isFile())
    {
      filename = fi->absFilePath();
      QString i_format = QImage::imageFormat(filename);
      if(!i_format.isEmpty())
      {//is image
        qs.sprintf("%i",fi->size());
        if(mMode == Mode_ImageBrowser)
        { //get date of last modification
          dt = fi->lastModified();
        }
        else
        { //get date of creation
          dt = mHistoryMapIterator.data();
        }
        if(!li_last)
        {
          li = new QListViewItemExt(mpListView,
                                    i_format,
                                    fi->fileName(),qs,dt.toString());
        }
        else
        {
          //If there is a previous listview item, insert the new
          //item right behind.
          li = new QListViewItemExt(mpListView,li_last,
                                    i_format,
                                    fi->fileName(),qs,dt.toString());
        }
        li_last = li;
        li->setHiddenText(filename);
        li->setHiddenDateTime(dt);
        xvfilename = fi->dirPath(true) + "/.xvpics/" + fi->fileName();
        setPreviewPixmap(li,xvfilename);
        row +=1;
      }
    }                          // goto next list element

    if(mMode == Mode_ImageBrowser)
    {
      ++(*info_list_iterator);
    }
    else
    {
      ++mHistoryMapIterator;
      if(fi) delete fi;
    }
    if((proc_cnt % 10) == 0)
      qApp->processEvents();
    ++proc_cnt;
  }
  mpListView->setSorting(0);
}
/**  */
void ImageHistoryBrowser::createIconViewContents()
{
  ImageIOSupporter iiosup;
  const QFileInfoList* info_list = 0;
  QFileInfoListIterator* info_list_iterator = 0;
  IconViewItemExt* ii;
  IconViewItemExt* ii_last;
  QFileInfo* fi = 0;
  QDateTime dt;
  QImage im;
  QString filename;
  QString xvfilename;
  QString qs;
  QString filter_string;
  int row=0;

  if(xmlConfig->boolValue("IMAGEBROWSER_EXTENSION_ONLY",false))
    filter_string = iiosup.getInFilterString();
  if((mViewMode == ViewMode_List) && mListContentsValid)
    return;
  if((mViewMode == ViewMode_Icon) && mIconContentsValid)
    return;
  mpIconView->clear();
  ii_last = 0;
  if(mMode == Mode_ImageBrowser)
  {
    mDir.setPath(mDirPath);
    mDir.setFilter( QDir::Files | QDir::Hidden | QDir::NoSymLinks );
    if(xmlConfig->boolValue("IMAGEBROWSER_EXTENSION_ONLY",false))
      mDir.setNameFilter(filter_string);
    info_list = mDir.entryInfoList();
    //info_list == 0 means, that the directory isn't readable or
    //doesn't exist; in this case, we change to the users home dir
    if(info_list == 0)
    {
      mDirPath = QDir::homePath();
      mDir.setPath(mDirPath);
      info_list = mDir.entryInfoList();
      //should not happen, but you never know
      if(info_list == 0)
        return;
      xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
      mpDirectoryListView->setDirectory(mDirPath);
    }
    info_list_iterator = new QFileInfoListIterator(*info_list);
    mpModeLabel->setText(tr("Directory:"));
    mpModeLineEdit->setText(QString("%1").arg(mDirPath));
  }
  else
  {
    if(!mHistoryValid) loadHistory();
    mpModeLabel->setText(tr("History"));
    mHistoryMapIterator = mHistoryMap.begin();
  }
  mRunning = true;
//  mpListView->setSorting(-1);
  int proc_cnt = 0;

  while(mRunning)
  {
    if(mMode == Mode_ImageBrowser)
    {
      if(!info_list_iterator || !info_list || mIconContentsValid)
      {
        mRunning = false;
        return;
      }
      if(!(fi=info_list_iterator->current()))
      {
        mIconContentsValid = true;
        mRunning = false;
        return;
      }
    }
    else //mode history
    {
      if(mIconContentsValid)
      {
        mRunning = false;
        return;
      }
      if(mHistoryMapIterator != mHistoryMap.end())
      {
        fi = new QFileInfo(mHistoryMapIterator.key());
        if(!fi) return;
      }
      else
      {
        mIconContentsValid = true;
        mRunning = false;
        return;
      }
    }

    if(fi->isFile())
    {
      filename = fi->absFilePath();
      if(QImage::imageFormat(filename))
      {//is image
        qs.sprintf("%i",fi->size());
        if(mMode == Mode_ImageBrowser)
        { //get date of last modification
          dt = fi->lastModified();
        }
        else
        { //get date of creation
          dt = mHistoryMapIterator.data();
        }
        xvfilename = fi->dirPath(true) + "/.xvpics/" + fi->fileName();
        QPixmap pix(xvfilename);
        if(!ii_last)
        {
          ii = new IconViewItemExt(mpIconView,fi->fileName(),pix);
        }
        else
        {
          //If there is a previous listview item, insert the new
          //item right behind.
          ii = new IconViewItemExt(mpIconView,ii_last,fi->fileName(),pix);
        }
        ii->setHiddenText(filename);
        ii_last = ii;
        row +=1;
      }
    }                          // goto next list element

    if(mMode == Mode_ImageBrowser)
    {
      ++(*info_list_iterator);
    }
    else
    {
      ++mHistoryMapIterator;
      if(fi) delete fi;
    }
    if((proc_cnt % 10) == 0)
      qApp->processEvents();
    ++proc_cnt;
  }
//  mpListView->setSorting(0);
}
/**  */
void ImageHistoryBrowser::slotChangeMode(int i)
{
  if(i == int(mMode)) return;
  xmlConfig->setIntValue("BROWSER_MODE",i);
  if(i == 0)
  {
    mMode = Mode_History;
    mpDeleteHistoryButton->setEnabled(true);
    setCaption(tr("QuiteInsane - History"));
    mpListView->setColumnText(3,tr("Date of creation"));
    mpDirectoryListView->hide();
    mpNavigationHBox->hide();
    if(!mHistoryValid) loadHistory();
    mpModeHBox->setStretchFactor(mpModeLabel,1);
    mpModeHBox->setStretchFactor(mpModeLineEdit,0);
    mpModeLineEdit->hide();
  }
  else
  {
    mMode = Mode_ImageBrowser;
    mpDeleteHistoryButton->setEnabled(false);
    setCaption(tr("QuiteInsane - Image browser"));
    mpListView->setColumnText(3,tr("Last modified"));
    mpDirectoryListView->show();
    mpNavigationHBox->show();
    mpModeHBox->setStretchFactor(mpModeLabel,0);
    mpModeHBox->setStretchFactor(mpModeLineEdit,1);
    mpModeLineEdit->show();
  }
  mListContentsValid = false;
  mIconContentsValid = false;
  createContents();
}
/**  */
void ImageHistoryBrowser::slotChangeViewMode(int i)
{
  if(i == int(mViewMode)) return;
  xmlConfig->setIntValue("BROWSER_VIEW_MODE",i);
  if(i == 0)
  {
    mViewMode = ViewMode_List;
    mpWidgetStack->raiseWidget(0);
  }
  else
  {
    mViewMode = ViewMode_Icon;
    mpWidgetStack->raiseWidget(1);
  }
  createContents();
}
/**  */
void ImageHistoryBrowser::slotDirectoryChanged(QString path)
{
  mDirPath = path;
  xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
  mRunning = false;
  mListContentsValid = false;
  mIconContentsValid = false;
  addHistoryPath(mDirPath);
  createContents();
}
/**  */
void ImageHistoryBrowser::slotCreatePreview()
{
  QListViewItemExt* li;
  IconViewItemExt* ii;
  QFileInfo* fi = 0;
  const QFileInfoList* info_list;
  QFileInfoListIterator* info_list_iterator;
  QString filename;
  QString xvfilename;
  QImage im;
  QPixmap pix;
  bool create_all = true;

  if((mViewMode == ViewMode_List) && !mListContentsValid)
    return;
  if((mViewMode == ViewMode_Icon) && !mIconContentsValid)
    return;

  QDir dir(mDirPath+"/.xvpics");
  if(!dir.exists())
  {
    if(!dir.mkdir(mDirPath+"/.xvpics")) return;
  }

  QDialog* dlg = new QDialog(this,0,true);
  dlg->setCaption(tr("Create preview images"));
  QGridLayout* dlglayout = new QGridLayout(dlg,2,3);
  dlglayout->setSpacing(5);
  dlglayout->setMargin(5);
  dlglayout->setColStretch(1,1);
  dlglayout->setRowStretch(0,1);
  QButtonGroup* bg = new QButtonGroup(1,Qt::Horizontal,tr("Create preview"),dlg);
  QRadioButton* radio1 = new QRadioButton(tr("Create &all preview images"),bg);
  QRadioButton* radio2 = new QRadioButton(tr("Create &missing preview images only"),bg);
  bg->setExclusive(true);
  bg->insert(radio1);
  bg->insert(radio2);
  radio1->setChecked(true);
  dlglayout->addMultiCellWidget(bg,0,0,0,2);

  QPushButton* pb1 = new QPushButton(tr("&OK"),dlg);
  dlglayout->addWidget(pb1,1,0);
  QPushButton* pb2 = new QPushButton(tr("&Cancel"),dlg);
  dlglayout->addWidget(pb2,1,2);

  connect(pb1,SIGNAL(clicked()),dlg,SLOT(accept()));
  connect(pb2,SIGNAL(clicked()),dlg,SLOT(reject()));

  if(dlg->exec())
  {
    if(radio1->isChecked())
      create_all = true;
    else
      create_all = false;
    QString filename;
    QString xvfilename;
    QString qs;
    unsigned int file_cnt;

    if(mMode == Mode_ImageBrowser)
    {
      mDir.setPath(mDirPath);
      mDir.setFilter( QDir::Files | QDir::Hidden | QDir::NoSymLinks );
      info_list = mDir.entryInfoList();
      //info_list == 0 means, that the directory isn't readable or
      //doesn't exist; in this case, we change to the users home dir
      if(info_list == 0)
      {
        mDirPath = QDir::homePath();
        mDir.setPath(mDirPath);
        info_list = mDir.entryInfoList();
        //should not happen, but you never know
        if(info_list == 0)
          return;
        xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
        mpDirectoryListView->setDirectory(mDirPath);
      }
      file_cnt = info_list->count();
      info_list_iterator = new QFileInfoListIterator(*info_list);
      QProgressDialog progress(tr("Creating preview..."),tr("Stop"),file_cnt,
                               this,0,true);
      progress.setCaption(tr("Create preview"));
      progress.setMinimumDuration(0);
      int p = 0;
      while((fi=info_list_iterator->current()))
      {
        progress.setProgress(p);
        progress.setLabelText(tr("Processing file %1 of %2").arg(p+1).arg(file_cnt));
        qApp->processEvents();
        if ( progress.wasCancelled() ) break;
        ++p;
        if(fi->isFile())
        {
          filename = fi->absFilePath();
          xvfilename = fi->dirPath(true) + "/.xvpics/" + fi->fileName();
          //if preview exists and user selected create missing, continue
          if(QFile::exists(xvfilename) && !create_all)
          {
            ++(*info_list_iterator);
            continue;
          }
          im.reset();
          im.load(filename);
          if(!im.isNull())
          {
            if(im.save(xvfilename,"PNM7"))
              im.reset();
          }
        }
        ++(*info_list_iterator);
      }
      progress.setProgress(file_cnt);
      mListContentsValid = false;
      mIconContentsValid = false;
    }
    else if(mMode == Mode_History)
    {
      if(mViewMode == ViewMode_List)
      {
        QListViewItemIterator it(mpListView);
        file_cnt = mpListView->childCount();
        QProgressDialog progress(tr("Creating preview..."),tr("Stop"),file_cnt,
                                 this,0,true);
        progress.setCaption(tr("Create preview"));
        progress.setMinimumDuration(0);
        int p = 0;
        for(;it.current();++it)
        {
          progress.setProgress(p);
          progress.setLabelText(tr("Processing file %1 of %2").arg(p+1).arg(file_cnt));
          qApp->processEvents();
          if ( progress.wasCancelled() ) break;
          ++p;
          li = (QListViewItemExt*)it.current();
          if(!li)
            continue;
          filename = li->hiddenText();
          QFileInfo fi(filename);
          QDir dir(fi.dirPath(true)+"/.xvpics");
          if(!dir.exists())
          {
            if(!dir.mkdir(fi.dirPath(true)+"/.xvpics"))
              continue;
          }
          xvfilename = fi.dirPath(true) + "/.xvpics/" + fi.fileName();
          //if preview exists and user selected create missing, continue
          if(QFile::exists(xvfilename) && !create_all)
            continue;
          im.reset();
          im.load(filename);
          if(im.isNull())
            continue;
          if(im.save(xvfilename,"PNM7"))
          {
            //Reload it; the saved image looks different
            im.reset();
            setPreviewPixmap(li,xvfilename);
          }
        }
        progress.setProgress(file_cnt);

      }
      else if(mViewMode == ViewMode_Icon)
      {
        ii = (IconViewItemExt*) mpIconView->firstItem();
        file_cnt = mpIconView->count();
        QProgressDialog progress(tr("Creating preview..."),tr("Stop"),file_cnt,
                                 this,0,true);
        progress.setCaption(tr("Create preview"));
        progress.setMinimumDuration(0);
        int p = 0;
        while(ii)
        {
          progress.setProgress(p);
          progress.setLabelText(tr("Processing file %1 of %2").arg(p+1).arg(file_cnt));
          qApp->processEvents();
          if ( progress.wasCancelled() ) break;
          ++p;
          filename = ii->hiddenText();
          QFileInfo fi(filename);
          QDir dir(fi.dirPath(true)+"/.xvpics");
          if(!dir.exists())
          {
            if(!dir.mkdir(fi.dirPath(true)+"/.xvpics"))
              continue;
          }
          xvfilename = fi.dirPath(true) + "/.xvpics/" + fi.fileName();
          //if preview exists and user selected create missing, continue
          if(QFile::exists(xvfilename) && !create_all)
          {
            ii = (IconViewItemExt*)ii->nextItem();
            continue;
          }
          im.reset();
          im.load(filename);
          if(im.isNull())
          {
            ii = (IconViewItemExt*)ii->nextItem();
            continue;
          }
          if(im.save(xvfilename,"PNM7"))
          {
            QPixmap pix(xvfilename);
            //Reload it; the saved image looks different
            im.reset();
            ii->setPixmap(pix,false);
          }
          ii = (IconViewItemExt*)ii->nextItem();
        }
        progress.setProgress(file_cnt);

      }
    }
  }
  if(dlg)
    delete dlg;
  createContents();
}
/**  */
ImageHistoryBrowser::PreviewMode ImageHistoryBrowser::previewMode()
{
  return mPreviewMode;
}
/**  */
void ImageHistoryBrowser::setPreviewMode(ImageHistoryBrowser::PreviewMode pm)
{
  mPreviewMode = pm;
}
/**  */
void ImageHistoryBrowser::setHistoryFilename(QString filename)
{
  mHistoryFilename = filename;
}
/**  */
QString ImageHistoryBrowser::historyFilename()
{
  return mHistoryFilename;
}
/**  */
bool ImageHistoryBrowser::loadHistory()
{
  if(mHistoryFilename == QString::null) return false;
  QFile histfile(mHistoryFilename);
  if (!histfile.open(IO_ReadOnly))
  {
    //If the history file doesn't exist, but history is requested
    //create an empty file and return true;
    if((!QFile::exists(mHistoryFilename)) &&
       (xmlConfig->boolValue("HISTORY_ENABLED")))
    {
       mHistoryValid = true;
       return saveHistory();
    }
    // error opening file
    return false;
  }
  // open dom document
  QDomDocument doc("QImageHistory");
  if (!doc.setContent(&histfile))
  {
    return false;
  }
  histfile.close();

  // check the doc type
  if (doc.doctype().name() != "QImageHistory")
  {
    // wrong file type
    return false;
  }
  QDomElement root = doc.documentElement();
  // get list of items
  QDomNodeList nodes = root.elementsByTagName("image_history_item");
  QDomElement ele;
  // iterate over the items
  for (unsigned n=0; n<nodes.count(); ++n)
  {
    if (nodes.item(n).isElement())
    {
      ele = nodes.item(n).toElement();
      if(ele.hasAttribute("filename"))
      {
        QDateTime dt;
        QDate d;
        QTime t;
        int i_year,i_month,i_day;
        int i_hour,i_min,i_sec;
        //get date
        if(ele.hasAttribute("year"))
          i_year = ele.attribute("year").toInt();
        else
          i_year = 1900;
        if(ele.hasAttribute("month"))
          i_month = ele.attribute("month").toInt();
        else
          i_month = 1;
        if(ele.hasAttribute("day"))
          i_day = ele.attribute("day").toInt();
        else
          i_day = 1;
        d.setYMD(i_year,i_month,i_day);
        //get time
        if(ele.hasAttribute("hour"))
          i_hour = ele.attribute("hour").toInt();
        else
          i_hour = 0;
        if(ele.hasAttribute("minute"))
          i_min = ele.attribute("minute").toInt();
        else
          i_min = 0;
        if(ele.hasAttribute("second"))
          i_sec = ele.attribute("second").toInt();
        else
          i_sec = 0;
        t.setHMS(i_hour,i_min,i_sec);
        dt.setDate(d);
        dt.setTime(t);
        mHistoryMap[ele.attribute("filename")] =dt;
      }
    }
  }
  mHistoryValid = true;
  return true;
}
/**  */
void ImageHistoryBrowser::slotUpdateHistory()
{
  //ignore, when in browser mode
  if(mMode == Mode_ImageBrowser) return;
  //reload history file
  if(loadHistory())
  {
    mIconContentsValid = false;
    mListContentsValid = false;
    createContents();
  }
}
/**  */
void ImageHistoryBrowser::addHistoryItem(QString filename)
{
  bool contents_ok = true;
  bool item_replaced = false;
  if(!xmlConfig->boolValue("HISTORY_ENABLED")) return;
  if(!mHistoryValid) loadHistory();
  QFileInfo fi(filename);
  //If the map already contains this filename,
  //replace date.
  QDateTime dt = fi.lastModified();
  if(mHistoryMap.contains(filename))
  {
    mHistoryMap.replace(filename,dt);
    item_replaced = true;
  }
  else
  {
    mHistoryMap.insert(filename,dt);
  }
  contents_ok = checkHistoryItemNumber();
  //If preview creation is requested, create preview;
  //remove an already existing preview otherwise;
  if(xmlConfig->boolValue("HISTORY_CREATE_PREVIEWS"))
  {
    createPreviewImage(filename);
  }
  else
  {
    if(QFile::exists(fi.dirPath(true) + "/.xvpics/" + fi.fileName()))
      QFile::remove(fi.dirPath(true) + "/.xvpics/" + fi.fileName());
  }
  //If the file isn't in the history map already,
  //then we have to create a new listview item.
  if(mMode == Mode_History)
  {
    if(contents_ok && !item_replaced)
    {
      QString qs;
      QListViewItemExt* li;
      IconViewItemExt* ii;
      qs.sprintf("%i",fi.size());
      li = new QListViewItemExt(mpListView,QImage::imageFormat(filename),
                                          fi.fileName(),qs,dt.toString());
      li->setHiddenDateTime(dt);
      li->setHiddenText(filename);
      qs = fi.dirPath(true) + "/.xvpics/" + fi.fileName();
      setPreviewPixmap(li,qs);
      QPixmap pix(qs);
      if(pix.isNull())
      {
        pix.resize(80,60);
        pix.fill(QColor(255,255,255));
      }
      ii = new IconViewItemExt(mpIconView,fi.fileName(),pix);
      ii->setHiddenText(filename);
    }
    else
    {
      mListContentsValid = false;
      mIconContentsValid = false;
      createContents();
    }
  }
}
/**  */
bool ImageHistoryBrowser::saveHistory()
{
  if(!xmlConfig->boolValue("HISTORY_ENABLED") || !mHistoryValid)
    return false;
  QString qs;
  QDomDocument doc("QImageHistory");

  // create the root element
  QDomElement root = doc.createElement(doc.doctype().name());
  root.setAttribute("version","1");

  //iterate over the options
  QMap<QString,QDateTime>::Iterator it;
  QDomElement option;
  for (it = mHistoryMap.begin(); it != mHistoryMap.end(); ++it)
  {
    //create an option element
    option = doc.createElement("image_history_item");
    option.setAttribute("filename", it.key());
    option.setAttribute("year",qs.setNum(it.data().date().year()));
    option.setAttribute("month",qs.setNum(it.data().date().month()));
    option.setAttribute("day",qs.setNum(it.data().date().day()));
    option.setAttribute("hour",qs.setNum(it.data().time().hour()));
    option.setAttribute("minute",qs.setNum(it.data().time().minute()));
    option.setAttribute("second",qs.setNum(it.data().time().second()));
    root.appendChild(option);
  }
  doc.appendChild(root);

  // open file
  QFile conffile(mHistoryFilename);
  if (!conffile.open(IO_WriteOnly))
  {
    // error opening file
    return false;
  }
  // write it
  QTextStream textstream(&conffile);
  doc.save(textstream, 0);
  conffile.close();
  return true;
}
/**  */
void ImageHistoryBrowser::setPreviewPixmap(QListViewItemExt* li,QString& filename)
{
  if((!li) || (filename == QString::null)) return;

  QImage im(filename);
  QPixmap pix;

  if(!im.isNull())
  {
    int x,y;
    double f;
    x = im.width();
    y = im.height();
    if(x <= 100 && y <= 100)
    {
      f=1.0;
    }
    else
    {
      if(x>y)
        f = 100.0/double(x);
      else
        f = 100.0/double(y);
    }
    im = im.smoothScale(int(double(x)*f),int(double(y)*f));
    pix.convertFromImage(im);
    if(!pix.isNull())
    {
      if(li)
      {
        li->setText(0,QString::null);
        li->setPixmap(0,pix);
      }
    }
  }
}
/**  */
void ImageHistoryBrowser::slotDeleteHistory()
{
  //let user confirm
  int ret;
  ret = QMessageBox::warning(this,tr("Delete history"),
                             tr("Are you sure, that you want to delete\n"
                             "the history ?\n"
                             "It can not be restored."),
                             tr("&Delete"),tr("&Cancel"),0,0,1);
  if(ret == 1) return;
  QFile::remove(mHistoryFilename);
  mpListView->clear();
  mpIconView->clear();
  mHistoryMap.clear();
  mHistoryFilename = QString::null;
  mHistoryValid = false;
}
/**  */
bool ImageHistoryBrowser::checkHistoryItemNumber()
{
  QDateTime date_time;
  QString qs;
  if(!xmlConfig->boolValue("HISTORY_LIMIT_ENTRIES")) return true;
  unsigned int max_num;
  max_num = (unsigned int) xmlConfig->intValue("HISTORY_MAX_ENTRIES");
  if(mHistoryMap.count() <= max_num) return true;
  //If number of items > max_num, remove oldest entry until
  //number of items == max_num
  while(mHistoryMap.count() > max_num)
  {
    qs = QString::null;
    QMap <QString,QDateTime>::Iterator it;
    date_time = mHistoryMap.begin().data();
    for(it=mHistoryMap.begin();it!=mHistoryMap.end();++it )
    {
      if(date_time >= it.data())
      {
        date_time = it.data();
        qs = it.key();
      }
    }
    if(qs != QString::null)
      mHistoryMap.remove(qs);
    else
      break;
  }
  return false;
}
/**  */
void ImageHistoryBrowser::slotItemDoubleClicked(QListViewItem* li)
{
  QListViewItemExt* liext;
  liext = (QListViewItemExt*) li;
  if(!QFile::exists(liext->hiddenText()))
  {
    QMessageBox::information(this,tr("Information"),
                             tr("The file doesn't exist.\n"
                                "The history will be updated."),
                             tr("OK"));
    mListContentsValid = false;
    createContents();
    return;
  }
  emit signalItemDoubleClicked(liext->hiddenText());
}
/**  */
void ImageHistoryBrowser::slotStartDrag(QListViewItem*)
{
  QDragObject *d;
 	QString qs;
  QString text;
  QStrList slist;
  QCString uri;

  //we (try to) drag all selected items
  QListViewItemIterator it(mpListView);
  for ( ; it.current(); ++it )
  {
    if(it.current()->isSelected())
    {
      qs = ((QListViewItemExt*)it.current())->hiddenText();
      uri = QUriDrag::localFileToUri(qs);
      slist.append(uri);
    }
  }
  if(slist.isEmpty()) return;
  d = new QUriDrag(slist,this);
  d->dragCopy();
}
/**  */
void ImageHistoryBrowser::slotStartIconDrag(DragIconView* iconview)
{
  QDragObject *d;
 	QString qs;
  QString text;
  QStrList slist;
  QCString uri;
//  //we (try to) drag all selected items
  QIconViewItem* item;
  for(item=iconview->firstItem();item;item=item->nextItem())
  {
    if(item->isSelected())
    {
      qs = ((IconViewItemExt*)item)->hiddenText();
      uri = QUriDrag::localFileToUri(qs);
      slist.append(uri);
    }
  }
  if(slist.isEmpty()) return;
  d = new QUriDrag(slist,this);
  d->dragCopy();
}
/**  */
void ImageHistoryBrowser::createWhatsThisHelp()
{
//we also add some tooltips here
//clear history
  QWhatsThis::add(mpDeleteHistoryButton,
              tr("<html>Click this button to delete the history.</html>"));
  QToolTip::add(mpDeleteHistoryButton,
              tr("Delete history"));
//close
  QWhatsThis::add(mpCloseButton,
              tr("<html>Click this button to close the window.</html>"));
//history mode
  QWhatsThis::add(mpHistoryModeButton,
              tr("<html>Click this button to enable the history mode.</html>"));
  QToolTip::add(mpHistoryModeButton,tr("History mode"));
//image browser mode
  QWhatsThis::add(mpBrowserModeButton,
              tr("<html>Click this button to enable the image browser mode.</html>"));
  QToolTip::add(mpBrowserModeButton,tr("Image-browser mode"));
//icon mode
  QWhatsThis::add(mpIconModeButton,
              tr("<html>Click this button to display the contents in an iconview.</html>"));
  QToolTip::add(mpIconModeButton,tr("Iconview"));
//list mode
  QWhatsThis::add(mpListModeButton,
              tr("<html>Click this button to display the contents in a listview.</html>"));
  QToolTip::add(mpListModeButton,tr("Listview"));
//list view
  QWhatsThis::add(mpListView,
              tr("<html>Doubleclick on an item to open the image with "
                 "the image viewer. Click a column header to sort the "
                 "items</html>"));
//icon view
  QWhatsThis::add(mpIconView,
              tr("<html>Doubleclick on an item to open the image with "
                 "the image viewer.</html>"));
//preview
  QWhatsThis::add(mpCreatePreviewsButton,
              tr("<html>Click this button to create the missing preview "
                 "images. It's also possible to recreate all preview "
                 "images.</html>"));
  QToolTip::add(mpCreatePreviewsButton,tr("Create preview images"));
//add bookmark
  QWhatsThis::add(mpAddButton,
              tr("<html>Click this button to add the current directory "
                 "to the list of bookmarks.</html>"));
  QToolTip::add(mpAddButton,tr("Add bookmark"));
//delete bookmark
  QWhatsThis::add(mpSubButton,
              tr("<html>Click this button to delete the current bookmark.</html>"));
  QToolTip::add(mpSubButton,tr("Delete bookmark"));
//add bookmark
  QWhatsThis::add(mpDelBookmarksButton,
              tr("<html>Click this button to delete all bookmarks.</html>"));
  QToolTip::add(mpDelBookmarksButton,tr("Delete all bookmarks"));
//bookmark combo
  QWhatsThis::add(mpBookmarkCombo,
              tr("<html>Select a bookmark.</html>"));
//home
  QWhatsThis::add(mpHomeButton,
              tr("<html>Change to home directory.</html>"));
  QToolTip::add(mpHomeButton,tr("Home directory"));
//dir up
  QWhatsThis::add(mpUpButton,
              tr("<html>Click this button to go to the parent directory.</html>"));
  QToolTip::add(mpUpButton,tr("Parent directory"));
//dir back
  QWhatsThis::add(mpBackButton,
              tr("<html>Click this button to go to the previously selected directory.</html>"));
  QToolTip::add(mpBackButton,tr("Back"));
//dir forward
  QWhatsThis::add(mpForwardButton,
              tr("<html>Click this button to go the next directory.</html>"));
  QToolTip::add(mpForwardButton,tr("Forward"));
}
/** No descriptions */
void ImageHistoryBrowser::slotIconDoubleClicked(QIconViewItem* item)
{
  IconViewItemExt* ie;
  ie = (IconViewItemExt*) item;
  if(!QFile::exists(ie->hiddenText()))
  {
    QMessageBox::information(this,tr("Information"),
                             tr("The file doesn't exist.\n"
                                "The history will be updated."),
                             tr("OK"));
    mListContentsValid = false;
    mIconContentsValid = false;
    createContents();
    return;
  }
  emit signalItemDoubleClicked(ie->hiddenText());
}
/** No descriptions */
void ImageHistoryBrowser::slotAddBookmark()
{
  QStringList::Iterator it = mBookmarkList.find(mDirPath);

  if(it != mBookmarkList.end())
     return;
  mBookmarkList.append(mDirPath);
  mpBookmarkCombo->clear();
  mpBookmarkCombo->insertStringList(mBookmarkList);
  mpBookmarkCombo->setCurrentItem(mBookmarkList.count() - 1);
  xmlConfig->setStringList("BROWSER_BOOKMARKS",mBookmarkList);
}
/** No descriptions */
void ImageHistoryBrowser::slotDeleteBookmark()
{
  int index = mpBookmarkCombo->currentItem();
  if(index < 0)
    return;
  mpBookmarkCombo->removeItem(index);
  mBookmarkList.clear();
  for(int i=0;i<mpBookmarkCombo->count();i++)
    mBookmarkList.append(mpBookmarkCombo->text(i));
  xmlConfig->setStringList("BROWSER_BOOKMARKS",mBookmarkList);
}
/** No descriptions */
void ImageHistoryBrowser::slotClearBookmarks()
{
  int i = QMessageBox::warning(this,tr("Delete bookmarks"),
                             tr("Do you really want to delete all bookmarks?"),
                             tr("&Delete"),tr("&Cancel"),QString::null,1,1);
  if(i == 1)
    return;
  mpBookmarkCombo->clear();
  mBookmarkList.clear();
  xmlConfig->setStringList("BROWSER_BOOKMARKS",mBookmarkList);
}
/** No descriptions */
void ImageHistoryBrowser::slotDirUp()
{
  QString p_dir = mDirPath;
  int index = p_dir.findRev("/",-2);
  qDebug("p_dir %s",p_dir.latin1());
  if(index > -1)
  {
    p_dir = p_dir.left(index + 1);
    qDebug("p_dir %s",p_dir.latin1());
  }
  else
    return;
  QDir dir(p_dir);
  if(dir.exists())
  {
    mDirPath = p_dir;
    addHistoryPath(mDirPath);
    xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
    mRunning = false;
    mListContentsValid = false;
    mIconContentsValid = false;
    mpDirectoryListView->setDirectory(mDirPath);
    createContents();
  }
}
/** No descriptions */
void ImageHistoryBrowser::slotDirBack()
{
  if(mHistoryIndex <= 0)
    return;
  --mHistoryIndex;
  mpForwardButton->setEnabled(true);
  if(mHistoryIndex <= 0)
    mpBackButton->setEnabled(false);
  mDirPath = mHistoryList[mHistoryIndex];
  xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
  mRunning = false;
  mListContentsValid = false;
  mIconContentsValid = false;
  mpDirectoryListView->setDirectory(mDirPath);
  checkUpButton();
  createContents();
}
/** No descriptions */
void ImageHistoryBrowser::slotDirForward()
{
  if(mHistoryIndex >= (int(mHistoryList.count())-1))
    return;
  ++mHistoryIndex;
  mpBackButton->setEnabled(true);
  if(mHistoryIndex >= (int(mHistoryList.count())-1))
    mpForwardButton->setEnabled(false);
  mDirPath = mHistoryList[mHistoryIndex];
  xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
  mRunning = false;
  mListContentsValid = false;
  mIconContentsValid = false;
  mpDirectoryListView->setDirectory(mDirPath);
  checkUpButton();
  createContents();
}
/** No descriptions */
void ImageHistoryBrowser::addHistoryPath(QString& path)
{
  if(mHistoryIndex < 20)
  {
    //remove items that probably follow
    while((int(mHistoryList.count())-1) > mHistoryIndex)
    {
      mHistoryList.remove(mHistoryList.last());
    }
    mHistoryList.append(path);
    ++mHistoryIndex;
  }
  else
  {
    mHistoryList.remove(mHistoryList.begin());
    mHistoryList.append(path);
  }
  //adding a new item means, that the forward button gets disabled
  mpForwardButton->setEnabled(false);
  if(mHistoryIndex > 0)
    mpBackButton->setEnabled(true);
  checkUpButton();
}
/** No descriptions */
void ImageHistoryBrowser::checkUpButton()
{
  QString p_dir = mDirPath;
  int index = p_dir.findRev("/",-2);
  if(index > -1)
  {
    p_dir = p_dir.left(index + 1);
  }
  else
  {
    mpUpButton->setEnabled(false);
    return;
  }
  QDir dir(p_dir);
  if(dir.exists())
    mpUpButton->setEnabled(true);
}
/** No descriptions */
void ImageHistoryBrowser::slotBookmarkSelected(const QString& bm)
{
  QDir dir(bm);
  if(dir.exists())
  {
    mDirPath = bm;
    addHistoryPath(mDirPath);
    xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
    mRunning = false;
    mListContentsValid = false;
    mIconContentsValid = false;
    mpDirectoryListView->setDirectory(mDirPath);
    createContents();
  }
}
/** No descriptions */
void ImageHistoryBrowser::slotHome()
{
  QString home_dir = QDir::homePath();
  if(mDirPath == home_dir)
    return;
  QDir dir(home_dir);
  if(dir.exists())
  {
    mDirPath = home_dir;
    addHistoryPath(mDirPath);
    xmlConfig->setStringValue("BROWSER_PATH",mDirPath);
    mRunning = false;
    mListContentsValid = false;
    mIconContentsValid = false;
    mpDirectoryListView->setDirectory(mDirPath);
    createContents();
  }
}
