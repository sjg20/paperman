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
#include <QRegularExpression>
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
#ifndef Q_OS_WIN
#include <dlfcn.h>
#endif
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
#include <qstring.h>
#include <stdlib.h>
#include <sys/poll.h>





bool QScanner::msAuthorizationCancelled = false;

QScanner::QScanner() : QObject()
{
  mOptionNumber = -1;
  mOptionBufferSize = -1;
  mSaneStatus = SANE_STATUS_GOOD;
  mNonBlockingIo = SANE_FALSE;
  mAppCancel = false;
  mCancelled = false;
  mNoReadDup = false;
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
	initScanner();
}
QScanner::~QScanner()
{
//     qDebug () << "~QScanner";
  exitScanner();
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

  /* the back ends are loaded by sane_init(), so this is the first point
     at which the log can say which ones are in use */
  utilLogBuild ("sane:");
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
void QScanner::setFormat (format_t f, bool select_compression)
   {
   (void)select_compression;
   if (mOptionFormat == -1 || f == other)
      return;
   QString fred = QString ("Lineart,Halftone,Gray,Color").section (',', f, f);
   QByteArray ba = fred.toLatin1 ();
   if (setOption (mOptionFormat, (void *)ba.constData ()) != SANE_STATUS_GOOD)
      {
      // Some backends spell Lineart as Binary
      if (fred == "Lineart")
         {
         QByteArray b = QByteArray ("Binary");
         setOption (mOptionFormat, (void *)b.constData ());
         }
      }
   }


/**  */
bool QScanner::reconnect()
   {
   /* Snapshot the value of every settable option so it can be restored
      after the teardown.  Without this the scanner reverts to defaults
      on every reconnect, and anything not on some hand-picked list -
      page size, scan area, compression, the Fujitsu's buffer mode and
      friends - silently goes back to its default.  Options are keyed
      by name, since their numbers can move on the fresh handle. */
   struct SavedOption
      {
      QByteArray name;
      QByteArray value;
      SANE_Value_Type type;
      };
   QList<SavedOption> saved;

   if (mOpenOk)
      {
      int count = optionCount ();

      for (int i = 1; i < count; i++)
         {
         const SANE_Option_Descriptor *desc
               = do_sane_get_option_descriptor (mDeviceHandle, i);

         if (!desc || !desc->name || !desc->name[0])
            continue;
         if (desc->type == SANE_TYPE_GROUP || desc->type == SANE_TYPE_BUTTON)
            continue;
         if (!SANE_OPTION_IS_ACTIVE (desc->cap)
            || !SANE_OPTION_IS_SETTABLE (desc->cap))
            continue;
         QByteArray value ((int)desc->size, '\0');
         if (do_sane_control_option (mDeviceHandle, i,
               SANE_ACTION_GET_VALUE, value.data (), 0)
               != SANE_STATUS_GOOD)
            continue;
         saved << SavedOption { desc->name, value, desc->type };
         }
      }

   // Close the handle, then fully tear down the SANE backend and bring it
   // back up. Just sane_close+sane_open is not enough: once a USB scanner
   // has slept the backend retains stale device state and sane_open
   // returns SANE_STATUS_INVAL. sane_exit/sane_init re-enumerates and
   // recovers.
   if (mOpenOk)
      {
      do_sane_close (mDeviceHandle);
      mDeviceHandle = nullptr;
      mOpenOk = false;
      }
   if (mInitOk)
      {
      sane_exit ();
      mInitOk = false;
      mpDeviceList = nullptr;
      }
   if (!initScanner ())
      return false;
   // re-enumerate so the backend is aware of the device again
   getDeviceList (false);

   if (!openDevice ())
      return false;

   /* Restore the snapshot to the fresh handle.  Options are applied in
      their original order, which puts source and mode (early options)
      before the geometry that depends on them.  Setting one option can
      change which others are active or what ranges they allow, so a
      second pass picks up any that could not be applied the first time
      or were clamped by a constraint that has since relaxed. */
   for (int pass = 0; pass < 2; pass++)
      foreach (const SavedOption &so, saved)
         {
         int num = findOption (so.name.constData ());

         if (num < 0 || !isOptionActive (num))
            continue;
         const SANE_Option_Descriptor *desc
               = do_sane_get_option_descriptor (mDeviceHandle, num);
         if (!desc || desc->type != so.type
            || (int)desc->size != so.value.size ())
            continue;

         // skip options which already have the right value
         QByteArray current ((int)desc->size, '\0');
         if (do_sane_control_option (mDeviceHandle, num,
               SANE_ACTION_GET_VALUE, current.data (), 0)
               == SANE_STATUS_GOOD && current == so.value)
            continue;

         QByteArray value = so.value;   // the backend may modify it
         setOption (num, value.data ());
         }

   return true;
   }


bool QScanner::openDevice()
   {
   QScanner::msAuthorizationCancelled = false;
         SANE_Status status;
   mOptionNumber = -1;
   mOptionByName.clear ();
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
	return mDeviceCnt;
}
/**  */
SANE_String_Const QScanner::name(int num)
{
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->name;
}
/**  */
SANE_String_Const QScanner::vendor(int num)
{
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->vendor;
}
/**  */
SANE_String_Const QScanner::model(int num)
{
    if(!mpDeviceList) return 0L;
	return (SANE_String) mpDeviceList[num]->model;
}
/**  */
SANE_String_Const QScanner::type(int num)
{
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


void QScanner::setBufferSize (int bytes)
   {
   if (!mOpenOk || mOptionBufferSize == -1)
      return;

   const SANE_Option_Descriptor *desc =
      do_sane_get_option_descriptor (mDeviceHandle, mOptionBufferSize);
   if (!desc || desc->type != SANE_TYPE_INT)
      return;

   if (desc->constraint_type == SANE_CONSTRAINT_RANGE
       && desc->constraint.range)
      {
      if (bytes < desc->constraint.range->min)
         bytes = desc->constraint.range->min;
      if (bytes > desc->constraint.range->max)
         bytes = desc->constraint.range->max;
      }
   SANE_Word v = bytes;
   SANE_Int info = 0;
   do_sane_control_option (mDeviceHandle, mOptionBufferSize,
                           SANE_ACTION_SET_VALUE, &v, &info);
   }


/* Cached pointer to libsane's optional sane_read_dup. dlsym(RTLD_DEFAULT)
 * returns the symbol exported by the patched libsane.so if loaded; otherwise
 * we just don't have it and the duplex-progressive path is skipped. */
typedef SANE_Status (*sane_read_dup_fn) (SANE_Handle, SANE_Byte *, SANE_Byte *,
                                         SANE_Int, SANE_Int *, SANE_Int *);

static sane_read_dup_fn lookup_read_dup (void)
   {
   static sane_read_dup_fn fn = NULL;
   static bool tried = false;
   if (!tried)
      {
#ifndef Q_OS_WIN
      fn = (sane_read_dup_fn) dlsym (RTLD_DEFAULT, "sane_read_dup");
#endif
      tried = true;
      }
   return fn;
   }


bool QScanner::hasReadDup (void)
   {
   /* PAPERMAN_NO_READ_DUP=1 reads duplex the ordinary way, side by side,
      for comparison */
   if (getenv ("PAPERMAN_NO_READ_DUP"))
      return false;
   return !mNoReadDup && lookup_read_dup () != NULL;
   }


SANE_Status QScanner::readDup (SANE_Byte *front_buf, SANE_Byte *back_buf,
                               SANE_Int max_len,
                               SANE_Int *front_len, SANE_Int *back_len)
   {
   sane_read_dup_fn fn = lookup_read_dup ();
   SANE_Status status;

   if (!fn)
      return SANE_STATUS_UNSUPPORTED;
   status = fn (mDeviceHandle, front_buf, back_buf, max_len, front_len,
                back_len);
   /* libsane's dispatcher answers for back ends that lack the entry point,
      so the scan must carry on through sane_read() */
   if (status == SANE_STATUS_UNSUPPORTED)
      mNoReadDup = true;
   return status;
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
   if (!mOpenOk || !mDeviceHandle)
      return SANE_STATUS_INVAL;
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
/* Most back ends, the fujitsu and finet ones among them, have a single
   resolution for both directions rather than one for each, so that is
   the vertical resolution too */
int QScanner::yResolutionDpi()
{
  SANE_Word val;
  int onum;
  onum = 0;
  val = 0;
  onum = yResolutionOption();
  if (!onum)
     return xResolutionDpi ();
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
void QScanner::setOptionsByName(QMap <QString,QString> omap,
                                bool allowTransport)
{
  if (!allowTransport)
    {
    foreach (const QString &name, omap.keys ())
      if (isTransportOption (name))
        omap.remove (name);
    }
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
           /* as on the command line: yes/no, true/false, on/off or 1/0 */
           qs = it.value().trimmed().toLower();
           sw = qs == "yes" || qs == "true" || qs == "on" || qs.toInt();
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
    if(isOptionActive(i) && isOptionSettable(i)
       && !isTransportOption(getOptionName(i)))
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
           fname2.replace(QRegularExpression("[/:]"),"_");
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
       QMessageBox::warning(nullptr,
                    QObject::tr("Warning - Insecure password file"),
                    QObject::tr("<html>A password file with insecure "
                    "permissions has been found. You should change the "
                    "permissions of the file<br>"
                    "<center><code><b>~/.sane/pass</b></code></center><br>"
                    "to 0600 or stricter. That means, that only the "
                    "owner may have read/write permission. If you don't "
                    "change the permissions, you will be prompted for your "
                    "username and password.</html>"),
                    QMessageBox::Ok);
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
    mainlayout->setContentsMargins(12, 12, 12, 12);
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
   QList<int> found;

   /* Asking the scanner for every option's name to find one by name is
      dear: a scanner has a hundred or so, a scan looks several of them
      up, and with the back end's debugging turned on each question
      fills a line of the log. Ask once and remember where they are.
      Which options are settable changes as the scan goes on, so that
      is still asked each time, but only of the options with the right
      name */
   if (mOptionByName.isEmpty ())
   {
      int count = optionCount ();

      for (int i = 0; i < count; i++)
      {
         const char *name = getOptionName (i);

         if (name)
            mOptionByName.insert (QString::fromLatin1 (name), i);
      }
   }

   const QList<int> nums = mOptionByName.values (QString::fromLatin1 (opt));

   for (int i : nums)
      if (!settable || isOptionSettable (i))
         found << i;

   if (found.size() == 1)
      return found[0];
   else if (found.size() > 1)
      qDebug() << "Found two options named" << opt << ": " << found;

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
  // qDebug() << "scan option" << mOptionButton[BUT_scan];


  // function number (for Fujitsu)
  mOptionFunction = findOption ("function", false);

  // double-feed hardware status (for Fujitsu)
  mOptionDoubleFeed = findOption ("double-feed", false);

  // Patched fujitsu backend exposes "buffer-size" as a runtime option.
  // 4 KB is the largest size that still streams progressively from
  // the fi-8170 during the physical scan. Larger sizes (8 KB and up
  // empirically; 32 KB confirmed) push the scanner into a buffer-
  // then-dump mode where readDup() blocks until the page has fully
  // transited and the preview stays blank until then. See
  // SCAN_PREVIEW_INVESTIGATION.md.
  /* A modest chunk shows the page filling in as it arrives. 4 KB was far
     too small once the backend delivered JPEG: hundreds of reads a side,
     each resuming the decoder and sending progress, cost more than the
     scan itself and paperman fell well behind a USB scanner that was
     delivering at full speed. 256 KB still gives a few updates a side */
  mOptionBufferSize = findOption ("buffer-size");
  if (mOptionBufferSize != -1)
    setBufferSize (256 * 1024);
}


int QScanner::checkButtons (void)
{
   int value, i;

   value = 0;
   for (i = 0; i < BUT_count; i++) {
       if (mOptionButton [i] != -1) {
           int val = saneWordValue (mOptionButton [i]);

           // qDebug() << "val" << mOptionButton [i] << Qt::hex << val;
           if (val == INT_MIN)
               return INT_MIN;
           else if (val)
               value |= 1 << i;
           // qDebug() << "value" << Qt::hex << value;
       }
   }

   // function button can emulate the others when 'scan' is pressed
   if (mOptionFunction != -1 && (value & 1))
   {
      int option = saneWordValue (mOptionFunction);

      // qDebug() << "option" << Qt::hex << option;
      if (option)
         value |= 1 << option;
   }
   // qDebug() << "buttons" << Qt::hex << value;

   return value;
}


bool QScanner::isScanning (void)
{
   return mScanning;
}


bool QScanner::checkDoubleFeed (void)
{
   if (mOptionDoubleFeed == -1)
      return false;

   int val = saneWordValue (mOptionDoubleFeed);

   return val > 0;
}


void QScanner::useFastTransfer (const QStringList &except)
{
   QMap<QString, QString> opts;
   int num;
   bool stats = getenv ("PAPERMAN_SCAN_STATS") != NULL;

   if (findOption ("buffermode") != -1 && !except.contains ("buffermode"))
      opts ["buffermode"] = "On";
   num = findOption ("compression");
   if (num != -1 && !except.contains ("compression"))
      {
      int mode = findOption ("mode", false);
      QString cur = saneStringValue (num);

      /* only in colour, where the option is active, and only from the
         backend's own default, so a choice of None in the dialog holds */
      if (mode != -1 && saneStringValue (mode) == "Color"
          && (cur.isEmpty () || cur == "None"))
         opts ["compression"] = "JPEG";
      }
   if (!opts.isEmpty ())
      setOptionsByName (opts);
   if (stats)
      {
      QStringList report;

      foreach (const char *name, QList<const char *> () << "mode"
               << "buffermode" << "compression")
         {
         int n = findOption (name, false);

         if (n != -1)
            report << QString ("%1=%2").arg (name).arg (saneStringValue (n));
         }
      qWarning ("fast transfer: set %s; backend now has %s; read_dup %s",
                qPrintable (QStringList (opts.keys ()).join (",")),
                qPrintable (report.join (" ")),
                hasReadDup () ? "in use" : "not used");
      }
}


int QScanner::imagesWaiting (void)
{
   int num = findOption ("images-waiting", false);

   if (num == -1)
      return -1;
   return saneWordValue (num);
}


bool QScanner::stopFeed (void)
{
   int num = findOption ("stop-feed");

   if (num == -1 || getOptionType (num) != SANE_TYPE_BUTTON)
      return false;

   SANE_Int info;
   SANE_Status status = do_sane_control_option (mDeviceHandle, num,
                                                SANE_ACTION_SET_VALUE, 0L,
                                                &info);

   return status == SANE_STATUS_GOOD;
}


/* Every call into libsane goes through these, so that there is one place
   to look for what paperman asks of a scanner */

SANE_Status QScanner::do_sane_open (SANE_String_Const name, SANE_Handle *handle)
   {
   return sane_open (name, handle);
   }


SANE_Status QScanner::do_sane_control_option (SANE_Handle handle,
                                              SANE_Int option,
                                              SANE_Action action, void *val,
                                              SANE_Int *info)
   {
   return sane_control_option (handle, option, action, val, info);
   }


const SANE_Option_Descriptor *QScanner::do_sane_get_option_descriptor
      (SANE_Handle handle, SANE_Int option)
   {
   return sane_get_option_descriptor (handle, option);
   }


SANE_Status QScanner::do_sane_start (SANE_Handle handle)
   {
   return sane_start (handle);
   }


void QScanner::do_sane_cancel (SANE_Handle handle)
   {
   sane_cancel (handle);
   }


SANE_Status QScanner::do_sane_set_io_mode (SANE_Handle handle, SANE_Bool nbio)
   {
   return sane_set_io_mode (handle, nbio);
   }


SANE_Status QScanner::do_sane_read (SANE_Handle handle, SANE_Byte *buf,
                                    SANE_Int maxlen, SANE_Int *len)
   {
   return sane_read (handle, buf, maxlen, len);
   }


SANE_Status QScanner::do_sane_get_parameters (SANE_Handle handle,
                                              SANE_Parameters *params)
   {
   return sane_get_parameters (handle, params);
   }


void QScanner::do_sane_close (SANE_Handle handle)
   {
   sane_close (handle);
   }
