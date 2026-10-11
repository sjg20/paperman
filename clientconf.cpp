/*
License: GPL-2
*/

#include "clientconf.h"

#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>


QString ClientConfig::configPath()
{
   QByteArray override = qgetenv("PAPERMAN_CONFIG");
   if (!override.isEmpty())
      return QString::fromLocal8Bit(override);

   QString d = QStandardPaths::writableLocation(
                  QStandardPaths::GenericConfigLocation)
               + "/paperman";
   return d + "/client.conf";
}


/* Canonical form used to decide whether two config lines name the same
   server.  QUrl::matches() leaves an empty path and "/" distinct, so
   http://host:8080 and http://host:8080/ would both get listed; compare
   on a normalised string instead. */
static QString urlKey(const QUrl &url)
{
   QUrl u = url.adjusted(QUrl::NormalizePathSegments
                         | QUrl::StripTrailingSlash);

   /* StripTrailingSlash leaves a lone root "/" in place, so
      http://host:8080/ and http://host:8080 still differ; drop it. */
   if (u.path() == QLatin1String("/"))
      u.setPath(QString());

   return u.toString(QUrl::FullyEncoded).toLower();
}


/* Turn one non-comment line into an entry.  Returns false (leaving
   @entry untouched) when the URL part is empty or doesn't parse. */
static bool parseLine(const QString &line, ServerEntry *entry)
{
   QString name;
   QString urlText = line;

   /* An '=' before the URL names the server.  URLs never contain a
      bare '=' before the authority, so the first one is the separator. */
   int eq = line.indexOf('=');
   if (eq >= 0) {
      name = line.left(eq).trimmed();
      urlText = line.mid(eq + 1).trimmed();
   }

   if (urlText.isEmpty())
      return false;

   QUrl url(urlText);
   if (!url.isValid() || url.scheme().isEmpty())
      return false;

   entry->url = url;
   entry->name = name.isEmpty() ? url.host() : name;
   return true;
}


QList<ServerEntry> ClientConfig::load(QStringList *badLines)
{
   QList<ServerEntry> entries;

   QFile f(configPath());
   if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
      return entries;   // missing file is simply an empty directory

   QTextStream in(&f);
   while (!in.atEnd()) {
      QString line = in.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#'))
         continue;

      ServerEntry entry;
      if (parseLine(line, &entry))
         entries.append(entry);
      else if (badLines)
         badLines->append(line);
   }
   return entries;
}


bool ClientConfig::sameServer(const QUrl &a, const QUrl &b)
{
   return urlKey(a) == urlKey(b);
}


bool ClientConfig::findByName(const QString &name, ServerEntry *out)
{
   const QList<ServerEntry> entries = load();
   for (const ServerEntry &e : entries) {
      if (e.name.compare(name, Qt::CaseInsensitive) == 0) {
         if (out)
            *out = e;
         return true;
      }
   }
   return false;
}


bool ClientConfig::add(const QUrl &url, const QString &name,
                       QString *errorOut, bool *added)
{
   if (added)
      *added = false;

   /* Already listed?  Compare on the normalised URL so trailing-slash
      and case differences in the host don't create a duplicate. */
   const QList<ServerEntry> entries = load();
   for (const ServerEntry &e : entries) {
      if (sameServer(e.url, url))
         return true;
   }

   QString path = configPath();
   QDir().mkpath(QFileInfo(path).absolutePath());

   QFile f(path);
   if (!f.open(QIODevice::Append | QIODevice::Text)) {
      if (errorOut)
         *errorOut = f.errorString();
      return false;
   }

   QTextStream out(&f);
   if (added)
      *added = true;
   if (name.isEmpty())
      out << url.toString() << '\n';
   else
      out << name << " = " << url.toString() << '\n';
   return true;
}


