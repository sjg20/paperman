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

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QDesktopServices>
#include <QKeySequence>
#include <QSettings>

#include "mailer.h"

#if defined(Q_OS_WIN)
#include <thread>

#include <QDir>
#include <QFileInfo>

#include <windows.h>
#include <mapi.h>
#include <shellapi.h>
#elif defined(Q_OS_MACOS)
bool mailerComposeMac (const QString &file, const QString &subject);
#else
#include <QProcess>
#endif

QUrl *Mailer::url_capture;
QString *Mailer::file_capture;

static const char *how_names[Mailer::How_count] = {"program", "gmail"};


QString Mailer::howName (e_how how)
   {
   return how >= 0 && how < How_count ? how_names[how] : how_names[0];
   }


Mailer::e_how Mailer::howFromName (const QString &name)
   {
   for (int i = 0; i < How_count; i++)
      if (name == how_names[i])
         return (e_how)i;
   return How_program;
   }


Mailer::e_how Mailer::how ()
   {
   return howFromName (QSettings ().value ("email/how").toString ());
   }


void Mailer::setHow (e_how how)
   {
   QSettings ().setValue ("email/how", howName (how));
   }


void Mailer::compose (const QString &file, const QString &subject)
   {
   if (how () == How_gmail)
      composeWithGmail (subject);
   else if (!composeWithProgram (file, subject))
      composeWithoutFile (subject);
   }


void Mailer::open (const QUrl &url)
   {
   if (url_capture)
      *url_capture = url;
   else if (!QDesktopServices::openUrl (url))
      {
#if defined(Q_OS_WIN)
      /* Qt looks in the registry for the command which opens mailto:
         links, which a Store app such as the new Outlook does not put
         there, so let the shell find the app */
      QString link = url.toString (QUrl::FullyEncoded);

      ShellExecuteW (nullptr, L"open",
                     reinterpret_cast<LPCWSTR> (link.utf16 ()), nullptr,
                     nullptr, SW_SHOWNORMAL);
#endif
      }
   }


//! what to say in a message which the file must be pasted into
static QString pasteNote ()
   {
   return QCoreApplication::translate ("Mailer",
            "The file is on the clipboard: paste it with %1")
      .arg (QKeySequence (QKeySequence::Paste)
               .toString (QKeySequence::NativeText));
   }


/** give a URL a query of names and values, with the values fully
    encoded: a '+' left as it is is read as a space by Gmail and by many
    mail programs */
static void setQuery (QUrl &url,
                      const QList<QPair<QString, QString>> &items)
   {
   QStringList parts;

   for (const auto &item : items)
      parts << item.first + "="
               + QString::fromLatin1 (QUrl::toPercentEncoding (item.second));
   url.setQuery (parts.join ('&'), QUrl::StrictMode);
   }


void Mailer::composeWithoutFile (const QString &subject)
   {
   QUrl url ("mailto:");

   setQuery (url, {{"subject", subject}, {"body", pasteNote ()}});
   open (url);
   }


void Mailer::composeWithGmail (const QString &subject)
   {
   /* Gmail's compose page takes no attachment, but the browser treats a
      file pasted into it from the clipboard as one */
   QUrl url ("https://mail.google.com/mail/");

   setQuery (url, {{"view", "cm"}, {"fs", "1"}, {"su", subject},
                   {"body", pasteNote ()}});
   open (url);
   }


#if defined(Q_OS_WIN)

/** the mail program which Simple MAPI uses, as Windows chooses it: the
    user's, else the machine's, or an empty string if there is none.
    Without one, MAPISendMailW() neither fails nor shows anything, but
    waits for ever */
static QString mapiProgram ()
   {
   for (const char *root : {"HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE"})
      {
      QSettings clients (QString ("%1\\Software\\Clients\\Mail").arg (root),
                         QSettings::NativeFormat);
      QString name = clients.value (".").toString ();

      if (!name.isEmpty ())
         return name;
      }
   return QString ();
   }


bool Mailer::composeWithProgram (const QString &file, const QString &subject)
   {
   if (file_capture)
      {
      *file_capture = file;
      return true;
      }

   QString program = mapiProgram ();

   qInfo () << "mail program for MAPI:" << program;
   if (program.isEmpty ())
      return false;

   HMODULE lib = LoadLibraryW (L"mapi32.dll");
   LPMAPISENDMAILW send = nullptr;

   if (lib)
      send = reinterpret_cast<LPMAPISENDMAILW> (
               reinterpret_cast<void *> (GetProcAddress (lib,
                                                         "MAPISendMailW")));
   if (!send)
      {
      if (lib)
         FreeLibrary (lib);
      return false;
      }

   /* MAPISendMailW() waits until the message is sent or put away, so ask
      in a thread of its own. If there is no mail program which takes
      MAPI, start a message without the file instead */
   std::thread ([lib, send, file, subject] () {
      std::wstring path = QDir::toNativeSeparators (file).toStdWString ();
      std::wstring name = QFileInfo (file).fileName ().toStdWString ();
      std::wstring title = subject.toStdWString ();
      MapiFileDescW attach = {};
      MapiMessageW msg = {};

      attach.nPosition = (ULONG)-1;
      attach.lpszPathName = path.data ();
      attach.lpszFileName = name.data ();
      msg.lpszSubject = title.data ();
      msg.nFileCount = 1;
      msg.lpFiles = &attach;

      qInfo () << "asking the mail program to attach" << file;
      ULONG ret = send (0, 0, &msg, MAPI_LOGON_UI | MAPI_DIALOG, 0);

      qInfo () << "the mail program answered" << ret;
      FreeLibrary (lib);
      if (ret != SUCCESS_SUCCESS && ret != MAPI_USER_ABORT)
         {
         qInfo () << "no mail program took the file";
         QMetaObject::invokeMethod (qApp, [subject] () {
            composeWithoutFile (subject);
         });
         }
   }).detach ();

   return true;
   }

#elif defined(Q_OS_MACOS)

bool Mailer::composeWithProgram (const QString &file, const QString &subject)
   {
   if (file_capture)
      {
      *file_capture = file;
      return true;
      }
   return mailerComposeMac (file, subject);
   }

#else

bool Mailer::composeWithProgram (const QString &file, const QString &subject)
   {
   if (file_capture)
      {
      *file_capture = file;
      return true;
      }

   /* xdg-email gives the file to Thunderbird, Evolution, KMail and the
      like; for any other mail program it starts a message without it */
   return QProcess::startDetached ("xdg-email",
                                   {"--subject", subject, "--attach", file});
   }

#endif
