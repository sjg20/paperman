/***************************************************************************
                          qdevicesettings.cpp  -  description
                             -------------------
    begin                : Mon Mar 05 2001
    copyright            : (C) 2001 by Michael. Herder
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

#include <Q3ListView>
#include <QTextStream>

#include "qdevicesettings.h"
#include "qdraglistbox.h"
#include "qscanner.h"
#include "qxmlconfig.h"

#include <qapplication.h>
#include <qdom.h>
#include <qfile.h>
#include <q3groupbox.h>
#include <q3hbox.h>
#include <q3header.h>
#include <qinputdialog.h>
#include <qlabel.h>
#include <qlayout.h>
//#include <qmap.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <q3textstream.h>
#include <qtoolbutton.h>
#include <q3vbox.h>
#include <q3whatsthis.h>
//Added by qt3to4:
#include <Q3GridLayout>

//Define the device settings version, which this class writes.
//This version number is not related to QuiteInsanes version.
//The number is siply increased by 1 with every new version.

#ifndef DEV_SETTINGS_VERSION
#define DEV_SETTINGS_VERSION "1"
#endif

QDeviceSettings::QDeviceSettings(QScanner* s,QWidget *parent,const char *name,
                                 bool modal,Qt::WFlags f)
                :QDialog(parent,name,modal,f)
{
  setCaption(tr("Device settings"));
  mpScanner = s;
  initWidget();
  createContents();
}

QDeviceSettings::~QDeviceSettings()
{
}
/**  */
void QDeviceSettings::initWidget()
{
  QString qs;
  Q3GridLayout* mainlayout = new Q3GridLayout(this,3,5);
  mainlayout->setSpacing( 6 );
  mainlayout->setMargin( 11 );
  mainlayout->setAlignment( Qt::AlignTop );

  Q3HBox* hbox1 = new Q3HBox(this);
  hbox1->setSpacing(4);
  qs = tr("Device: ");
  qs += mpScanner->vendor()+" "+mpScanner->model();
//qDebug("DeviceName: %s",qs.latin1());
  QLabel* label1 = new QLabel(qs,hbox1);
  QToolButton* WhatsThisButton = Q3WhatsThis::whatsThisButton(hbox1);
  WhatsThisButton->setAutoRaise(FALSE);
  hbox1->setStretchFactor(label1,1);
  mainlayout->addMultiCellWidget(hbox1,0,0,0,4);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    WhatsThisButton->hide();
//sublayout
  Q3GroupBox* gb = new Q3GroupBox(this);
  Q3GridLayout* sublayout = new Q3GridLayout(gb,2,3);
  sublayout->setSpacing( 6 );
  sublayout->setMargin( 11 );
//the listview
	mpListView = new Q3ListView(gb);
  mpListView->setMinimumHeight(100);
  mpListView->setSelectionMode(Q3ListView::Single);
  mpListView->addColumn(tr("Available settings"));
  sublayout->addMultiCellWidget(mpListView,0,0,0,2);
//new button
	mpButtonNew = new QPushButton(tr("&New"),gb);
  sublayout->addWidget(mpButtonNew,1,0);
//delete button
	mpButtonDelete = new QPushButton(tr("&Delete"),gb);
  mpButtonDelete->setEnabled(FALSE);
  sublayout->addWidget(mpButtonDelete,1,2);
  sublayout->setColStretch(1,1);
  sublayout->setRowStretch(0,1);
  mainlayout->addMultiCellWidget(gb,1,1,0,4);
//load button
	mpButtonCancel = new QPushButton(tr("&Close"),this);
//load button
	mpButtonLoad = new QPushButton(tr("&Load"),this);
  mpButtonLoad->setDefault(true);
  mpButtonLoad->setEnabled(FALSE);
//close button
	mpButtonSave = new QPushButton(tr("&Save"),this);
  mpButtonSave->setEnabled(FALSE);
///add to sublayout
	mainlayout->addWidget(mpButtonCancel,2,0);
	mainlayout->addWidget(mpButtonSave,2,2);
	mainlayout->addWidget(mpButtonLoad,2,4);
  mainlayout->setColStretch(1,1);
  mainlayout->setColStretch(3,1);

  connect(mpButtonCancel,SIGNAL(clicked()),this,SLOT(reject()));
  connect(mpButtonNew,SIGNAL(clicked()),this,SLOT(slotNew()));
  connect(mpButtonSave,SIGNAL(clicked()),this,SLOT(slotSave()));
  connect(mpButtonLoad,SIGNAL(clicked()),this,SLOT(slotLoad()));
  connect(mpButtonDelete,SIGNAL(clicked()),this,SLOT(slotDelete()));
	connect(mpListView,SIGNAL(selectionChanged()),
            this,SLOT(slotSelectionChanged()));
 	connect(mpListView,SIGNAL(doubleClicked(Q3ListViewItem*)),
          this,SLOT(slotDoubleClicked(Q3ListViewItem*)));
  //help
  createWhatsThisHelp();
}
/**  */
void QDeviceSettings::slotClearList()
{
	mpListView->clear();
}
/**  */
void QDeviceSettings::createContents()
{
//try to load the settings file
  QDomDocument doc("maxview_device_settings");
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if ( !f.open( QIODevice::ReadOnly ) ) return;
  if( !doc.setContent( &f ) )
  {
    f.close();
    return;
  }
  f.close();

//Check, whether it's really a device settings file.
  if(doc.doctype().name() != "maxview_device_settings") return; //nope
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
      //Check whether there are saved settings for the device in use
//qDebug("mpScanner->deviceSettingsName() == %s",mpScanner->deviceSettingsName().latin1());
      if(mpScanner->deviceSettingsName() == ele.attribute("name"))
      {
        //We found one; we create a new listview item and insert it
        if(!ele.attribute("username").isNull())
        {
          //We use tr(), because the previous settings are saved
          //under Last settings and this string has to be translated
          new Q3ListViewItem(mpListView,tr(ele.attribute("username")));
        }
      }
    }
  }
  if(mpListView->childCount() > 0)
    mpListView->setCurrentItem(mpListView->firstChild());
}
/**  */
void QDeviceSettings::slotDelete()
{
  Q3ListViewItem* li;
  li = mpListView->currentItem();
  if(!li) return;
  QString username;
  username = mpListView->currentItem()->text(0);
//try to load the settings file
  QDomDocument doc("maxview_device_settings");
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if ( !f.open( QIODevice::ReadOnly ) )
  {
    //strange!
    return;
  }
  if( !doc.setContent( &f ) )
  {
    f.close();//also strange!
    return;
  }
  f.close();
//Check, whether it's really a device settings file.
//This method to find the settings is quite primitive.
//It easily fails, if the file was modified manually.
  QDomElement docElem;
  if(doc.doctype().name() != "maxview_device_settings") return; //nope
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
      //Check whether there are saved settings for the device in use
      if((mpScanner->deviceSettingsName() == ele.attribute("name")) &&
         (username == ele.attribute("username")))
      {
        //We found an already saved setting; we parse the settings to
        //see, whether there are vector files, which we have to delete, too.
        QDomNodeList dl = ele.childNodes();
        QString vals;
        for(int cnt=0;cnt<dl.count();cnt++)
        {
          QDomElement e2 = dl.item(cnt).toElement();
          if(e2.tagName() == "sane_option")
          {
            //check whether there's a value attribute
            vals = e2.attribute("value");
            if(!vals.isNull())
            { //ok
              if(vals.right(4) == ".vec")
              {
                //seems to be a vector file, delete it
                QFile::remove(vals);
              }
            }
          }
          root.removeChild(ele);
        }
      }
    }
  }
  QFile of( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if ( !of.open( QIODevice::WriteOnly ) ) return;
  QTextStream ts(&of);
  doc.save(ts, 0);
  of.close();
  slotClearList();
  createContents();
  return;
}
/**  */
void QDeviceSettings::createWhatsThisHelp()
{
  //save
  Q3WhatsThis::add(mpButtonSave,tr("Click this button to save the "
                     "device settings under the currently "
                     "selected(highlighted) name."));
  //delete
  Q3WhatsThis::add(mpButtonDelete,tr("Click this button to delete the "
                     "selected(highlighted) device setting."));
  //load
  Q3WhatsThis::add(mpButtonLoad,tr("Click this button to load the "
                     "selected(highlighted) device setting."));
  //cancel
  Q3WhatsThis::add(mpButtonCancel,tr("Click this button to close the "
                                  "dialog."));
  //new
  Q3WhatsThis::add(mpButtonNew,tr("Click this button to create a new device "
                     "setting. You will be prompted for a name. The settings are "
                     "automatically saved."));
  //listview
  Q3WhatsThis::add(mpListView,tr("This listview shows the available  "
                     "settings. Doubleclick on an item to load the setting. "));
}
/**  */
void QDeviceSettings::slotSelectionChanged()
{
  Q3ListViewItemIterator it(mpListView);
  for ( ; it.current(); ++it )
  {
    if(it.current()->isSelected())
    {
      mpButtonLoad->setEnabled(TRUE);
      if(it.current()->text(0) != tr("Last settings"))
      {
        mpButtonDelete->setEnabled(TRUE);
        mpButtonSave->setEnabled(TRUE);
      }
      else
      {
        mpButtonSave->setEnabled(FALSE);
        mpButtonDelete->setEnabled(FALSE);
      }
      return;
    }
  }
  mpButtonDelete->setEnabled(FALSE);
  mpButtonLoad->setEnabled(FALSE);
  mpButtonSave->setEnabled(FALSE);
}
/**  */
void QDeviceSettings::slotNew()
{
  bool ok = FALSE;
  QString text;
#ifdef USE_QT3
  text = QInputDialog::getText(tr("New entry"),
                               tr("Please enter a name"),
                               QLineEdit::Normal,
                               QString::null, &ok, this );
#else
  text = QInputDialog::getText(tr("New entry"),
                               tr("Please enter a name"),
                               QString::null, &ok, this );
#endif
  if(!ok || text.isEmpty())return;//user entered nothing
  //the user entered a new entry
  //check, whether it has a unique name
  Q3ListViewItemIterator it(mpListView);
  for ( ; it.current(); ++it )
  {
    if(it.current()->text(0) == text)
    {
      //the name already exists
      QMessageBox::warning(this,tr("Warning"),
                           tr("An entry with this name already exists.\n"
                              "Please enter a unique name."),
                           tr("OK"));
      return;
    }
  }
  Q3ListViewItem* li = new Q3ListViewItem(mpListView,text);
  mpListView->setCurrentItem(li);
  //we save the current settings automatically if a new entry has
  //been created
  if(!saveDeviceSettings())
    QMessageBox::warning(this,tr("Warning"),
                         tr("The settings could not be saved."),
                         tr("OK"));

}
/**  */
void QDeviceSettings::slotSave()
{
  if(saveDeviceSettings())
    accept();
  //else todo: warning

}
/**  */
void QDeviceSettings::slotLoad()
{
  QString qs;
  mOptionMap.clear();
  Q3ListViewItem* li;
  li = mpListView->currentItem();
  if(!li) return;
//try to load the settings file
  QDomDocument doc("maxview_device_settings");
  doc.clear();
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if (f.open( QIODevice::ReadOnly ) )
  {
    doc.setContent( &f );
    f.close();
  }
  else
    return;
  if(!doc.isNull())
  {//it's a setting
    //We search the specific entry in the xml file.
    QDomElement docElem = doc.documentElement();
    if(doc.doctype().name() == "maxview_device_settings")//Elem.tagName() == "maxview_device_settings")
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
            //Search the device settings.
            qs = e.attribute("username");
            //Only translated the Last settings entry to avoid confusion
            //if a user specified name matches a translated string
            if(qs == "Last settings") qs = tr(qs);
            if((mpScanner->deviceSettingsName() == e.attribute("name"))  &&
               (li->text(0) == qs))
            {
              //We found the entry.
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
              mpScanner->setOptionsByName(mOptionMap);
              break;
            }
          }
        }
        n = n.nextSibling();
      }
    }
  }
  accept();
}
/**  */
bool QDeviceSettings::saveDeviceSettings(QString uname)
{
  QString username;
  if(uname.isNull())
  {
    if(!mpListView->currentItem()) return false;
    username = mpListView->currentItem()->text(0);
  }
  else
    username = uname;
//try to load the settings file
  QDomDocument doc("maxview_device_settings");
//  QDomDocument doc;
  QFile f( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  //if there's no devicesettings file, try to create it
  if(!f.exists())
  {
    if(f.open(QIODevice::WriteOnly))
    {
      QTextStream textstream(&f);
      // create the root element
      QDomElement root = doc.createElement(doc.doctype().name());
      root.setAttribute("version", DEV_SETTINGS_VERSION);
      doc.appendChild(root);
      doc.save(textstream, 0);
      f.close();
      //file successfully created
    }
    else
    {
      //could not create file
      if(uname.isNull())
      {
        QMessageBox::warning(this,tr("Warning"),
                     tr("The device settings could not be saved, "
                        "because the device settings file could "
                        "not be created.\n"
                        "This can mean, that your disk is full, or "
                        "that you don't have write permission."),
                     tr("OK"));
      }
      return false;
    }
  }
  if ( !f.open( QIODevice::ReadOnly ) )
  {
    //add message box !
    return false;
  }
  if( !doc.setContent( &f ) )
  {
    f.close();
    //The file exists, but isn't a valid device settings file.
    //We create a new file.
    if(f.open(QIODevice::WriteOnly))
    {
      QDomDocument doc2("maxview_device_settings");
      QTextStream textstream(&f);
      // create the root element
      QDomElement root = doc2.createElement(doc2.doctype().name());
      root.setAttribute("version", DEV_SETTINGS_VERSION);
      doc2.appendChild(root);
      doc2.save(textstream, 0);
      f.close();
      //file successfully created
    }
    else
      return false;
  }
  f.close();
//Check, whether it's really a device settings file.
  if(doc.doctype().name() != "maxview_device_settings")
    return false;
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
      //Check whether there are saved settings for the device in use
      if((mpScanner->deviceSettingsName() == ele.attribute("name")) &&
         (username == ele.attribute("username")))
      {
        //We found an already saved setting; we parse the settings to
        //see, whether there are vector files, which we have to delete, too.
        QDomNodeList dl = ele.elementsByTagName("sane_option");
        QString vals;
        for(int cnt=0;cnt<dl.count();cnt++)
        {
          if(dl.item(cnt).isElement())
          {
            QDomElement e2 = dl.item(cnt).toElement();
            if(e2.tagName() == "sane_option")
            {
              //check whether there's a value attribute
              vals = e2.attribute("value");
              if(!vals.isNull())
              { //ok
                if(vals.right(4) == ".vec")
                {
                  //seems to be a vector file, delete it
                  QFile::remove(vals);
                }
              }
            }
          }
        }
        root.removeChild(ele);
        break;
      }
    }
  }
  QDomElement elem = doc.createElement("sane_device");
  elem.setAttribute("name",mpScanner->deviceSettingsName());
  elem.setAttribute("username",username);
  mpScanner->settingsDomElement(doc,elem);
  root.appendChild(elem);
  QFile of( xmlConfig->absConfDirPath()+"devicesettings.xml" );
  if ( !of.open( QIODevice::WriteOnly ) ) return false;
  QTextStream ts(&of);
  doc.save(ts, 0);
  of.close();
  return true;
}
/**  */
void QDeviceSettings::slotDoubleClicked(Q3ListViewItem*)
{
  slotLoad();
}
