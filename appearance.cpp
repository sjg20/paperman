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

#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QWidget>

#include "appearance.h"
#include "utils.h"

static const char *const mode_names[Appearance::Mode_count] =
   {
   "system", "light", "dark"
   };


Appearance::Appearance (QObject *parent)
   : QObject (parent)
   {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
   connect (QGuiApplication::styleHints (), &QStyleHints::colorSchemeChanged,
            this, &Appearance::desktopChanged);
#endif
   }


Appearance *Appearance::instance ()
   {
   static Appearance *appearance;

   if (!appearance)
      appearance = new Appearance (qApp);
   return appearance;
   }


QString Appearance::modeName (e_mode mode)
   {
   return mode >= 0 && mode < Mode_count ? mode_names[mode] : mode_names[0];
   }


Appearance::e_mode Appearance::modeFromName (const QString &name)
   {
   for (int i = 0; i < Mode_count; i++)
      if (name == mode_names[i])
         return (e_mode)i;
   return Mode_system;
   }


Appearance::e_mode Appearance::mode ()
   {
   return modeFromName (QSettings ().value ("look/appearance").toString ());
   }


void Appearance::setMode (e_mode mode)
   {
   QSettings ().setValue ("look/appearance", modeName (mode));
   apply ();
   }


void Appearance::apply ()
   {
   instance ()->update ();
   }


bool Appearance::desktopWantsDark (bool &known)
   {
   known = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
   switch (QGuiApplication::styleHints ()->colorScheme ())
      {
      case Qt::ColorScheme::Dark:
         return true;
      case Qt::ColorScheme::Light:
         return false;
      default:
         break;
      }
#elif defined (Q_OS_LINUX)
   /* Qt cannot say, so ask GNOME, whose setting other desktops often set
      too; it is 'default' when there is no preference */
   static int wants = -1;

   if (wants == -1 && !QStandardPaths::findExecutable ("gsettings").isEmpty ())
      {
      QProcess proc;

      proc.start ("gsettings", QStringList () << "get"
                  << "org.gnome.desktop.interface" << "color-scheme");
      wants = 0;
      if (proc.waitForFinished (2000))
         {
         QByteArray out = proc.readAllStandardOutput ();

         if (out.contains ("prefer-dark"))
            wants = 2;
         else if (out.contains ("prefer-light"))
            wants = 1;
         }
      }
   if (wants > 0)
      return wants == 2;
#endif
   known = false;
   return false;
   }


QPalette Appearance::palette (bool dark)
   {
   struct Colours
      {
      const char *window, *text, *base, *alt_base, *button, *light,
                 *midlight, *mid, *shadow_dark, *shadow, *highlight,
                 *link, *tooltip, *tooltip_text, *placeholder, *disabled;
      };
   static const Colours light_colours =
      {
      "#efefef", "#000000", "#ffffff", "#f7f7f7", "#efefef", "#ffffff",
      "#cacaca", "#b8b8b8", "#9f9f9f", "#767676", "#308cc6",
      "#0000ff", "#ffffdc", "#000000", "#7f7f7f", "#bebebe"
      };
   static const Colours dark_colours =
      {
      "#323232", "#f0f0f0", "#242424", "#2c2c2c", "#3c3c3c", "#505050",
      "#464646", "#282828", "#1e1e1e", "#141414", "#3d7ad6",
      "#6fa8ff", "#3c3c3c", "#f0f0f0", "#909090", "#7f7f7f"
      };
   const Colours &c = dark ? dark_colours : light_colours;
   QPalette pal (QColor (c.button), QColor (c.window));

   pal.setColor (QPalette::Window, QColor (c.window));
   pal.setColor (QPalette::WindowText, QColor (c.text));
   pal.setColor (QPalette::Base, QColor (c.base));
   pal.setColor (QPalette::AlternateBase, QColor (c.alt_base));
   pal.setColor (QPalette::Text, QColor (c.text));
   pal.setColor (QPalette::Button, QColor (c.button));
   pal.setColor (QPalette::ButtonText, QColor (c.text));
   pal.setColor (QPalette::BrightText, QColor ("#ff3030"));
   pal.setColor (QPalette::Light, QColor (c.light));
   pal.setColor (QPalette::Midlight, QColor (c.midlight));
   pal.setColor (QPalette::Mid, QColor (c.mid));
   pal.setColor (QPalette::Dark, QColor (c.shadow_dark));
   pal.setColor (QPalette::Shadow, QColor (c.shadow));
   pal.setColor (QPalette::Highlight, QColor (c.highlight));
   pal.setColor (QPalette::HighlightedText, QColor ("#ffffff"));
   pal.setColor (QPalette::Link, QColor (c.link));
   pal.setColor (QPalette::LinkVisited, QColor (c.link).darker (dark ? 80 : 130));
   pal.setColor (QPalette::ToolTipBase, QColor (c.tooltip));
   pal.setColor (QPalette::ToolTipText, QColor (c.tooltip_text));
   pal.setColor (QPalette::PlaceholderText, QColor (c.placeholder));
   for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text,
                                    QPalette::ButtonText})
      pal.setColor (QPalette::Disabled, role, QColor (c.disabled));
   return pal;
   }


QString Appearance::styleName ()
   {
   QStyle *style = QApplication::style ();

   if (!style)
      return QString ();
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
   return style->name ();
#else
   // the name QStyleFactory knows it by
   return style->objectName ();
#endif
   }


void Appearance::restore ()
   {
   if (!_overriding)
      return;
   QApplication::setStyle (_style);

   /* back to the desktop's colours, as they are now rather than as they
      were, since the desktop may have changed between light and dark */
   QApplication::setPalette (QPalette ());
   _overriding = false;
   }


void Appearance::update ()
   {
   if (!_saved)
      {
      _style = styleName ();
      _saved = true;
      }
   restore ();

   bool known;
   bool want_dark;

   switch (mode ())
      {
      case Mode_light:
         want_dark = false;
         break;
      case Mode_dark:
         want_dark = true;
         break;
      default:
         /* the desktop's theme may be dark whatever Qt says it asks for,
            so only turn dark, as when a snap cannot reach the theme */
         want_dark = (desktopWantsDark (known) && known) || utilIsDarkMode ();
         break;
      }

   if (want_dark != utilIsDarkMode ())
      {
      /* Fusion takes any colours, where a native style may draw some
         parts in its own */
      QApplication::setStyle (QStyleFactory::create ("Fusion"));
      QApplication::setPalette (palette (want_dark));
      _overriding = true;
      }

   /* a style sheet which names a colour of the palette keeps the colour it
      had when the widget was polished, so polish those widgets again */
   for (QWidget *w : QApplication::allWidgets ())
      if (!w->styleSheet ().isEmpty ())
         {
         w->style ()->unpolish (w);
         w->style ()->polish (w);
         w->update ();
         }
   qCDebug (logBuild).noquote ()
      << QString ("look: appearance %1, %2%3")
         .arg (modeName (mode ()))
         .arg (utilIsDarkMode () ? "dark" : "light")
         .arg (_overriding ? ", Fusion with paperman's colours" : "");
   }


void Appearance::desktopChanged ()
   {
   if (mode () == Mode_system)
      update ();
   }
