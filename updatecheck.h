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

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/** Asks the Paperman website, at most once a day, which version is the
    latest, so that the user can be told when there is a newer one.

    The request carries the version in use, the operating system, the
    processor and the kind of package, and nothing else: there is no
    identifier, so one installation cannot be told from another. The
    website counts these requests to see roughly how many people use
    Paperman. The check can be turned off in the options.

    A snap or flatpak is kept up to date by its store, so the user is not
    told about a newer version there */
class UpdateCheck : public QObject
   {
   Q_OBJECT

public:
   explicit UpdateCheck (QObject *parent = nullptr);

   //! whether to check, from the settings; on unless turned off
   static bool enabled ();

   //! turn checking on or off, keeping it in the settings
   static void setEnabled (bool enable);

   /** the kind of package paperman is running from

      \returns "snap", "flatpak", "appimage" or "other" */
   static QString packageKind ();

   //! the address asked, with what the request carries
   QUrl requestUrl () const;

   /** tell of a newer version found by an earlier check, then ask again
       if checking is on and the last time was more than about a day ago

      \returns true if a request is sent */
   bool start ();

   //! ask this address instead of the website's, for testing
   void setBaseUrl (const QUrl &url) { _base = url; }

   //! claim to be this version, for testing
   void setVersion (const QString &version) { _version = version; }

signals:
   /** a newer version is available

      \param version  its version, such as "1.5.0"
      \param url      the page to download it from */
   void newer (const QString &version, const QUrl &url);

   //! a request has finished, whatever its outcome
   void done ();

private:
   //! tell of the latest version known, if it is newer than this one
   void tell ();

   //! a reply has come
   void finished (QNetworkReply *reply);

   QNetworkAccessManager *_net;
   QUrl _base;          //!< the address to ask
   QString _version;    //!< the version in use
   };
