/*
License: GPL-2
*/

#include "clientconf.h"

#include "blockwatch.h"
#include "remotebackend.h"

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

#include <memory>


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


/* Say why no server was found among @candidates, given what went
   wrong with each (null if it never answered) */
static QString probeError(const QList<QUrl> &candidates,
                          const QStringList &why)
{
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
      return QString("no Paperman server found (%1); tried %2")
                .arg(reasons.first(), tried.join(", "));
   }
   return "no Paperman server found; tried " + tried.join(", ");
}


void ClientConfig::probe(const QList<QUrl> &candidates, QObject *context,
                         Found done, int timeoutMs)
{
   /* The state of the probe, shared by the replies' callbacks; the
      manager owns the replies, and goes once the probe settles */
   struct State
   {
      QList<QUrl> candidates;
      QStringList why;   // what went wrong with each, once it has
      QNetworkAccessManager *mgr;
      Found done;
      bool settled = false;
   };
   if (candidates.isEmpty()) {
      QMetaObject::invokeMethod(context, [done]() {
         done(QUrl(), probeError({}, {}));
      }, Qt::QueuedConnection);
      return;
   }

   auto state = std::make_shared<State>();
   state->candidates = candidates;
   state->mgr = new QNetworkAccessManager(context);
   state->done = done;

   /* Finish, with the first in the list that answered as a server; if
      @all, wait for none of those ahead of it, since time is up */
   auto settle = [state](bool all) {
      QUrl chosen;
      for (int i = 0; i < state->candidates.size(); i++) {
         if (state->why[i].isNull() && !all)
            return;   // still waiting on this one
         if (!state->why[i].isNull() && state->why[i].isEmpty()) {
            chosen = state->candidates[i];
            break;
         }
      }
      if (state->settled)
         return;
      state->settled = true;
      qCDebug(logNet).noquote() << "probe chose"
         << (chosen.isEmpty() ? QStringLiteral("none") : chosen.toString());

      /* leave nothing to call back once the probe is done */
      for (QNetworkReply *reply
           : state->mgr->findChildren<QNetworkReply *>()) {
         reply->disconnect();
         reply->abort();
      }
      state->mgr->deleteLater();
      state->done(chosen, chosen.isEmpty()
                              ? probeError(state->candidates, state->why)
                              : QString());
   };

   for (const QUrl &cand : candidates) {
      QUrl status = cand;
      status.setPath("/status");
      QNetworkRequest req(status);
      QNetworkReply *reply = state->mgr->get(req);
      int idx = state->why.size();
      state->why << QString();
      qCDebug(logNet).noquote() << "probe" << status.toString();

      QObject::connect(reply, &QNetworkReply::finished, state->mgr,
                       [state, settle, reply, idx]() {
         /* Any HTTP answer means something is listening, so say
            whether it is a Paperman server rather than how the request
            failed */
         QVariant code = reply->attribute(
                            QNetworkRequest::HttpStatusCodeAttribute);
         if (!code.isValid()) {
            state->why[idx] = reply->errorString();
         } else {
            QJsonObject obj = QJsonDocument::fromJson(reply->readAll())
                                 .object();
            if (code.toInt() == 200
                && obj.value("status").toString() == "running")
               state->why[idx] = "";
            else
               state->why[idx] = "not a Paperman server";
         }
         qCDebug(logNet).noquote() << "probe" << reply->url().toString()
            << (state->why[idx].isEmpty() ? QStringLiteral("answers")
                                          : state->why[idx]);
         settle(false);
      });
   }

   /* Settle as soon as the most likely candidate still in the running
      answers; a later one answering first must wait for those ahead of
      it, which usually fail quickly when nothing is listening, or for
      the time to run out.  Then take the best of those which did
      answer: one ahead of it may simply be slow to be refused, as on
      Windows */
   QTimer::singleShot(timeoutMs, state->mgr, [settle]() { settle(true); });
}


QUrl ClientConfig::probe(const QList<QUrl> &candidates, QString *errorOut,
                         int timeoutMs)
{
   QUrl found;
   QString error;
   bool finished = false;
   QEventLoop loop;

   probe(candidates, &loop, [&](const QUrl &url, const QString &err) {
      found = url;
      error = err;
      finished = true;
      loop.quit();
   }, timeoutMs);
   if (!finished) {
      BlockWatch::note("probe " + (candidates.isEmpty()
                                   ? QString() : candidates.first().host()));
      loop.exec();
   }
   if (found.isEmpty() && errorOut)
      *errorOut = error;
   return found;
}


void ClientConfig::resolve(const QString &arg, QObject *context, Found done)
{
   QList<QUrl> candidates = probeCandidates(arg);
   QString error;

   if (findByName(arg, nullptr) || candidates.isEmpty()) {
      QUrl url = resolve(arg, &error);
      QMetaObject::invokeMethod(context, [done, url, error]() {
         done(url, error);
      }, Qt::QueuedConnection);
      return;
   }
   probe(candidates, context, done);
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
