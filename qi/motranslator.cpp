/***************************************************************************
                          motranslator.cpp  -  description
                             -------------------
    begin                : Fri Apr 25 2003
    copyright            : (C) 2003 by Michael Herder
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

#include "motranslator.h"

#include <qapplication.h>
#include <qarray.h>
#include <qdatastream.h>
#include <qfile.h>

#include <stdlib.h>

MoTranslator::MoTranslator(QObject* parent,const char* name)
             :QTranslator(parent,name)
{
}
MoTranslator::~MoTranslator()
{
}
/** No descriptions */
bool MoTranslator::loadMoFile(QString filename,const char* context)
{
  int max_len = 0;
  int word_size;
  bool big_endian;
  char* orig_string = 0;
  char* trans_string;
  Q_UINT32 magic_number;
  qint32 file_revision;
  qint32 string_number;
  qint32 original_offset;
  qint32 translation_offset;
  QFile mofile(filename);

  if(!qSysInfo(&word_size,&big_endian))
    return false;
  if(mofile.open(IO_ReadOnly) == false)
    return false;
  QDataStream ds(&mofile);
  //read magic number
  ds >> magic_number;
  if(magic_number == 0xde120495)
  {
    //The file has been saved in reversed byte order in comparison
    //to this machine.
    //If this machines byte order is big endian, than the files
    //byte order is little endian and vice versa.
    qDebug("Reverse mo file byte order.");
    if(big_endian == true)
    {
      ds.setByteOrder(QDataStream::BigEndian);
    }
    else
    {
      ds.setByteOrder(QDataStream::LittleEndian);
    }
  }
  else if(magic_number != 0x950412de)
  {
    //wrong file format
    mofile.close();
    qDebug("mo file has wrong magic number.");
    return false;
  }
  //read file revision
  ds >> file_revision;
  if(file_revision != 0)
  {
    //wrong file format
    mofile.close();
    qDebug("Wrong mo file revision.");
    return false;
  }
  //read number of strings
  ds >> string_number;
  //read offset to original strings
  ds >> original_offset;
  //read offset to translation strings
  ds >> translation_offset;
  //We do not use the hash table; in fact, a mo file isn't required to contain a hash
  //table at all.
  qDebug("string number: %u",string_number);
  qDebug("orig offset: %u",original_offset);
  qDebug("trans offset: %u",translation_offset);
  QVector<qint32> orig_table(string_number*2);
  QVector<qint32> trans_table(string_number*2);
  //read original table
  if(ds.device()->at(original_offset))
  {
    for(qint32 i=0;i<string_number;i++)
    {
      ds >> orig_table[i*2];
      ds >> orig_table[i*2+1];
      if(orig_table[i*2] > max_len)
        max_len = orig_table[i*2];
    }
  }
  //read translation table
  if(ds.device()->at(translation_offset))
  {
    for(qint32 i=0;i<string_number;i++)
    {
      ds >> trans_table[i*2];
      ds >> trans_table[i*2+1];
      if(trans_table[i*2] > max_len)
        max_len = trans_table[i*2];
    }
  }

  QString trans;
  orig_string = new char [max_len+10];
  trans_string = new char [max_len+10];
  for(int i=0;i<string_number;i++)
  {
    if(ds.device()->at(orig_table[i*2+1]))
    {
      ds.readRawBytes(orig_string,orig_table[i*2]+1);
      qDebug("orig: %s",orig_string);
    }
    if(ds.device()->at(trans_table[i*2+1]))
    {
      ds.readRawBytes(trans_string,trans_table[i*2]+1);
      trans = QString::fromUtf8(trans_string);
      qDebug("trans: %s",trans.latin1());
     }
     QTranslatorMessage tm(context,orig_string,0,trans);
     insert(tm);
  }
  delete [] orig_string;
  delete [] trans_string;
  mofile.close();
  squeeze(Stripped);
  return true;
}
