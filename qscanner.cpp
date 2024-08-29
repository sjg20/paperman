/***************************************************************************
                          qscanner.cpp  -  description
                             -------------------
    begin                : Thu Jul 6 2000
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

#include <QPainter>
#include <QTextStream>

extern "C"
{
#include "md5.h"
}

#include "config.h"

#include "err.h"
//s #include "previewwidget.h"
#include "qscanner.h"
#include "qxmlconfig.h"
#include "resource.h"
#include "utils.h"

extern "C"
{
#include <sane/sane.h>
}
#include <sane/saneopts.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>

#include <QDebug>
#include <QProgressDialog>

#include <qapplication.h>
//s #include <qarray.h>
#include <qbuffer.h>
#include <qdatastream.h>
#include <qdialog.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qstring.h>
#include <stdlib.h>
#include <sys/poll.h>


/* simulated scanner

We support a 'pretend' scanner for testing purposes.

Options supported are
- x, y resolution
- mono
- duplex
*/


// only support simulscan on the latest version (needs QImage::Format_RGB888)
#if QT_VERSION >= 0x040400

#define SIMUL

#endif


#ifndef SANE_TITLE_STANDARD

/* these are only defined in the new Sane. We really want to make the
simulator available without having to replace sane with an unpackaged
version (in Ubuntu 8.10 for example), so define these here. */

#define SANE_NAME_STANDARD       "standard"
#define SANE_NAME_GEOMETRY       "geometry"
#define SANE_NAME_ENHANCEMENT    "enhancement"
#define SANE_NAME_ADVANCED       "advanced"
#define SANE_NAME_PAGE_WIDTH        "page-width"
#define SANE_NAME_PAGE_HEIGHT       "page-height"

#define SANE_TITLE_STANDARD         SANE_I18N("Standard")
#define SANE_TITLE_GEOMETRY         SANE_I18N("Geometry")
#define SANE_TITLE_ENHANCEMENT      SANE_I18N("Enhancement")
#define SANE_TITLE_ADVANCED         SANE_I18N("Advanced")
#define SANE_TITLE_PAGE_WIDTH       SANE_I18N("Page width")
#define SANE_TITLE_PAGE_HEIGHT      SANE_I18N("Page height")

#define SANE_DESC_STANDARD    SANE_I18N("Source, mode and resolution options")
#define SANE_DESC_GEOMETRY    SANE_I18N("Scan area and media size options")
#define SANE_DESC_ENHANCEMENT SANE_I18N("Image modification options")
#define SANE_DESC_ADVANCED    SANE_I18N("Hardware specific options")
#define SANE_DESC_PAGE_WIDTH \
SANE_I18N("Specifies the width of the media.  Required for automatic " \
"centering of sheet-fed scans.")

#define SANE_DESC_PAGE_HEIGHT \
SANE_I18N("Specifies the height of the media.")

#endif


#define DBG(level,fmt,...) //qDebug (fmt, ## __VA_ARGS__)

#ifdef SIMUL

#define SIMUL_NAME "simulscan"

#define SIMUL_DEV            ((void *)4)  // simul device number

static SANE_Device simul_dev =
{
    .name = SIMUL_NAME,
    .vendor = "Bluewater Systems Ltd",
    .model = "Bluewater Scanner 100",
    .type = "simulated scanner",
};



/* ------------------------------------------------------------------------- */
static const char string_Flatbed[] = "Flatbed";
static const char string_ADFFront[] = "ADF Front";
static const char string_ADFBack[] = "ADF Back";
static const char string_ADFDuplex[] = "ADF Duplex";

static const char string_Lineart[] = "Lineart";
static const char string_Halftone[] = "Halftone";
static const char string_Grayscale[] = "Gray";
static const char string_Color[] = "Color";

static const char string_None[] = "None";
static const char string_JPEG[] = "JPEG";


#endif


bool QScanner::msAuthorizationCancelled = false;

QScanner::QScanner() : QObject()
{
  mOptionNumber = -1;
  mSaneStatus = SANE_STATUS_GOOD;
  mNonBlockingIo = SANE_FALSE;
  mAppCancel = false;
  mCancelled = false;
  mOpenOk = false;
  mInitOk = false;
  mDeviceCnt = 0;
  mpDeviceList = 0L;
  mDeviceName  = "";
  mpProgress = 0L;
  mDeviceHandle   = 0L;
  mEmitSignals = true;
  mScanning = false;
  mTempFilePath = "";
  _compress = COMP_NONE;
#ifdef SIMUL
  _started = 0;
  _reading = 0;
  _cancelled = 0;
  _side = 0;

  /* total to read/write */
  _bytes_tot[0] = 0;
  _bytes_tot[1] = 0;

  /* how far we have read */
  _bytes_rx[0] = 0;
  _bytes_rx[1] = 0;
  _bytes_tx[0] = 0;
  _bytes_tx[1] = 0;
  _buffers[0] = 0;
  _buffers[1] = 0;
  _fds[0] = 0;
  _fds[1] = 0;
#endif
	initScanner();
}
QScanner::~QScanner()
{
//     qDebug () << "~QScanner";
  exitScanner();
#ifdef SIMUL
  if (_buffers[0])
    free (_buffers[0]);
  if (_buffers[1])
    free (_buffers[1]);
#endif
}
/**  */
bool QScanner::isInit()
{
  return mInitOk;
}
/**Initializes the scanner with a call to sane_init(). If this action
  * was successfull, true is returned, otherwise false.
  * Call saneStatus() to get the exact error.
  * Use isInit() to see, whether Sane is initialized already.
  */
bool QScanner::initScanner()
{
  if(mInitOk == true) return true;
	SANE_Status status;
  status = sane_init(0,qis_authorization);//authorize
	if(status == SANE_STATUS_GOOD)
  {
		mInitOk = true;
    return true;
  }
	else
  {
    mSaneStatus = status;
  	mInitOk = false;
  }
  return false;
}
/**  */
bool QScanner::getDeviceList(bool local_only)
{
	if(mInitOk==false) return false;
	QString qs;
	int i;
  SANE_Status status;

  mDeviceCnt = 0;
  status=sane_get_devices(&mpDeviceList,local_only ? SANE_TRUE:SANE_FALSE);
	if(status==SANE_STATUS_GOOD)
	{
		//devices succesfully queried
    for (i = 0; mpDeviceList[i] != 0L; i++)
 	  {
			mDeviceCnt += 1;
    }
    return true;
	}
  mSaneStatus = status;
  return false;
}


/**  */
bool QScanner::openDevice()
   {
   QScanner::msAuthorizationCancelled = false;
         SANE_Status status;
   mOptionNumber = -1;
   //if device already open or no device name chosen return
   if(mOpenOk==true) return true;
   if(mDeviceName.isEmpty())
      {
      mSaneStatus = SANE_STATUS_INVAL;
      return false;
      }
   status = do_sane_open(mDeviceName.toLatin1(), &mDeviceHandle);
   if(status == SANE_STATUS_GOOD)
      {
      mOpenOk = true;
      mOptionNumber = optionCount();
      findOptions ();
      return true;
      }
   mSaneStatus = status;
         mOpenOk = false;
   return false;
   }


/**  */
void QScanner::setDeviceName(SANE_String_Const dev_name)
{
	mDeviceName = dev_name;
    if (!strcmp(dev_name, SIMUL_NAME))
    {
        mDeviceVendor = simul_dev.vendor;
        mDeviceType = simul_dev.type;
        mDeviceModel = simul_dev.model;
        return;
    }
  for(int i=0;i<mDeviceCnt;i++)
  {
	  if(QString(mpDeviceList[i]->name) == mDeviceName)
    {
      mDeviceVendor = mpDeviceList[i]->vendor;
      mDeviceType = mpDeviceList[i]->type;
      mDeviceModel = mpDeviceList[i]->model;
      break;
    }
  }
}
/**  */
bool QScanner::isOpen()
{
	return mOpenOk;
}


/**  */
int QScanner::optionCount()
   {
   SANE_Int count;

   if(mOpenOk != true) return -1;
   if(mOptionNumber > -1)
      return mOptionNumber;
   SANE_Status status;

   status = do_sane_control_option (mDeviceHandle,0,SANE_ACTION_GET_VALUE,&count,0);
//      status = do_sane_control_option(mDeviceHandle,0,SANE_ACTION_GET_VALUE,&count,0);
   if(status == SANE_STATUS_GOOD)
      {
      mOptionNumber = count;
      return int(count);
      }
   return -1;
   }


/**  */
SANE_Value_Type QScanner::getOptionType(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
 	return option_desc->type;
}
/**  */
QString QScanner::saneStringValue(int num)
{
  QString qs;
  SANE_Status status;
  status = SANE_STATUS_INVAL;
	if(mOpenOk != true) return 0;
  const SANE_Option_Descriptor *option_desc;
  SANE_Char* val;
  val = 0L;
	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
  {
    if(option_desc->type == SANE_TYPE_STRING)
  	{
  		val = new SANE_Char[option_desc->size];
  		status = do_sane_control_option(mDeviceHandle,num,
                                   SANE_ACTION_GET_VALUE,val,0);
//printf ("len %d, string '%s'", option_desc->size, val);
  	}
    if(status == SANE_STATUS_GOOD)
  	{
      qs = (const char*) val;
      if(val) delete [] val;
  		return qs;
    }
  }
  if(val) delete [] val;
  qs = QString();
  return qs;
}
/**  */
int QScanner::saneWordValue(int num)
{
  SANE_Word val;
  SANE_Status status;
  status = SANE_STATUS_INVAL;
	if(mOpenOk != true) return INT_MIN;
  const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
  {
   	switch(option_desc->type)
  	{
  		case SANE_TYPE_BOOL:
  		case SANE_TYPE_INT:
  		case SANE_TYPE_FIXED:
        status = do_sane_control_option(mDeviceHandle,num,
                                     SANE_ACTION_GET_VALUE,&val,0);
//                     printf ("got %d", val);
  			break;
  		default:;
  	}
    if(status == SANE_STATUS_GOOD)
  		return int(val);
  }
  return INT_MIN;
}
QVector<SANE_Word> QScanner::saneWordArray(int num)
{
  QVector<SANE_Word> a;
  a.resize(0);
  if(mOpenOk != true) return a;
  const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
  {
    if(option_desc->size > (int)sizeof(SANE_Word))
    {
   	  switch(option_desc->type)
  	  {
    		case SANE_TYPE_INT:
    		case SANE_TYPE_FIXED:
          a.resize(option_desc->size/sizeof(SANE_Word));
          (void)do_sane_control_option(mDeviceHandle,num,
                                       SANE_ACTION_GET_VALUE,a.data(),0);
    			break;
    		default:;
      }
  	}
  }
  return a;
}
QVector<SANE_Word> QScanner::saneWordList(int num)
{
  int c;
  int i;
  QVector<SANE_Word> a;
  a.resize(0);
	if(mOpenOk != true) return a;
  const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
  {
    if(getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST)
    {
      //first element in list holds the number of values
      c = (int)option_desc->constraint.word_list[0];
      a.resize(c);
      for(i=0;i<c;i++)
        a[i] = (SANE_Word)option_desc->constraint.word_list[i+1];
  	}
  }
  return a;
}
/**  */
void QScanner::exitScanner()
   {
   if(mOpenOk)
      {
      do_sane_close(mDeviceHandle);
//    mDeviceHandle = 0;
      mOpenOk       = false;
      }
   if(mInitOk)
      {
      sane_exit();
      mInitOk     = false;
      mpDeviceList = 0;
      }
   }


/**  */
int QScanner::deviceCount()
{
#ifdef SIMUL
    return mDeviceCnt + 1;
#else
	return mDeviceCnt;
#endif
}
/**  */
SANE_String_Const QScanner::name(int num)
{
#ifdef SIMUL
    if (num == mDeviceCnt)
        return SIMUL_NAME;
#endif
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->name;
}
/**  */
SANE_String_Const QScanner::vendor(int num)
{
#ifdef SIMUL
    if (num == mDeviceCnt)
        return simul_dev.vendor;
#endif
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->vendor;
}
/**  */
SANE_String_Const QScanner::model(int num)
{
#ifdef SIMUL
    if (num == mDeviceCnt)
        return simul_dev.type;
#endif
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->model;
}
/**  */
SANE_String_Const QScanner::type(int num)
{
#ifdef SIMUL
    if (num == mDeviceCnt)
        return simul_dev.type;
#endif
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->type;
}
/**  */
SANE_Constraint_Type QScanner::getConstraintType(int num)
{
  const SANE_Option_Descriptor *option_desc;
  option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
    return option_desc->constraint_type;
  return SANE_CONSTRAINT_NONE;
}

/**  */
SANE_String_Const QScanner::getOptionName(int num)
   {
   const SANE_Option_Descriptor *option_desc;

   option_desc = do_sane_get_option_descriptor (mDeviceHandle, num);
   if(option_desc)
      return option_desc->name;
   return 0L;
   }


/**  */
bool QScanner::isOptionActive(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
   	if(SANE_OPTION_IS_ACTIVE(option_desc->cap) == SANE_TRUE)
      return true;
  return false;
}
/**  */
bool QScanner::isOptionSettable(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
   	if(SANE_OPTION_IS_SETTABLE(option_desc->cap) == SANE_TRUE)
      return true;
  return false;
}
/**  */
QString QScanner::getOptionTitle(int num)
{
  QString qs=QString();
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
    qs = tr(option_desc->title);
 	return qs;
}
/**Returns the number of groups  */
int QScanner::getGroupCount()
{
	int cnt;
	int num;
  cnt = 0;
	num = 0;
  const SANE_Option_Descriptor *option_desc;
	for(num = 1;num<mOptionNumber;num++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
    if(option_desc)
   		if(option_desc->type == SANE_TYPE_GROUP) cnt+=1;
	}
	return cnt;
}
/**Returns the index of the first item in the specified group
	 or -1 if an error occurs  */
int QScanner::firstGroupItem(int num)
{
	if(num>getGroupCount()) return -1;
	int cnt;
	int i;
  cnt = 0;
	i = 0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
    {
   		if(option_desc->type == SANE_TYPE_GROUP) cnt+=1;
  		if(cnt == num) //group found
  		{
  			//get next option, if there is one
        i+=1;
  			if(i<mOptionNumber)
  				  option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
  			if(option_desc->type != SANE_TYPE_GROUP)
  			{
  				return i;//return number of first item
  			}
  			else
  			{
  				return -1;//error; a group without an item?
  			}
  		}
    }
	}
	return -1;//error
}
/**Return the index of the last item in the specified group
  *or -1 if an error occurs
*/
int QScanner::lastGroupItem(int num)
{
	if(num>getGroupCount()) return -1;//error
	int cnt;
	int i;
  cnt = getGroupCount();
	i = 0;
  const SANE_Option_Descriptor *option_desc;
	for(i=mOptionNumber-1;i>0;i--)//start with last option
	{
	  option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
    {
  		if(cnt == num) //group found
  		{
  			//in group ?
  			if(option_desc->type != SANE_TYPE_GROUP)
  			{
  				return (i);//return number of last item
  			}
  			else
  			{
  				return -1;//error; a group without an item?
  			}
      }
  		if(option_desc->type == SANE_TYPE_GROUP)cnt -=1;//previous group
    }
	}
	return -1;//error
}
/**Returns the number of items in the specified
  *group.
*/
int QScanner::itemsInGroup(int num)
{
	int i;
	i = 0;
	i= lastGroupItem(num) - firstGroupItem(num);
	return i+1;
}
/**Checks whether there are active and settable
  *items in the specified group.
*/
bool QScanner::groupHasActiveItems(int num)
{
  const SANE_Option_Descriptor *option_desc;
	bool b;
  int  i;
	b = false;
	i = 0;
	for(i=firstGroupItem(num);i<=lastGroupItem(num);i++)
	{
	  option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
    {
  		if((SANE_OPTION_IS_ACTIVE(option_desc->cap) == SANE_TRUE) &&
         (SANE_OPTION_IS_SETTABLE(option_desc->cap) == SANE_TRUE))
  		{
  			b = true;
  			break;
  		}
    }
	}
	return b;
}

