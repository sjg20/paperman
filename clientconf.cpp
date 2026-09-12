/*
License: GPL-2
*/

#include "clientconf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>


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
