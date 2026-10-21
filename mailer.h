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

#include <QString>
#include <QUrl>

/** How stacks are emailed, as the user chooses: by the system's mail
    program, which is given the file to attach, or by Gmail's compose page
    in the web browser, with the file on the clipboard to paste in.

    The mail program is reached through Simple MAPI on Windows, the
    compose service of the share menu on macOS and xdg-email elsewhere.
    Where none will take the file, a message is started without it, for
    the user to paste the file into. In each case the message is left for
    the user to address and send */
class Mailer
   {
public:
   enum e_how
      {
      How_program,     //!< the system's mail program
      How_gmail,       //!< Gmail's compose page in the web browser

      How_count
      };

   //! how the user chose, from the settings
   static e_how how ();

   /** choose how, keeping it in the settings

      \param how  the way to email */
   static void setHow (e_how how);

   //! the name of a way for the settings, and back
   static QString howName (e_how how);
   static e_how howFromName (const QString &name);

   /** start a message with a file attached, as the user chose

      The file must be on the clipboard already, ready to paste in where
      it cannot be attached.

      \param file     the file to attach (full path)
      \param subject  the message's subject */
   static void compose (const QString &file, const QString &subject);

   //! for tests: where to record each URL opened, rather than opening it
   static QUrl *url_capture;

   //! for tests: where to record the file given to the mail program
   static QString *file_capture;

private:
   /** give the file to the system's mail program

      \returns true if a mail program was asked, false if there is none
               to ask. On Windows the mail program is asked in the
               background, and if it fails then, a message is started
               without the file, as composeWithoutFile() does */
   static bool composeWithProgram (const QString &file,
                                   const QString &subject);

   //! start a message without the file, through the mailto: handler
   static void composeWithoutFile (const QString &subject);

   //! start a message on Gmail's compose page in the web browser
   static void composeWithGmail (const QString &subject);

   //! open a URL, or record it for a test
   static void open (const QUrl &url);
   };