SANE_Word QScanner::getRangeMax(int num)
{
  QVector <SANE_Word> qa;
  SANE_Word sw;
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
  {
    if(option_desc->constraint_type == SANE_CONSTRAINT_RANGE)
  	{
  		if(option_desc->type == SANE_TYPE_FIXED)
  		{
   			return SANE_Word(option_desc->constraint.range->max);
  		}
  		else
  	  {
   			return option_desc->constraint.range->max;
  		}
  	}
  	else if(option_desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
   	{
      qa = saneWordList(num);
      sw = INT_MIN;
      for(int i=0;i<qa.size();i++)
        if(qa[i] > sw) sw = qa[i];
      return sw;
    }
  }
	return  INT_MIN;
}

SANE_Word QScanner::getRangeMin(int num)
{
  QVector <SANE_Word> qa;
  SANE_Word sw;
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
  {
    if(option_desc->constraint_type == SANE_CONSTRAINT_RANGE)
  	{
  		if(option_desc->type == SANE_TYPE_FIXED)
  		{
   			return SANE_Word(option_desc->constraint.range->min);
  		}
  		else
  	  {
   			return option_desc->constraint.range->min;
  		}
  	}
  	else if(option_desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
   	{
      qa = saneWordList(num);
      sw = INT_MAX;
      for(int i=0;i<qa.size();i++)
        if(qa[i] < sw) sw = qa[i];
      if(sw < INT_MAX) return sw;
    }
  }
  return  INT_MIN;
}

SANE_Word QScanner::getRangeQuant(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
    if(option_desc->constraint_type == SANE_CONSTRAINT_RANGE)
 	  	return option_desc->constraint.range->quant;
  return  INT_MIN;
}
/**  */
SANE_Unit QScanner::getUnit(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
    return option_desc->unit;
  return SANE_UNIT_NONE;
}
/**  */
SANE_String_Const QScanner::getStringListItem(int num, int item)
{
  int c;
	c = 0;
	const SANE_Option_Descriptor *option_desc;
  if(item>=0)
  {
    option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
    if(option_desc)
    {
      if(option_desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
      {
	      for(c=0;c<item;c++)
	      {
  	    	if(!option_desc->constraint.string_list[c]) return 0L;
  	    }
   	    return option_desc->constraint.string_list[c];
      }
    }
  }
  return 0L;
}
/**  */
QStringList QScanner::getStringList(int option)
{
  QStringList slist;
  int c;
	c = 0;
	const SANE_Option_Descriptor *option_desc;
  option_desc=do_sane_get_option_descriptor(mDeviceHandle,option);
  if(option_desc)
  {
    if(option_desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
    {
      while(option_desc->constraint.string_list[c])
      {
        slist.append(option_desc->constraint.string_list[c]);
        ++c;
	    }
    }
  }
  return slist;
}
/**  */
QString QScanner::getGroupTitle(int num)
{
  QString qs;
	if(num>getGroupCount()) return 0L;
	int cnt;
	int i;
  cnt = 0;
	i = 0;
  qs = "";
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(!option_desc) continue;
 		if(option_desc->type == SANE_TYPE_GROUP)
    {
			cnt+=1;
			if(cnt == num) //group found
	  	{
			  option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
        if(option_desc)
        {
          qs =tr(option_desc->title);// qApp->translate(0,option_desc->title);
          return qs;
        }
		  }
		}
	}
	return qs;//error
}
/**  */
bool QScanner::isTlxSettable()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
   		if((QString(option_desc->name)=="tl-x") &&
	  		 (isOptionSettable(i) == SANE_TRUE))
        return true;
	}
	return false;
}
/**  */
bool QScanner::isTlySettable()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
   		if((QString(option_desc->name)=="tl-y") &&
	  		 (isOptionSettable(i) == SANE_TRUE))
        return true;
	}
	return false;

}
/**  */
bool QScanner::isBrxSettable()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
   		if((QString(option_desc->name)=="br-x") &&
  			 (isOptionSettable(i) == SANE_TRUE))
        return true;
	}
	return false;
}
/**  */
bool QScanner::isBrySettable()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
   		if((QString(option_desc->name)=="br-y") &&
  			 (isOptionSettable(i) == SANE_TRUE))
        return true;
	}
	return false;
}
/**  */
int QScanner::getBrxOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
   		if((QString(option_desc->name)=="br-x") &&
	  		 (isOptionSettable(i) == SANE_TRUE))
	  		return i;
	}
	return -1;
}
/**  */
int QScanner::getBryOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
   		if((QString(option_desc->name)=="br-y") &&
  			 (isOptionSettable(i)  == SANE_TRUE))
  			return i;
	}
	return -1;
}
/**  */
int QScanner::getTlxOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
 		  if((QString(option_desc->name)=="tl-x") &&
		  	 (isOptionSettable(i)  == SANE_TRUE))
		  	return i;
	}
	return -1;
}
/**  */
int QScanner::getTlyOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
 		  if((QString(option_desc->name)=="tl-y") &&
		  	 (isOptionSettable(i)  == SANE_TRUE))
		  	return i;
	}
	return -1;
}
/**  */
SANE_Status QScanner::setOption(int num,void* v,bool automatic)
{
	SANE_Status sst;
  SANE_Int i = 0;
  void* pval = 0;
  SANE_Int si;
  SANE_Fixed sf;
  SANE_Value_Type stype;
//For options of type SANE_INT and SANE_FIXED with a constraint type
//SANE_CONSTRAINT_RANGE and a size of sizeof(SANE_Word), we perform
//an additional check to ensure that the value really is within the
//allowed range; especially for option values of type SANE_FIXED it's
//possible that lossy transformations result in a value outside the
//specified range. Most backends will check this anyways, but you never
//know...
  stype = getOptionType(num);
  pval = v;
  if(getConstraintType(num) == SANE_CONSTRAINT_RANGE)
  {
    if((stype == SANE_TYPE_INT) &&
       (optionValueSize(num) == sizeof(SANE_Int)))
    {
      if(*(SANE_Int*)v < getRangeMin(num))
      {
        si = getRangeMin(num);
        pval = &si;
      }
      if(*(SANE_Int*)v > getRangeMax(num))
      {
        si = getRangeMax(num);
        pval = &si;
      }
    }
    if((stype == SANE_TYPE_FIXED) &&
       (optionValueSize(num) == sizeof(SANE_Fixed)))
    {
      if(*(SANE_Fixed*)v < getRangeMin(num))
      {
        sf = getRangeMin(num);
        pval = &sf;
      }
      if(*(SANE_Fixed*)v > getRangeMax(num))
      {
        sf = getRangeMax(num);
        pval = &sf;
      }
    }
  }
//  printf ("set value %d", *(SANE_Int*)pval);
  if(automatic)
  	sst = do_sane_control_option(mDeviceHandle,num,SANE_ACTION_SET_AUTO,
														  0L,&i);
  else
  {
	  sst = do_sane_control_option(mDeviceHandle,num,SANE_ACTION_SET_VALUE,
		  												pval,&i);
  }
  if(mEmitSignals)
  {
//       qDebug () << "num=" << num << "i=" << i;
  	if(i & SANE_INFO_INEXACT)
    	emit signalInfoInexact(num);
  	if(i & SANE_INFO_RELOAD_PARAMS)
    	emit signalReloadParams();
    if (i & SANE_INFO_RELOAD_OPTIONS) {
    	emit signalReloadOptions();
        emit signalSetOption(num);
    }
  }
	return sst;
}
/**  */
SANE_Status QScanner::start()
{
  SANE_Status st;

  st = do_sane_start(mDeviceHandle);
  if (st == SANE_STATUS_GOOD)
    {
    st = do_sane_set_io_mode(mDeviceHandle, mNonBlockingIo);
    if (st == SANE_STATUS_UNSUPPORTED)
       st = SANE_STATUS_GOOD;
    }
  if (st == SANE_STATUS_GOOD)
     mScanning = true;
  return st;
}

/**  */
SANE_Status QScanner::read(SANE_Byte* buf,SANE_Int maxlen,SANE_Int* len)
   {
   return do_sane_read(mDeviceHandle,buf,maxlen,len);
   }


/**  */
void QScanner::cancel()
   {
   mCancelled = true;
   if (mDeviceHandle)
      do_sane_cancel(mDeviceHandle);
   mScanning = false;

   emit signalScanDone ();
   }


/**  */
void QScanner::setAppCancel(bool app)
{
  if(mpProgress)
  {
    mpProgress->cancel();
    qApp->processEvents();
  }
  mAppCancel = app;
}
/**returns the number of the resolution option or 0 if
	 the resolution isn't settable. */
int QScanner::resolutionOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
 		  if((QString(option_desc->name)==SANE_NAME_SCAN_RESOLUTION) &&
		  	 (isOptionSettable(i) == true))
		  	return i;
	}
	return 0;
}
  /**Returns the number of the preview option or 0 if
	 there is no preview option. */
int QScanner::previewOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
 		  if((QString(option_desc->name)==SANE_NAME_PREVIEW) &&
		  	 (isOptionSettable(i) == true))
		  	return i;
	}
	return 0;
}
/**  */
void QScanner::setPreviewScanArea(double tlx,double tly,double brx,double bry)
{
  int num;
  SANE_Word  sword;
	num = getTlxOption();
	if(num!=-1)
	{
		switch(getOptionType(num))
		{
			case SANE_TYPE_INT:
			case SANE_TYPE_FIXED:
				if((getConstraintType(num) == SANE_CONSTRAINT_RANGE) ||
				   (getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST))
       	{
          sword = getRangeMax(num);
          sword = int(double(sword)*tlx);
          if(sword > INT_MIN) setOption(num,&sword);
				}
        break;
			default:;
    }
	}
	num = getTlyOption();
	if(num!=-1)
	{
		switch(getOptionType(num))
		{
			case SANE_TYPE_INT:
			case SANE_TYPE_FIXED:
				if((getConstraintType(num) == SANE_CONSTRAINT_RANGE) ||
				   (getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST))
       	{
          sword = getRangeMax(num);
          sword = int(double(sword)*tly);
          if(sword > INT_MIN) setOption(num,&sword);
				}
        break;
			default:;
    }
	}
	num = getBrxOption();
	if(num!=-1)
	{
		switch(getOptionType(num))
		{
			case SANE_TYPE_INT:
			case SANE_TYPE_FIXED:
				if((getConstraintType(num) == SANE_CONSTRAINT_RANGE) ||
				   (getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST))
       	{
          sword = getRangeMax(num);
          sword = int(double(sword)*brx);
          if(sword > INT_MIN) setOption(num,&sword);
				}
        break;
			default:;
    }
	}
	num = getBryOption();
	if(num!=-1)
	{
		switch(getOptionType(num))
		{
      case SANE_TYPE_INT:
			case SANE_TYPE_FIXED:
				if((getConstraintType(num) == SANE_CONSTRAINT_RANGE) ||
				   (getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST))
       	{
          sword = getRangeMax(num);
          sword = int(double(sword)*bry);
          if(sword > INT_MIN)setOption(num,&sword);
				}
        break;
			default:;
    }
	}
}
/**  */
void QScanner::setPreviewResolution(int res)
{
  QVector<SANE_Word> qa;
  int num;
  int quant;
  int minres;
  int maxres;
  int k;
  int min_res = 25;
  SANE_Word  resolution = 0;
  bool limit_res = xmlConfig->boolValue("PREVIEW_DO_LIMIT_SIZE",false);
  int res_limit = -1;
  if(limit_res)
    res_limit = xmlConfig->intValue("PREVIEW_SIZE_LIMIT",50);

//actually there seems to be no difference between
//resolution and x-resolution; see saneopts.h
//We try to set a resolution of at least 25dpi; simply setting the lowest resolution caused
//problems with backends that support very low resolutions like 1 dpi. (->Umax problem)
//The requested resolution passed by parameter res can be bigger than the maximal resolution
//supported by the device; in this case we use the maximal resolution.

  if(limit_res && (res > res_limit))
    res = res_limit;
  if(res < min_res)
    res = min_res;

// qDebug("requ resolution: %i",res);
	num = resolutionOption();
	if(num)//num!=0
	{
		switch(getOptionType(num))
		{
			case SANE_TYPE_FIXED:
				if(getConstraintType(num) == SANE_CONSTRAINT_RANGE)
       	{
          minres = getRangeMin(num);
          maxres = getRangeMax(num);
          if(int(SANE_UNFIX(minres)) > res)
          {
            //requested resolution is smaller than minimal value
            resolution = minres;
          }
          else if(int(SANE_UNFIX(maxres)) < res)
          {
            //requested resolution is bigger than maximal value
            resolution = maxres;
          }
          else
          {
            //requested resolution is between minimal and maximal value;
            //try to find a resolution that's close to the requested value
            quant = getRangeQuant(num);
            if(quant > 0) //has quant
            {
              resolution = 0;
              k = 0;
              while(int(SANE_UNFIX(resolution)) <= res)
              {
                resolution = k*quant + minres;
                k += 1;
              }
            }
            else //no quant
            {
               resolution = SANE_FIX(double(res));
            }
          }
// qDebug("resolution: %i",int(SANE_UNFIX(resolution)));
          setOption(num,&resolution);
				}
				else if(getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST)
       	{
          int diff = INT_MAX;
          int temp_res;
          int max_res = 0;
          qa = saneWordList(num);
          resolution = qa[0];
          for(int i=0;i<qa.size();i++)
          {
            temp_res = qa[i];
            if(temp_res > max_res)
              max_res = temp_res;
            if((int(SANE_UNFIX(temp_res)) - res < diff) &&
               (int(SANE_UNFIX(temp_res)) - res >= 0))
            {
              diff = int(SANE_UNFIX(temp_res)) - res;
              resolution = temp_res;
            }
            if(SANE_FIX(res) > max_res)
              resolution = max_res;
          }
// qDebug("resolution: %i",int(SANE_UNFIX(resolution)));
          setOption(num,&resolution);
				}
				break;
			case SANE_TYPE_INT:
				if(getConstraintType(num) == SANE_CONSTRAINT_RANGE)
       	{
          minres = getRangeMin(num);
          maxres = getRangeMax(num);
          if(minres > res)
          {
            //requested resolution is smaller than minimal value
            resolution = minres;
          }
          else if(maxres < res)
          {
            //requested resolution is bigger than maximal value
            resolution = maxres;
          }
          else
          {
            //requested resolution is between minimal and maximal value;
            //try to find a resolution that's close to the requested value
            quant = getRangeQuant(num);
            if(quant > 0) //has quant
            {
              resolution = 0;
              k = 0;
              while(resolution <= res)
              {
                resolution = k*quant + minres;
                k += 1;
              }
            }
            else //no quant
            {
               resolution = res;
            }
          }
qDebug("resolution: %i",resolution);
          setOption(num,&resolution);
				}
				else if(getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST)
       	{
          int diff = INT_MAX;
          int temp_res;
          int max_res = 0;
          qa = saneWordList(num);
          resolution = qa[0];
          for(int i=0;i<qa.size();i++)
          {
            temp_res = qa[i];
            if(temp_res > max_res)
              max_res = temp_res;
            if((temp_res - res < diff) && (temp_res - res >= 0))
            {
              diff = temp_res - res;
              resolution = temp_res;
            }
            if(res > max_res)
              resolution = max_res;
          }
          setOption(num,&resolution);
qDebug("resolution: %i",resolution);
				}
				break;
			default:;
    }
	}
  //check whether the y resolution can be set separately
  qa.resize(0);
  resolution = 0;
	num = yResolutionOption();
	if(num)//num>0
	{
		switch(getOptionType(num))
		{
			case SANE_TYPE_FIXED:
				if(getConstraintType(num) == SANE_CONSTRAINT_RANGE)
       	{
          minres = getRangeMin(num);
          maxres = getRangeMax(num);
          if(int(SANE_UNFIX(minres)) > res)
          {
            //requested resolution is smaller than minimal value
            resolution = minres;
          }
          else if(int(SANE_UNFIX(maxres)) < res)
          {
            //requested resolution is bigger than maximal value
            resolution = maxres;
          }
          else
          {
            //requested resolution is between minimal and maximal value;
            //try to find a resolution that's close to the requested value
            quant = getRangeQuant(num);
            if(quant > 0) //has quant
            {
              resolution = 0;
              k = 0;
              while(int(SANE_UNFIX(resolution)) <= res)
              {
                resolution = k*quant + minres;
                k += 1;
              }
            }
            else //no quant
            {
               resolution = SANE_FIX(double(res));
            }
          }
qDebug("resolution: %i",resolution);
          setOption(num,&resolution);
				}
				else if(getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST)
       	{
          int diff = INT_MAX;
          int temp_res;
          int max_res = 0;
          qa = saneWordList(num);
          resolution = qa[0];
          for(int i=0;i<qa.size();i++)
          {
            temp_res = qa[i];
            if(temp_res > max_res)
              max_res = temp_res;
            if((int(SANE_UNFIX(temp_res)) - res < diff) &&
               (int(SANE_UNFIX(temp_res)) - res >= 0) &&
               (int(SANE_UNFIX(temp_res)) <= res))
            {
              diff = int(SANE_UNFIX(temp_res)) - res;
              resolution = temp_res;
            }
            if(SANE_FIX(res) > max_res)
              resolution = max_res;
          }
qDebug("resolution: %i",resolution);
          setOption(num,&resolution);
				}
				break;
			case SANE_TYPE_INT:
				if(getConstraintType(num) == SANE_CONSTRAINT_RANGE)
       	{
          minres = getRangeMin(num);
          maxres = getRangeMax(num);
          if(minres > res)
          {
            //requested resolution is smaller than minimal value
            resolution = minres;
          }
          else if(maxres < res)
          {
            //requested resolution is bigger than maximal value
            resolution = maxres;
          }
          else
          {
            //requested resolution is between minimal and maximal value;
            //try to find a resolution that's close to the requested value
            quant = getRangeQuant(num);
            if(quant > 0) //has quant
            {
              resolution = 0;
              k = 0;
              while(resolution <= res)
              {
                resolution = k*quant + minres;
                k += 1;
              }
            }
            else //no quant
            {
               resolution = res;
            }
          }
qDebug("resolution: %i",resolution);
          setOption(num,&resolution);
				}
				else if(getConstraintType(num) == SANE_CONSTRAINT_WORD_LIST)
       	{
          int diff = INT_MAX;
          int temp_res;
          int max_res = 0;
          qa = saneWordList(num);
          resolution = qa[0];
          for(int i=0;i<qa.size();i++)
          {
            temp_res = qa[i];
            if(temp_res > max_res)
              max_res = temp_res;
            if((temp_res - res < diff) && (temp_res - res >= 0) && (temp_res <= res))
            {
              diff = temp_res - res;
              resolution = temp_res;
            }
            if(res > max_res)
              resolution = max_res;
          }
qDebug("resolution: %i",resolution);
          setOption(num,&resolution);
				}
				break;
			default:;
    }
	}
}
/**  */
QString QScanner::getOptionDescription(int num)
{
  QString qs;
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  qs = tr(option_desc->desc);//qApp->translate("QScanner",option_desc->desc);
 	return qs;
}
/**  */
QString QScanner::createPNMHeader(SANE_Frame format,int lines,int ppl,
                                  int depth,int resx,int resy)
{
	QString qs;
  QString qs2;
  int dots_m_x,dots_m_y;
//Get the resolution
//This is an extension to the PNM format; other readers will simply skip
//this information.
  dots_m_x = resx;
  dots_m_y = resy;

  if(dots_m_x != 0)
  {
    dots_m_x = int(double(dots_m_x)*100.0/2.54);
    if(dots_m_y != 0)
      dots_m_y = int(double(dots_m_y)*100.0/2.54);
    else
      dots_m_y = dots_m_x;
  }
  if((dots_m_x == 0) || (dots_m_y == 0))
  {
    dots_m_x = 0;
    dots_m_y = 0;
  }
//write the header
  switch (format)
  {
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    case SANE_FRAME_RGB:
      qs = "P6";
      if(dots_m_x != 0)
      {
        qs2.asprintf("#DOTS_PER_METER_X %i",dots_m_x);
        qs2.asprintf("#DOTS_PER_METER_X %i",dots_m_x);
        qs += qs2;
        qs2.asprintf("#DOTS_PER_METER_Y %i",dots_m_y);
        qs += qs2;
      }
      if(lines > 0)
        qs2.asprintf("%d %d\n%d",ppl,lines,(depth <= 8) ? 255 : 65535);
      else
        qs2.asprintf("%d %%1\n%d",ppl,(depth <= 8) ? 255 : 65535);
      qs += qs2;
      break;
    default:
      if (depth == 1)
      {
        qs = "P4";
        if(dots_m_x != 0)
        {
          qs2.asprintf("#DOTS_PER_METER_X %i",dots_m_x);
          qs += qs2;
          qs2.asprintf("#DOTS_PER_METER_Y %i",dots_m_y);
          qs += qs2;
        }
        if(lines > 0)
          qs2.asprintf("%d %d",ppl,lines);
        else
          qs2.asprintf("%d %%1",ppl);
      }
      else
      {
        qs = "P5";
        if(dots_m_x != 0)
        {
          qs2.asprintf("#DOTS_PER_METER_X %i",dots_m_x);
          qs += qs2;
          qs2.asprintf("#DOTS_PER_METER_Y %i",dots_m_y);
          qs += qs2;
        }
        if(lines > 0)
          qs2.asprintf("%d %d\n%d",ppl,lines,(depth <= 8) ? 255 : 65535);
        else
          qs2.asprintf("%d %%1\n%d",ppl,(depth <= 8) ? 255 : 65535);
      }
      qs += qs2;
    break;
  }
  return qs;
}

