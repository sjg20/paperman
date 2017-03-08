/***************************************************************************
                          qxmlconfig.cpp  -  description
                             -------------------
    begin                : Tue Apr 24 2001
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2        *
 *   as published by the Free Software Foundation.                          *
 *                                                                         *
 ***************************************************************************/

#include <QTextStream>

#include <qdom.h>
#include <qfile.h>
#include <qfileinfo.h>

#include "qxmlconfig.h"

QXmlConfig* xmlConfig=0;
//Read in the configuration file filename. Creates the file, if it
//doesn't exist.
QXmlConfig::QXmlConfig(const QString& filename,const QString& creator,
                       const QString& version)

{
  mModified = false;
  mValid = false;
  mEmpty = true;
  mFilePath = filename;
  mCreator = creator;
  mVersion = version;
  readConfigFile();
  if(xmlConfig)
    qWarning("QXmlConfig: Only one xmlConfig object allowed");
  xmlConfig = (QXmlConfig*) this;
}

//Constructs an empty QXmlConfig
QXmlConfig::QXmlConfig()
{
  mModified = false;
  mValid = false;
  mFilePath = QString::null;
  mCreator = QString::null;
  mVersion = QString::null;
  if(xmlConfig)
    qWarning("QXmlConfig: Only one xmlConfig object allowed");
  xmlConfig = (QXmlConfig*) this;
}
// Destructor
QXmlConfig::~QXmlConfig()
{
}

bool QXmlConfig::boolValue(const QString& key,bool def_val)
{
  if(!mConfMap.contains(key))
  {
    mModified = true;
    if(def_val)
    {
      mConfMap[key] = "true";
      return true;
    }
    else
    {
      mConfMap[key] = "false";
      return false;
    }
  }
  mTempString = mConfMap[key];
  if(mTempString == "true")
    return true;
  else
    return false;
}

void QXmlConfig::setBoolValue(const QString& key, bool value)
{
  mConfMap[key] = value ? "true" : "false";
  mModified = true;
}

int QXmlConfig::intValue(const QString& key,int def_val)
{
  if(!mConfMap.contains(key))
  {
    mTempString.setNum(def_val);
    mConfMap[key] = mTempString;
    mModified = true;
    return def_val;
  }
  mTempString = mConfMap[key];
  bool ok;
  int num = mTempString.toInt(&ok);
  if (ok)
    return num;
  else
    return 0;
}

void QXmlConfig::setIntValue(const QString& key, int value)
{
  mTempString.setNum(value);
  mConfMap[key] = mTempString;
  mModified = true;
}

unsigned int QXmlConfig::uintValue(const QString& key,unsigned int def_val)
{
  if(!mConfMap.contains(key))
  {
    mTempString.setNum(def_val);
    mConfMap[key] = mTempString;
    mModified = true;
    return def_val;
  }
  mTempString = mConfMap[key];
  bool ok;
  unsigned int num = mTempString.toUInt(&ok);
  if (ok)
    return num;
  else
    return 0;
}

void QXmlConfig::setUintValue(const QString& key,unsigned int value)
{
  mTempString.setNum(value);
  mConfMap[key] = mTempString;
  mModified = true;
}

double QXmlConfig::doubleValue(const QString& key,double def_val)
{
  if(!mConfMap.contains(key))
  {
    mTempString.setNum(def_val);
    mConfMap[key] = mTempString;
    mModified = true;
    return def_val;
  }
  mTempString = mConfMap[key];
  bool ok;
  double num = mTempString.toDouble(&ok);
  if (ok)
    return num;
  else
    return 0.0;
}

void QXmlConfig::setDoubleValue(const QString& key, double value)
{
  mTempString.setNum(value);
  mConfMap[key] = mTempString;
  mModified = true;
}

QString QXmlConfig::stringValue(const QString& key,QString def_val)
{
  if(!mConfMap.contains(key))
  {
    mConfMap[key] = def_val;
    mModified = true;
    return def_val;
  }
  if (mConfMap.contains(key))
  {
    return mConfMap[key];
  }
  return QString::null;
}

