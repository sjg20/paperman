/***************************************************************************
                          qxmlconfig.h  -  description
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


#ifndef QXMLCONFIG_H
#define QXMLCONFIG_H

#include <qapplication.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qmap.h>

class QDomElement;
class QXmlConfig;

extern QXmlConfig* xmlConfig;

class QXmlConfig
{
public:
  QXmlConfig(const QString& filepath, const QString& creator,
             const QString& version);
  QXmlConfig();
  virtual ~QXmlConfig();

  // read/write file
  void readConfigFile();
  void writeConfigFile();

  const QString& filepath();
  const QString& creator();
  const QString& version();
  void setFilePath(const QString& filepath);
  void setCreator(const QString& creator);
  void setVersion(const QString& version);
  bool isValid();
  bool isEmpty();
  QString absConfDirPath();

  // string value
  QString stringValue(const QString& key,QString def_val=QString());
  void setStringValue(const QString& key, const QString& value);
  // boolean value
  bool boolValue(const QString& key,bool def_val=false);
  void setBoolValue(const QString& key, bool value);
  // integer value
  int intValue(const QString& key,int def_val=0);
  void setIntValue(const QString& key, int value);
  // unsigned integer value
  unsigned int uintValue(const QString& key,unsigned int def_val=0);
  void setUintValue(const QString& key,unsigned int value);
  // double value
  double doubleValue(const QString& key,double def_val=0.0);
  void setDoubleValue(const QString& key, double value);
  //int value list
  void setStringList(const QString& key,QStringList list);
  QStringList stringList(const QString& key,
                         QStringList default_list=QStringList());
  //int value list
  void setIntValueList(const QString& key,QList<int> list);
  QList<int> intValueList(const QString& key,
                  QList<int> default_list=QList<int>());
  //uint value list
  void setUintValueList(const QString& key,QList<unsigned int> list);
  QList<unsigned int> uintValueList(const QString& key,
               QList<unsigned int> default_list=QList<unsigned int>());

  // remove a key/value from the preferences
  void removeKey(const QString& key);

private:
  bool mModified;
  bool mValid;
  bool mEmpty;
  QString mTempString;
  QString mFilePath;
  QString mCreator;
  QString mVersion;
  QMap<QString, QString> mConfMap;
};

#endif // PREFERENCES
