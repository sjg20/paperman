/***************************************************************************
                          qscannersetupdlg.cpp  -  description
                             -------------------
    begin                : Thu Jun 29 2000
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

#include "err.h"
#include "resource.h"
#include "motranslator.h"
//s #include "qsanestatusmessage.h"
//s #include "qscandialog.h"
#include "qscannersetupdlg.h"
#include "qscanner.h"
#include "qxmlconfig.h"
#include <QDesktopWidget>
#include <QShowEvent>
#include <QTextStream>

#include "saneconfig.h"

#include <stdlib.h>

#include <QDialog>
#include <QButtonGroup>
#include <QGroupBox>
#include <QTreeWidget>

#include <qapplication.h>
//s #include <qarray.h>
#include <qcursor.h>
#include <qdatastream.h>
#include <qdir.h>
#include <qdom.h>
#include <qfile.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qprinter.h>
#include <qradiobutton.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <qtranslator.h>
#include <qtoolbutton.h>
#include <qwidget.h>
#ifndef DEV_SETTINGS_VERSION
#define DEV_SETTINGS_VERSION "1"
#endif

QScannerSetupDlg::QScannerSetupDlg(QScanner *sc, QWidget *parent,
                                   const char *name,bool modal,Qt::WindowFlags f)
                 : QDialog(parent,f)
{
    UNUSED(modal);
    setObjectName(name);
  mStyle = 0;
  mpScanner = sc;
  mpScanDialog = 0;
  mQueryType = -1;
  setWindowTitle(QString(tr("Welcome to MaxView ")));
  mpLastItem = 0L;
//  initConfig();
  initScanner();
  initDialog();
  createContents(false);
  loadDeviceSettings();
}
QScannerSetupDlg::~QScannerSetupDlg()
{
}
/**  */
void QScannerSetupDlg::initDialog()
{
  QButtonGroup * deviceButtonGroup = new QButtonGroup(this);

  /*
   * 6 rows, 2 columns
   */
  QGridLayout *qgl=new QGridLayout (this);
  qgl->setSpacing(4);
  qgl->setMargin(6);

  QHBoxLayout* hb1 = new QHBoxLayout;
  QLabel* label1 = new QLabel(tr("Choose the device"));
  hb1->addWidget(label1);
  hb1->setStretchFactor(label1,1);
  qgl->addLayout(hb1,0,0,1,2);

  mpListView = new QTreeWidget(this);
  mpListView->setColumnCount(4);
  QStringList labels;
  labels << tr("Device name") << tr("Vendor") << tr("Model") << tr("Type");
  mpListView->setHeaderLabels(labels);
  mpListView->setAllColumnsShowFocus(true);
  mpListView->setRootIsDecorated(true);
  qgl->addWidget(mpListView,1,0,1,2);

  QGroupBox* gb2 = new QGroupBox(tr("On next program start"),this);

  mpAllDevicesRadio = new QRadioButton(tr("List &all devices"),gb2);
  mpLocalDevicesRadio = new QRadioButton(tr("List &local devices only"),gb2);
  mpLastDeviceRadio = new QRadioButton(tr("List &selected device only"),gb2);
  mpSameDeviceRadio = new QRadioButton(tr("&Use selected device, do not show dialog"),gb2);

  qgl->addWidget(gb2,2,0,3,1);

  QVBoxLayout *vbox = new QVBoxLayout;
  vbox->addWidget(mpAllDevicesRadio);
  vbox->addWidget(mpLocalDevicesRadio);
  vbox->addWidget(mpLastDeviceRadio);
  vbox->addWidget(mpSameDeviceRadio);
  gb2->setLayout(vbox);

  deviceButtonGroup->addButton(mpAllDevicesRadio, 0);
  deviceButtonGroup->addButton(mpLocalDevicesRadio, 1);
  deviceButtonGroup->addButton(mpLastDeviceRadio, 2);
  deviceButtonGroup->addButton(mpSameDeviceRadio, 3);

  mpDeviceButton = new QPushButton(tr("L&ist all devices"),this);
  qgl->addWidget(mpDeviceButton,3,1);

  mpLocalDeviceButton = new QPushButton(tr("Lis&t local devices"),this);
  qgl->addWidget(mpLocalDeviceButton,4,1);

  QHBoxLayout* hb2 = new QHBoxLayout();
  mpQuitButton=new QPushButton(tr("&Quit"),this);
  hb2->addWidget(mpQuitButton);
  QWidget* dummy = new QWidget();
  hb2->addWidget(dummy);
  hb2->setStretchFactor(dummy,1);
  mpSelectButton=new QPushButton(tr("Select &device"),this);
  hb2->addWidget(mpSelectButton);
  mpSelectButton->setDefault(true);

  qgl->addLayout(hb2,5,0,1,2);
  qgl->setRowStretch ( 1,1 );
  qgl->setColumnStretch ( 0,1 );
  qgl->activate();

  mpSelectButton->setEnabled(false);
  connect(mpQuitButton,SIGNAL(clicked()),SLOT(reject()));
  connect(mpSelectButton,SIGNAL(clicked()),SLOT(slotDeviceSelected()));
  connect(mpListView,SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
          this,SLOT(slotDeviceSelected(QTreeWidgetItem*, int)));
  connect(mpListView,SIGNAL(itemClicked(QTreeWidgetItem*, int)),
          this,SLOT(slotListViewClicked(QTreeWidgetItem*, int)));
  connect(deviceButtonGroup,SIGNAL(buttonClicked(int)),
          this,SLOT(slotDeviceGroup(int)));
  connect(mpDeviceButton,SIGNAL(clicked()),
          this,SLOT(slotAllDevices()));
  connect(mpLocalDeviceButton,SIGNAL(clicked()),
          this,SLOT(slotLocalDevices()));
  setMaximumHeight(qApp->desktop()->height()-200);
//  qDebug("set max height h %i",qApp->desktop()->height()-200);
}
/**  */
void QScannerSetupDlg::addLVItem(QString name,QString vendor,QString model,QString type)
{
  QStringList sl;
  sl << name << vendor << model << type;

  if(name == xmlConfig->stringValue("LAST_DEVICE"))
    mpLastItem = new QTreeWidgetItem(mpListView, sl);
  else
    new QTreeWidgetItem(mpListView,sl);

}
/**  */
void QScannerSetupDlg::clearList()
{
	mpListView->clear();
}
/**  */
QString QScannerSetupDlg::device()
{
  QString qs;
  mOptionMap.clear();
  QTreeWidgetItem* li;
  QTreeWidgetItem* rootli;
  li = mpListView->currentItem();
  rootli = li;
//We have to check, whether the user selected a root item
//or a child (settings!)
  while(rootli->parent())
  {
    rootli = mpListView->currentItem()->parent();
  }
//try to load the settings file
  QDomDocument doc;
  doc.clear();
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if (f.open( QIODevice::ReadOnly ) )
  {
    doc.setContent( &f );
    f.close();
  }
//If this item has a parent, then the user clicked on a
//device setting.
//The name of such a setting is unique for a given device.
  if(li->parent() && !doc.isNull())
  {//it's a setting
    //We search the specific entry in the xml file.
    QDomElement docElem = doc.documentElement();
    if(docElem.tagName() == "maxview_device_settings")
    {
      //Ok, now try to find the device settings
      QDomNode n = docElem.firstChild();
      while( !n.isNull() )
      {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        if( !e.isNull() )
        { // the node was really an element.
          if(e.tagName() == "sane_device")
          {
            qs = e.attribute("username");
            if(qs == "Last settings") qs = tr(qs.toLatin1());
            //Search the device settings.
            if(((rootli->text(1)+rootli->text(2)) == e.attribute("name"))   &&
               (li->text(0) == qs))
            {
              //We found  the entry.
              QDomNodeList dl = n.childNodes();
              for(int cnt=0;cnt<dl.count();cnt++)
              {
                QDomElement e2 = dl.item(cnt).toElement();
                if(e2.tagName() == "sane_option")
                {
                  mOptionMap.insert(e2.attribute("name"),
                                   e2.attribute("value"));
                }
              }
              break;
            }
          }
        }
        n = n.nextSibling();
      }
    }
  }
  xmlConfig->setStringValue("LAST_DEVICE",rootli->text(0));
  xmlConfig->setStringValue("LAST_DEVICE_VENDOR",rootli->text(1));
  xmlConfig->setStringValue("LAST_DEVICE_MODEL",rootli->text(2));
  xmlConfig->setStringValue("LAST_DEVICE_TYPE",rootli->text(3));
	return rootli->text(0);
}
/**  */
QString QScannerSetupDlg::lastDevice()
{
  QString qs;
  QString last_dev;
  QString last_dev_settings_name;
  mOptionMap.clear();
//try to load the settings file
  QDomDocument doc;
  doc.clear();
  last_dev = xmlConfig->stringValue("LAST_DEVICE",QString());
//qDebug("LAST_DEVICE: %s",last_dev.latin1());
  last_dev_settings_name = xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString()) +
                           xmlConfig->stringValue("LAST_DEVICE_MODEL",QString());