void QXmlConfig::setStringValue(const QString& key, const QString& value)
{
  mConfMap[key] = value;
  mModified = true;
}

void QXmlConfig::removeKey(const QString& key)
{
    mConfMap.remove(key);
}

void QXmlConfig::readConfigFile()
{
  QFile conffile(mFilePath);
  //If createnew=true and the file doesn't exist,
  //try to create a new file.
  if(!conffile.exists())
  {
    writeConfigFile();
    mEmpty = true;
  }
  else
    mEmpty = false;
  if (!conffile.open(QIODevice::ReadOnly))
  {
    // error opening file
    conffile.close();
    mValid = false;
    mEmpty = true;
    return;
  }
  // open dom document
  QDomDocument doc("QXmlConfig");
  if (!doc.setContent(&conffile))
  {
    conffile.close();
    mValid = false;
    mEmpty = true;
    return;
  }
  conffile.close();

  // check the doc type
  if (doc.doctype().name() != "QXmlConfig")
  {
    // wrong file type
    mValid = false;
    mEmpty = true;
    return;
  }
  QDomElement root = doc.documentElement();
  if (root.attribute("creator") != mCreator)
  {
    //wrong creator
    mValid = false;
    mEmpty = true;
    return;
  }
  // get list of items
  QDomNodeList nodes = root.elementsByTagName("config_item");
  QDomElement ele;
  // iterate over the items
  for (int n=0; n<nodes.count(); ++n)
  {
    if (nodes.item(n).isElement())
    {
      ele = nodes.item(n).toElement();
      if((ele.hasAttribute("key"))&&(ele.hasAttribute("value")))
      {
        mConfMap[ele.attribute("key")] = ele.attribute("value");
      }
    }
  }
  mEmpty = false;
  mValid = true;
}

void QXmlConfig::writeConfigFile()
{
  QDomDocument doc("QXmlConfig");

  // create the root element
  QDomElement root = doc.createElement(doc.doctype().name());
  root.setAttribute("version", mVersion);
  root.setAttribute("creator", mCreator);

  //iterate over the options
  QMap<QString,QString>::Iterator it;
  QDomElement option;
  for (it = mConfMap.begin(); it != mConfMap.end(); ++it)
  {
    //create an option element
    option = doc.createElement("config_item");
    option.setAttribute("key", it.key());
    option.setAttribute("value", it.value());
    root.appendChild(option);
  }
  doc.appendChild(root);

  // open file
  QFile conffile(mFilePath);
  if (!conffile.open(QIODevice::WriteOnly))
  {
    // error opening file
    conffile.close();
    return;
  }
  // write it out
  QTextStream textstream(&conffile);
  doc.save(textstream, 0);
  conffile.close();
}
const QString& QXmlConfig::filepath()
{
  return mFilePath;
}

void QXmlConfig::setFilePath(const QString& filepath)
{
  mFilePath = filepath;
}

const QString& QXmlConfig::creator()
{
  return mCreator;
}

void QXmlConfig::setCreator(const QString& creator)
{
  mCreator = creator;
}

const QString& QXmlConfig::version()
{
  return mVersion;
}

void QXmlConfig::setVersion(const QString& version)
{
  mVersion = version;
}

bool QXmlConfig::isValid()
{
  return mValid;
}

bool QXmlConfig::isEmpty()
{
  return mEmpty;
}

QString QXmlConfig::absConfDirPath()
{
  QString qs;
  qs = QFileInfo(mFilePath).absolutePath();
  if(qs.right(1) != "/")
    qs += "/";
  return qs;
}

void QXmlConfig::setStringList(const QString& key,QStringList list)
{
  QString qs;
  QStringList::Iterator it;
  mTempString = QString::null;
  for(it=list.begin();it!=list.end();++it)
    mTempString += *it + "|";
  if(mTempString.right(1) == "|")
    mTempString = mTempString.left(mTempString.length() - 1);
  mConfMap[key] = mTempString;
  mModified = true;
}

