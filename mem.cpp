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


#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "mem.h"


void mem_check (void)
{
    void *ptr;
    int i;

    for (i = 0; i < 50; i++)
    {
        ptr = malloc (rand () % 100000);
        free (ptr);
    }
}


err_info *mem_alloc (void **ptr, int size, const char *func_name)
   {
   *ptr = NULL;
   if (size > 0)
      *ptr = malloc (size);
   if (size != 0 && !*ptr)
      return err_make (func_name, ERR_out_of_memory_bytes1, size);
   return NULL;
   }


err_info *mem_realloc (void **ptr, int newsize, const char *func_name)
   {
   if (newsize > 0)
      *ptr = realloc (*ptr, newsize);
   if (newsize != 0 && !*ptr)
      return err_make (func_name, ERR_out_of_memory_bytes1, newsize);
   return NULL;
   }


err_info *mem_allocz (void **ptr, int size, const char *func_name)
   {
   CALL (mem_alloc (ptr, size, func_name));
   memset (*ptr, '\0', size);
   return NULL;
   }


void mem_free (void **ptr)
   {
   free (*ptr);
   *ptr = NULL;
   }


err_info *mem_str (void **ptr, const char *str, const char *func_name)
   {
   CALL (mem_alloc (ptr, strlen (str) + 1, func_name));
   strcpy ((char *)*ptr, str);
   return NULL;
   }


err_info *mem_restr (void **ptr, const char *str, const char *func_name)
   {
   CALL (mem_realloc (ptr, strlen (str) + 1, func_name));
   strcpy ((char *)*ptr, str);
   return NULL;
   }

