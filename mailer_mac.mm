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

#include <QString>

#import <AppKit/AppKit.h>

/** give a file to the mail program, through the share menu's service for
    writing an email, which uses the one chosen in Mail's settings

   \param file     the file to attach (full path)
   \param subject  the message's subject
   \returns true if the mail program was asked, false if there is none */
bool mailerComposeMac (const QString &file, const QString &subject)
   {
   @autoreleasepool
      {
      /* the service is reached by name, which macOS 13 deprecates in
         favour of the share menu, though that needs a button to show the
         menu from */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      NSSharingService *service = [NSSharingService
         sharingServiceNamed:NSSharingServiceNameComposeEmail];
#pragma clang diagnostic pop
      NSArray *items = @[[NSURL fileURLWithPath:file.toNSString ()]];

      if (!service || ![service canPerformWithItems:items])
         return false;
      service.subject = subject.toNSString ();
      [service performWithItems:items];
      return true;
      }
   }
