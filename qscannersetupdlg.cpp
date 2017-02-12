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

#include "resource.h"
#include "motranslator.h"
//s #include "qsanestatusmessage.h"
//s #include "qscandialog.h"
#include "qscannersetupdlg.h"
#include "qscanner.h"
#include "qxmlconfig.h"
//Added by qt3to4:
#include <Q3GridLayout>
#include <QDesktopWidget>
#include <QShowEvent>
#include <QTextStream>

#include "saneconfig.h"

#include <stdlib.h>

#include <qapplication.h>
//s #include <qarray.h>
#include <q3buttongroup.h>
#include <qcursor.h>
#include <q3cstring.h>
#include <qdatastream.h>
#include <qdir.h>
#include <qdom.h>
#include <qfile.h>
#include <q3groupbox.h>
#include <q3hbox.h>
#include <q3header.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <q3listbox.h>
#include <q3listview.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qprinter.h>
#include <qradiobutton.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <q3textstream.h>
#include <qtranslator.h>
#include <qtoolbutton.h>
#include <q3valuelist.h>
#include <q3whatsthis.h>
#include <qwidget.h>
#ifndef QIS_NO_STYLES
#include <qwindowsstyle.h>
#include <qcdestyle.h>
//#include <qsgistyle.h>
#include <qmotifstyle.h>
//#include <qmotifplusstyle.h>
//#include <qplatinumstyle.h>
#endif
#ifndef DEV_SETTINGS_VERSION
#define DEV_SETTINGS_VERSION "1"
#endif

QScannerSetupDlg::QScannerSetupDlg(QScanner *sc, QWidget *parent,
                                   const char *name,bool modal,Qt::WFlags f)
                 : QDialog(parent,name,modal,f)
{
  mStyle = 0;
  mpScanner = sc;
  mpScanDialog = 0;
  mQueryType = -1;
  setCaption(QString(tr("Welcome to MaxView ")));
  mpLastItem = 0L;
//  initConfig();
#ifndef QIS_NO_STYLES
  slotChangeStyle(xmlConfig->intValue("STYLE"));
#endif
  initScanner();
  initDialog();
  createWhatsThisHelp();
  createContents(false);
  loadDeviceSettings();
}
QScannerSetupDlg::~QScannerSetupDlg()
{
}
/**  */
void QScannerSetupDlg::initDialog()
{
  Q3ButtonGroup* DeviceButtonGroup = new Q3ButtonGroup(this);
  DeviceButtonGroup->hide();
  DeviceButtonGroup->setRadioButtonExclusive(true);

  Q3GridLayout *qgl=new	Q3GridLayout (this,6,2);
  qgl->setSpacing(4);
  qgl->setMargin(6);

  Q3HBox* hb1 = new Q3HBox(this);
  QLabel* label1 = new QLabel(tr("Choose the device"),hb1);
  hb1->setStretchFactor(label1,1);
  QToolButton* tb = Q3WhatsThis::whatsThisButton(hb1);
  tb->setAutoRaise(FALSE);
  qgl->addMultiCellWidget(hb1,0,0,0,1);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    tb->hide();

  mpListView = new Q3ListView(this);
  mpListView->addColumn(tr("Device name"));
  mpListView->addColumn(tr("Vendor"));
  mpListView->addColumn(tr("Model"));
  mpListView->addColumn(tr("Type"));
  mpListView->setAllColumnsShowFocus(TRUE);
  mpListView->setRootIsDecorated(true);
  qgl->addMultiCellWidget(mpListView,1,1,0,1);

  Q3GroupBox* gb2 = new Q3GroupBox(1,Qt::Horizontal,
                                 tr("On next program start"),this);

  mpAllDevicesRadio = new QRadioButton(tr("List &all devices"),gb2);
  mpLocalDevicesRadio = new QRadioButton(tr("List &local devices only"),gb2);
  mpLastDeviceRadio = new QRadioButton(tr("List &selected device only"),gb2);
  mpSameDeviceRadio = new QRadioButton(tr("&Use selected device, do not show dialog"),gb2);

  qgl->addMultiCellWidget(gb2,2,4,0,0);

  DeviceButtonGroup->insert(mpAllDevicesRadio,0);
  DeviceButtonGroup->insert(mpLocalDevicesRadio,1);
  DeviceButtonGroup->insert(mpLastDeviceRadio,2);
  DeviceButtonGroup->insert(mpSameDeviceRadio,3);

  mpDeviceButton = new QPushButton(tr("L&ist all devices"),this);
  qgl->addWidget(mpDeviceButton,3,1);

  mpLocalDeviceButton = new QPushButton(tr("Lis&t local devices"),this);
  qgl->addWidget(mpLocalDeviceButton,4,1);

  Q3HBox* hb2 = new Q3HBox(this);
  mpQuitButton=new QPushButton(tr("&Quit"),hb2);
  QWidget* dummy = new QWidget(hb2);
  hb2->setStretchFactor(dummy,1);
  mpSelectButton=new QPushButton(tr("Select &device"),hb2);
  mpSelectButton->setDefault(TRUE);

  qgl->addMultiCellWidget(hb2,5,5,0,1);
  qgl->setRowStretch ( 1,1 );
  qgl->setColStretch ( 0,1 );
  qgl->activate();

  mpSelectButton->setEnabled(FALSE);
  connect(mpQuitButton,SIGNAL(clicked()),SLOT(reject()));
  connect(mpSelectButton,SIGNAL(clicked()),SLOT(slotDeviceSelected()));
  connect(mpListView,SIGNAL(doubleClicked(Q3ListViewItem*)),
          this,SLOT(slotDeviceSelected(Q3ListViewItem*)));
  connect(mpListView,SIGNAL(clicked(Q3ListViewItem*)),
          this,SLOT(slotListViewClicked(Q3ListViewItem*)));
  connect(DeviceButtonGroup,SIGNAL(clicked(int)),
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
  if(name == xmlConfig->stringValue("LAST_DEVICE"))
    mpLastItem = new Q3ListViewItem(mpListView,name,vendor,model,type);
  else
    new Q3ListViewItem(mpListView,name,vendor,model,type);

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
  Q3ListViewItem* li;
  Q3ListViewItem* rootli;
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
            if(qs == "Last settings") qs = tr(qs);
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
  last_dev = xmlConfig->stringValue("LAST_DEVICE",QString::null);
//qDebug("LAST_DEVICE: %s",last_dev.latin1());
  last_dev_settings_name = xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString::null) +
                           xmlConfig->stringValue("LAST_DEVICE_MODEL",QString::null);
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
void QScannerSetupDlg::slotListViewClicked(Q3ListViewItem*)
{
	if(mpListView->currentItem() != 0L)
    mpSelectButton->setEnabled(TRUE);
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
    mpScanner->setVendor(xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString::null));
    mpScanner->setModel(xmlConfig->stringValue("LAST_DEVICE_MODEL",QString::null));
    mpScanner->setType(xmlConfig->stringValue("LAST_DEVICE_TYPE",QString::null));
  }
  mpScanner->setDeviceName(dev.latin1());
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
  setCursor(Qt::waitCursor);
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
    setCursor(Qt::arrowCursor);
  }