//qDebug("last_dev_settings_name: %s",last_dev_settings_name.latin1());
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if (f.open( QIODevice::ReadOnly ) )
  {
//qDebug("could open dev_settings");
    doc.setContent( &f );
    f.close();
  }
  else
  {
qDebug("could NOT open dev_settings");
  }
  if(!doc.isNull())
  {
    //We search the specific entry in the xml file.
    QDomElement docElem = doc.documentElement();
    if(docElem.tagName() == "maxview_device_settings")
    {
      //Ok, now try to find the device settings
      QDomNode n = docElem.firstChild();
      while( !n.isNull() )
      {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        if( !e.isNull() )
        { // the node was really an element.
          if(e.tagName() == "sane_device")
          {
//qDebug("found sane_device");
            qs = e.attribute("username");
            //Search the device settings.
//qDebug("e.attribute(""name""): %s",qs.latin1());
            if((last_dev_settings_name == e.attribute("name")) &&
               (qs == "Last settings"))
            {
//qDebug("found Last settings");
              //We found  the entry.
              QDomNodeList dl = n.childNodes();
              for(int cnt=0;cnt<dl.count();cnt++)
              {
                QDomElement e2 = dl.item(cnt).toElement();
                if(e2.tagName() == "sane_option")
                {
                  mOptionMap.insert(e2.attribute("name"),
                                   e2.attribute("value"));
                }
              }
              break;
            }
          }
        }
        n = n.nextSibling();
      }
    }
  }
  return last_dev;
}
/**  */
void QScannerSetupDlg::slotListViewClicked(QTreeWidgetItem*, int)
{
	if(mpListView->currentItem() != 0L)
    mpSelectButton->setEnabled(true);
}
/**  */
void QScannerSetupDlg::slotDeviceSelected()
{
  QString dev;
  setEnabled(false);
  qApp->processEvents();
  if(mQueryType == 3)
  {
    dev = lastDevice();
  }
  else
    dev = device();

  if((mQueryType == 3) || (mQueryType == 2))
  {
    //if we don't query the devices, then we normally don't know
    //vendor, model and type; therefore we pass the saved values
    //to our QScanner object
    mpScanner->setVendor(xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString()));
    mpScanner->setModel(xmlConfig->stringValue("LAST_DEVICE_MODEL",QString()));
    mpScanner->setType(xmlConfig->stringValue("LAST_DEVICE_TYPE",QString()));
  }
  mpScanner->setDeviceName(dev.toLatin1());
  if(!mpScanner->openDevice())
  {
    if(QScanner::msAuthorizationCancelled)
    {
      setEnabled(true);
      return;
    }
//s    QSaneStatusMessage status_msg(mpScanner->saneStatus(),this);
//s    status_msg.exec();
    setEnabled(true);
    if(isHidden())
    {
      xmlConfig->setIntValue("DEVICE_QUERY",2);
      mQueryType = 2;
      mpLastDeviceRadio->setChecked(true);
      show();
    }
    return;
  }
  if(!mOptionMap.isEmpty())
  {
//     qDebug("option-map not empty");
     mpScanner->setOptionsByName(mOptionMap);
  }
  else
  {
     qDebug("option-map IS empty");
  }
  //Create a QScanDialog with the selected scanner
  setCursor(Qt::WaitCursor);