QList<QUrl> ClientConfig::probeCandidates(const QString &arg)
{
   QList<QUrl> found;

   /* Anything with a scheme is a URL already.  Look for "://" rather
      than asking QUrl, which reads "host:8080" as scheme "host". */
   if (arg.isEmpty() || arg.contains("://"))
      return found;

   QUrl url("http://" + arg);
   if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()
       || url.hasQuery() || url.hasFragment()
       || (!url.path().isEmpty() && url.path() != "/"))
      return found;
   url.setPath(QString());

   if (url.port() != -1) {
      found << url;
      url.setScheme("https");
      found << url;
   } else {
      url.setPort(8080);
      found << url;
      url.setPort(8081);
      found << url;
      url.setPort(-1);
      url.setScheme("https");
      found << url;
   }
   return found;
}


QUrl ClientConfig::probe(const QList<QUrl> &candidates, QString *errorOut,
                         int timeoutMs)
{
   QNetworkAccessManager mgr;
   QEventLoop loop;
   QList<QNetworkReply *> replies;
   QStringList why;   // what went wrong with each, once it has
   QUrl chosen;

   /* Settle as soon as the most likely candidate still in the running
      answers; a later one answering first must wait for those ahead of
      it, which usually fail quickly when nothing is listening, or for
      the time to run out. */
   auto settle = [&]() {
      for (int i = 0; i < candidates.size(); i++) {
         if (why[i].isNull())
            return;   // still waiting on this one
         if (why[i].isEmpty()) {
            chosen = candidates[i];
            break;
         }
      }
      loop.quit();
   };

   for (const QUrl &cand : candidates) {
      QUrl status = cand;
      status.setPath("/status");
      QNetworkRequest req(status);
      QNetworkReply *reply = mgr.get(req);
      int idx = replies.size();
      replies << reply;
      why << QString();

      QObject::connect(reply, &QNetworkReply::finished, &loop,
                       [&, reply, idx]() {
         /* Any HTTP answer means something is listening, so say
            whether it is a Paperman server rather than how the request
            failed */
         QVariant code = reply->attribute(
                            QNetworkRequest::HttpStatusCodeAttribute);
         if (!code.isValid()) {
            why[idx] = reply->errorString();
         } else {
            QJsonObject obj = QJsonDocument::fromJson(reply->readAll())
                                 .object();
            if (code.toInt() == 200
                && obj.value("status").toString() == "running")
               why[idx] = "";
            else
               why[idx] = "not a Paperman server";
         }
         settle();
      });
   }

   if (!candidates.isEmpty()) {
      QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
      loop.exec();
   }

   /* Out of time, take the best of those which did answer: one ahead
      of it may simply be slow to be refused, as on Windows */
   for (int i = 0; chosen.isEmpty() && i < candidates.size(); i++) {
      if (!why[i].isNull() && why[i].isEmpty())
         chosen = candidates[i];
   }

   /* Leave nothing to call back into this frame once it is gone */
   for (QNetworkReply *reply : replies) {
      reply->disconnect();
      reply->abort();
   }

   if (chosen.isEmpty() && errorOut) {
      QStringList tried;
      QStringList reasons;
      for (int i = 0; i < candidates.size(); i++) {
         QString reason = why[i].isNull() ? "no answer" : why[i];
         tried << QString("%1 (%2)").arg(candidates[i].toString(), reason);
         reasons << reason;
      }

      /* An unknown host fails the same way everywhere; say so once */
      reasons.removeDuplicates();
      if (reasons.size() == 1 && candidates.size() > 1) {
         tried.clear();
         for (const QUrl &cand : candidates)
            tried << cand.toString();
         *errorOut = QString("no Paperman server found (%1); tried %2")
                        .arg(reasons.first(), tried.join(", "));
      } else {
         *errorOut = "no Paperman server found; tried " + tried.join(", ");
      }
   }
   return chosen;
}


QUrl ClientConfig::resolve(const QString &arg, QString *errorOut)
{
   ServerEntry named;
   if (findByName(arg, &named))
      return named.url;

   QList<QUrl> candidates = probeCandidates(arg);
   if (!candidates.isEmpty())
      return probe(candidates, errorOut);

   QUrl url(arg);
   if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty())
      return url;

   if (errorOut)
      *errorOut = QString("invalid URL: %1").arg(arg);
   return QUrl();
}