*/
  setCursor(Qt::arrowCursor);
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
  mpScanner->setVendor(xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString::null));
  mpScanner->setModel(xmlConfig->stringValue("LAST_DEVICE_MODEL",QString::null));
  mpScanner->setType(xmlConfig->stringValue("LAST_DEVICE_TYPE",QString::null));
  mpScanner->setDeviceName(dev.latin1());
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
void QScannerSetupDlg::slotDeviceSelected(Q3ListViewItem*)
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

  if(mpListView->contentsWidth()< qApp->desktop()->width()*2/3)
    mpListView->setMinimumWidth(mpListView->contentsWidth()+
                                 qr1.width()-qr2.width()+20);
  markLastDevice();
  QDialog::showEvent(e);
}
/**  */
void QScannerSetupDlg::createContents(bool intcall)
{
  int i;
  setCursor(Qt::waitCursor);
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
    setCursor(Qt::arrowCursor);
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
    setCursor(Qt::arrowCursor);
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
      setCursor(Qt::arrowCursor);
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
      setCursor(Qt::arrowCursor);
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
  setCursor(Qt::arrowCursor);
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
  mpSelectButton->setEnabled(FALSE);
  if(!mpScanner || (mpListView->childCount() <= 0))
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
  mpSelectButton->setEnabled(FALSE);
  if(!mpScanner || (mpListView->childCount() <= 0))
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
  int x,y,h;
  //ensure that no parts of the dialog are outside the desktop, because I hate this
  h= height()-mpListView->height();
  if(mpListView->contentsHeight()> mpListView->viewport()->height())
    resize(width(),mpListView->contentsHeight()+h+mpListView->header()->height()+20);
  //ensure that the dialog is centered on desktop
  x = (qApp->desktop()->width()-width())/2;
  y = (qApp->desktop()->height()-height())/2;
  move(x,y);
}
/**  */
void QScannerSetupDlg::createWhatsThisHelp()
{
  Q3WhatsThis::add(mpAllDevicesRadio,tr("If you activate this radio button, "
              "all devices will be queried on the next program start."));
  Q3WhatsThis::add(mpLocalDevicesRadio,tr("If you activate this radio button, "
              "all local devices will be queried on the next program "
              "start."));
  Q3WhatsThis::add(mpLastDeviceRadio,tr("If you activate this radio button, "
              "only the device you're going to select now will be listed "
              "on the next program start."));
  Q3WhatsThis::add(mpSameDeviceRadio,tr("The device you're going to select now will "
              "be used on the next program start and this dialog will not be shown anymore. You "
              "can re-enable the dialog in the options dialog."));
  Q3WhatsThis::add(mpQuitButton,tr("Click this button to quit QuiteInsane."));
  Q3WhatsThis::add(mpSelectButton,tr("Click this button to select the "
              "currently highlighted device."));
  Q3WhatsThis::add(mpLocalDeviceButton,tr("Click this button to list all "
              "local devices. This may take a while."));
  Q3WhatsThis::add(mpDeviceButton,tr("Click this button to list all "
              "devices. This may take a while."));
  Q3WhatsThis::add(mpListView,tr("Doubleclick on a listview item to "
              "select a device. You can also highlight an item with "
              "a single mouse click and click the Select device button."));
}
/**  */
void QScannerSetupDlg::initConfig()
{
//  if (!xmlConfig)
//     new QXmlConfig();
  QString local_dir;
  QString temp_dir;
  local_dir = QString::null;
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
  xmlConfig->setFilePath(QDir::homeDirPath()+
                     "/.maxview/qmaxview_config.xml");
  xmlConfig->setCreator("MaxView");
*/
  xmlConfig->setStringValue("CURVE_SAVE_PATH", QDir::homeDirPath());
  xmlConfig->setStringValue("CURVE_OPEN_PATH", QDir::homeDirPath());
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
  xmlConfig->setStringValue("SINGLEFILE_SAVE_PATH",QDir::homeDirPath());
  xmlConfig->setStringValue("SINGLEFILE_OPEN_PATH",QDir::homeDirPath());
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",0);
  //viewer settings
  xmlConfig->setStringValue("VIEWER_IMAGE_TYPE","BMP (*.bmp)");
  xmlConfig->setStringValue("VIEWER_SAVEIMAGE_PATH",QDir::homeDirPath());
  xmlConfig->setStringValue("VIEWER_LOADIMAGE_PATH",QDir::homeDirPath());
  xmlConfig->setStringValue("VIEWER_SAVETEXT_PATH",QDir::homeDirPath());
  xmlConfig->setBoolValue("VIEWER_OCR_MODE",false);
  xmlConfig->setIntValue("VIEWER_UNDO_STEPS",5);
  xmlConfig->setIntValue("FILTER_PREVIEW_SIZE",150);
  xmlConfig->setIntValue("DISPLAY_SUBSYSTEM",0);

  Q3ValueList<int> list;
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
  xmlConfig->setStringValue("FILELIST_SAVE_PATH",QDir::homeDirPath());
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
  xmlConfig->setStringValue("TEXTLIST_SAVE_PATH",QDir::homeDirPath());
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
  Q3ListViewItem* li;
  Q3ListViewItem* childitem;
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
      li = mpListView->firstChild();
      while(li)
      {
        if((li->text(1)+li->text(2)) == ele.attribute("name"))
        {
          //We found one; we create a new listview item and insert it
          //as a child, if it has a valid name.
          if(!ele.attribute("username").isNull())
          {
            qs = ele.attribute("username");
            //Only translated the Last settings entry to avoid confusion
            //if a user specified name matches a translated string
            if(qs == "Last settings") qs = tr(qs);
            childitem = new Q3ListViewItem(li,qs);
            if(!li->isExpandable()) li->setExpandable(true);
          }
        }
        li = li->nextSibling();
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
  if(mpScanner)
  {
    mpScanner->close();
    mpScanner->exitScanner();
    delete mpScanner;
  }
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

/**  */
void QScannerSetupDlg::slotChangeStyle(int s)
{
#ifndef QIS_NO_STYLES
  if(s == mStyle) return;
  switch(s)
  {
    case 0:
     QApplication::setStyle(new QWindowsStyle());
     break;
    case 1:
     QApplication::setStyle(new QMotifStyle());
     break;
#if 0
    case 2:
     QApplication::setStyle(new QMotifPlusStyle());
     break;
    case 3:
     QApplication::setStyle(new QPlatinumStyle());
     break;
    case 4:
     QApplication::setStyle(new QSGIStyle());
     break;
#endif
    case 5:
     {
       QApplication::setStyle(new QCDEStyle(true));
       QPalette p( QColor( 75, 123, 130 ) );
       p.setColor( QPalette::Active, QColorGroup::Base, QColor( 55, 77, 78 ) );
       p.setColor( QPalette::Inactive, QColorGroup::Base, QColor( 55, 77, 78 ) );
       p.setColor( QPalette::Disabled, QColorGroup::Base, QColor( 55, 77, 78 ) );
       p.setColor( QPalette::Active, QColorGroup::Highlight, Qt::white );
       p.setColor( QPalette::Active, QColorGroup::HighlightedText, QColor( 55, 77, 78 ) );
       p.setColor( QPalette::Inactive, QColorGroup::Highlight, Qt::white );
       p.setColor( QPalette::Inactive, QColorGroup::HighlightedText, QColor( 55, 77, 78 ) );
       p.setColor( QPalette::Disabled, QColorGroup::Highlight, Qt::white );
       p.setColor( QPalette::Disabled, QColorGroup::HighlightedText, QColor( 55, 77, 78 ) );
       p.setColor( QPalette::Active, QColorGroup::Foreground, Qt::white );
       p.setColor( QPalette::Active, QColorGroup::Text, Qt::white );
       p.setColor( QPalette::Active, QColorGroup::ButtonText, Qt::white );
       p.setColor( QPalette::Inactive, QColorGroup::Foreground, Qt::white );
       p.setColor( QPalette::Inactive, QColorGroup::Text, Qt::white );
       p.setColor( QPalette::Inactive, QColorGroup::ButtonText, Qt::white );
       p.setColor( QPalette::Disabled, QColorGroup::Foreground, Qt::lightGray );
       p.setColor( QPalette::Disabled, QColorGroup::Text, Qt::lightGray );
       p.setColor( QPalette::Disabled, QColorGroup::ButtonText, Qt::lightGray );
       qApp->setPalette( p, true );
       break;
     }
    default:
     QApplication::setStyle(new QWindowsStyle());
     break;
  }
  mStyle = s;
#endif
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
  Q3ListViewItem* li = 0;
  Q3ListViewItem* match_li = 0;
  bool was_net = false;
  QString qs;
  QString last_dev;
  QString last_dev_vendor;
  QString last_dev_model;
  last_dev = xmlConfig->stringValue("LAST_DEVICE",QString::null);
  last_dev_vendor = xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString::null);
  last_dev_model = xmlConfig->stringValue("LAST_DEVICE_MODEL",QString::null);
  if(last_dev.isEmpty() || last_dev_vendor.isEmpty() || last_dev_model.isEmpty())
    return;
  if(last_dev.left(4) == "net:")
    was_net = true;
  //iterate over root elements, and check whether last_dev is present
  li = mpListView->firstChild();
  if(li == 0)
    return; //no elements at all
  do
  {
    if(li->text(0) == last_dev)
    {
      match_li = li;
      break;
    }
    li = li->nextSibling();
  }while (li != 0);
  if(match_li == 0) //last device not present
  {
    li = mpListView->firstChild();
    do
    {
      bool is_net = false;
      if(li->text(0).left(4) == "net:")
        is_net = true;
      if((li->text(1) == last_dev_vendor) && (li->text(2) == last_dev_model) &&
         (is_net == was_net))
      {
        match_li = li;
        break;
      }
      li = li->nextSibling();
    }while (li != 0);
  }
  if(match_li == 0) //last attempt: check vendor/model only
  {
    li = mpListView->firstChild();
    do
    {
      if((li->text(1) == last_dev_vendor) && (li->text(2) == last_dev_model))
      {
        match_li = li;
        break;
      }
      li = li->nextSibling();
    }while (li != 0);
  }
  if(!match_li)
    return;
  li = 0;
  li = match_li->firstChild();
  if(!li)
    return;
  do
  {
    if(li->text(0) == tr("Last settings"))
    {
      li->setOpen(true);
      mpListView->setSelected(li,true);
      mpListView->ensureItemVisible(li);
      mpSelectButton->setEnabled(true);
      mpLastItem = li;
      break;
    }
    li = li->nextSibling();
  }while (li != 0);
}