/*s
  mpScanDialog = new QScanDialog(mpScanner,0,0,WType_TopLevel | WStyle_Title | WStyle_ContextHelp |
                                 WStyle_NormalBorder | WStyle_SysMenu |
                                 WStyle_MinMax | WStyle_Customize);
  if(mpScanDialog)
  {
    hide();
    connect(mpScanDialog,SIGNAL(signalQuit()),this,SLOT(slotQuit()));
#ifndef QIS_NO_STYLES
    connect(mpScanDialog,SIGNAL(signalChangeStyle(int)),
            this,SLOT(slotChangeStyle(int)));
#endif
    mpScanDialog->show();
    setCursor(Qt::ArrowCursor);
  }
*/
  setCursor(Qt::ArrowCursor);
  setEnabled(true);
  close();
}


bool QScannerSetupDlg::setupLast()
{
  QString dev;

  dev = lastDevice();

  //if we don't query the devices, then we normally don't know
  //vendor, model and type; therefore we pass the saved values
  //to our QScanner object
  mpScanner->setVendor(xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString()));
  mpScanner->setModel(xmlConfig->stringValue("LAST_DEVICE_MODEL",QString()));
  mpScanner->setType(xmlConfig->stringValue("LAST_DEVICE_TYPE",QString()));
  mpScanner->setDeviceName(dev.toLatin1());
  if(!mpScanner->openDevice())
  {
    return false;
  }
  if(!mOptionMap.isEmpty())
  {
//     qDebug("option-map not empty");
     mpScanner->setOptionsByName(mOptionMap);
  }
  else
  {
     qDebug("option-map IS empty");
  }
  return true;
}