#if 0
/**Somehow I have to clean up this bloated thing  */
SANE_Status QScanner::scanImage(QString file,QWidget* parent,PreviewWidget* preview_widget)
{
  mAppCancel = false;
  mCancelled = false;
  mTempFilePath = file;
  //polling stuff
  pollfd pfd;
  bool b_poll;
  int fd;
  int res_x;
  int res_y;
  unsigned char b[3] = {0,0,0};
  bool color_lineart = false;
  bool resize_ok;
  bool threepass_flag;
  bool update_preview = false;
  int offset;
  int i;
  int bsize;
  int steps;
  int a;
  int prog_cnt;
  int buffer_offset;
  int preview_line;
  int line_cnt;
  int buffer_cnt;
  unsigned int expected_size;

  SANE_Frame format;
  SANE_Int lines;
  SANE_Int depth;
  SANE_Int ppl;
  SANE_Int bpl;

  SANE_Int len;
  SANE_Status status;
  SANE_Status st;
  SANE_Parameters parameters;
//these arrays are used to store the image data
  QVector <SANE_Byte> qarray_rgbgray(0);
  QVector <SANE_Byte> qarray_red(0);
  QVector <SANE_Byte> qarray_green(0);
  QVector <SANE_Byte> qarray_blue(0);
  //a pointer to a QVector of type SANE_Byte
  QVector <SANE_Byte> *p_array;
  QByteArray preview_array;
  QByteArray preview_temp;
  QFile f;
  QString s;
  QString dlginfo;
  QString preview_header;
  int word_size;
  bool big_endian;
  int hang_over = -1;
  qSysInfo (&word_size,&big_endian);

//get resolution (needed for the PNM header) before
//sane_start is called; that's neccessary for some devices
  res_x = xResolutionDpi();
  res_y = yResolutionDpi();
  if(mpProgress)
    delete mpProgress;

  status = getParameters(&parameters);
  if(status == SANE_STATUS_GOOD)
  {
    if(preview_widget &&
       ((parameters.format == SANE_FRAME_RGB) || (parameters.format == SANE_FRAME_GRAY)) &&
       (parameters.lines > 0) && !((parameters.depth == 1) && (parameters.format == SANE_FRAME_RGB)))
      update_preview = true;
    if(!xmlConfig->boolValue("PREVIEW_CONTINOUS_UPDATE",true))
      update_preview = false;
  }
  else
    return status;

  if(!update_preview)
  {
    mpProgress = new Q3ProgressDialog(parent,0,false,0);
    if(!mpProgress)
      return SANE_STATUS_INVAL;
  }
  if(!update_preview)
    mpProgress->setWindowTitle(tr("Scanning..."));

  preview_line = 0;
  line_cnt=0;
  buffer_cnt = 0;
  buffer_offset = 0;
  offset = 0;
  i = 0;
  bsize = 0;
  resize_ok = true;
  threepass_flag = false;
  do //until the last frame is aquired
  {
    //if start() fails, return status
    status = start();
    if(status != SANE_STATUS_GOOD)
    {
      cancel();
      if(!update_preview)
      {
        delete mpProgress;
        mpProgress = 0L;
      }
      return status;
    }
    st = sane_set_io_mode(mDeviceHandle,mNonBlockingIo);
    st = sane_get_select_fd(mDeviceHandle,&fd);
    switch(st)
    {
      case SANE_STATUS_GOOD:
      {
        b_poll = true;
        pfd.fd = fd;
        pfd.events = POLLIN;
        break;
      }
      default:
        b_poll = false;
    }

    //if we couldn't get the parameters, return status
    status = getParameters(&parameters);
    if(status != SANE_STATUS_GOOD)
    {
      cancel();
      if(!update_preview)
      {
        delete mpProgress;
        mpProgress = 0L;
      }
      return status;
    }
    //needed for the pnm header
    depth  = parameters.depth;
    ppl    = parameters.pixels_per_line;
    format = parameters.format;
    lines  = parameters.lines;
    bpl    = parameters.bytes_per_line;

    expected_size = lines*bpl;

    switch(parameters.format)
    {
       case SANE_FRAME_RGB:
         dlginfo = tr("Scanning RGB frame ...");
         p_array = &qarray_rgbgray;
         if(depth == 1)
	       {
           color_lineart = true;
	       }
         break;
       case SANE_FRAME_GRAY:
         dlginfo = tr("Scanning GRAY frame ...");
         p_array = &qarray_rgbgray;
         break;
       case SANE_FRAME_RED:
         dlginfo = tr("Scanning RED frame ...");
         p_array = &qarray_red;
          break;
       case SANE_FRAME_GREEN:
         dlginfo = tr("Scanning GREEN frame ...");
         p_array = &qarray_green;
         break;
       case SANE_FRAME_BLUE:
         dlginfo = tr("Scanning BLUE frame ...");
         p_array = &qarray_blue;
         break;
       default://shouldn't happen
         p_array = 0L;
     }
     //p_array == 0L means that there was an error
     if(!p_array)
     {
       cancel();
       if(!update_preview)
       {
         delete mpProgress;
         mpProgress = 0L;
       }
       return SANE_STATUS_INVAL;
     }
     //we know the number of lines
     if(lines >= 0)
     {
       //if SANE_Frame is of Type SANE_FRAME_RGB or SANE_FRAME_GRAY
       //and last_frame == true,
       //then there's no need to store the whole image data in
       //memory; we write the data to a file instead
       //In preview mode, we save the data to a QByteArray, but only if the
       //line number is known and if format is GRAY or RGB
       if((parameters.last_frame == SANE_TRUE) &&
           ((parameters.format == SANE_FRAME_RGB) ||
            (parameters.format == SANE_FRAME_GRAY)))
       {
         s = createPNMHeader(format,lines,ppl,depth,res_x,res_y);
         bool is_open = false;
         QBuffer buf;
         if(!update_preview)
         {
           f.setName(file);
           is_open = f.open(QIODevice::WriteOnly);
         }
         else
         {
           buf.setBuffer(&preview_temp);
           preview_header = createPNMHeader(format,-1,ppl,depth,res_x,res_y);
           is_open =buf.open(QIODevice::WriteOnly);
           buffer_offset = 0;
         }
         if(is_open)
         {    // file opened successfully
           Q3TextStream t;
           QDataStream d;
           if(!update_preview)
           {
             t.setDevice( &f );        // use a text stream
             t<<s; //write header to file
             d.setDevice(&f);
           }
           else
             d.setDevice(&buf);
           if(update_preview)
           {
//s              preview_widget->initPixmap(parameters.pixels_per_line,
//s                                         parameters.lines);
           }
           if(color_lineart)
             bsize=lines*bpl + 1;
           else
             bsize=32*1024;
           p_array->resize(bsize);
           steps = bpl * lines;
           prog_cnt = 0;
           a = 0;
           if(!update_preview)
           {
             mpProgress->setLabelText(dlginfo);
             mpProgress->setMinimumDuration(0);
             mpProgress->setTotalSteps(steps);
             mpProgress->setProgress(0);
           }
           if(b_poll)
           {
             do
             {
               poll(&pfd,1,10);
               qApp->processEvents();
             }while((!pfd.revents&POLLIN)&&(!pfd.revents&POLLNVAL));
           }
           else
             qApp->processEvents(0);
           if(hang_over > - 1)
           {
             offset = 1;
             bsize = p_array->size() - 1;
           }
           else
           {
             offset = 0;
             bsize = p_array->size();
           }
           while((status=read((SANE_Byte*)&(p_array->at(offset)),bsize,&len))==SANE_STATUS_GOOD)
           {
             if(hang_over > -1)
             {
               (*p_array) [0] = (SANE_Byte)hang_over;
               ++len;
             }
             if(len>0)
             {
               if(!color_lineart)
               {
                 if((depth == 16) && !big_endian)
                 {
                   //must swap
                   if(len % 2 > 0)
                   {
                     hang_over = (int)p_array->at(len-1);
                     --len;
                   }
                   else
                     hang_over = -1;
                   for(int c=0;c<len-1;c+=2)
                   {
                     SANE_Byte hi;
                     hi = p_array->at(c);
                     (*p_array) [c] = p_array->at(c+1);
                     (*p_array) [c+1] = hi;
                   }
                 }
                 if(update_preview)
                 {
                    d.writeRawBytes((const char*)&p_array->at(0),len);
                    buffer_cnt += len;
                    if(buffer_cnt > 30 * parameters.bytes_per_line)
                    {
                      line_cnt = buffer_cnt/parameters.bytes_per_line;
                      int byte_size = parameters.bytes_per_line * line_cnt;
                      preview_array.resize(0);
                      QBuffer b(&preview_array);
                      b.open(QIODevice::WriteOnly);
                      QTextStream t3( &b );        // use a text stream
                      t3 << preview_header.arg(line_cnt);
                      QDataStream d3(&b);

                      const char *data = preview_temp.constData () + buffer_offset;
                      d3.writeRawBytes(data,byte_size);
                      b.close();
                      buffer_cnt -= byte_size;
//s                      preview_widget->setData(preview_array);
                      preview_line += line_cnt;
                      buffer_offset += byte_size;
                    }
                 }
                 else
                   d.writeRawBytes((const char*)&p_array->at(0),len);
               }
               else
                 offset += len;
               a+=len;
               int pc = int(100.0*double(a)/double(steps));
               if(pc > prog_cnt)
               {
                 prog_cnt = pc;
                 if(!update_preview)
                   mpProgress->setProgress(a);
               }
             }
             if(b_poll && (status!=SANE_STATUS_EOF) && fd)
             {
               do
               {
                 poll(&pfd,1,10);
                 if(cancelled()) break;
                 if(update_preview)
                 {
//s                   if(preview_widget->wasCancelled())
//s                     break;
                 }
                 else
                 {
                   if(mpProgress->wasCancelled())
                     break;
                 }
                 qApp->processEvents();
               }while((!pfd.revents&POLLIN)&&(!pfd.revents&POLLNVAL));
             }
             else
               qApp->processEvents();
             if(update_preview)
             {
#if 0 //s
               if(preview_widget->wasCancelled() || cancelled())
               {
                 //cancel scanning
                 cancel();
                 buf.close();
                 //we save the data to the file; if necessary, we fill
                 //missing data with 0
                 if(preview_temp.size() < expected_size)
                 {
                   unsigned int index = preview_temp.size();
                   preview_temp.resize(expected_size);
                   for(unsigned int i=index;i<preview_temp.size();i++)
                     if(depth == 1)
                       preview_temp[i] = 255;
                     else
                       preview_temp[i] = 0;
                 }
                 f.setName(file);
                 if(f.open(QIODevice::WriteOnly))
                 {
                   Q3TextStream t(&f);        // use a text stream
                   t << s;
                   QDataStream d(&f);
                   d.writeRawBytes((const char*)&preview_temp.at(0),preview_temp.size());
                   f.close();
                   return SANE_STATUS_GOOD;
                 }
                 //return
                 return SANE_STATUS_CANCELLED;
               }
#endif //s
             }
             else
             {
               if(mpProgress->wasCancelled() || cancelled())
               {
                 //cancel scanning
                 cancel();
                 f.close();
                 delete mpProgress;
                 mpProgress = 0L;
                 return SANE_STATUS_CANCELLED;
               }
             }
           }
           if(!update_preview)
             mpProgress->reset();
           if(((status != SANE_STATUS_EOF) && (status != SANE_STATUS_GOOD)) ||
              (a <= 0))
           {
             if(!update_preview)
             {
               f.close();
               delete mpProgress;
               mpProgress = 0L;
             }
             else
               buf.close();
             cancel();
             return status;
           }
           // for 1bit RGB we had to buffer the data
           if(color_lineart)
           {
             p_array->resize(offset);
             //resize arrays to hold one line
             qarray_red.resize(bpl/3);
             qarray_green.resize(bpl/3);
             qarray_blue.resize(bpl/3);
             for(int y=0;y<lines;y++)
             {
               for(int c=0;c<bpl/3;c++)
               {
                 qarray_red[c] = qarray_rgbgray[c*3 + y*bpl];
                 qarray_green[c] = qarray_rgbgray[c*3 + y*bpl + 1];
                 qarray_blue[c] = qarray_rgbgray[c*3 + y*bpl + 2];
               }
               for(int x=0;x<ppl;x++)
               {
                	if(((*(&qarray_red[0] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
                   b[0] = 0;
                 else
                   b[0] = 255;
                	if(((*(&qarray_green[0] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
                   b[1] = 0;
                 else
                   b[1] = 255;
                	if(((*(&qarray_blue[0] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
                   b[2] = 0;
                 else
                   b[2] = 255;
                 d.writeRawBytes((const char*)b,3);
               }
             }
           }
           if(update_preview)
           {
             f.setName(file);
             if(f.open(QIODevice::WriteOnly))
             {
               Q3TextStream t(&f);        // use a text stream
               t << s;
               QDataStream d(&f);
               const char *data = preview_temp.constData ();
               d.writeRawBytes(data,preview_temp.size());
               cancel();
               f.close();
               return SANE_STATUS_GOOD;
             }
             return SANE_STATUS_INVAL;
           }
           f.close();
           cancel();
           delete mpProgress;
           mpProgress = 0L;
           return SANE_STATUS_GOOD;
         }
         cancel();
         delete mpProgress;
         mpProgress = 0L;
         return SANE_STATUS_INVAL;
       }
       else
       {
         offset = 0;
	       bsize=int(lines * bpl);
         p_array->resize(bsize+1);// + parameters.bytes_per_line);
         steps = bpl * lines;
         prog_cnt = 0;
         a = 0;
         mpProgress->setLabelText(dlginfo);
         mpProgress->setTotalSteps(100);
         mpProgress->setMinimumDuration(0);
         mpProgress->setProgress(0);
         if(b_poll)
         {
           do
           {
             poll(&pfd,1,10);
             if(cancelled()) break;
             if(mpProgress->wasCancelled()) break;
             qApp->processEvents();
           }while((!pfd.revents&POLLIN)&&(!pfd.revents&POLLNVAL));
         }
         else
           qApp->processEvents();
         while((status=read((SANE_Byte*)&(p_array->at(0+a)),bsize,&len))==SANE_STATUS_GOOD)
	       {
           if(len>0)
           {
  		       offset+=len;
             a+=len;
             int pc = int(100.0*double(a)/double(steps));
             if(pc > prog_cnt)
             {
               prog_cnt = pc;
               mpProgress->setProgress(pc);
               qApp->processEvents();
             }
           }
           if(b_poll && (status!=SANE_STATUS_EOF) && fd)
           {
             do
             {
               poll(&pfd,1,10);
               if(cancelled()) mpProgress->cancel();
               if(mpProgress->wasCancelled()) break;
               qApp->processEvents();
             }while((!pfd.revents&POLLIN)&&(!pfd.revents&POLLNVAL));
           }
           else
             qApp->processEvents();
           if(mpProgress->wasCancelled() || cancelled())
           {
             //cancel scanning
             cancel();
             delete mpProgress;
             mpProgress = 0L;
             //return
             return SANE_STATUS_CANCELLED;
           }
	       }
         if((status != SANE_STATUS_EOF) && (status != SANE_STATUS_GOOD))
         {
           cancel();
           delete mpProgress;
           mpProgress = 0L;
           return status;
         }
         mpProgress->reset();
         p_array->resize(offset);
       }
     }
     else
     { //we don't know the number of lines
       prog_cnt = 0;
       QString text;
       text = tr("Line number unknown - ")+dlginfo+""+tr("Lines scanned: %1");
       bsize=32*1024;
       p_array->resize(bsize);
       mpProgress->setMinimumDuration(0);
       mpProgress->setLabelText(text.arg(0));
       mpProgress->setTotalSteps(100);
       mpProgress->setProgress(0);
       if(b_poll)
       {
         do
         {
           poll(&pfd,1,10);
           if(cancelled()) mpProgress->cancel();
           if(mpProgress->wasCancelled()) break;
           qApp->processEvents();
         }while((!pfd.revents&POLLIN)&&(!pfd.revents&POLLNVAL));
       }
       else
         qApp->processEvents();
       offset = 0;
       len = 0;
	     while((status=read((SANE_Byte*)&(p_array->at(0+offset)),bsize,&len))
              ==SANE_STATUS_GOOD)
	     {
          if(len>0)
          {
  		      offset+=len;
            //if we don't have enough memory, we have to resize the array
            if(offset + bsize >= (int)p_array->size()-1)
            {
              p_array->resize(p_array->size()+ (2 * bsize));
            }
            if(offset/bpl > prog_cnt)
            {
              prog_cnt = offset/bpl;
              mpProgress->setLabelText(text.arg(prog_cnt));
              qApp->processEvents();
            }
            if(b_poll && (status!=SANE_STATUS_EOF) && fd)
            {
              do
              {
                poll(&pfd,1,10);
                if(cancelled()) mpProgress->cancel();
                if(mpProgress->wasCancelled()) break;
                qApp->processEvents();
              }while((!pfd.revents&POLLIN)&&(!pfd.revents&POLLNVAL));
            }
          }
          else
            qApp->processEvents();
          if(mpProgress->wasCancelled() || cancelled())
          {
            //cancel scanning
            cancel();
            //return
            delete mpProgress;
            mpProgress = 0L;
            return SANE_STATUS_CANCELLED;
          }
	     }
       if((status != SANE_STATUS_EOF) && (status != SANE_STATUS_GOOD))
       {
         cancel();
         delete mpProgress;
         mpProgress = 0L;
         return status;
       }
       mpProgress->reset();
       //Array size can be bigger than neccessary; resize to number of bytes
       //actually scanned.
       p_array->resize(offset);
    }
  }while(parameters.last_frame != SANE_TRUE);
	cancel();
  int size;
  size = 0;
  //check whether we have more than one frame
  //if true, concatenate the frames to one array
  if((qarray_blue.size()>0)  || (qarray_red.size()>0)   ||
     (qarray_green.size()>0))
  {
    threepass_flag = true;
    if(size < (int)qarray_red.size())
      size = qarray_red.size();
    if(size < (int)qarray_green.size())
      size = qarray_green.size();
    if(size < (int)qarray_blue.size())
      size = qarray_blue.size();
    //if the number of lines hasn't been known a priori
    //we calculate it now
    if(lines < 0)
    {
      lines = size / bpl;
    }
  }
  else
  {
    size = qarray_rgbgray.size();
    //if the number of lines hasn't been known a priori
    //we calculate it now
    if(lines < 0)
    {
      lines = size / bpl;
    }
  }
  s=createPNMHeader(format,lines,ppl,depth,res_x,res_y);
  f.setName(file);
  if(!f.open(QIODevice::WriteOnly))
  {
    // file not opened
    delete mpProgress;
    mpProgress = 0L;
    return SANE_STATUS_INVAL;
  }
 	Q3TextStream t( &f );        // use a text stream
  t<<s;//write header
  QDataStream d(&f);
  //check whether we have more than one frame
  if(threepass_flag == true)
  {
    //the three arrays should have the same size
    //if it was a three pass scan,
    //but it's also possible that one or two arrays
    //aren't valid at all
    //the missing data is then filled with zeros
    //for three pass scanning, depth should normally be 8 or 16?
    //However, the SANE standard also allows 1bit/channel RGB modes...
    if(depth == 1)
    {
      //color lineart
      for(int y=0;y<lines;y++)
      {
        for(int x=0;x<ppl;x++)
        {
          if(((*(&qarray_red[y*bpl] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
            b[0] = 0;
          else
            b[0] = 255;
          if(((*(&qarray_green[y*bpl] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
            b[1] = 0;
          else
            b[1] = 255;
          if(((*(&qarray_blue[y*bpl] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
            b[2] = 0;
          else
            b[2] = 255;
          d.writeRawBytes((const char*)b,3);
        }
      }
    }
    else if(depth == 8)
    {
      for(i=0;i<size;i++)
      {
        if(i < (int)qarray_red.size())
        {
          d.writeRawBytes((const char*)&qarray_red[i],1);
        }
        else
        {
          d.writeRawBytes((const char*)b,1);
        }
        if(i < (int)qarray_green.size())
        {
          d.writeRawBytes((const char*)&qarray_green[i],1);
        }
        else
        {
          d.writeRawBytes((const char*)b,1);
        }
        if(i < (int)qarray_blue.size())
        {
          d.writeRawBytes((const char*)&qarray_blue[i],1);
        }
        else
        {
          d.writeRawBytes((const char*)b,1);
        }
      }
    }
    if(depth == 16)
    {//2 bytes needed for every pixel
      if(!big_endian)
      {
        SANE_Byte hi;
        //must swap
        for(i=0;i<int(qarray_red.size())-1;i+=2)
        {
          hi = qarray_red.at(i);
          qarray_red [i] = qarray_red.at(i+1);
          qarray_red [i+1] = hi;
        }
        for(i=0;i<int(qarray_green.size())-1;i+=2)
        {
          hi = qarray_green.at(i);
          qarray_green[i] = qarray_green.at(i+1);
          qarray_green[i+1] = hi;
        }
        for(i=0;i<int(qarray_blue.size())-1;i+=2)
        {
          hi = qarray_blue.at(i);
          qarray_blue[i] = qarray_blue.at(i+1);
          qarray_blue[i+1] = hi;
        }
      }
      for(i=0;i<size-1;i+=2)
      {
        if(i < (int)qarray_red.size())
        {
          d.writeRawBytes((const char*)&qarray_red[i],2);
        }
        else
        {
          d.writeRawBytes((const char*)b,2);
        }
        if(i < (int)qarray_green.size())
        {
          d.writeRawBytes((const char*)&qarray_green[i],2);
        }
        else
        {
          d.writeRawBytes((const char*)b,2);
        }
        if(i < (int)qarray_blue.size())
        {
          d.writeRawBytes((const char*)&qarray_blue[i],2);
        }
        else
        {
          d.writeRawBytes((const char*)b,2);
        }
      }
    }
  }
  else
  {
    if(color_lineart)
    {
      //resize arrays to hold one line
      qarray_red.resize(bpl/3);
      qarray_green.resize(bpl/3);
      qarray_blue.resize(bpl/3);
      for(int y=0;y<lines;y++)
      {
        for(int c=0;c<bpl/3;c++)
        {
          qarray_red[c] = qarray_rgbgray[c*3 + y*bpl];
          qarray_green[c] = qarray_rgbgray[c*3 + y*bpl + 1];
          qarray_blue[c] = qarray_rgbgray[c*3 + y*bpl + 2];
        }
        for(int x=0;x<ppl;x++)
        {
         	if(((*(&qarray_red[0] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
            b[0] = 0;
          else
            b[0] = 255;
         	if(((*(&qarray_green[0] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
            b[1] = 0;
          else
            b[1] = 255;
         	if(((*(&qarray_blue[0] + (x >> 3)) >> (7 - (x & 7))) & 1) == 0)
            b[2] = 0;
          else
            b[2] = 255;
          d.writeRawBytes((const char*)b,3);
        }
      }
    }
    else
    {
      if((depth == 16) && !big_endian)
      {
        SANE_Byte hi;
        //must swap
        for(i=0;i<int(qarray_rgbgray.size())-1;i+=2)
        {
          hi = qarray_rgbgray.at(i);
          qarray_rgbgray[i] = qarray_rgbgray.at(i+1);
          qarray_rgbgray[i+1] = hi;
        }
      }
      d.writeRawBytes((const char*)&qarray_rgbgray[0],qarray_rgbgray.size() );
    }
  }
  f.close();
  delete mpProgress;
  mpProgress = 0L;
  return SANE_STATUS_GOOD;
}
#endif


/**  */
void QScanner::setIOMode(bool b)
{
  mNonBlockingIo = (b == true) ? SANE_TRUE : SANE_FALSE;
}
/**  */
int QScanner::nonGroupOptionCount()
{
	int cnt;
	int num;
  cnt = 0;
	num = 0;
  const SANE_Option_Descriptor *option_desc;
  int mOptionNumber = optionCount();
	for(num = 1;num<mOptionNumber;num++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
 		if(option_desc->type == SANE_TYPE_GROUP) return cnt;
    cnt+=1;
	}
	return cnt;
}

#if 0
/**  */
SANE_Status QScanner::scanPreview(QString path,QWidget* parent,
                                  double tlx,double tly,double brx,double bry,int res)
{
  QString qs;
  SANE_Word sw;
  SANE_Word resolution;
  SANE_Word yresolution;
  SANE_Word tlx_val;
  SANE_Word tly_val;
  SANE_Word brx_val;
  SANE_Word bry_val;
  SANE_Status status;

  int num;
  void* v;
	int i;
  num = 0;
  v = 0L;
	i = 0;
  resolution = -1;
  yresolution = -1;
  tlx_val = -1;
  tly_val = -1;
  brx_val = -1;
  bry_val = -1;
  sw = SANE_FALSE;
//try to store scan area and resolution
// other option are not affected
  enableReloadSignal(false);
//if there's a preview option, set it
  if(previewOption())
  {
    sw = SANE_TRUE;
    setOption(previewOption(),&sw);
  }
  if(resolutionOption())
  { //backend supports resolution
    if(isOptionActive(resolutionOption()))
      resolution = saneWordValue(resolutionOption());
  }
  if(yResolutionOption())
  { //backend supports resolution
    if(isOptionActive(yResolutionOption()))
      yresolution = saneWordValue(yResolutionOption());
  }
  if(getTlxOption() != -1)
  { //backend supports tlx
    tlx_val = saneWordValue(getTlxOption());
  }
  if(getTlyOption() != -1)
  { //backend supports tly
    tly_val = saneWordValue(getTlyOption());
  }
  if(getBrxOption() != -1)
  { //backend supports brx
    brx_val = saneWordValue(getBrxOption());
  }
  if(getBryOption() != -1)
  { //backend supports bry
    bry_val = saneWordValue(getBryOption());
  }
  setPreviewResolution(res);
	setPreviewScanArea(tlx,tly,brx,bry);
//scan preview
  status = scanImage(path,parent,(PreviewWidget*)parent);
//restore previous settings
  //disable preview option if necessary
  if(sw == SANE_TRUE)
  {
    sw = SANE_FALSE;
    setOption(previewOption(),&sw);
  }
  if(resolution != -1)
  {
    setOption(resolutionOption(),&resolution);
  }
  if(yresolution != -1)
  {
    setOption(yResolutionOption(),&yresolution);
  }
  if(tlx_val != -1)
  {
    setOption(getTlxOption(),&tlx_val);
  }
  if(tly_val != -1)
  {
    setOption(getTlyOption(),&tly_val);
  }
  if(brx_val != -1)
  {
    setOption(getBrxOption(),&brx_val);
  }
  if(bry_val != -1)
  {
    setOption(getBryOption(),&bry_val);
  }
  enableReloadSignal(true);
  return status;
}
#endif


/**  */
SANE_Status QScanner::getParameters(SANE_Parameters* par)
{
   return do_sane_get_parameters(mDeviceHandle, par);
}
/**  */
void QScanner::enableReloadSignal(bool status)
{
  mEmitSignals = status;
}
/** Creates an QImage from the file specified by
 * path. If path is empty, the QImage is created
 * from the last temporary file created during the
 * last scan. Returns NULL if the Image creation fails.
 * The caller is responsible for the deletion of the image.*/
QImage* QScanner::createImage(QString path)
{
  QImage* image;
  image = 0L;
  //first, try to open the image under path
  if((path.isNull() != true) && (path.isEmpty() != true))
  {
    image = new QImage(path);
  } //no path given, try to open the image last scanned
  else if ((mTempFilePath.isNull() != true) &&
           (mTempFilePath.isEmpty() != true))
  {
    image = new QImage(path);
  }
  if(image)
  {
    if(image->isNull() != true)
      return image; //we have a valid image
    else
      delete image;//the image is a null image
  }
  return 0L;
}
/** Creates an QPixmap from the file specified by
  * path. If path is empty, the QPixmap is created
  * from the last temporary file created during the
  * last scan. Returns NULL if the pixmap creation fails.
  * The caller is responsible for the deletion of the pixmap.*/
QPixmap* QScanner::createPixmap(QString path)
{
  QPixmap* pixmap;
  pixmap = 0L;
  //first, try to open the image under path
  if((path.isNull() != true) && (path.isEmpty() != true))
  {
    pixmap = new QPixmap(path);
  } //no path given, try to open the image last scanned
  else if ((mTempFilePath.isNull() != true) &&
           (mTempFilePath.isEmpty() != true))
  {
    pixmap = new QPixmap(path);
  }
  if(pixmap)
  {
    if(pixmap->isNull() != true)
      return pixmap; //we have a valid pixmap
    else
      delete pixmap;//the pixmap is a null pixmap
  }
  return 0L;
}
/** Returns an info string which contains information
about pixel size, colormode and byte size.
This string can be used to inform the user about the
current settings. */
QString QScanner::imageInfo()
{
	SANE_Status status;
  SANE_Parameters parameters;
  QString info;
  double bytesize;
  QString qs;
  QString qs2;
  qs = tr("byte");
  status = getParameters(&parameters);
  if(status != SANE_STATUS_GOOD)
  {  //we couldn't get the parameters, return status
     info = tr("Image size: not available");
		 return info;
  }
  bytesize = fabs((double)(parameters.lines * parameters.bytes_per_line));
  if((parameters.format != SANE_FRAME_RGB) &&
     (parameters.format != SANE_FRAME_GRAY))
    bytesize *= 3;
  if(bytesize > 1024.0 * 1024.0)
  {
    bytesize = bytesize / (1024.0*1024.0);
    qs = tr("MB");
  }
  else if (bytesize > 1024.0)
  {
    bytesize = bytesize / 1024.0;
    qs = tr("kB");
  }
  qs2 = tr("Image size: ");
  if(parameters.lines != -1)
    info.asprintf("%d x %d, %.2f %s",abs(parameters.pixels_per_line),
                                    abs(parameters.lines),
                                    bytesize,
                                    qs.toLatin1().constData());
  else
    info.asprintf("%d x %d",abs(parameters.pixels_per_line),parameters.lines);
  qs2+=info;
  return qs2;
}
/**  */
int QScanner::optionValueSize(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
 	  return option_desc->size;
  return -1;
}
/**  */
int QScanner::xResolutionOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
		  {
            if ((0 == QString(option_desc->name).compare (SANE_NAME_SCAN_X_RESOLUTION, Qt::CaseInsensitive)
                 || 0 == QString(option_desc->name).compare (SANE_NAME_SCAN_RESOLUTION, Qt::CaseInsensitive)) &&
		  	 (isOptionSettable(i) == true))
		   	return i;
			}
	}
	printf ("Warning - no xres\n");
	return 0;
}
/**  */
int QScanner::yResolutionOption()
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)
		  {
            if(0 == QString(option_desc->name).compare (SANE_NAME_SCAN_Y_RESOLUTION, Qt::CaseInsensitive) &&
		  	 (isOptionSettable(i) == true))
		  	return i;
			}
	}
    printf ("Warning - no yres\n");
    return 0;
}
/**  */
int QScanner::xResolution()
{
  int onum;
  onum = 0;
  onum = xResolutionOption();
  if(onum)
    if(isOptionActive(onum)) return saneWordValue(onum);
  return 0;
}
/**  */
int QScanner::yResolution()
{
  int onum;
  onum = 0;
  onum = yResolutionOption();
  if(onum)
    if(isOptionActive(onum))
      return saneWordValue(onum);
  return 0;
}
/**  */
int QScanner::xResolutionDpi()
{
  SANE_Word val;
  int onum;
  onum = 0;
  val = 0;
  onum = xResolutionOption();
  if(onum)
  {
    if(isOptionActive(onum))
    {
      val = saneWordValue(onum);
      if(getOptionType(onum) == SANE_TYPE_FIXED)
        val = int(SANE_UNFIX(val));
    }
  }
  return val;
}
/**  */
int QScanner::yResolutionDpi()
{
  SANE_Word val;
  int onum;
  onum = 0;
  val = 0;
  onum = yResolutionOption();
  if(onum)
  {
    if(isOptionActive(onum))
    {
      val = saneWordValue(onum);
      if(getOptionType(onum) == SANE_TYPE_FIXED)
        val = int(SANE_UNFIX(val));
    }
  }
  return val;
}
/**  */
int QScanner::pixelWidth()
{
	SANE_Status status;
  SANE_Parameters parameters;
  status = getParameters(&parameters);
  if(status != SANE_STATUS_GOOD) return 0;
  return parameters.pixels_per_line;
}
/**  */
int QScanner::pixelHeight()
{
	SANE_Status status;
  SANE_Parameters parameters;
  status = getParameters(&parameters);
  if(status != SANE_STATUS_GOOD) return 0;
  return parameters.lines;
}
/**  */
void QScanner::close()
{
  if(mOpenOk)
	{
		if(mDeviceHandle) do_sane_close(mDeviceHandle);
		mOpenOk = false;
    mOptionNumber = -1;
	}
}
/**  */
bool QScanner::appCancel()
{
  return mAppCancel;
}
/**  */
bool QScanner::cancelled()
{
  return mCancelled;
}
/** Returns true, if SANE_CAP_AUTOMATIC is
set for option num. This means, that the backend
is able to choose an option value automatically. */
bool QScanner::automaticOption(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(option_desc)
   	if(option_desc->cap & SANE_CAP_AUTOMATIC)
      return true;
  return false;
}
/**  */
QString QScanner::saneReadOnly(int num)
{
  QString qs;
	if(mOpenOk != true) return "";
  const SANE_Option_Descriptor *option_desc;
  qs = "";

	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
  if(!isReadOnly(num)) return "";
  qs = tr("Current value: ");
  switch(option_desc->type)
  {
    case SANE_TYPE_BOOL:
      if((SANE_Bool) saneWordValue(num) == SANE_TRUE)
        qs += tr("on");
      else
        qs += tr("off");
      break;
    case SANE_TYPE_INT:
      if(option_desc->size == sizeof(SANE_Word))
        qs += QString::number(saneWordValue(num));
      else
        qs += tr("unknown");
      break;
    case SANE_TYPE_FIXED:
      if(option_desc->size == sizeof(SANE_Word))
        qs += QString::number(SANE_UNFIX(saneWordValue(num)),'f',2);
      else
        qs += tr("unknown");
      break;
    case SANE_TYPE_STRING:
      qs += saneStringValue(num);
      break;
    default:
      qs += tr("unknown");
  }
  qs += " ";
  switch(option_desc->unit)
  {
    case SANE_UNIT_PIXEL:
        qs += tr("pixel");
      break;
    case SANE_UNIT_BIT:
        qs += tr("bit");
      break;
    case SANE_UNIT_MM:
        qs += tr("mm");
      break;
    case SANE_UNIT_DPI:
        qs += tr("dpi");
      break;
    case SANE_UNIT_PERCENT:
        qs += tr("%");
      break;
    case SANE_UNIT_MICROSECOND:
        qs += tr("%");
      break;
    default:;
  }
  return qs;
}
/**  */
bool QScanner::isReadOnly(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
 	if((option_desc->cap & SANE_CAP_SOFT_DETECT) &&
     !(option_desc->cap & SANE_CAP_SOFT_SELECT))// &&
//     !(option_desc->cap & SANE_CAP_HARD_SELECT))
    return true;
  else
    return false;
}
/**  */
void QScanner::setOptionsByName(QMap <QString,QString> omap)
{
  //We can only set active and settable options, otherwise
  //most backends return an error.
  //The following approach is used
  //--set the active+settable options and mark them in
  //  the map
  //--iterate over the map again, until all options have been set
  //  or the map contains inactive options only
  bool inactive_only;
  SANE_Word sw;
  QString qs;
  int optnum;
  if(omap.isEmpty()) return;
  inactive_only = false;
  QMap<QString,QString>::Iterator it;
  enableReloadSignal(false);
  while(!inactive_only)
  {
    inactive_only = true;
    for( it = omap.begin(); it != omap.end(); ++it )
    {
      optnum = optionNumberByName(it.key());
      if(optnum<0) continue;
      if(isOptionSettable(optnum) && isOptionActive(optnum) &&
         omap[it.key()] != "---set")
      {
        inactive_only = false;
        switch(getOptionType(optnum))
        {
          case SANE_TYPE_INT:
          case SANE_TYPE_FIXED:
           if(optionValueSize(optnum)==sizeof(SANE_Word))
           {
             sw = it.value().toInt();
             setOption(optnum,&sw);
           }
           if((unsigned int)optionValueSize(optnum)>sizeof(SANE_Word))
           {
             QString fname;
             QFile qf(it.value());
             if(qf.open(QIODevice::ReadOnly))
             {
               QDataStream ds(&qf);
               QVector <SANE_Word> a;
               a.resize(optionValueSize(optnum)/sizeof(SANE_Word));
               int i = 0;
               qint32 data;
               while(i<a.size() && !ds.atEnd())
               {
                 ds >> data;
                 a[i] = (SANE_Word) data;
                 i += 1;
               }
               qf.close();
               setOption(optnum,a.data());
             }
           }
           break;
          case SANE_TYPE_BOOL:
           sw = it.value().toInt();
           setOption(optnum,&sw);
           break;
          case SANE_TYPE_STRING:
           qs = it.value();
             setOption(optnum,(SANE_String*)qs.toLatin1().constData());
           break;
          default:;
        }
        omap[it.key()] = "---set";
      }
      if(!inactive_only) break;//break for loop
    }
  }
  enableReloadSignal(true);
  emit signalReloadOptions();
}


void QScanner::reloadOptions (void)
{
  emit signalReloadOptions();
}


/**  */
int QScanner::optionNumberByName(QString name)
{
	int i;
	i=0;
  const SANE_Option_Descriptor *option_desc;
	for(i = 1;i<mOptionNumber;i++)
	{
		option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
    if(option_desc)//might be 0
 		  if(QString(option_desc->name)==name)
		  	return i;
	}
	return -1;
}
/**Returns the name of the selected device.
  */
QString QScanner::name()
{
  return mDeviceName;
}
/**Returns the name of the selected device.
  */
QString QScanner::vendor()
{
  return mDeviceVendor;
}
/**Returns the name of the selected device.
  */
QString QScanner::type()
{
  return mDeviceType;
}
/**Returns the name of the selected device.
  */
QString QScanner::model()
{
  return mDeviceModel;
}
/**Appends the currently active and settable options to a QDomElement
*/
void QScanner::settingsDomElement(QDomDocument doc,QDomElement domel)
{
  int type;
  QString sval;
  for(int i=0;i<mOptionNumber;i++)
  {
    //only append active & settable options
    if(isOptionActive(i) && isOptionSettable(i))
    {
      type = getOptionType(i);
      //we don't save vectors here
      if((type==SANE_TYPE_INT || type==SANE_TYPE_FIXED ||
          type==SANE_TYPE_BOOL))
      {
         if(optionValueSize(i)==sizeof(SANE_Word))
         {//a normal fixed or int value
           QDomElement newelement = doc.createElement("sane_option");
           newelement.setAttribute("name",getOptionName(i));
           newelement.setAttribute("type",sval.setNum(type));
           newelement.setAttribute("value",sval.setNum(saneWordValue(i)));
           domel.appendChild(newelement);
         }
         if((unsigned int)optionValueSize(i)>sizeof(SANE_Word))
         { //a vector
           //Vector data isn't stored in the XML file. We create
           //a file instead, with a name like:
           //device_name-optionname-username.vec
           //The attribute value is set to the filename under which
           //the vector has been stored
           QDomElement newelement = doc.createElement("sane_option");
           newelement.setAttribute("name",getOptionName(i));
           newelement.setAttribute("type",sval.setNum(type));
           QString fname;
           QString fname2;
           fname  = xmlConfig->absConfDirPath();
           fname2 = mDeviceName+"-"+getOptionName(i)+"-"+
                   domel.attribute("username")+".vec";
           //we replace ":" and "/" with "_"
           fname2.replace(QRegExp("[/:]"),"_");
           fname += fname2;
           newelement.setAttribute("value",fname);
           QFile qf(fname);
           if(qf.open(QIODevice::WriteOnly))
           {
             QDataStream ds(&qf);
             QVector <SANE_Word> a = saneWordArray(i);
             for(int i=0;i<a.size();i++)
               ds << (qint32)a[i];
           }
           domel.appendChild(newelement);
         }
      }
      if(type==SANE_TYPE_STRING)
      {
         QDomElement newelement = doc.createElement("sane_option");
         newelement.setAttribute("name",getOptionName(i));
         newelement.setAttribute("type",sval.setNum(type));
         sval = saneStringValue(i);
         newelement.setAttribute("value",sval);
         domel.appendChild(newelement);
      }
    }
  }
}
/**  */
SANE_Status QScanner::saneStatus()
{
  return mSaneStatus;
}

void QScanner::qis_authorization(SANE_String_Const resource,
                       SANE_Char username[SANE_MAX_USERNAME_LEN],
                       SANE_Char password[SANE_MAX_PASSWORD_LEN])
{
  QDialog* pd = 0L;
  QFile passfile;
  QFileInfo fileinfo;
  unsigned char md5digest[16];
  QString buf;
  bool is_secure = false;
  bool pass_file_insecure = false;
  bool ask_user = true;
  QString qs;
  QString string_username;
  QString string_password;
  QString res_string;
  QString dev_name;

  buf = QString();
  qs = QString();
  res_string = QString();
  dev_name = QString();
  string_username = QString();
  string_password = QString();

  res_string = resource;

  if(res_string.indexOf("$MD5$") > -1) //secure
  {
    is_secure = true;
    dev_name = res_string.left(res_string.indexOf("$MD5$"));
  }
  else //insecure
  {
    dev_name = res_string;
    is_secure = false;
  }
  //If password transmission is secure, we can try to read
  //the password file; if it exists, it's located under
  //~/.sane/pass
  qs = QDir::homePath();
  if(qs.right(1) != "/") qs += "/";
  qs += ".sane/pass";
  fileinfo.setFile(qs);
  if(fileinfo.exists())
  {
     pass_file_insecure = true;
     if(!fileinfo.permission(QFile::WriteGroup)  &&
        !fileinfo.permission(QFile::ReadGroup)   &&
        !fileinfo.permission(QFile::ExeGroup)   &&
        !fileinfo.permission(QFile::WriteOther) &&
        !fileinfo.permission(QFile::ReadOther)  &&
        !fileinfo.permission(QFile::ExeOther))
     {
       if(fileinfo.permission(QFile::ReadUser))
         pass_file_insecure = false;
     }
     //If passfile insecure print error message, else
     //try to find username and password for the chosen device
     if(pass_file_insecure)
     {
       QMessageBox::warning(0,
                    QObject::tr("Warning - Insecure password file"),
                    QObject::tr("<html>A password file with insecure "
                    "permissions has been found. You should change the "
                    "permissions of the file<br>"
                    "<center><code><b>~/.sane/pass</b></code></center><br>"
                    "to 0600 or stricter. That means, that only the "
                    "owner may have read/write permission. If you don't "
                    "change the permissions, you will be prompted for your "
                    "username and password.</html>"),
                    QObject::tr("OK"));
     }
     else
     {
       passfile.setFileName(qs);
       if(passfile.open(QIODevice::ReadOnly))
       {
         QTextStream ts(&passfile);
         QString s;
         int n = 0;
         //A valid entry in the pass file looks like this:
         //<user>:<password>:<device>
         //See e.g. "man scanimage" for reference
         while(!ts.atEnd())
         {
           s = ts.readLine();
           //get username
           n = s.indexOf(":");
           string_username = s.left(n);
           s = s.right(s.length()-n-1);
           n = s.indexOf(":");
           string_password = s.left(n);
           s = s.right(s.length()-n-1);
           if(s == res_string.left(s.length()))
           {
             ask_user = false;
             break;
           }
         }
         passfile.close();
       }
     }
  }
  //If we couldn't load password from pass file or if password
  //transmission is insecure, then we have to prompt the user.
  if(ask_user)
  {
    pd = new QDialog();
    pd->setModal(true);
    pd->setWindowTitle(QObject::tr("QuiteInsane Authorization"));
    QGridLayout* mainlayout = new QGridLayout(pd);
    mainlayout->setMargin(12);
    mainlayout->setSpacing(5);

    qs = QObject::tr("<center>The device</center>"
                     "<center><b>%1</b></center>"
                     "<center>requires authorization.</center><br>").arg(dev_name);
    if(is_secure)
      qs += QObject::tr("<center>Password transmission is secure.</center>");
    else
      qs += QObject::tr("<center>Password transmission is <b>insecure</b>!</center>");
    QLabel* infolabel = new QLabel(qs,pd);
    mainlayout->addWidget(infolabel,0,0,0,3);

    QLabel* userlabel = new QLabel(QObject::tr("Username:"),pd);
    mainlayout->addWidget(userlabel,1,1,0,1);

    QLineEdit* userle = new QLineEdit(pd);
    userle->setMaxLength(SANE_MAX_USERNAME_LEN-1);
    mainlayout->addWidget(userle,1,1,2,3);
    userle->setFocus();

    QLabel* passlabel = new QLabel(QObject::tr("Password:"),pd);
    mainlayout->addWidget(passlabel,2,2,0,1);

    QLineEdit* passle = new QLineEdit(pd);
    passle->setMaxLength(SANE_MAX_PASSWORD_LEN-1);
    passle->setEchoMode(QLineEdit::NoEcho);
    mainlayout->addWidget(passle,2,2,2,3);

    QPushButton* okbutton = new QPushButton(QObject::tr("&OK"),pd);
    mainlayout->addWidget(okbutton,3,0);
    okbutton->setDefault(true);

    QPushButton* cancelbutton = new QPushButton(QObject::tr("&Cancel"),pd);
    mainlayout->addWidget(cancelbutton,3,3);

    QObject::connect(okbutton,SIGNAL(clicked()),pd,SLOT(accept()));
    QObject::connect(cancelbutton,SIGNAL(clicked()),pd,SLOT(reject()));

    QScanner::msAuthorizationCancelled = false;
    if(!pd->exec())
    {
      QScanner::msAuthorizationCancelled = true;
    }
    string_username  =  userle->text();
    string_password  =  passle->text();
  }

  sprintf(username,"%s",string_username.toLatin1().constData());
  if(is_secure)
  {
    buf = res_string.mid(res_string.indexOf("$MD5$")+5);
    buf += string_password.toLatin1();
    md5_buffer(buf.toLatin1().constData(),buf.length(), md5digest);
    memset(password, 0, SANE_MAX_PASSWORD_LEN); /* clear password */
    sprintf(password, "$MD5$%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            md5digest[0],  md5digest[1],  md5digest[2],  md5digest[3],
            md5digest[4],  md5digest[5],  md5digest[6],  md5digest[7],
            md5digest[8],  md5digest[9],  md5digest[10], md5digest[11],
            md5digest[12], md5digest[13], md5digest[14], md5digest[15]);
  }
  else
  {
    sprintf(password,"%s",string_password.toLatin1().constData());
  }
  buf = QString();
  qs = QString();
  res_string = QString();
  dev_name = QString();
  string_password = QString();
  string_username = QString();
  if (pd) delete pd;
}
/** No descriptions */
int QScanner::optionSize(int num)
{
	const SANE_Option_Descriptor *option_desc;
 	option_desc=do_sane_get_option_descriptor(mDeviceHandle,num);
 	return option_desc->size;
}
/** No descriptions */
double QScanner::imageInfoMB()
{
	SANE_Status status;
  SANE_Parameters parameters;
  double bytesize;
  QString qs;
  QString qs2;
  qs = tr("byte");
  status = getParameters(&parameters);
  if(status != SANE_STATUS_GOOD)
  {
    return -1.0;
  }
  bytesize = fabs((double)(parameters.lines * parameters.bytes_per_line));
  if((parameters.format != SANE_FRAME_RGB) &&
     (parameters.format != SANE_FRAME_GRAY))
    bytesize *= 3;
  bytesize = bytesize / (1024.0*1024.0);
  return bytesize;
}
/** No descriptions */
QString QScanner::deviceSettingsName()
{
  QString qs;
  qs = vendor() + model();
  return qs;
}
/** No descriptions */
void QScanner::setVendor(QString vendor)
{
  mDeviceVendor = vendor;
}
/** No descriptions */
void QScanner::setModel(QString model)
{
  mDeviceModel = model;
}
/** No descriptions */
void QScanner::setType(QString type)
{
  mDeviceType = type;
}


bool QScanner::useAdf (void)
{
   return mOptionSource != -1
      && saneStringValue (mOptionSource).indexOf ("ADF", 0, Qt::CaseInsensitive)
           != -1;
}


bool QScanner::duplex (void)
{
   // use the duplex option if present
   if (mOptionDuplex != -1)
      return saneStringValue (mOptionDuplex).indexOf ("both", 0,
                                                      Qt::CaseInsensitive) != -1;

   if (mOptionSource == -1)
      return false;

   QString val = saneStringValue (mOptionSource);
//   printf ("source = %s", val.latin1 ());

   return val.indexOf ("duplex", 0, Qt::CaseInsensitive) != -1
      || val.indexOf ("both", 0, Qt::CaseInsensitive) != -1;
}


QScanner::format_t QScanner::format (void)
{
   if (mOptionFormat == -1)
      return other;
   QString str = saneStringValue (mOptionFormat).toLower ();
   if (str == "lineart")
      return mono;

   // I wish Americans could spell :-)
   if (str == "halftone")
      return dither;
   if (str == "gray")
      return grey;
   if (str == "color")
      return colour;
   return other;
}


#if 0
void QScanner::setFormat (format_t f, bool select_compression)
{
//   SANE_Word word = f;

//   if (mOptionFormat != -1)
//      setOption(mOptionFormat, &word);
    bool want_jpeg = select_compression && (f != mono && f != dither);

   QString fred = QString ("lineart,halftone,gray,color").section (',', f, f);
// find combo box, set option to given string
//!!! up to here
   if (mOptionFormat != -1)
       setOption(mOptionFormat, (SANE_String*)fred.latin1 ());
   if (mOptionCompression != -1)
      // set compression to JPEG if enabled and we are doing grey/colour scanning
       setOption(mOptionCompression, want_jpeg ? (SANE_String*)"JPEG" : (SANE_String*)"None");
}
                                   

bool QScanner::compression (void)
{
   return mOptionCompression != -1
      && saneStringValue (mOptionCompression) == "JPEG";
}
#endif


int QScanner::adfType(void)
{
   int flatbed_option = -1, adf_option = -1;
   int option = -1;

   if (mOptionSource == -1)
      return -1;
   QStringList name = getStringList (mOptionSource);

   for (int i = 0; option == -1 && i != name.count (); i++)
      {
      if (name [i].indexOf ("flatbed", 0, Qt::CaseInsensitive) != -1 ||
          name [i].indexOf ("fb", 0, Qt::CaseInsensitive) != -1)
         flatbed_option = i;
      else if (name [i].indexOf ("front", 0, Qt::CaseInsensitive) != -1
          || name [i].indexOf ("adf", 0, Qt::CaseInsensitive) != -1)
         adf_option = i;
      }

   bool has_flatbed = flatbed_option != -1;
   bool has_adf = adf_option != -1;

   return !has_adf ? 0 : !has_flatbed ? 1 : -1;
}

bool QScanner::setAdf (bool adf)
{
   int option = -1;

   if (mOptionSource == -1)
      return false;

   QStringList name = getStringList (mOptionSource);

   // we expect either 'flatbed'/'fb' or 'adf'/'adf front'
   // find the option we need to set
   for (int i = 0; option == -1 && i != name.count (); i++)
      {
      if (!adf &&
          (name [i].indexOf ("flatbed", 0, Qt::CaseInsensitive) != -1
           || name [i].indexOf ("fb", 0, Qt::CaseInsensitive) != -1))
         option = i;
      else if (adf &&
         (name [i].indexOf ("front", 0, Qt::CaseInsensitive) != -1
          || name [i].indexOf ("adf", 0, Qt::CaseInsensitive) != -1))
         option = i;
      }
//   printf ("option = %d", option);
   if (option != -1)
      setOption(mOptionSource, (void *)name [option].toLatin1 ().constData());
   return option != -1;
}


bool QScanner::setDuplex (bool duplex)
{
   int which;
   int option = -1;

   // use the duplex option if present
   which = mOptionDuplex;
   if (which == -1)
      which = mOptionSource;

   if (which == -1)
      return false;

   QStringList name = getStringList (which);

   // we expect either 'front' or 'both'/'duplex'
   // find the option we need to set
   for (int i = 0; option == -1 && i != name.count (); i++)
      {
      if (!duplex && name [i].indexOf ("front", 0, Qt::CaseInsensitive) != -1)
         option = i;
      else if (duplex &&
         (name [i].indexOf ("both", 0, Qt::CaseInsensitive) != -1
          || name [i].indexOf ("duplex", 0, Qt::CaseInsensitive) != -1))
         option = i;
      }
   if (option != -1)
      setOption(which, (void *)name [option].toLatin1 ().constData());
   return option != -1;
}


  /** set DPI */
void QScanner::setDpi (int dpi)
{
   SANE_Word word = dpi;

   if (mOptionXRes != -1)
      setOption (mOptionXRes, &word);
   if (mOptionYRes != -1)
      setOption (mOptionYRes, &word);
}


void QScanner::set256 (int opt, int value)
{
   SANE_Word word;

   if (opt != -1)
   {
      //max = getRangeMax (opt);
      //word = (int)(((double)exposure * max + 0.5) / 100);
      word = value;
      setOption (opt, &word);
   }
}


bool QScanner::get_range (int opt, int *minp, int *maxp)
{
   if (opt != -1 && isOptionActive(opt))
      {
      *minp = getRangeMin (opt);
      *maxp = getRangeMax (opt);
      return true;
      }
   return false;
}


int QScanner::get256 (int opt)
{
   int val = -1;
   //int max;

   if (opt != -1 && isOptionActive(opt))
      {
      //max = getRangeMax (opt);
      val = saneWordValue(opt);
      //      val = (int)(((double)val * 100 + 0.5) / max);
      if (getOptionType(opt) == SANE_TYPE_FIXED)
         val = int(SANE_UNFIX(val));
      }
   return val;
}


void QScanner::setExposure (int value)
{
   set256 (mOptionThreshold, value);
}


void QScanner::setBrightness (int value)
{
   set256 (mOptionBrightness, value);
}


void QScanner::setContrast (int value)
{
   set256 (mOptionContrast, value);
}


int QScanner::getExposure (void)
{
   return get256 (mOptionThreshold);
}


bool QScanner::getRangeExposure (int *minp, int *maxp)
{
   return get_range (mOptionThreshold, minp, maxp);
}


int QScanner::getBrightness (void)
{
   return get256 (mOptionBrightness);
}


bool QScanner::getRangeBrightness (int *minp, int *maxp)
{
   return get_range (mOptionBrightness, minp, maxp);
}


int QScanner::getContrast (void)
{
   return get256 (mOptionContrast);
}


bool QScanner::getRangeContrast (int *minp, int *maxp)
{
   return get_range (mOptionContrast, minp, maxp);
}


int QScanner::findOption (const char *opt, bool settable)
{
   int count = optionCount ();
   const char *name;

   for (int i = 0; i < count; i++)
   {
      name = getOptionName(i);
      if (name && 0 == strcmp (name, opt) && (!settable || isOptionSettable (i)))
         return i;
   }
   return -1;
}


void QScanner::findOptions (void)
{
/* print all options
    int i;
    const SANE_Option_Descriptor *option_desc;
    for(i = 1;i<mOptionNumber;i++)
    {
        option_desc=do_sane_get_option_descriptor(mDeviceHandle,i);
        qDebug () << option_desc->name;
    }
*/
  mOptionSource = findOption (SANE_NAME_SCAN_SOURCE);
  mOptionDuplex = findOption ("duplex");
  mOptionXRes = findOption (SANE_NAME_SCAN_X_RESOLUTION);
  if (mOptionXRes == -1)
      mOptionXRes = findOption (SANE_NAME_SCAN_RESOLUTION);
  mOptionYRes = findOption (SANE_NAME_SCAN_Y_RESOLUTION);
  mOptionFormat = findOption (SANE_NAME_SCAN_MODE);
  mOptionCompression = findOption ("compression");
  mOptionThreshold = findOption (SANE_NAME_THRESHOLD);
  mOptionBrightness = findOption (SANE_NAME_BRIGHTNESS);
  mOptionContrast = findOption (SANE_NAME_CONTRAST);

  for (int i = 0; i < BUT_count; i++)
    mOptionButton [i] = -1;
#ifdef SANE_NAME_SCAN
  mOptionButton [BUT_scan] = findOption (SANE_NAME_SCAN, false);
  mOptionButton [BUT_email] = findOption (SANE_NAME_EMAIL, false);
  mOptionButton [BUT_copy] = findOption (SANE_NAME_COPY, false);
  mOptionButton [BUT_pdf] = findOption (SANE_NAME_PDF, false);
#endif

  // function number (for Fujitsu)
  mOptionFunction = findOption ("function", false);
}


int QScanner::checkButtons (void)
{
   int value, i;

   value = 0;
   for (i = 0; i < BUT_count; i++) {
       if (mOptionButton [i] != -1) {
           int val = saneWordValue (mOptionButton [i]);

           if (val == INT_MIN)
               return INT_MIN;
           else if (val)
               value |= 1 << i;
       }
   }

   // function button can emulate the others when 'scan' is pressed
   if (mOptionFunction != -1 && (value & 1))
   {
      value |= 1 << (saneWordValue (mOptionFunction) - 1);
   }
   return value;
}


bool QScanner::isScanning (void)
{
   return mScanning;
}

#if 0
  s->opt[SIMUL_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[SIMUL_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[SIMUL_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[SIMUL_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[SIMUL_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
#endif


SANE_Status QScanner::do_sane_open (SANE_String_Const name, SANE_Handle *handle)
   {
#ifdef SIMUL
   if (0 == strcmp (name, SIMUL_NAME))
      {
      int i;

      *handle = SIMUL_DEV;
      memset (_opt, 0, sizeof (_opt));
      for (i = 0; i < NUM_OPTIONS; ++i) {
            _opt[i].name = "filler";
            _opt[i].size = sizeof (SANE_Word);
            _opt[i].cap = SANE_CAP_INACTIVE;
      }

      /* go ahead and setup the first opt, because
         * frontend may call control_option on it
         * before calling get_option_descriptor
         */
      _opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
      _opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
      _opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
      _opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
      _opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

      _simul_params.format = SANE_FRAME_GRAY;
      _simul_params.last_frame = SANE_TRUE;
      _simul_params.bytes_per_line = 2400 / 8;
      _simul_params.pixels_per_line = 2400;
      _simul_params.lines = 3507;
      _simul_params.depth = 1;
      _max_x = 1200 * 8.5;
      _max_y = 1200 * 12;
      _min_x = 0;
      _min_y = 0;
      _max_x_fb = _max_x;
      _max_y_fb = _max_y;

      /* source */
      _source = SOURCE_ADF_FRONT;

      _mode = MODE_LINEART;

      /*x res*/
      _resolution_x = 300;

      /*y res*/
      _resolution_y = 300;

      /* page width US-Letter */
      _page_width = 8 * 1200;
      if(_page_width > _max_x){
         _page_width = _max_x;
      }

      /* page height US-Letter */
      _page_height = 11.75 * 1200;
      if(_page_height > _max_y){
         _page_height = _max_y;
      }

      _tl_x = _tl_y = 0;

      /* bottom-right x */
      _br_x = _page_width;

      /* bottom-right y */
      _br_y = _page_height;

      _jpeg_interlace = JPEG_INTERLACE_NONE;
      _duplex_interlace = DUPLEX_INTERLACE_ALT;
      _page_upto = 0;

      return SANE_STATUS_GOOD;
      }
   else
#endif
      return sane_open (name, handle);
   }


SANE_Status QScanner::do_sane_control_option (SANE_Handle handle, SANE_Int option,
                                      SANE_Action action, void *val,
                                      SANE_Int *info)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      {
      SANE_Int dummy = 0;

      /* Make sure that all those statements involving *info cannot break (better
         * than having to do "if (info) ..." everywhere!)
         */
      if (info == 0)
         info = &dummy;
      if (option >= NUM_OPTIONS)
         return SANE_STATUS_INVAL;
      /*
         * SANE_ACTION_GET_VALUE: We have to find out the current setting and
         * return it in a human-readable form (often, text).
         */
      if (action == SANE_ACTION_GET_VALUE)
         {
         SANE_Word * val_p = (SANE_Word *) val;

         switch (option)
            {
            case OPT_NUM_OPTS:
               *val_p = NUM_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_SOURCE:
               if(_source == SOURCE_FLATBED){
                  strcpy ((char *)val, string_Flatbed);
               }
               else if(_source == SOURCE_ADF_FRONT){
                  strcpy ((char *)val, string_ADFFront);
               }
               else if(_source == SOURCE_ADF_BACK){
                  strcpy ((char *)val, string_ADFBack);
               }
               else if(_source == SOURCE_ADF_DUPLEX){
                  strcpy ((char *)val, string_ADFDuplex);
               }
               return SANE_STATUS_GOOD;

            case OPT_MODE:
               if(_mode == MODE_LINEART){
                  strcpy ((char *)val, string_Lineart);
               }
               else if(_mode == MODE_HALFTONE){
                  strcpy ((char *)val, string_Halftone);
               }
               else if(_mode == MODE_GRAYSCALE){
                  strcpy ((char *)val, string_Grayscale);
               }
               else if(_mode == MODE_COLOR){
                  strcpy ((char *)val, string_Color);
               }
               return SANE_STATUS_GOOD;

            case OPT_X_RES:
               *val_p = _resolution_x;
               return SANE_STATUS_GOOD;

            case OPT_Y_RES:
               *val_p = _resolution_y;
               return SANE_STATUS_GOOD;

            case OPT_TL_X:
               *val_p = SCANNER_UNIT_TO_FIXED_MM(_tl_x);
               return SANE_STATUS_GOOD;

            case OPT_TL_Y:
               *val_p = SCANNER_UNIT_TO_FIXED_MM(_tl_y);
               return SANE_STATUS_GOOD;

            case OPT_BR_X:
               *val_p = SCANNER_UNIT_TO_FIXED_MM(_br_x);
               return SANE_STATUS_GOOD;

            case OPT_BR_Y:
               *val_p = SCANNER_UNIT_TO_FIXED_MM(_br_y);
               return SANE_STATUS_GOOD;

            case OPT_PAGE_WIDTH:
               *val_p = SCANNER_UNIT_TO_FIXED_MM(_page_width);
               return SANE_STATUS_GOOD;

            case OPT_PAGE_HEIGHT:
               *val_p = SCANNER_UNIT_TO_FIXED_MM(_page_height);
               return SANE_STATUS_GOOD;

            case OPT_BRIGHTNESS:
               *val_p = _brightness;
               return SANE_STATUS_GOOD;

            case OPT_CONTRAST:
               *val_p = _contrast;
               return SANE_STATUS_GOOD;

            case OPT_GAMMA:
               *val_p = SANE_FIX(_gamma);
               return SANE_STATUS_GOOD;

            case OPT_THRESHOLD:
               *val_p = _threshold;
               return SANE_STATUS_GOOD;

            /* Advanced Group */
            case OPT_COMPRESS:
               if(_compress == COMP_JPEG){
                  strcpy ((char *)val, string_JPEG);
               }
               else{
                  strcpy ((char *)val, string_None);
               }
               return SANE_STATUS_GOOD;

            case OPT_COMPRESS_ARG:
               *val_p = _compress_arg;
               return SANE_STATUS_GOOD;

            }
         }
      else if (action == SANE_ACTION_SET_VALUE)
         {
         int tmp;
         SANE_Word val_c;
//          SANE_Status status;

         if ( _started ) {
            return SANE_STATUS_DEVICE_BUSY;
         }

         if (!SANE_OPTION_IS_SETTABLE (_opt[option].cap)) {
            return SANE_STATUS_INVAL;
         }

/*            status = sanei_constrain_value (_opt + option, val, info);
            if (status != SANE_STATUS_GOOD) {
            return status;
            }*/

         /* may have been changed by constrain, so dont copy until now */
         val_c = *(SANE_Word *)val;

            /*
            * Note - for those options which can assume one of a list of
            * valid values, we can safely assume that they will have
            * exactly one of those values because that's what
            * sanei_constrain_value does. Hence no "else: invalid" branches
            * below.
            */
         switch (option)
            {

            /* Mode Group */
            case OPT_SOURCE:
               if (!strcmp ((char *)val, string_ADFFront)) {
                  tmp = SOURCE_ADF_FRONT;
               }
               else if (!strcmp ((char *)val, string_ADFBack)) {
                  tmp = SOURCE_ADF_BACK;
               }
               else if (!strcmp ((char *)val, string_ADFDuplex)) {
                  tmp = SOURCE_ADF_DUPLEX;
               }
               else{
                  tmp = SOURCE_FLATBED;
               }

               if (_source == tmp)
                  return SANE_STATUS_GOOD;

               _source = tmp;
               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_MODE:
               if (!strcmp ((char *)val, string_Lineart)) {
                  tmp = MODE_LINEART;
               }
               else if (!strcmp ((char *)val, string_Halftone)) {
                  tmp = MODE_HALFTONE;
               }
               else if (!strcmp ((char *)val, string_Grayscale)) {
                  tmp = MODE_GRAYSCALE;
               }
               else{
                  tmp = MODE_COLOR;
               }

               if (tmp == _mode)
                  return SANE_STATUS_GOOD;

               _mode = tmp;
               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_X_RES:

               if (_resolution_x == val_c)
                  return SANE_STATUS_GOOD;

               /* currently the same? move y too */
               if (_resolution_x == _resolution_y){
                  _resolution_y = val_c;
                  /*sanei_constrain_value (_opt + OPT_Y_RES, (void *) &val_c, 0) == SANE_STATUS_GOOD*/
               }

               _resolution_x = val_c;

               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_Y_RES:

               if (_resolution_y == val_c)
                  return SANE_STATUS_GOOD;

               _resolution_y = val_c;

               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            /* Geometry Group */
            case OPT_TL_X:
               if (_tl_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
                  return SANE_STATUS_GOOD;

               _tl_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_TL_Y:
               if (_tl_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
                  return SANE_STATUS_GOOD;

               _tl_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_BR_X:
               if (_br_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
                  return SANE_STATUS_GOOD;

               _br_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_BR_Y:
               if (_br_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
                  return SANE_STATUS_GOOD;

               _br_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

               *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_PAGE_WIDTH:
               if (_page_width == FIXED_MM_TO_SCANNER_UNIT(val_c))
                  return SANE_STATUS_GOOD;

               _page_width = FIXED_MM_TO_SCANNER_UNIT(val_c);
               *info |= SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            case OPT_PAGE_HEIGHT:
               if (_page_height == FIXED_MM_TO_SCANNER_UNIT(val_c))
                  return SANE_STATUS_GOOD;

               _page_height = FIXED_MM_TO_SCANNER_UNIT(val_c);
               *info |= SANE_INFO_RELOAD_OPTIONS;
               return SANE_STATUS_GOOD;

            /* Enhancement Group */
            case OPT_BRIGHTNESS:
               _brightness = val_c;
               return SANE_STATUS_GOOD;

            case OPT_CONTRAST:
               _contrast = val_c;
               return SANE_STATUS_GOOD;

            case OPT_GAMMA:
               _gamma = SANE_UNFIX(val_c);
               return SANE_STATUS_GOOD;

            case OPT_THRESHOLD:
               _threshold = val_c;
               return SANE_STATUS_GOOD;

            /* Advanced Group */
            case OPT_COMPRESS:
               if (!strcmp ((char *)val, string_JPEG)) {
                  tmp = COMP_JPEG;
               }
               else{
                  tmp = COMP_NONE;
               }

               if (tmp == _compress)
                  return SANE_STATUS_GOOD;

               _compress = tmp;
               return SANE_STATUS_GOOD;

            case OPT_COMPRESS_ARG:
               _compress_arg = val_c;
               return SANE_STATUS_GOOD;

            }                       /* switch */
         }                           /* else */
      return SANE_STATUS_INVAL;
      }
   else
#endif
      return sane_control_option (handle, option, action, val, info);
   }


/**
 * Convenience method to determine longest string size in a list.
 */
static size_t
maxStringSize (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i) {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
      max_size = size;
  }

  return max_size;
}

/* s->page_width stores the user setting
 * for the paper width in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
int QScanner::get_page_width()
{
  return _page_width;
}

/* s->page_height stores the user setting
 * for the paper height in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
int QScanner::get_page_height(void)
{
  return _page_height;
}


const SANE_Option_Descriptor *QScanner::do_sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      {
      SANE_Option_Descriptor *opt = &_opt [option];
      int i;

      if ((unsigned) option >= NUM_OPTIONS)
         return NULL;

      /* "Mode" group -------------------------------------------------------- */
      if(option==OPT_STANDARD_GROUP){
         opt->name = SANE_NAME_STANDARD;
         opt->title = SANE_TITLE_STANDARD;
         opt->desc = SANE_DESC_STANDARD;
         opt->type = SANE_TYPE_GROUP;
         opt->constraint_type = SANE_CONSTRAINT_NONE;
      }

      /* source */
      if(option==OPT_SOURCE){
         i=0;
//          _source_list[i++]=string_Flatbed;
         _source_list[i++]=string_ADFFront;
         _source_list[i++]=string_ADFBack;
         _source_list[i++]=string_ADFDuplex;
         _source_list[i]=NULL;

         opt->name = SANE_NAME_SCAN_SOURCE;
         opt->title = SANE_TITLE_SCAN_SOURCE;
         opt->desc = SANE_DESC_SCAN_SOURCE;
         opt->type = SANE_TYPE_STRING;
         opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
         opt->constraint.string_list = _source_list;
         opt->size = maxStringSize (opt->constraint.string_list);
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* scan mode */
      if(option==OPT_MODE){
         i=0;
         _mode_list[i++]=string_Lineart;
//          _mode_list[i++]=string_Halftone;
         _mode_list[i++]=string_Grayscale;
         _mode_list[i++]=string_Color;
         _mode_list[i]=NULL;

         opt->name = SANE_NAME_SCAN_MODE;
         opt->title = SANE_TITLE_SCAN_MODE;
         opt->desc = SANE_DESC_SCAN_MODE;
         opt->type = SANE_TYPE_STRING;
         opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
         opt->constraint.string_list = _mode_list;
         opt->size = maxStringSize (opt->constraint.string_list);
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* x resolution */
      /* some scanners only support fixed res
         * build a list of possible choices */
      if(option==OPT_X_RES){
         i=0;
         _x_res_list[++i] = 200;
         _x_res_list[++i] = 300;
         _x_res_list[++i] = 400;
         _x_res_list[0] = i;

         opt->name = SANE_NAME_SCAN_X_RESOLUTION;
         opt->title = SANE_TITLE_SCAN_X_RESOLUTION;
         opt->desc = SANE_DESC_SCAN_X_RESOLUTION;
         opt->type = SANE_TYPE_INT;
         opt->unit = SANE_UNIT_DPI;
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

         if(1){
            _x_res_range.min = 200;
            _x_res_range.max = 400;
            _x_res_range.quant = 100;
            opt->constraint_type = SANE_CONSTRAINT_RANGE;
            opt->constraint.range = &_x_res_range;
         }
         else{
//             opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
//             opt->constraint.word_list = _x_res_list;
         }
      }

      /* y resolution */
      if(option==OPT_Y_RES){
         i=0;
         _y_res_list[++i] = 200;
         _y_res_list[++i] = 300;
         _y_res_list[++i] = 400;
         _y_res_list[0] = i;

         opt->name = SANE_NAME_SCAN_Y_RESOLUTION;
         opt->title = SANE_TITLE_SCAN_Y_RESOLUTION;
         opt->desc = SANE_DESC_SCAN_Y_RESOLUTION;
         opt->type = SANE_TYPE_INT;
         opt->unit = SANE_UNIT_DPI;
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

         if(1){
            _y_res_range.min = 200;
            _y_res_range.max = 400;
            _y_res_range.quant = 100;
            opt->constraint_type = SANE_CONSTRAINT_RANGE;
            opt->constraint.range = &_y_res_range;
         }
         else{
//             opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
//             opt->constraint.word_list = _y_res_list;
         }
      }

      /* "Geometry" group ---------------------------------------------------- */
      if(option==OPT_GEOMETRY_GROUP){
         opt->name = SANE_NAME_GEOMETRY;
         opt->title = SANE_TITLE_GEOMETRY;
         opt->desc = SANE_DESC_GEOMETRY;
         opt->type = SANE_TYPE_GROUP;
         opt->constraint_type = SANE_CONSTRAINT_NONE;
      }

      /* top-left x */
      if(option==OPT_TL_X){
         /* values stored in 1200 dpi units */
         /* must be converted to MM for sane */
         _tl_x_range.min = SCANNER_UNIT_TO_FIXED_MM(0);
         _tl_x_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_width());
         _tl_x_range.quant = MM_PER_UNIT_FIX;

         opt->name = SANE_NAME_SCAN_TL_X;
         opt->title = SANE_TITLE_SCAN_TL_X;
         opt->desc = SANE_DESC_SCAN_TL_X;
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_MM;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &(_tl_x_range);
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* top-left y */
      if(option==OPT_TL_Y){
         /* values stored in 1200 dpi units */
         /* must be converted to MM for sane */
         _tl_y_range.min = SCANNER_UNIT_TO_FIXED_MM(_min_y);
         _tl_y_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_height());
         _tl_y_range.quant = MM_PER_UNIT_FIX;

         opt->name = SANE_NAME_SCAN_TL_Y;
         opt->title = SANE_TITLE_SCAN_TL_Y;
         opt->desc = SANE_DESC_SCAN_TL_Y;
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_MM;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &(_tl_y_range);
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* bottom-right x */
      if(option==OPT_BR_X){
         /* values stored in 1200 dpi units */
         /* must be converted to MM for sane */
         _br_x_range.min = SCANNER_UNIT_TO_FIXED_MM(_min_x);
         _br_x_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_width());
         _br_x_range.quant = MM_PER_UNIT_FIX;

         opt->name = SANE_NAME_SCAN_BR_X;
         opt->title = SANE_TITLE_SCAN_BR_X;
         opt->desc = SANE_DESC_SCAN_BR_X;
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_MM;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &(_br_x_range);
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* bottom-right y */
      if(option==OPT_BR_Y){
         /* values stored in 1200 dpi units */
         /* must be converted to MM for sane */
         _br_y_range.min = SCANNER_UNIT_TO_FIXED_MM(_min_y);
         _br_y_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_height());
         _br_y_range.quant = MM_PER_UNIT_FIX;

         opt->name = SANE_NAME_SCAN_BR_Y;
         opt->title = SANE_TITLE_SCAN_BR_Y;
         opt->desc = SANE_DESC_SCAN_BR_Y;
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_MM;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &(_br_y_range);
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* page width */
      if(option==OPT_PAGE_WIDTH){
         /* values stored in 1200 dpi units */
         /* must be converted to MM for sane */
         _paper_x_range.min = SCANNER_UNIT_TO_FIXED_MM(_min_x);
         _paper_x_range.max = SCANNER_UNIT_TO_FIXED_MM(_max_x);
         _paper_x_range.quant = MM_PER_UNIT_FIX;

         opt->name = SANE_NAME_PAGE_WIDTH;
         opt->title = SANE_TITLE_PAGE_WIDTH;
         opt->desc = SANE_DESC_PAGE_WIDTH;
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_MM;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_paper_x_range;

         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
         if(_source == SOURCE_FLATBED){
         opt->cap |= SANE_CAP_INACTIVE;
         }
      }

      /* page height */
      if(option==OPT_PAGE_HEIGHT){
         /* values stored in 1200 dpi units */
         /* must be converted to MM for sane */
         _paper_y_range.min = SCANNER_UNIT_TO_FIXED_MM(_min_y);
         _paper_y_range.max = SCANNER_UNIT_TO_FIXED_MM(_max_y);
         _paper_y_range.quant = MM_PER_UNIT_FIX;

         opt->name = SANE_NAME_PAGE_HEIGHT;
         opt->title = SANE_TITLE_PAGE_HEIGHT;
         opt->desc = SANE_DESC_PAGE_HEIGHT;
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_MM;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_paper_y_range;

         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
         if(_source == SOURCE_FLATBED){
         opt->cap |= SANE_CAP_INACTIVE;
         }
      }

      /* "Enhancement" group ------------------------------------------------- */
      if(option==OPT_ENHANCEMENT_GROUP){
         opt->name = SANE_NAME_ENHANCEMENT;
         opt->title = SANE_TITLE_ENHANCEMENT;
         opt->desc = SANE_DESC_ENHANCEMENT;
         opt->type = SANE_TYPE_GROUP;
         opt->constraint_type = SANE_CONSTRAINT_NONE;
      }

      /* brightness */
      if(option==OPT_BRIGHTNESS){
         opt->name = SANE_NAME_BRIGHTNESS;
         opt->title = SANE_TITLE_BRIGHTNESS;
         opt->desc = SANE_DESC_BRIGHTNESS;
         opt->type = SANE_TYPE_INT;
         opt->unit = SANE_UNIT_NONE;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_brightness_range;
         _brightness_range.quant=1;

         /* some have hardware brightness (always 0 to 255?) */
         /* some use LUT or GT (-127 to +127)*/
         _brightness_range.min=-127;
         _brightness_range.max=127;
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* contrast */
      if(option==OPT_CONTRAST){
         opt->name = SANE_NAME_CONTRAST;
         opt->title = SANE_TITLE_CONTRAST;
         opt->desc = SANE_DESC_CONTRAST;
         opt->type = SANE_TYPE_INT;
         opt->unit = SANE_UNIT_NONE;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_contrast_range;
         _contrast_range.quant=1;

         /* some have hardware contrast (always 0 to 255?) */
         /* some use LUT or GT (-127 to +127)*/
         _contrast_range.min=-127;
         _contrast_range.max=127;
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      }

      /* gamma */
      if(option==OPT_GAMMA){
         opt->name = "gamma";
         opt->title = "Gamma function exponent";
         opt->desc = "Changes intensity of midtones";
         opt->type = SANE_TYPE_FIXED;
         opt->unit = SANE_UNIT_NONE;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_gamma_range;

         /* value ranges from .3 to 5, should be log scale? */
         _gamma_range.quant=SANE_FIX(0.01);
         _gamma_range.min=SANE_FIX(0.3);
         _gamma_range.max=SANE_FIX(5);

         /* scanner has gamma via LUT or GT */
         opt->cap = SANE_CAP_INACTIVE;
      }

      /*threshold*/
      if(option==OPT_THRESHOLD){
         opt->name = SANE_NAME_THRESHOLD;
         opt->title = SANE_TITLE_THRESHOLD;
         opt->desc = SANE_DESC_THRESHOLD;
         opt->type = SANE_TYPE_INT;
         opt->unit = SANE_UNIT_NONE;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_threshold_range;
         _threshold_range.min=0;
         _threshold_range.max=255;
         _threshold_range.quant=1;

         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
         if(_mode != MODE_LINEART){
         opt->cap |= SANE_CAP_INACTIVE;
         }
      }
      /* "Advanced" group ------------------------------------------------------ */
      if(option==OPT_ADVANCED_GROUP){
         opt->name = SANE_NAME_ADVANCED;
         opt->title = SANE_TITLE_ADVANCED;
         opt->desc = SANE_DESC_ADVANCED;
         opt->type = SANE_TYPE_GROUP;
         opt->constraint_type = SANE_CONSTRAINT_NONE;
      }

      /*image compression*/
      if(option==OPT_COMPRESS){
         i=0;
         _compress_list[i++]=string_None;

         _compress_list[i++]=string_JPEG;

         _compress_list[i]=NULL;

         opt->name = "compression";
         opt->title = "Compression";
         opt->desc = "Enable compressed data. May crash your front-end program";
         opt->type = SANE_TYPE_STRING;
         opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
         opt->constraint.string_list = _compress_list;
         opt->size = maxStringSize (opt->constraint.string_list);

         if (i > 1){
            opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
            if (_mode != MODE_COLOR && _mode != MODE_GRAYSCALE){
            opt->cap |= SANE_CAP_INACTIVE;
            }
         }
         else
            opt->cap = SANE_CAP_INACTIVE;
      }

      /*image compression arg*/
      if(option==OPT_COMPRESS_ARG){

         opt->name = "compression-arg";
         opt->title = "Compression argument";
         opt->desc = "Level of JPEG compression. 1 is small file, 7 is large file. 0 (default) is same as 4";
         opt->type = SANE_TYPE_INT;
         opt->unit = SANE_UNIT_NONE;
         opt->constraint_type = SANE_CONSTRAINT_RANGE;
         opt->constraint.range = &_compress_arg_range;
         _compress_arg_range.quant=1;

         _compress_arg_range.min=0;
         _compress_arg_range.max=7;
         opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

         if(_compress != COMP_JPEG){
         opt->cap |= SANE_CAP_INACTIVE;
         }
      }
      return opt;
      }
   else
#endif
      return sane_get_option_descriptor (handle, option);
   }


/* update our private copy of params with actual data size scanner reports */
SANE_Status QScanner::get_pixelsize()
{
//    SANE_Status ret;
#if 0
   DBG (10, "get_pixelsize: start");

   _params.pixels_per_line = _simul_params.pixels_per_line;

   /* bytes per line differs by mode */
   if (_mode == MODE_COLOR) {
      _params.bytes_per_line = _params.pixels_per_line * 3;
      while (_params.bytes_per_line & 3)
         _params.bytes_per_line++;
   }
   else if (_mode == MODE_GRAYSCALE) {
      _params.bytes_per_line = _params.pixels_per_line;
   }
   else {
      _params.bytes_per_line = _params.pixels_per_line / 8;
   }

   DBG (15, "get_pixelsize: scan_x=%d, Bpl=%d, scan_y=%d",
      _params.pixels_per_line, _params.bytes_per_line, _params.lines );
#endif
   return SANE_STATUS_GOOD;
   }


/*
 * Creates a temporary file, opens it, and stores file pointer for it.
 * OR, callocs a buffer to hold the scan data
 *
 * Will only create a file that
 * doesn't exist already. The function will also unlink ("delete") the file
 * immediately after it is created. In any "sane" programming environment this
 * has the effect that the file can be used for reading and writing as normal
 * but vanishes as soon as it's closed - so no cleanup required if the
 * process dies etc.
 */
SANE_Status QScanner::setup_buffers (void)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side;

  DBG (10, "setup_buffers: start");

  /* cleanup existing first */
  for(side=0;side<2;side++){

      /* close old file */
      if (_fds[side] != -1) {
        DBG (15, "setup_buffers: closing old tempfile %d.",side);
        if(::close(_fds[side])){
          DBG (5, "setup_buffers: attempt to close tempfile %d returned %d.", side, -1 /*errno*/);
        }
        _fds[side] = -1;
      }

      /* free old mem */
      if (_buffers[side]) {
        DBG (15, "setup_buffers: free buffer %d.",side);
        free(_buffers[side]);
        _buffers[side] = NULL;
      }

      if(_bytes_tot[side]){
        _buffers[side] = (unsigned char *)calloc (1,_bytes_tot[side]);
        if (!_buffers[side]) {
          DBG (5, "setup_buffers: Error, no buffer %d.",side);
          return SANE_STATUS_NO_MEM;
        }
      }
  }

  DBG (10, "setup_buffers: finish");

  return ret;
}


/* checks started and cancelled flags in scanner struct,
 * sends cancel command to scanner if required. don't call
 * this function asyncronously, wait for pending operation */
SANE_Status QScanner::check_for_cancel()
{
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "check_for_cancel: start");

  if(_started && _cancelled){
      DBG (15, "check_for_cancel: cancelling");

      ret = SANE_STATUS_CANCELLED;
      _started = 0;
      _cancelled = 0;
  }
  else if(_cancelled){
    DBG (15, "check_for_cancel: already cancelled");
    ret = SANE_STATUS_CANCELLED;
    _cancelled = 0;
  }

  DBG (10, "check_for_cancel: finish %d",ret);
  return ret;
}

SANE_Status QScanner::do_sane_start (SANE_Handle handle)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      {
      SANE_Status ret = SANE_STATUS_GOOD;

      DBG (10, "sane_start: start");
      DBG (15, "started=%d, side=%d, source=%d", _started, _side, _source);

      /* undo any prior sane_cancel calls */
      _cancelled=0;

      /* protect this block from sane_cancel */
      _reading=1;

      /* not finished with current side, error */
      if (_started && _bytes_tx[_side] != _bytes_tot[_side]) {
            DBG(5,"sane_start: previous transfer not finished?");
            return SANE_STATUS_INVAL;
         }

      /* batch start? inititalize struct and scanner */
      if(!_started) {
         /* load side marker */
         if(_source == SOURCE_ADF_BACK){
            _side = SIDE_BACK;
            }
         else{
            _side = SIDE_FRONT;
            }
#if 0
         /* load our own private copy of scan params */
         ret = do_sane_get_parameters (handle, &_params);
         if (ret != SANE_STATUS_GOOD) {
         DBG (5, "sane_start: ERROR: cannot get params");
         return ret;
         }
#endif
         /* try to read actual scan size from scanner */
         ret = get_pixelsize();
         if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: cannot get pixelsize");
            return ret;
            }
         }
      /* if already running, duplex needs to switch sides */
      else if(_source == SOURCE_ADF_DUPLEX) {
            _side = !_side;
         }

      if (_source != SOURCE_ADF_DUPLEX || _side == 0)
         {
         if (!generate_pages ())
            {
            _reading = 0;
            return SANE_STATUS_NO_DOCS;
            }
         }

      /* set clean defaults with new sheet of paper */
      /* dont reset the transfer vars on backside of duplex page */
      /* otherwise buffered back page will be lost */
      /* ingest paper with adf (no-op for fb) */
      /* dont call object pos or scan on back side of duplex scan */
      if(_side == SIDE_FRONT || _source == SOURCE_ADF_BACK) {
         _bytes_rx[0]=0;
         _bytes_rx[1]=0;
         _lines_rx[0]=0;
         _lines_rx[1]=0;

         _bytes_tx[0]=0;
         _bytes_tx[1]=0;

         /* reset jpeg just in case... */
         _jpeg_stage = JPEG_STAGE_HEAD;
         _jpeg_ff_offset = 0;
         _jpeg_front_rst = 0;
         _jpeg_back_rst = 0;

         /* store the number of front bytes */
         if ( _source != SOURCE_ADF_BACK ){
            _bytes_tot[SIDE_FRONT] = _simul_params.bytes_per_line * _simul_params.lines;
            }
         else{
            _bytes_tot[SIDE_FRONT] = 0;
            }

         /* store the number of back bytes */
         if ( _source == SOURCE_ADF_DUPLEX || _source == SOURCE_ADF_BACK ){
            _bytes_tot[SIDE_BACK] = _simul_params.bytes_per_line * _simul_params.lines;
            }
         else{
            _bytes_tot[SIDE_BACK] = 0;
            }

         /* first page of batch */
         /* make large buffer to hold the images */
         /* and set started flag */
         if(!_started) {
            ret = setup_buffers();
            if (ret != SANE_STATUS_GOOD) {
               DBG (5, "sane_start: ERROR: cannot load buffers");
               _reading = 0;
               return ret;
               }

            _started=1;
            }
         }

      DBG (15, "started=%d, side=%d, source=%d", _started, _side, _source);

      /* check if user cancelled during this start */
      ret = check_for_cancel();

      /* unprotect this block from sane_cancel */
      _reading=0;

      DBG (10, "sane_start: finish %d", ret);

      return ret;
      }
   else
#endif
      return sane_start (handle);
   }


void QScanner::do_sane_cancel (SANE_Handle handle)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      {
      _cancelled = 1;

      /* if there is no other running function to check, we do it */
      if (!_reading)
         check_for_cancel ();

      _page_upto = 0; // start again from the simulated page number
      }
   else
#endif
      sane_cancel (handle);
   }



SANE_Status QScanner::do_sane_set_io_mode (SANE_Handle handle, SANE_Bool nbio)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      return SANE_STATUS_GOOD;
   else
#endif
      return sane_set_io_mode (handle, nbio);
   }


/*
 * Called by SANE to read data.
 *
 * From the SANE spec:
 * This function is used to read image data from the device
 * represented by handle h.  Argument buf is a pointer to a memory
 * area that is at least maxlen bytes long.  The number of bytes
 * returned is stored in *len. A backend must set this to zero when
 * the call fails (i.e., when a status other than SANE_STATUS_GOOD is
 * returned).
 *
 * When the call succeeds, the number of bytes returned can be
 * anywhere in the range from 0 to maxlen bytes.
 */

SANE_Status QScanner::do_sane_read (SANE_Handle handle, SANE_Byte* buf,SANE_Int maxlen,SANE_Int* len)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      {
      SANE_Status ret=SANE_STATUS_GOOD;

      DBG (10, "sane_read: start");

      *len=0;

      /* maybe cancelled? */
      if(!_started){
            DBG (5, "sane_read: not started, call sane_start");
            return SANE_STATUS_CANCELLED;
      }

      /* sane_start required between sides */
      if(_bytes_tx[_side] == _bytes_tot[_side]){
            DBG (15, "sane_read: returning eof");
            return SANE_STATUS_EOF;
      }

      /* protect this block from sane_cancel */
      _reading = 1;
      ret = SANE_STATUS_GOOD;

      if(_source == SOURCE_ADF_DUPLEX
         && _simul_params.format == HACK_SANE_FRAME_JPEG
         && _jpeg_interlace == JPEG_INTERLACE_ALT)
         {
         /* read from front side if either side has remaining */
         if ( _bytes_tot[SIDE_FRONT] > _bytes_rx[SIDE_FRONT]
            || _bytes_tot[SIDE_BACK] > _bytes_rx[SIDE_BACK])
            {
            ret = read_from_JPEGduplex();
            if(ret){
               DBG(5,"sane_read: jpeg duplex returning %d",ret);
               }
            }
         } /* end alt jpeg */

      /* alternating pnm interlacing */
      else if(_source == SOURCE_ADF_DUPLEX
         && _simul_params.format != HACK_SANE_FRAME_JPEG
         && _duplex_interlace == DUPLEX_INTERLACE_ALT)
         {
         /* buffer front side */
         if(_bytes_tot[SIDE_FRONT] > _bytes_rx[SIDE_FRONT]){
            ret = read_from_scanner(SIDE_FRONT);
            if(ret){
               DBG(5,"sane_read: front returning %d",ret);
              }
            }

         /* buffer back side */
         if(_bytes_tot[SIDE_BACK] > _bytes_rx[SIDE_BACK] ){
            ret = read_from_scanner(SIDE_BACK);
            if(ret){
               DBG(5,"sane_read: back returning %d",ret);
               }
            }
         } /* end alt pnm */

      /* simplex or non-alternating duplex */
      else {
         if(_bytes_tot[_side] > _bytes_rx[_side] ){
            ret = read_from_scanner(_side);
            if(ret){
            DBG(5,"sane_read: side %d returning %d",_side,ret);
               }
            }
         }

      /* copy a block from buffer to frontend */
      if (!ret)
         ret = read_from_buffer(buf,maxlen,len,_side);

      /* check if user cancelled during this read */
      if (!ret)
         ret = check_for_cancel();

      /* unprotect this block from sane_cancel */
      _reading = 0;

      DBG (10, "sane_read: finish %d", ret);
      return ret;
      }
   else
#endif
      return sane_read (handle, buf, maxlen, len);
   }


// if not already done,

SANE_Status QScanner::read_from_JPEGduplex (void)
   {
//    SANE_Status ret=SANE_STATUS_GOOD;

   return SANE_STATUS_EOF;
   }


SANE_Status QScanner::read_from_scanner(int side)
{
    SANE_Status ret=SANE_STATUS_GOOD;

    unsigned char * in;
    size_t inLen = 0;

    int bytes = qMax (1000000, (int)_front.sizeInBytes ());   // it won't be larger than this!
    int remain = _bytes_tot[side] - _bytes_rx[side];

    DBG (10, "read_from_scanner: start");

    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }

    /* all requests must end on line boundary */
    bytes -= (bytes % _simul_params.bytes_per_line);

    /* this should never happen */
    if(bytes < 1){
        DBG(5, "read_from_scanner: ERROR: no bytes this pass");
        ret = SANE_STATUS_INVAL;
    }

    DBG(15, "read_from_scanner: si:%d to:%d rx:%d re:%d bu:%d pa:%d", side,
      _bytes_tot[side], _bytes_rx[side], remain, -1 /*_buffer_size*/, bytes);

    if(ret){
        return ret;
    }

    inLen = bytes;
    in = (uchar *)malloc(inLen);
    if(!in){
        DBG(5, "read_from_scanner: not enough mem for buffer: %d",(int)inLen);
        return SANE_STATUS_NO_MEM;
    }

    if (side == SIDE_BACK) {
       ret = read_bytes (_back_data, _back_upto, in, inLen);
    }
    else{
       ret = read_bytes (_front_data, _front_upto, in, inLen);
    }

    if (ret == SANE_STATUS_GOOD) {
        DBG(15, "read_from_scanner: got GOOD, returning GOOD");
    }
    else if (ret == SANE_STATUS_EOF) {
        DBG(15, "read_from_scanner: got EOF, finishing");
    }
    else if (ret == SANE_STATUS_DEVICE_BUSY) {
        DBG(5, "read_from_scanner: got BUSY, returning GOOD");
        inLen = 0;
        ret = SANE_STATUS_GOOD;
    }
    else {
        DBG(5, "read_from_scanner: error reading data block status = %d",ret);
        inLen = 0;
    }

    if(inLen){
         copy_buffer (in, inLen, side);
    }

    free(in);

    if(ret == SANE_STATUS_EOF){
      _bytes_tot[side] = _bytes_rx[side];
      ret = SANE_STATUS_GOOD;
    }

    DBG (10, "read_from_scanner: finish");

    return ret;
}


void QScanner::build_jpeg (QImage &image, QByteArray &ba)
   {
   QBuffer buffer(&ba);
   buffer.open(QIODevice::WriteOnly);

//    qDebug () << "image" << image.depth ();
   image.save(&buffer, "JPG"); // writes image into ba in PNG format
//    qDebug () << "jpeg size" << ba.size ();
/*
   QFile file ("/home/sglass/tmp/tmp.jpg");

   file.open(QIODevice::WriteOnly);
   image.save(&file, "JPG"); // writes image into ba in PNG format*/
   }


SANE_Status QScanner::read_bytes (QByteArray &ba, int &upto, unsigned char *buff, size_t &nread)
   {
   size_t total = ba.size ();
   size_t left = total - upto;
   const char *data = ba.data ();

   if (nread > left)
      nread = left;
   if (left == 0)
      return SANE_STATUS_EOF;

   /* wait a while (no scanner is instant!)
      also limit to 1/50th returned each time, but make sure we return a
      multiple of bytes_per_lines bytes */
   int div = 50;
   size_t numbytes;
   if (_simul_params.depth > 1 && _compress == COMP_JPEG)
      {
      div = 10; // 50
      numbytes = total / div;
      }
   else
      {
      size_t nlines = total / div / _simul_params.bytes_per_line;
      nlines++;
      numbytes = nlines * _simul_params.bytes_per_line;
      }

   nread = qMin (nread, (size_t)numbytes);
   usleep (20000);

   memcpy (buff, data + upto, nread);
   upto += nread;

   return SANE_STATUS_GOOD;
   }


SANE_Status QScanner::read_from_buffer (SANE_Byte * buf,
  SANE_Int max_len, SANE_Int * len, int side)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int bytes = max_len, i=0;
    int remain = _bytes_rx[side] - _bytes_tx[side];

    DBG (10, "read_from_buffer: start");

    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }

    *len = bytes;

    DBG(15, "read_from_buffer: si:%d to:%d tx:%d re:%d bu:%d pa:%d", side,
      _bytes_tot[side], _bytes_tx[side], remain, max_len, bytes);

    /*FIXME this needs to timeout eventually */
    if(!bytes){
        DBG(5,"read_from_buffer: nothing to do");
        return SANE_STATUS_GOOD;
    }

    /* jpeg data does not use typical interlacing or inverting, just copy */
    if(_compress == COMP_JPEG &&
      (_mode == MODE_COLOR || _mode == MODE_GRAYSCALE)){

        memcpy(buf,_buffers[side]+_bytes_tx[side],bytes);
    }

    /* not using jpeg, colors interlaced, pixels inverted */
    else {

        /* scanners interlace colors in many different ways */
        /* use separate code to convert to regular rgb */
        if(_mode == MODE_COLOR){
            int byteOff, lineOff;

            switch (_color_interlace) {

                /* scanner returns pixel data as bgrbgr... */
                case COLOR_INTERLACE_BGR:
                    for (i=0; i < bytes; i++){
                        byteOff = _bytes_tx[side] + i;
                        buf[i] = _buffers[side][ byteOff-((byteOff%3)-1)*2 ];
                    }
                    break;

                /* one line has the following format:
                 * rrr...rrrggg...gggbbb...bbb */
                case COLOR_INTERLACE_3091:
                case COLOR_INTERLACE_RRGGBB:
                    for (i=0; i < bytes; i++){
                        byteOff = _bytes_tx[side] + i;
                        lineOff = byteOff % _simul_params.bytes_per_line;

                        buf[i] = _buffers[side][
                          byteOff - lineOff                       /* line  */
                          + (lineOff%3)*_simul_params.pixels_per_line /* color */
                          + (lineOff/3)                           /* pixel */
                        ];
                    }
                    break;

                default:
                    memcpy(buf,_buffers[side]+_bytes_tx[side],bytes);
                    break;
            }
        }
        /* gray/ht/binary */
        else{
            memcpy(buf,_buffers[side]+_bytes_tx[side],bytes);
        }
    }

    _bytes_tx[side] += *len;

    DBG (10, "read_from_buffer: finish");

    return ret;
}


SANE_Status QScanner::copy_buffer (unsigned char * buf, int len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "copy_buffer: start");

  memcpy(_buffers[side]+_bytes_rx[side],buf,len);
  _bytes_rx[side] += len;

  DBG (10, "copy_buffer: finish");

  return ret;
}


/*
 * @@ Section 4 - SANE scanning functions
 */
/*
 * Called by SANE to retrieve information about the type of data
 * that the current scan will return.
 *
 * From the SANE spec:
 * This function is used to obtain the current scan parameters. The
 * returned parameters are guaranteed to be accurate between the time
 * a scan has been started (sane_start() has been called) and the
 * completion of that request. Outside of that window, the returned
 * values are best-effort estimates of what the parameters will be
 * when sane_start() gets invoked.
 *
 * Calling this function before a scan has actually started allows,
 * for example, to get an estimate of how big the scanned image will
 * be. The parameters passed to this function are the handle h of the
 * device for which the parameters should be obtained and a pointer p
 * to a parameter structure.
 */
SANE_Status QScanner::do_sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
#ifdef SIMUL
   if (mDeviceHandle == SIMUL_DEV)
      {
      SANE_Status ret = SANE_STATUS_GOOD;

      DBG (10, "sane_get_parameters: start");

      /* started? get param data from struct */
      if(_started){
         DBG (15, "sane_get_parameters: started, copying to caller");
         params->format = _simul_params.format;
         params->last_frame = _simul_params.last_frame;
         params->lines = _simul_params.lines;
         params->depth = _simul_params.depth;
         params->pixels_per_line = _simul_params.pixels_per_line;
         params->bytes_per_line = _simul_params.bytes_per_line;
      }

      /* not started? get param data from user settings */
      else {
         DBG (15, "sane_get_parameters: not started, updating");

         /* this backend only sends single frame images */
         params->last_frame = 1;

         params->pixels_per_line = _resolution_x * (_br_x - _tl_x) / 1200;

         params->lines = _resolution_y * (_br_y - _tl_y) / 1200;

         if (_mode == MODE_COLOR) {
            params->format = SANE_FRAME_RGB;
            params->depth = 24;
            if(_compress == COMP_JPEG){
                  params->format = HACK_SANE_FRAME_JPEG;
            }
            params->bytes_per_line = params->pixels_per_line * 3;
            while (params->bytes_per_line & 3)
               params->bytes_per_line++;
         }
         else if (_mode == MODE_GRAYSCALE) {
            params->format = SANE_FRAME_GRAY;
            params->depth = 8;
            if(_compress == COMP_JPEG){
                  params->format = HACK_SANE_FRAME_JPEG;
            }
            params->bytes_per_line = params->pixels_per_line;
         }
         else {
            params->format = SANE_FRAME_GRAY;
            params->depth = 1;
            params->bytes_per_line = ((params->pixels_per_line+31) / 32) * 4;
         }
      }

      DBG(15,"sane_get_parameters: x: max=%d, page=%d, gpw=%d, res=%d",
      _max_x, _page_width, get_page_width(), _resolution_x);

      DBG(15,"sane_get_parameters: y: max=%d, page=%d, gph=%d, res=%d",
      _max_y, _page_height, get_page_height(), _resolution_y);

      DBG(15,"sane_get_parameters: area: tlx=%d, brx=%d, tly=%d, bry=%d",
      _tl_x, _br_x, _tl_y, _br_y);

      DBG (15, "sane_get_parameters: params: ppl=%d, Bpl=%d, lines=%d",
      params->pixels_per_line, params->bytes_per_line, params->lines);

      DBG (15, "sane_get_parameters: params: format=%d, depth=%d, last=%d",
      params->format, params->depth, params->last_frame);

      DBG (10, "sane_get_parameters: finish");

      _simul_params = *params;
      return ret;
      }
   else
#endif
      return sane_get_parameters (handle, params);
}


void QScanner::do_sane_close (SANE_Handle handle)
   {
#ifdef SIMUL
   if (handle == SIMUL_DEV)
      ;
   else
#endif
      sane_close (handle);
   }


#ifdef SIMUL

bool QScanner::generate_pages (void)
   {
   _front_data.clear ();
   _back_data.clear ();
   _front_upto = 0;
   _back_upto = 0;

   // only have a small number of pages in our pretend scan
//    if (_page_upto == 6)
   if (_page_upto == 120)
      return false;
   generate_page (_page_upto, true, _front, _front_data);
   generate_page (_page_upto, false, _back, _back_data);
   _page_upto++;
   return true;
   }


void QScanner::generate_page (int pagenum, bool front, QImage &image, QByteArray &data)
   {
   QImage::Format format;
   QVector<QRgb> colours;
   int bpp, scale;

   UNUSED (pagenum);
//    qDebug () << "generate_page" << pagenum << "front" << front;
   int ppl = _simul_params.pixels_per_line;
   format = QImage::Format_Invalid;
   if (_simul_params.format == SANE_FRAME_RGB)
      {
      format = QImage::Format_RGB888;
//       while ((ppl * 3) & 3)
//          ppl++;
//       qDebug () << "ppl" << ppl;
      }
   else if (_simul_params.format == SANE_FRAME_GRAY)
      {
      if (_simul_params.depth == 1)
         format = QImage::Format_Mono;
      else if (_simul_params.depth == 8)
         format = QImage::Format_Indexed8;
      }
   else if (_simul_params.format == HACK_SANE_FRAME_JPEG)
      {
      if (_simul_params.depth == 8)
         format = QImage::Format_Indexed8;
      else if (_simul_params.depth == 24)
         format = QImage::Format_RGB32;  //888
      }
   Q_ASSERT (format != QImage::Format_Invalid);

   // cannot paint onto a grey image!
   if (format == QImage::Format_Indexed8)
      format = QImage::Format_RGB32;

   image = QImage (ppl, _simul_params.lines, format);
//    qDebug () << "bpl" << image.bytesPerLine ();
   if (format == QImage::Format_Indexed8)
      {
      colours.reserve (256);
      for (int i = 0; i < 256; i++)
         colours << qRgb (i, i, i);
      image.setColorTable (colours);
      image.fill (-1);
      bpp = 8;
      }
   else if (format == QImage::Format_Mono)
      {
      colours.reserve (2);
      colours << qRgb (255, 255, 255);
      colours << qRgb (0, 0, 0);
      image.setColorTable (colours);
      image.fill (0);
      bpp = 1;
      }
   else
      {
      bpp = 24;
      image.fill (qRgb (255, 255, 255));
//       qDebug () << "qt bpl" << image.bytesPerLine ();
      }

   // every second back page is blank
   // page 2 front is blank
   bool blank = !front && (_page_upto & 1);
   blank |= front && _page_upto == 2;

   scale = image.width () / 20;

   if (!blank)
      {
      QPainter p;

      p.begin (&image);
      p.setPen (Qt::black);
      QFont f;
      f.setPointSize (scale * 2);
      p.setFont (f);
      p.drawText (QPoint (scale, scale * 3),
         QString ("Sheet %1").arg (_page_upto * 2 + !front + 1));
      f.setPointSize (80);
      p.setFont (f);
      p.drawText (QPoint (scale, scale * 4),
         QString ("This is the %1 side").arg (front ? "front" : "back"));
      QColor col1, col2;

      col1 = bpp == 1 ? Qt::black : Qt::red;
      col2 = bpp == 1 ? Qt::black : Qt::blue;
      if (_simul_params.depth == 8)
         {
         col1 = qRgb (qGray (col1.rgb ()), qGray (col1.rgb ()), qGray (col1.rgb ()));
         col2 = qRgb (qGray (col2.rgb ()), qGray (col2.rgb ()), qGray (col2.rgb ()));
         }
      if (front)
         p.fillRect (QRect (scale * 10, scale * 10, scale * 4, scale * 4), QBrush (col1));
      else
         p.fillRect (QRect (scale, scale * 10, scale * 4, scale * 4), QBrush (col2));
      p.fillRect (QRect (scale, scale * 15, scale * 4, scale * 4), Qt::white);
      p.end ();
      }
#if 0
   QString fname = QString ("/tmp/p%1.jpg").arg (_page_upto * 2 + 1 + !front);
   QFile file (fname);

   file.open (QIODevice::WriteOnly);
   image.save (&file, "JPG");
   file.close ();
#endif
   if (_simul_params.depth == 8)
      {
      QImage grey = utilConvertImageToGrey (image);
      image = grey;
      }

#if 0
   fname = QString ("/tmp/p%1a.jpg").arg (_page_upto * 2 + 1 + !front);
   file.setFileName (fname);

   file.open (QIODevice::WriteOnly);
   image.save (&file, "JPG"); // writes image into ba in PNG format*/
   file.close ();
#endif
   // convert to JPEG if required
   if (_simul_params.depth > 1 && _compress == COMP_JPEG)
      build_jpeg (image, data);
   else
      data = QByteArray ((const char *)image.bits (), image.sizeInBytes ());
#if 0
   fname = QString ("/tmp/p%1b.bin").arg (_page_upto * 2 + 1 + !front);
   file.setFileName (fname);

   file.open (QIODevice::WriteOnly);
   file.write (data);
   file.close ();
#endif
   }


#endif

