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
#include <QPalette>
#include <QString>

/** Whether paperman is light or dark: as the desktop asks, or always one
    or the other, as the user chooses.

    The desktop's choice comes from Qt where it can say (Qt 6.5 on), which
    covers Windows, macOS, GNOME and KDE, and paperman follows it as it
    changes; with an older Qt on Linux it comes from GNOME's setting, read
    at startup. Where paperman is to look other than the desktop's own
    theme does, it uses the Fusion style with colours of its own, since a
    native style may not take other colours. The icons are drawn in the
    colours in use, so they follow at once */
class Appearance : public QObject
   {
   Q_OBJECT

public:
   enum e_mode
      {
      Mode_system,     //!< as the desktop asks
      Mode_light,      //!< always light
      Mode_dark,       //!< always dark

      Mode_count
      };

   //! the one appearance, created on first use
   static Appearance *instance ();

   //! the mode the user chose, from the settings
   static e_mode mode ();

   /** choose the mode, keeping it in the settings, and look that way now

      \param mode  the mode */
   static void setMode (e_mode mode);

   /** look as the mode says; called at startup, once the application has
       its style and colours */
   static void apply ();

   /** whether the desktop asks for dark

      \param known  returns false if the desktop does not say
      \returns true for dark */
   static bool desktopWantsDark (bool &known);

   //! the colours used for a light or dark look which is not the desktop's
   static QPalette palette (bool dark);

   //! the name of a mode for the settings, and back
   static QString modeName (e_mode mode);
   static e_mode modeFromName (const QString &name);

   //! the name of the style in use, as QStyleFactory knows it
   static QString styleName ();

   //! true while paperman looks other than the desktop's own theme does
   bool overriding () const { return _overriding; }

private:
   explicit Appearance (QObject *parent = nullptr);

   //! look as the mode says
   void update ();

   //! go back to the desktop's own style and colours
   void restore ();

   //! the desktop has changed between light and dark
   void desktopChanged ();

   bool _overriding = false;   //!< using Fusion with our own colours
   QString _style;             //!< the desktop's own style, to go back to
   bool _saved = false;        //!< _style has been saved
   };