/**  */
void QScannerSetupDlg::slotDeviceSelected(QTreeWidgetItem*, int)
{
	slotDeviceSelected();
}
/**  */
void QScannerSetupDlg::showEvent(QShowEvent * e)
{
  mpListView->setColumnWidth(0,mpListView->columnWidth(0)+5);
  mpListView->setColumnWidth(1,mpListView->columnWidth(1)+5);
  mpListView->setColumnWidth(2,mpListView->columnWidth(2)+5);
  mpListView->setColumnWidth(3,mpListView->columnWidth(3)+5);

  QRect qr1 = mpListView->frameRect();
  QRect qr2 = mpListView->contentsRect();

  if (qr2.width()< qApp->desktop()->width()*2/3)
    mpListView->setMinimumWidth(qr2.width()+ qr1.width()-qr2.width()+20);
  markLastDevice();
  QDialog::showEvent(e);
}
/**  */
void QScannerSetupDlg::createContents(bool intcall)
{
  int i;
  setCursor(Qt::WaitCursor);
  clearList();
  if(mQueryType < 0)
  {
    mQueryType = 0;
    mQueryType = xmlConfig->intValue("DEVICE_QUERY");
  }
  //there are 4 query types
  //0: list all devices
  //1: list local devices only
  //2: list only the device used in the previous session
  //3: try to open last devices and don't show dialog

  if(mQueryType == 2)
  {
    if(!intcall)
    {
      mpLastDeviceRadio->setChecked(true);
    }
    addLVItem(xmlConfig->stringValue("LAST_DEVICE"),
              xmlConfig->stringValue("LAST_DEVICE_VENDOR"),
              xmlConfig->stringValue("LAST_DEVICE_MODEL"),
              xmlConfig->stringValue("LAST_DEVICE_TYPE"));
    setCursor(Qt::ArrowCursor);
    return;
  }
  if(mQueryType == 3)
  {
    if(!intcall)
    {
      mpSameDeviceRadio->setChecked(true);
    }
    addLVItem(xmlConfig->stringValue("LAST_DEVICE"),
              xmlConfig->stringValue("LAST_DEVICE_VENDOR"),
              xmlConfig->stringValue("LAST_DEVICE_MODEL"),
              xmlConfig->stringValue("LAST_DEVICE_TYPE"));
    setCursor(Qt::ArrowCursor);
    return;
  }
  //query local devices only ?
  if(mQueryType == 1)
  {
    if(!intcall)
    {
      mpLocalDevicesRadio->setChecked(true);
    }
    mpScanner->getDeviceList(true);
    if(mpScanner->deviceCount()<=0)
    {
      QMessageBox::critical(0,QObject::tr("No local devices found"),
      QObject::tr("No local devices were found."),
       QObject::tr("&OK"));
      setCursor(Qt::ArrowCursor);
      return;
    }
  }
  else
  {
    if(!intcall)
    {
     mpAllDevicesRadio->setChecked(true);
    }
    mpScanner->getDeviceList(false);
    if(mpScanner->deviceCount()<=0)
    {
      QMessageBox::critical(0,QObject::tr("No devices found"),
      QObject::tr("No devices were found."),
       QObject::tr("&OK"));
      setCursor(Qt::ArrowCursor);
      return;
    }
  }
  if(mpScanner->deviceCount()>0)
  {
    for(i=0;i<mpScanner->deviceCount();i++)
    {
      addLVItem(QString(mpScanner->name(i)),
                QString(mpScanner->vendor(i)),
                QString(mpScanner->model(i)),
                QString(mpScanner->type(i)));
    }
  }
  setCursor(Qt::ArrowCursor);
  for (int i = 0; i < 4; i++)
    mpListView->resizeColumnToContents(i);
}
/**  */
void QScannerSetupDlg::slotDeviceGroup(int id)
{
  if(id == 3)
    QMessageBox::information(0,QObject::tr("Information"),
       QObject::tr("<qt>With this setting, this dialog will not be shown when you start "
                   "QuiteInsane the next time. You can change this in the options dialog under "
                   "<b>Start dialog</b>.</qt>"),
       QObject::tr("&OK"));
  //save value to config file
  xmlConfig->setIntValue("DEVICE_QUERY",id);
}
/**  */
void QScannerSetupDlg::slotAllDevices()
{
  mQueryType = 0;
  mpSelectButton->setEnabled(false);
  if(!mpScanner || (mpListView->topLevelItemCount() <= 0))
  {
    initScanner();
  }
  createContents(true);
  loadDeviceSettings();
}
/**  */
void QScannerSetupDlg::slotLocalDevices()
{
  mQueryType = 1;
  mpSelectButton->setEnabled(false);
  if(!mpScanner || (mpListView->topLevelItemCount() <= 0))
  {
    initScanner();
  }
  createContents(true);
  loadDeviceSettings();
}
/**  */
void QScannerSetupDlg::show()
{
  if(mQueryType == 3)
  {
    slotDeviceSelected();
    return;
  }
  QWidget::show();
  qApp->processEvents();
  int x,y;
#if 0
  //ensure that no parts of the dialog are outside the desktop, because I hate this
  h= height()-mpListView->height();
  if(mpListView->contentsHeight()> mpListView->viewport()->height())
    resize(width(),mpListView->contentsHeight()+h+mpListView->header()->height()+20);
#endif
  //ensure that the dialog is centered on desktop
  x = (qApp->desktop()->width()-width())/2;
  y = (qApp->desktop()->height()-height())/2;
  move(x,y);
}