QStringList QXmlConfig::stringList(const QString& key,
                                   QStringList default_list)
{
  QString qs;
  if(!mConfMap.contains(key))
  {
    QStringList::Iterator it;
    mTempString = QString::null;
    for(it=default_list.begin();it!=default_list.end();++it)
      mTempString += *it + "|";
    if(mTempString.right(1) == "|")
      mTempString = mTempString.left(mTempString.length() - 1);
    mConfMap[key] = mTempString;
    mModified = true;
    return default_list;
  }
  QStringList list;
  mTempString = mConfMap[key];

  bool ready = false;
  int index;
  while(!ready)
  {
    index = mTempString.indexOf("|");
    if(index == -1)
    {
      ready = true;
      qs = mTempString;
    }
    else
    {
      qs = mTempString.left(index);
      mTempString = mTempString.right(mTempString.length() - qs.length() - 1);
    }
    list.append(qs);
  }
  return list;
}

void QXmlConfig::setIntValueList(const QString& key,QList<int> list)
{
  QString qs;
  QList<int>::Iterator it;
  mTempString = QString::null;
  for(it=list.begin();it!=list.end();++it)
    mTempString += qs.setNum(*it) + " ";
  mConfMap[key] = mTempString;
  mModified = true;
}

QList<int> QXmlConfig::intValueList(const QString& key,
                                         QList<int> default_list)
{
  QString qs;
  if(!mConfMap.contains(key))
  {
    QList<int>::Iterator it;
    mTempString = QString::null;
    for(it=default_list.begin();it!=default_list.end();++it)
      mTempString += qs.setNum(*it) + " ";
    mConfMap[key] = mTempString;
    mModified = true;
    return default_list;
  }
  QList<int> list;
  mTempString = mConfMap[key];
  mTempString = mTempString.trimmed();
  mTempString = mTempString.simplified();

  bool ok;
  bool ready = false;
  int index;
  int value;
  while(!ready)
  {
    index = mTempString.indexOf(" ");
    if(index == -1)
    {
      ready = true;
      qs = mTempString;
    }
    else
    {
      qs = mTempString.left(index + 1);
      mTempString = mTempString.right(mTempString.length()-qs.length());
    }
    value = qs.toInt(&ok);
    if(ok)
      list.append(value);
  }
  return list;
}

void QXmlConfig::setUintValueList(const QString& key,QList<unsigned int> list)
{
  QString qs;
  QList<unsigned int>::Iterator it;
  mTempString = QString::null;
  for(it=list.begin();it!=list.end();++it)
    mTempString += qs.setNum(*it) + " ";
  mConfMap[key] = mTempString;
  mModified = true;
}

QList<unsigned int> QXmlConfig::uintValueList(const QString& key,
                                                   QList<unsigned int> default_list)
{
  QString qs;
  if(!mConfMap.contains(key))
  {
    QList<unsigned int>::Iterator it;
    mTempString = QString::null;
    for(it=default_list.begin();it!=default_list.end();++it)
      mTempString += qs.setNum(*it) + " ";
    mConfMap[key] = mTempString;
    mModified = true;
    return default_list;
  }
  QList<unsigned int> list;
  mTempString = mConfMap[key];
  mTempString = mTempString.trimmed();
  mTempString = mTempString.simplified();

  bool ok;
  bool ready = false;
  int index;
  unsigned int value;
  while(!ready)
  {
    index = mTempString.indexOf(" ");
    if(index == -1)
    {
      ready = true;
      qs = mTempString;
    }
    else
    {
      qs = mTempString.left(index + 1);
      mTempString = mTempString.right(mTempString.length()-qs.length());
    }
    value = qs.toUInt(&ok);
    if(ok)
      list.append(value);
  }
  return list;
}
