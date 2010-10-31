/***************************************************************************
                          fileiosupporter.cpp  -  description
                             -------------------
    begin                : Thu Nov 21 2002
    copyright            : (C) 2002 by Michael Herder
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

#include "fileiosupporter.h"

#include <math.h>

#include <qdir.h>
#include <qfile.h>
#include <qobject.h>
#include <qregexp.h>

FileIOSupporter::FileIOSupporter()
{
  mErrorString = QString::null;
}
FileIOSupporter::~FileIOSupporter()
{
}
/** No descriptions */
QString FileIOSupporter::getIncreasingFilename(QString filepath,int step,int width,bool fill_gap)
{
  int max_counter = 0;
  int next_counter = 0; //only used if fill_gap == true
  QVector<int> counter_array;
  QStringList filelist;
  QString qs;
  QString new_filepath;
  QString filename;
  QString filetemplate;
  QString ext;
  QFileInfo fi(filepath);
  mErrorString = QString::null;
  if(fi.isDir())
    return QString::null;
  filename = fi.fileName();
  if(!(fi.extension(false).isEmpty()))
    ext = "." + fi.extension(false);
  counter_array.resize(0);

  filetemplate = filename.left(filename.length()-ext.length());
  //If there's a number on the right side of the filetemplate, get it's
  //value
  QString temp;
  unsigned int cnt;
  bool ok;
  int number = 0;
  int digit_cnt = -1;
  for(cnt=filetemplate.length()-1;cnt>=0;cnt--)
  {
     QChar ch = filetemplate[cnt];
     if(!ch.isDigit())
       break;
     digit_cnt = cnt;
  }
  if(digit_cnt > -1)
  {
    number = filetemplate.right(filetemplate.length()-digit_cnt).toInt();
    filetemplate = filetemplate.left(digit_cnt);
  }

  qDebug("digit_cnt %i",digit_cnt);
  qDebug("filetemplate.right() %s",filetemplate.latin1());
  qDebug("number found %i",number);

  filelist = getAbsPathList(fi.dir(true).absPath(),filetemplate,width,ext);

  //get the highest counter or the highest number not occupied by an existing file
  //if fill_gap is true
  qDebug("filelist.count() %i",filelist.count());
  for(int n=0;n<int(filelist.count());n++)
  {
     QString qs = filelist[n];
     qDebug("qs in %s",qs.latin1());
     qs = qs.mid(filetemplate.length(),qs.length()-filetemplate.length()-ext.length());
     qDebug("qs %s",qs.latin1());
     int val = qs.toInt(&ok);
     if(ok)
     {
       counter_array.resize(counter_array.size() + 1);
       counter_array[counter_array.size() - 1] = val;
       if(val > max_counter)
       {
         max_counter = val;
       }
    }
  }
  qDebug("counter_array.size() %i",counter_array.size());
  if(fill_gap == true)
  {
    //search the next possible counter, which isn't occupied already
    int s = step;
    if(s == 0)
      s = 1;//just to be sure
    next_counter = number;
    if(counter_array.size() > 0)
    {
      while(counter_array.find(next_counter) != -1)
      {
        next_counter += s;
      }
      if(counter_array.find(next_counter) != -1)
        next_counter += s;
    }
  }
  else
  {
    if(counter_array.size() == 0)//no match
    {
      next_counter = number;
    }
    else
    {
      if(max_counter > number)
      {
        next_counter = number;
        while(next_counter <= max_counter)
          next_counter += step;
      }
      else
        next_counter = number + step;
    }
  }
  //when the user set a counter width, it's possible
  //that max_counter is greater than the maximal possible counter value
  qDebug("next_counter %i",next_counter);
  qDebug("max_counter %i",max_counter);
  if(width > 0)
  {
    if(next_counter > (pow(10,width)-1))
    {
      mErrorString = QObject::tr("The file counter exceeded its maximal value.") + "\n";
      return QString::null;
    }
  }
  QString c_s;
  QString c_s2;
  if(width > 0)
    c_s = "%0."+QString("%1").arg(width)+"u";
  else
    c_s = "%u";
  QString dir_path = fi.dir(true).absPath();
  if(dir_path.right(1) != "/")
    dir_path+="/";
  c_s2.sprintf(c_s.latin1(),next_counter);
  new_filepath = dir_path + filetemplate + c_s2 + ext;
  qDebug("new_filepath %s",new_filepath.latin1());
  return new_filepath;
}
/** No descriptions */
QStringList FileIOSupporter::getAbsPathList(QString dir_path,QString filetemplate,
                                            int width,QString ext)
{
  QStringList filelist;
  QString temp_ext; //template extension
  if(!ext.isEmpty())
  {
    //if user added the same extension to the filetemplate too,
    //remove it
    if(filetemplate.right(ext.length()) == ext)
      filetemplate = filetemplate.left(filetemplate.length() - ext.length());
    ext = "\\" + ext;
  }
  ext += "$";
  //If there's a number on the right side of the filetemplate, remove it
  bool ok;
  int number = 0;
  do
  {
     number = filetemplate.right(1).toInt(&ok);
     if(ok)
     {
       //number found
       filetemplate = filetemplate.left(filetemplate.length()-1);
     }
  }while((ok == true) && !filetemplate.isEmpty());

  if(dir_path.right(1) != "/")
    dir_path += "/";
  QDir dir(dir_path);
  dir.setNameFilter(filetemplate+"*");
  filelist = dir.entryList();
  //filelist can still contain entries which we don't want
  //create a QRegExp and remove entries that don't fit the template
  QString reg_exp_str;
  QString reg_exp_str2;
  if(width<=0)
  {
    reg_exp_str = filetemplate + "[1-9]+[0-9]*"+ext;
    //special case filename0.ext, can't be covered with a single QRegExp
    reg_exp_str2 = filetemplate + "[0]" +ext;
  }
  else
  {
    reg_exp_str = filetemplate;
    for(int n=0;n<width;n++)
      reg_exp_str += "[0-9]";
    reg_exp_str += ext;
  }
qDebug("reg_exp_str %s",reg_exp_str.latin1());
qDebug("reg_exp_str2 %s",reg_exp_str2.latin1());
  QRegExp re(reg_exp_str);
  QRegExp re2(reg_exp_str2);
  //remove entries from filelist
  if(width<=0)
  {
    for(int n=int(filelist.count())-1;n>=0;n--)
    {
      if((re.match(filelist[n]) == -1) && (re2.match(filelist[n]) == -1))
        filelist.remove(filelist.at(n));
      else
        qDebug("matches %s",filelist[n].latin1());
    }
  }
  else
  {
    for(int n=int(filelist.count())-1;n>=0;n--)
    {
      if(re.match(filelist[n]) == -1)
        filelist.remove(filelist.at(n));
      else
        qDebug("matches %s",filelist[n].latin1());
    }
  }
qDebug("fileiosupporter filelist.count(): %i",int(filelist.count()));
  return filelist;
}
/**  */
bool FileIOSupporter::isValidFilename(QString filepath)
{
  QString qs = QString::null;
  QFileInfo fi(filepath);
  mErrorString = QString::null;
  if(!QFile::exists(filepath))
  {
    QFile f(filepath);
    if(!f.open(IO_WriteOnly))
    {
      mErrorString = QObject::tr("The file could not be opened in write mode.")+"\n";
      if(!fi.dir(true).exists())
        mErrorString += QObject::tr("The directory does not exist.")+"\n";
      else if(!fi.isWritable())
        mErrorString += QObject::tr("You don't have write permission for the directory.")+"\n";
      return false;
    }
    f.close();
    QFile::remove(filepath);
    return true;
  }
  if(!fi.isWritable())
    mErrorString = QObject::tr("The file could not be opened in write mode.")+"\n";
  if(fi.isDir())
    mErrorString += QObject::tr("The filename points to a directory.")+"\n";
  if(fi.isSymLink())
    mErrorString += QObject::tr("The file is a symbolic link.")+"\n";
  if(!mErrorString.isEmpty())
    return false;
  return true;
}
/** No descriptions */
QString FileIOSupporter::lastErrorString()
{
  return mErrorString;
}