/**  */
void QScannerSetupDlg::initConfig()
{
//  if (!xmlConfig)
//     new QXmlConfig();
  QString local_dir;
  QString temp_dir;
  local_dir = QString();
  QString inst_dir;
#ifdef INSTALL_DIR
  inst_dir = INSTALL_DIR;
#else
  inst_dir = "/usr/local";
#endif
  if(inst_dir.right(1) != "/") inst_dir += "/";
  temp_dir = getenv("TEMP");
  if(temp_dir.isEmpty())
    temp_dir = "/tmp/";
/*
  xmlConfig->setVersion(VERSION);
  xmlConfig->setFilePath(QDir::homePath()+
                     "/.maxview/qmaxview_config.xml");
  xmlConfig->setCreator("MaxView");
*/
  xmlConfig->setStringValue("CURVE_SAVE_PATH", QDir::homePath());
  xmlConfig->setStringValue("CURVE_OPEN_PATH", QDir::homePath());
  //main settings
  xmlConfig->setStringValue("TEMP_PATH","/tmp/");
  xmlConfig->setIntValue("SCAN_MODE",0);
  xmlConfig->setIntValue("METRIC_SYSTEM",int(QIN::Millimetre));
  xmlConfig->setBoolValue("IO_MODE",true);
#ifndef QIS_NO_STYLES
  xmlConfig->setIntValue("STYLE",0);
#endif
  xmlConfig->setStringValue("LAST_DEVICE","");
  xmlConfig->setStringValue("LAST_DEVICE_VENDOR","");
  xmlConfig->setStringValue("LAST_DEVICE_MODEL","");
  xmlConfig->setStringValue("LAST_DEVICE_TYPE","");
  xmlConfig->setIntValue("DEVICE_QUERY",0);
  xmlConfig->setIntValue("LAYOUT",int(QIN::ScrollLayout));
  xmlConfig->setBoolValue("SEPARATE_PREVIEW",true);
  xmlConfig->setStringValue("SINGLEFILE_SAVE_PATH",QDir::homePath());
  xmlConfig->setStringValue("SINGLEFILE_OPEN_PATH",QDir::homePath());
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",0);
  //viewer settings
  xmlConfig->setStringValue("VIEWER_IMAGE_TYPE","BMP (*.bmp)");
  xmlConfig->setStringValue("VIEWER_SAVEIMAGE_PATH",QDir::homePath());
  xmlConfig->setStringValue("VIEWER_LOADIMAGE_PATH",QDir::homePath());
  xmlConfig->setStringValue("VIEWER_SAVETEXT_PATH",QDir::homePath());
  xmlConfig->setBoolValue("VIEWER_OCR_MODE",false);
  xmlConfig->setIntValue("VIEWER_UNDO_STEPS",5);
  xmlConfig->setIntValue("FILTER_PREVIEW_SIZE",150);
  xmlConfig->setIntValue("DISPLAY_SUBSYSTEM",0);

  QList<int> list;
  xmlConfig->setIntValueList("VIEWER_SPLITTER_SIZE",list);
  //viewer toolbars
  xmlConfig->setIntValue("TEXTTOOLBAR_DOCK",2);
  xmlConfig->setIntValue("TEXTTOOLBAR_INDEX",1);
  xmlConfig->setBoolValue("TEXTTOOLBAR_NL",true);
  xmlConfig->setIntValue("TEXTTOOLBAR_EXTRA_OFFSET",-1);
  xmlConfig->setIntValue("IMAGETOOLBAR_DOCK",2);
  xmlConfig->setIntValue("IMAGETOOLBAR_INDEX",0);
  xmlConfig->setBoolValue("IMAGETOOLBAR_NL",true);
  xmlConfig->setIntValue("IMAGETOOLBAR_EXTRA_OFFSET",-1);
  xmlConfig->setIntValue("TOOLSTOOLBAR_DOCK",2);
  xmlConfig->setIntValue("TOOLSTOOLBAR_INDEX",2);
  xmlConfig->setBoolValue("TOOLSTOOLBAR_NL",true);
  xmlConfig->setIntValue("TOOLSTOOLBAR_EXTRA_OFFSET",-1);
  //viewer toolbars ocr off
  xmlConfig->setIntValue("IMAGETOOLBAR_DOCK_OCROFF",2);
  xmlConfig->setIntValue("IMAGETOOLBAR_INDEX_OCROFF",0);
  xmlConfig->setBoolValue("IMAGETOOLBAR_NL_OCROFF",true);
  xmlConfig->setIntValue("IMAGETOOLBAR_EXTRA_OFFSET_OCROFF",-1);
  xmlConfig->setIntValue("TOOLSTOOLBAR_DOCK_OCROFF",2);
  xmlConfig->setIntValue("TOOLSTOOLBAR_INDEX_OCROFF",1);
  xmlConfig->setBoolValue("TOOLSTOOLBAR_NL_OCROFF",true);
  xmlConfig->setIntValue("TOOLSTOOLBAR_EXTRA_OFFSET_OCROFF",-1);
  //image/history browser setting
  xmlConfig->setBoolValue("HISTORY_ENABLED",false);
  xmlConfig->setBoolValue("HISTORY_LIMIT_ENTRIES",false);
  xmlConfig->setIntValue("HISTORY_MAX_ENTRIES",25);
  xmlConfig->setBoolValue("HISTORY_DELETE_EXIT",true);
  xmlConfig->setBoolValue("HISTORY_CREATE_PREVIEWS",false);
  //editor settings
  xmlConfig->setStringValue("EDITOR_FONT_FAMILY","");
  xmlConfig->setIntValue("EDITOR_FONT_POINTSIZE",-1);
  xmlConfig->setBoolValue("EDITOR_FONT_WEIGHT",-1);
  xmlConfig->setBoolValue("EDITOR_FONT_ITALIC",false);
  xmlConfig->setIntValue("EDITOR_FONT_CHARSET",-1);
  xmlConfig->setIntValue("EDITOR_LEFT_MARGIN",1000);
  xmlConfig->setIntValue("EDITOR_TOP_MARGIN",1000);
  xmlConfig->setIntValue("EDITOR_RIGHT_MARGIN",1000);
  xmlConfig->setIntValue("EDITOR_BOTTOM_MARGIN",1000);
  xmlConfig->setIntValue("EDITOR_PRINT_MODE",0);
  xmlConfig->setBoolValue("EDITOR_PRINT_SELECTED",false);
  //filelist settings
  xmlConfig->setStringValue("FILELIST_SAVE_PATH",QDir::homePath());
  xmlConfig->setStringValue("FILELIST_TEMPLATE","QuiteInsaneImage");
  xmlConfig->setIntValue("FILELIST_IMAGE_TYPE",0);
  //help viewer settings
  xmlConfig->setStringValue("HELP_INDEX",
                   inst_dir+"share/quiteinsane/doc/en/html/index.html");
  xmlConfig->setStringValue("HELP_LAST_PAGE","index.html");
  xmlConfig->setBoolValue("PREVIEW_EXTENSION",true);
  xmlConfig->setBoolValue("SAVE_SETTINGS",true);
  xmlConfig->setIntValue("DRAG_IMAGE_TYPE",0);
  xmlConfig->setStringValue("DRAG_FILEPATH","");
  xmlConfig->setBoolValue("DRAG_AUTOMATIC_FILENAME",false);
  //gocr
  xmlConfig->setStringValue("GOCR_COMMAND","gocr");
  //copy
  xmlConfig->setIntValue("COPY_SCALE",0);
  xmlConfig->setIntValue("COPY_SCALE_FACTOR",100);
  xmlConfig->setBoolValue("COPY_KEEP_ASPECT",true);
  xmlConfig->setIntValue("COPY_MARGIN_LEFT",1000);
  xmlConfig->setIntValue("COPY_MARGIN_RIGHT",1000);
  xmlConfig->setIntValue("COPY_MARGIN_TOP",1000);
  xmlConfig->setIntValue("COPY_MARGIN_BOTTOM",1000);
  xmlConfig->setBoolValue("COPY_RESOLUTION_BIND",false);
  //printer
  xmlConfig->setBoolValue("PRINTER_MODE",false);
  xmlConfig->setStringValue("PRINTER_FILENAME","");
  xmlConfig->setBoolValue("PRINTER_COLOR",false);
  xmlConfig->setIntValue("PRINTER_PAPER_ORIENTATION",int(QPrinter::Portrait));
  xmlConfig->setIntValue("PRINTER_PAPER_FORMAT",int(QPrinter::A4));
  xmlConfig->setIntValue("PRINTER_COPIES",1);
  xmlConfig->setStringValue("PRINTER_NAME","");
  //multi scan
  xmlConfig->setIntValue("MULTI_NUMBER",1);
  xmlConfig->setBoolValue("MULTI_ADF",false);
  xmlConfig->setBoolValue("MULTI_SAVE_IMAGE",false);
  xmlConfig->setBoolValue("MULTI_PRINT",false);
  xmlConfig->setBoolValue("MULTI_SAVE_TEXT",false);
  xmlConfig->setBoolValue("MULTI_CONFIRM",false);
  xmlConfig->setBoolValue("MULTI_OWN_RESOLUTION",false);
  //filelist settings
  xmlConfig->setStringValue("TEXTLIST_SAVE_PATH",QDir::homePath());
  xmlConfig->setStringValue("TEXTLIST_TEMPLATE","QuiteInsaneText");
  xmlConfig->setIntValue("TEXTLIST_FILE_TYPE",0);
  //warnings (that the user can switch off)
  xmlConfig->setBoolValue("WARNING_ADF",true);
  xmlConfig->setBoolValue("WARNING_MULTI",true);
  //Tiff modes/compression
  xmlConfig->setIntValue("TIFF_8BIT_MODE",0);
  xmlConfig->setIntValue("TIFF_LINEART_MODE",0);
  xmlConfig->setIntValue("TIFF_JPEG_QUALITY",80);
  xmlConfig->setIntValue("JPEG_QUALITY",80);
  xmlConfig->setIntValue("PNG_COMPRESSION",6);
  //Tiff modes/compression for QualityDialog
  xmlConfig->setIntValue("QUALITY_TIFF_8BIT_MODE",0);
  xmlConfig->setIntValue("QUALITY_TIFF_LINEART_MODE",0);
  xmlConfig->setIntValue("QUALITY_TIFF_JPEG_QUALITY",80);
  xmlConfig->setIntValue("QUALITY_JPEG_QUALITY",80);
  xmlConfig->setIntValue("QUALITY_PNG_COMPRESSION",6);

  // scanning options
  xmlConfig->setBoolValue("SCAN_USE_JPEG",true);
  xmlConfig->setIntValue("SCAN_SINGLE",0);
  xmlConfig->setIntValue("SCAN_BLANK",0);
  xmlConfig->setIntValue("SCAN_BLANK_THRESHOLD",500);
  xmlConfig->setIntValue("SCAN_STACK_COUNT",0);
//  xmlConfig->readConfigFile();
}
/** Open the device settings file, if it exists at all.
Create an empty file, if it doesn't exist.
This file is in XML format. If there are saved
settings for the currently listed devices, list them
as child items of the device entries. */
void QScannerSetupDlg::loadDeviceSettings()
{
  bool create_empty=false;
  QString qs;
//try to load the settings file
  QDomDocument doc;
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if (f.open(QIODevice::ReadOnly))
  {
    if( !doc.setContent( &f ) )
    {
      create_empty = true;
    }
    f.close();
  }
  else
    create_empty = true;
  if(create_empty == true)
  {
    //create an empty settings file
    QDomDocument tempdoc("maxview_device_settings");
    QDomElement root = tempdoc.createElement(tempdoc.doctype().name());
    root.setAttribute("version", DEV_SETTINGS_VERSION);
    tempdoc.appendChild(root);
    QFile of( xmlConfig->absConfDirPath()+"devicesettings.xml" );
    if(of.open(QIODevice::WriteOnly))
    {
      QTextStream ts(&of);
      tempdoc.save(ts, 0);
      of.close();
      doc=tempdoc;
    }
    return;
  }
  QDomNode child = doc.firstChild();
  if(!child.isElement()) return;
  QDomElement e = child.toElement();
//Check, whether it's really a device settings file.
  if((doc.doctype().name() != "maxview_device_settings") ||
      (e.attribute("version") != "1"))
  {
    QDomDocument tempdoc("maxview_device_settings");
    QDomElement root = tempdoc.createElement(tempdoc.doctype().name());
    root.setAttribute("version", DEV_SETTINGS_VERSION);
    tempdoc.appendChild(root);
   //If it's a settings file from a previous version,
    //we can try to load it anyways.
    if(e.tagName() == "maxview_device_settings")
    {
      //create a QDomNodeList
      QDomNodeList nodes = e.elementsByTagName("sane_device");
      //Finally, save it, which converts the old settings file to
      //a file in the correct format.
      // create the root element
      // iterate over the items
      for (int n=0; n<nodes.count();n++)
      {
        if (nodes.item(n).isElement())
        {
          root.appendChild((nodes.item(n)).cloneNode(true).toElement());
        }
        //file successfully created
      }
    }
    QFile of( xmlConfig->absConfDirPath()+"devicesettings.xml" );
    if(of.open(QIODevice::WriteOnly))
    {
      QTextStream ts(&of);
      tempdoc.save(ts, 0);
      of.close();
      doc=tempdoc;
    }
  }
//Ok, now try to find the device settings
  QDomElement root = doc.documentElement();
  QDomNodeList nodes = root.elementsByTagName("sane_device");
  QDomElement ele;

  // iterate over the items
  for (int n=0; n<nodes.count(); ++n)
  {
    if (nodes.item(n).isElement())
    {
      ele = nodes.item(n).toElement();
      //Iterate over the listview items
      //to see whether there's a corresponding device.
      //We assume, that this is true if the device names are the same.
      for (int i = 0; i < mpListView->topLevelItemCount(); i++)
      {
        QTreeWidgetItem *li = mpListView->topLevelItem(i);

        if((li->text(1)+li->text(2)) == ele.attribute("name"))
        {
          //We found one; we create a new listview item and insert it
          //as a child, if it has a valid name.
          if(!ele.attribute("username").isNull())
          {
            qs = ele.attribute("username");
            //Only translated the Last settings entry to avoid confusion
            //if a user specified name matches a translated string
            if(qs == "Last settings")
                qs = tr(qs.toLatin1());
            QStringList sl;
            sl << qs;
            new QTreeWidgetItem(li, sl);
            li->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
          }
        }
      }
    }
  }
}
/**  */
QMap <QString,QString> QScannerSetupDlg::optionMap()
{
  return mOptionMap;
}
/**  */
void QScannerSetupDlg::slotProcessEvents()
{
  qApp->processEvents();
}

