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


struct err_info;


/** allocate and zero memory: *ptr = malloc (size) */
struct err_info *mem_allocz (void **ptr, int size, const char *func_name);
struct err_info *mem_realloc (void **ptr, int newsize, const char *func_name);
struct err_info *mem_alloc (void **ptr, int size, const char *func_name);
void mem_free (void **ptr);

/** allocate memory for string: *ptr = strdup (str) */
struct err_info *mem_str (void **ptr, const char *str, const char *func_name);

/** reallocate memory for string: free (*ptr); *ptr = strdup (str) */
err_info *mem_restr (void **ptr, const char *str, const char *func_name);
