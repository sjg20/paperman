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
#ifndef _EPEG_H
#define _EPEG_H

#include <stdio.h>
#include <jpeglib.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

#define snprintf _snprintf
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1

#define MIN(__x,__y) ((__x) < (__y) ? (__x) : (__y))
#define MAX(__x,__y) ((__x) > (__y) ? (__x) : (__y))


typedef enum _Epeg_Colorspace
{
	EPEG_GRAY8,
	EPEG_YUV8,
	EPEG_RGB8,
	EPEG_BGR8,
	EPEG_RGBA8,
	EPEG_BGRA8,
	EPEG_ARGB32,
	EPEG_CMYK
}
Epeg_Colorspace;

typedef struct _epeg_error_mgr *emptr;

struct _epeg_error_mgr
{
	struct     jpeg_error_mgr pub;
	jmp_buf    setjmp_buffer;
    int        last_valid_row;
};

typedef struct _Epeg_Image
{
	struct _epeg_error_mgr          jerr;
	//struct stat                     stat_info;

	unsigned char                  *pixels;
	unsigned char                 **lines;

	char                            scaled : 1;

	int                             error;

	Epeg_Colorspace                 color_space;
    int                             last_valid_row; //.. last row of valid JPEG data received

	struct {
		char                          *file;
		struct {
			unsigned char           **data;
			int                       size;
		} mem;
		int                            w, h;		// width and height of input jpg
		int                            x, y;		// start of zone to extract
		int                            xw, xh;    // width and height of zone to extract
//		char                          *comment;
		FILE                          *f;
		J_COLOR_SPACE                  color_space;
		struct jpeg_decompress_struct  jinfo;
//		struct {
//			char                       *uri;
//			unsigned long long int      mtime;
//			int                         w, h;
//			char                       *mime;
//		} thumb_info;
	} in;
	struct {
		char                        *file;
		struct {
			unsigned char           **data;
			int                      *size;
		} mem;
		int                          w, h;		// width and height of the output jpg
//		char                        *comment;
		FILE                        *f;
		struct jpeg_compress_struct  jinfo;
		int                          quality;
		int                          smoothing;	// double output size and then average.
		char                         thumbnail_info : 1;
	} out;
}
Epeg_Image;

// predeclaration
Epeg_Image *	epeg_file_open(const char *file);
Epeg_Image *
epeg_memory_open(unsigned char *data, int size);
void			epeg_close(Epeg_Image *im);
void			epeg_size_get(Epeg_Image *im, int *w, int *h);
void			epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int xw, int xh, int w, int h);
void			epeg_decode_size_set(Epeg_Image *im, int w, int h);
void			epeg_quality_set(Epeg_Image *im, int quality, int smoothing);
int				epeg_encode(Epeg_Image *im);
int epeg_raw(Epeg_Image *im, int stride);
int epeg_copy(Epeg_Image *im, int width, int height, int stride);
void			epeg_file_output_set(Epeg_Image *im, const char *file);
void
epeg_memory_output_set(Epeg_Image *im, unsigned char **data, int *size);

#endif
