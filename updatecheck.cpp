/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QVersionNumber>

#include "config.h"
#include "updatecheck.h"

/* a little less than a day, so that someone who starts paperman at about
   the same time each day is counted each day */
static const qint64 check_secs = 20 * 60 * 60;


UpdateCheck::UpdateCheck (QObject *parent)
   : QObject (parent),
     _net (new QNetworkAccessManager (this)),
     _base ("https://getpaperman.com/latest.json"),
     _version (CONFIG_version_str)
   {
   connect (_net, &QNetworkAccessManager::finished,
            this, &UpdateCheck::finished);
   }


bool UpdateCheck::enabled ()
   {
   return QSettings ().value ("updates/check", true).toBool ();
   }


void UpdateCheck::setEnabled (bool enable)
   {
   QSettings ().setValue ("updates/check", enable);
   }


QString UpdateCheck::packageKind ()
   {
   if (qEnvironmentVariableIsSet ("SNAP"))
      return "snap";
   if (qEnvironmentVariableIsSet ("FLATPAK_ID")
       || QFile::exists ("/.flatpak-info"))
      return "flatpak";
   if (qEnvironmentVariableIsSet ("APPIMAGE"))
      return "appimage";
   return "other";
   }


QUrl UpdateCheck::requestUrl () const
   {
   QUrl url (_base);
   QUrlQuery query;

   query.addQueryItem ("v", _version);
   query.addQueryItem ("os", QSysInfo::productType ());
   query.addQueryItem ("arch", QSysInfo::currentCpuArchitecture ());
   query.addQueryItem ("pkg", packageKind ());
   url.setQuery (query);

   return url;
   }


void UpdateCheck::tell ()
   {
   QSettings qs;
   QString latest = qs.value ("updates/latest").toString ();
   QUrl url = qs.value ("updates/url").toUrl ();
   QString kind = packageKind ();

   // a store keeps these up to date, so there is nothing to do
   if (kind == "snap" || kind == "flatpak")
      return;
   if (latest.isEmpty () || url.isEmpty ())
      return;
   if (QVersionNumber::fromString (latest)
       > QVersionNumber::fromString (_version))
      emit newer (latest, url);
   }


bool UpdateCheck::start ()
   {
   QSettings qs;

   /* what an earlier check found is told at once, once the caller has
      had a chance to connect */
   QTimer::singleShot (0, this, &UpdateCheck::tell);

   if (!enabled ())
      return false;

   QDateTime last = qs.value ("updates/checked").toDateTime ();
   QDateTime now = QDateTime::currentDateTimeUtc ();

   if (last.isValid () && last <= now && last.secsTo (now) < check_secs)
      return false;

   // note the time now, so that a failure is not retried at each start
   qs.setValue ("updates/checked", now);

   QNetworkRequest req (requestUrl ());

   req.setHeader (QNetworkRequest::UserAgentHeader,
                  QString ("Paperman/%1").arg (_version));
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
   req.setTransferTimeout (30000);
#endif
   _net->get (req);

   return true;
   }


void UpdateCheck::finished (QNetworkReply *reply)
   {
   reply->deleteLater ();

   if (reply->error () == QNetworkReply::NoError)
      {
      QJsonObject obj = QJsonDocument::fromJson (reply->readAll ()).object ();
      QVersionNumber latest = QVersionNumber::fromString (
                                    obj.value ("version").toString ());
      QUrl url (obj.value ("url").toString ());

      // the link is offered to the user, so take only a web page
      bool web = url.scheme () == "https"
            || (url.scheme () == "http" && _base.scheme () == "http");

      if (!latest.isNull () && url.isValid () && web)
         {
         QSettings qs;

         qs.setValue ("updates/latest", latest.toString ());
         qs.setValue ("updates/url", url);
         tell ();
         }
      }

   emit done ();
   }
