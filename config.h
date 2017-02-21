/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
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
/** maxview config file

sets up some build-specific things

*/


/* define this to support JPEG compression in the scanner - only later version of SANE do */
#define CONFIG_sane_jpeg  1

// hack for now, as current versions of SANE out there don't have SANE_FRAME_JPEG
// and since it is an enum I'm not sure how to detected it at build time when they do!

#define HACK_SANE_FRAME_JPEG ((SANE_Frame)11)


/* define this to delay directory scanning until a redraw is triggered for a
particular item */
#define CONFIG_delay_dirscan



// this must be a multiple of 8 at the moment due to assumptions in scale_2bpp
// previews are scaled 1:24
#define CONFIG_preview_scale  24


// a larger preview that we probably should try to use...
#define CONFIG_large_preview_scale  16


/** this is the fraction of full colour that the blue RGB value should be
to show a blank page - don't add brackets or you will break the code */
#define CONFIG_preview_col_mult 7 / 8



/** define this to use Nuance's Omnipage OCR. This will require a license
purchase, currently around US400 per user. Please contact the author for
details */
//#define CONFIG_use_omnipage


/** define this to use the poppler library, which allows display of PDF files */
#define CONFIG_use_poppler




// version numbers
#define CONFIG_version_str "1.0"
#define CONFIG_version 100


//! port number to use for deliverymv server
#define CONFIG_port 1968


