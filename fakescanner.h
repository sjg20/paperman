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

/* Offering the fake Fujitsu scanner in test/fakescan to libsane

   The fake scanner is a SANE back end which is built with the tests.
   libsane's dll back end finds back ends through its configuration, so
   offering the fake one means giving libsane a configuration which
   lists it. That is read by sane_init(), so it must be done first */

#ifndef FAKESCANNER_H
#define FAKESCANNER_H

#include <QString>

/** \returns where the fake scanner is built: in test/fakescan beside
             this program */
QString fakescanLibDir (void);

/** \returns the directory libsane reads its configuration from unless
             told otherwise, or an empty string if there is none */
QString fakescanSaneConfigDir (void);

/** Offer the fake scanner to libsane

    \param libdir    where the fake scanner is built
    \param dir       directory through which a person drives it, see
                     test/fakescan/fakescan.h, or empty for none
    \param sysconf   libsane's own configuration directory, to offer the
                     fake scanner beside the scanners that lists, or
                     empty to offer it alone, as the tests want
    \returns an empty string if OK, else what went wrong */
QString fakescanOffer (const QString &libdir, const QString &dir,
                       const QString &sysconf);

#endif // FAKESCANNER_H
