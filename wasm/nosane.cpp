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
*/

/* A browser has no scanner library, so this stands in for one: it is a
   SANE library with no scanners. Everything which uses SANE then builds
   and runs as it is, and finds nothing to scan with. A real one, reaching
   a scanner through the browser, would take this file's place */

#include "sane/sane.h"

extern "C" {

SANE_Status sane_init (SANE_Int *version_code, SANE_Auth_Callback)
   {
   if (version_code)
      *version_code = SANE_VERSION_CODE (1, 0, 0);
   return SANE_STATUS_GOOD;
   }


void sane_exit (void)
   {
   }


SANE_Status sane_get_devices (const SANE_Device ***device_list, SANE_Bool)
   {
   static const SANE_Device *none [] = { nullptr };

   if (device_list)
      *device_list = none;
   return SANE_STATUS_GOOD;
   }


SANE_Status sane_open (SANE_String_Const, SANE_Handle *)
   {
   return SANE_STATUS_INVAL;
   }


void sane_close (SANE_Handle)
   {
   }


const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle,
                                                          SANE_Int)
   {
   return nullptr;
   }


SANE_Status sane_control_option (SANE_Handle, SANE_Int, SANE_Action, void *,
                                 SANE_Int *)
   {
   return SANE_STATUS_UNSUPPORTED;
   }


SANE_Status sane_get_parameters (SANE_Handle, SANE_Parameters *)
   {
   return SANE_STATUS_INVAL;
   }


SANE_Status sane_start (SANE_Handle)
   {
   return SANE_STATUS_INVAL;
   }


SANE_Status sane_read (SANE_Handle, SANE_Byte *, SANE_Int, SANE_Int *length)
   {
   if (length)
      *length = 0;
   return SANE_STATUS_EOF;
   }


void sane_cancel (SANE_Handle)
   {
   }


SANE_Status sane_set_io_mode (SANE_Handle, SANE_Bool)
   {
   return SANE_STATUS_UNSUPPORTED;
   }


SANE_Status sane_get_select_fd (SANE_Handle, SANE_Int *)
   {
   return SANE_STATUS_UNSUPPORTED;
   }


SANE_String_Const sane_strstatus (SANE_Status status)
   {
   return status == SANE_STATUS_GOOD ? "Success" : "No scanner here";
   }

}
