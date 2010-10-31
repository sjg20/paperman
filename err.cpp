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


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"

static const char *err_msg [ERR_count] =
   {
   "OK",
   "Out of memory (%d bytes)",
   "Page number %d out of range (0..%d)",
   "Invalid (null) name",
   "Failed to read %d bytes",
   "Failed to write %d bytes",
   "Cannot open file '%s'",
   "Chunk at pos %d not found",
   "Signature failure (%x)",
   "Could not find image chunk for page %d",
   "Decompression - invalid data",
   "Cannot make unique filename from '%s'",
   "Cannot process that file format",
   "Unknown file type",
   "Could not remove file '%s' (%s)",
   "Failed to generate preview image",
   "Operation cancelled (%s)",
   "Could not make temporary file",
   "Could not execute '%s'",
   "Unknown pixel depth (%d)",
   "File '%s' already exists",
   "Could not rename file from '%s' to '%s'",
   "Bermuda chunk corrupt at %x",
   "G4 compression failed",
   "G4 decompression failed",
   "Could not create directory '%s'",
   "Could not write image to '%s' as %s",
   "Could not read multi-page images from file '%s'",
   "Could not convert image '%s' to greyscale",
   "PDF file '%s' does not contain images",
   "Directory '%s' not found",
   "Could not rename directory to '%s'",
   "File position %d out of range (%d..%d)",
   "Unable to read image header information ('%s')",
   "Unable to read preview",
   "No trash directory is defined",
   "No scanning directory is defined",
   "Scanning cancelled",
   "Invalid annot block data",
   "Could not find tesseract: please install package tesseract-ocr (cmd: %s, error: %s)",
   "Not available for this file type",
   "There is no valid directory to scan into",
   "Cannot open document '%s' as it is locked",
   "Cannot find any pages in source document '%s'",
   "PDF creation error: '%s'",
   "File '%s' is not open",
   "OCR engine '%s' not present or broken (Error: %s)",
   "PDF preview features require the poppler-qt4 library - please install this, see config.h and rebuild maxview",
   "Cannot close file '%s'",
   "Cannot add file '%s' to zip file",
   "Transfer file '%s' corrupt on line %d",
   "Failed to read username: uid = %d",
   "Failed to find my own username '%s' in the transfer config file '%s'",
   "Lost access to file '%s' - persistent model index no longer valid",
   "Could not remove file '%s'",
   "Could not copy file from '%s' to '%s'",
   "Cannot stack type '%s' on to type '%s' - please convert first",
   "PDF decoder returned too little image data for '%s' page %d - expected %d but got %d",
   "Cannot read PDF file '%s'",
   "Cannot write to envelope file '%s': %s",
   "Not supported in delivermv",
   "Cannot read from envelope file '%s': %s",
   "Socket error %d",
   "File '%s' not loaded yet"
   };


static err_info static_err;
static err_info static_copy;


err_info *err_copy (err_info *err)
   {
   if (err)
      {
      static_copy = *err;
      return &static_copy;
      }
   return err;
   }


err_info *err_vmake (const char *func_name, int errnum, va_list ptr)
   {
   static_err.func_name = func_name;
   static_err.errnum = errnum;
   vsprintf (static_err.errstr, err_msg [errnum], ptr);
   return &static_err;
   }


err_info *err_make (const char *func_name, int errnum, ...)
   {
   va_list ptr;
   err_info *e;

   va_start (ptr, errnum);
   e = err_vmake (func_name, errnum, ptr);
   va_end (ptr);
   printf ("**Error: %s\n", e->errstr);
   return e;
   }


err_info *err_subsume (const char *func_name, err_info *err, int errnum, ...)
   {
   va_list ptr;
   if (!err)
      return NULL;
   err_info serr, *e;

   serr = *err;
   va_start (ptr, errnum);
   e = err_vmake (func_name, errnum, ptr);
   va_end (ptr);
   strcat (e->errstr, ": ");
   strcat (e->errstr, serr.errstr);
   return e;
   }


int err_systemf (const char *cmd, ...)
   {
   char str [256];
   va_list ptr;

   va_start (ptr, cmd);
   vsprintf (str, cmd, ptr);
   va_end (ptr);
//   printf ("cmd: %s.\n", str);
   return system (str);
   }


/** fix a filename so that it can be passed to a shell:

   - quotes ' */

void err_fix_filename (const char *in, char *out)
   {
   for (; *in; in++)
      {
      if (strchr ("\' &()$", *in))
         *out++ = '\\';
//      if (*in == '$')
//         *out++ == '$';
      *out++ = *in;
      }
   *out = '\0';
   }


/* util_bytes_to_user: convert a number of bytes to a more useful string
representation. Eg 5653 becomes 5k. Similar (I hope) to the way the filer
does it */

char *util_bytes_to_user (char *buff, unsigned bytes)
    {
    if (bytes < 0x1000)
       sprintf (buff, "%4d ", bytes);
    else
       {
       bytes = (bytes + 512) >> 10;
       if (bytes < 0x1000)
          sprintf (buff, "%4dK", bytes);
       else
          {
          bytes = (bytes + 512) >> 10;
          sprintf (buff, "%4dM", bytes);
          }
       }
    return buff;
    }