void QScannerSetupDlg::initScanner()
{
  uninitScanner();
  mpScanner = new QScanner();
  if(mpScanner)
  {
    mpScanner->initScanner();
  	if(mpScanner->isInit())
      return;
  }
  QMessageBox::critical(0,tr("Initialisation failed"),
               tr("<center>A call to sane_init() failed.</center><br>"
	                "<center>Press Quit to quit QuiteInsane.</center>"),
               tr("&Quit"));
  slotQuit();
}

void QScannerSetupDlg::uninitScanner()
{
    if(mpScanner)
    {
      mpScanner->close();
      mpScanner->exitScanner();
      delete mpScanner;
    }
}

/**  */
void QScannerSetupDlg::slotQuit()
{
  if(mpScanner)
  {
    mpScanner->close();
    delete mpScanner;
  }
  close();
}

/** First we iterate over the list and test, whether a device
    matches the device name of the previous session. If there's
    no such device, we try to find a device with the same vendor/model.
    We try to use a local device, if the previously selected device was
    a local device, or a net device if it was a net device.
    If this fails too, we look for the same vendor/model only. */
void QScannerSetupDlg::markLastDevice()
{
  QTreeWidgetItem* li = 0;
  QTreeWidgetItem* match_li = 0;
  bool was_net = false;
  QString qs;
  QString last_dev;
  QString last_dev_vendor;
  QString last_dev_model;
  last_dev = xmlConfig->stringValue("LAST_DEVICE",QString());
  last_dev_vendor = xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString());
  last_dev_model = xmlConfig->stringValue("LAST_DEVICE_MODEL",QString());
  if(last_dev.isEmpty() || last_dev_vendor.isEmpty() || last_dev_model.isEmpty())
    return;
  if(last_dev.left(4) == "net:")
    was_net = true;

  if (!mpListView->topLevelItemCount())
      return;
  QTreeWidgetItemIterator it(mpListView);

  //iterate over root elements, and check whether last_dev is present
  while (*it)
  {
    li = *it;
    if(li->text(0) == last_dev)
    {
      match_li = li;
      break;
    }
    it++;
  }
  if(match_li == 0) //last device not present
  {
    it = QTreeWidgetItemIterator(mpListView);
    while (*it)
    {
      li = *it;
      bool is_net = false;
      if(li->text(0).left(4) == "net:")
        is_net = true;
      if((li->text(1) == last_dev_vendor) && (li->text(2) == last_dev_model) &&
         (is_net == was_net))
      {
        match_li = li;
        break;
      }
      it++;
    }
  }
  if(match_li == 0) //last attempt: check vendor/model only
  {
    it = QTreeWidgetItemIterator(mpListView);
    while (*it)
    {
      li = *it;
      if((li->text(1) == last_dev_vendor) && (li->text(2) == last_dev_model))
      {
        match_li = li;
        break;
      }
      it++;
    }
  }
  if(!match_li)
    return;
  if (!match_li->childCount())
      return;

  it = QTreeWidgetItemIterator(match_li);
  while (*it)
  {
    if(li->text(0) == tr("Last settings"))
    {
      li->setExpanded(true);
      li->setSelected(true);
      mpListView->scrollToItem(li);
      mpSelectButton->setEnabled(true);
      mpLastItem = li;
      break;
    }
    it++;
  }
}
