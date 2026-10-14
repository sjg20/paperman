/*
License: GPL-2
*/

#include <QUrlQuery>

#include "backendstats.h"

//! how many of the latest times to an answer responseMs() averages
static const int kResponses = 20;


int BackendStats::activeRequests(Kind kind) const
{
   int count = 0;

   for (const Request &request : _requests)
      count += request.kind == kind;
   return count;
}


qint64 BackendStats::bytesToCome() const
{
   qint64 bytes = 0;

   for (const Request &request : _requests)
      if (request.total >= 0)
         bytes += qMax(request.total - request.received, qint64(0));
   return bytes;
}


int BackendStats::lengthsUnknown() const
{
   int count = 0;

   for (const Request &request : _requests)
      count += request.total < 0;
   return count;
}


qint64 BackendStats::longestWaitMs() const
{
   qint64 longest = 0;

   for (const Request &request : _requests)
      longest = qMax(longest, request.started.elapsed());
   return longest;
}


qint64 BackendStats::responseMs() const
{
   if (_responses.isEmpty())
      return -1;

   qint64 sum = 0;

   for (qint64 ms : _responses)
      sum += ms;
   return sum / _responses.size();
}


int BackendStats::eventStreams() const
{
   int count = 0;

   for (bool connected : _streams)
      count += connected;
   return count;
}


int BackendStats::eventStreamsDown() const
{
   return _streams.size() - eventStreams();
}


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
   _responses.clear();
   _failures = 0;
   _lastError.clear();
   emit changed();
}


quint64 BackendStats::requestStarted(Kind kind)
{
   quint64 id = _nextId++;
   Request &request = _requests[id];

   request.kind = kind;
   request.started.start();
   emit changed();
   return id;
}


void BackendStats::requestAnswered(quint64 id)
{
   auto it = _requests.find(id);

   if (it == _requests.end() || it->answered)
      return;
   it->answered = true;
   _responses.append(it->started.elapsed());
   if (_responses.size() > kResponses)
      _responses.removeFirst();
   emit changed();
}


void BackendStats::requestProgress(quint64 id, qint64 received,
                                   qint64 total)
{
   auto it = _requests.find(id);

   if (it == _requests.end())
      return;
   if (received > it->received) {
      _received += received - it->received;
      it->received = received;
   }
   it->total = total;
   emit changed();
}


void BackendStats::requestFinished(quint64 id, const QString &error)
{
   _requests.remove(id);
   if (!error.isEmpty()) {
      _failures++;
      _lastError = error;
   }
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


void BackendStats::setEventStream(const QString &repo, bool connected)
{
   auto it = _streams.find(repo);

   if (it != _streams.end() && *it == connected)
      return;
   _streams[repo] = connected;
   emit changed();
}


BackendStats::Kind BackendStats::kindOf(const QString &path, bool post)
{
   QString route = path.section('?', 0, 0);

   if (route.endsWith("/upload"))
      return Upload;
   if (post)
      return route.startsWith("/v1/auth/") ? Other : Change;
   if (route == "/browse" || route == "/repos")
      return Listing;
   if (route == "/thumbnail") {
      // one asked for in pixels is a page, at the size it is shown
      bool pixels = false;

      QUrlQuery(path.section('?', 1)).queryItemValue("size").toInt(&pixels);
      return pixels ? Page : Thumbnail;
   }
   if (route == "/file" || route.contains("/stacks/"))
      return Page;
   return Other;
}
