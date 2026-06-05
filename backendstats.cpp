/*
License: GPL-2
*/

#include "backendstats.h"


void BackendStats::setUrl(const QString &url)
{
   if (_url == url)
      return;
   _url = url;
   emit changed();
}


void BackendStats::reset()
{
   _sent = 0;
   _received = 0;
   _active = 0;
   emit changed();
}


void BackendStats::requestStarted()
{
   _active++;
   emit changed();
}


void BackendStats::requestFinished()
{
   if (_active > 0)
      _active--;
   emit changed();
}


void BackendStats::recordSent(qint64 bytes)
{
   if (bytes <= 0)
      return;
   _sent += bytes;
   emit changed();
}


void BackendStats::recordReceived(qint64 bytes)
{
   if (bytes <= 0)
      return;
   _received += bytes;
   emit changed();
}
