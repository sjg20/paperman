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
/*
   Project:    Maxview
   Author:     Simon Glass
   Copyright:  2001-2009 Bluewater Systems Ltd, www.bluewatersys.com
   File:       err.h
   Started:    2001 sometime

   This file has a enum which holds information on all the different error
   types that maxview can generate, along with a structure to hold both
   the number and text for a particular error. Errors are printf()
   strings so can have additional informative information for the user

   Macros are provided to check for an error and return if one is found,
   or to break out of a loop when an error is detected.
*/

#ifndef __err_h
#define __err_h

#include <stdarg.h>

enum
   {
   ERR_ok,
   ERR_out_of_memory_bytes1,
   ERR_page_number_out_of_range2,
   ERR_invalid_null_name,
   ERR_failed_to_read_bytes1,
   ERR_failed_to_write_bytes1,
   ERR_cannot_open_file1,
   ERR_chunk_at_pos_not_found1,
   ERR_signature_failure1,
   ERR_could_not_find_image_chunk_for_page1,
   ERR_decompression_invalid_data,
   ERR_no_unique_filename1,
   ERR_cannot_process_that_file_format,
   ERR_unknown_file_type,
   ERR_could_not_remove_file2,
   ERR_failed_to_generate_preview_image,
   ERR_operation_cancelled1,
   ERR_could_not_make_temporary_file,
   ERR_could_not_execute1,
   ERR_unknown_pixel_depth1,
   ERR_file_already_exists1,
   ERR_could_not_rename_file2,
   ERR_bermuda_chunk_corrupt1,
   ERR_g4_compression_failed,
   ERR_g4_decompression_failed,
   ERR_could_not_create_directory1,
   ERR_could_not_write_image_to_as2,
   ERR_could_not_read_multipage_images1,
   ERR_could_not_convert_image_to_greyscale1,
   ERR_pdf_file_does_not_contain_images1,
   ERR_directory_not_found1,
   ERR_could_not_rename_dir1,
   ERR_file_position_out_of_range3,
   ERR_unable_to_read_image_header_information1,
   ERR_unable_to_read_preview,
   ERR_no_trash_directory_is_defined,
   ERR_no_scanning_directory_is_defined,
   ERR_scan_cancelled,
   ERR_invalid_annot_block_data,
   ERR_tesseract_not_present2,
   ERR_no_available_for_this_file_type,
   ERR_do_not_have_a_valid_directory_to_scan_into,
   ERR_cannot_open_document_as_it_is_locked1,
   ERR_cannot_find_any_pages_in_source_document1,
   ERR_pdf_creation_error1,
   ERR_file_is_not_open1,
   ERR_ocr_engine_not_present_or_broken2,
   ERR_pdf_previewing_requires_poppler,
   ERR_cannot_close_file1,
   ERR_cannot_add_file_to_zip1,
   ERR_transfer_file_corrupt_on_line2,
   ERR_failed_to_read_username1,
   ERR_failed_to_find_own_username2,
   ERR_lost_access_to_file1,
   ERR_could_not_remove_file1,
   ERR_could_not_copy_file2,
   ERR_cannot_stack_type_onto_type2,
   ERR_pdf_decoder_returned_too_little_data_for_page_expected_got4,
   ERR_cannot_read_pdf_file1,
   ERR_cannot_write_to_envelope_file2,
   ERR_not_supported_in_delivermv,
   ERR_cannot_read_from_envelope_file2,
   ERR_socket_error1,
   ERR_file_not_loaded_yet1,
   ERR_file_type_unsupported1,
   ERR_file_type_only_supports_a_single_page1,
   ERR_cannot_exiftool_error2,
   ERR_directory_is_already_present_as2,
   ERR_directory_could_not_be_added1,
   ERR_directories_and_overlap3,

   ERR_count
   };


typedef struct err_info
   {
   const char *func_name;
   int errnum;
   char errstr [256];
   } err_info;


#define CALL(x)  do { err_info *_e; _e = (x); if (_e) return _e; } while (0)
#define CALLB(x)  { e = (x); if (e) break; }

// cast to void ** for mem_alloc()
#define CV (void **)

err_info *err_subsume (const char *func_name, err_info *err, int errnum, ...);
err_info *err_make (const char *func_name, int errnum, ...);
err_info *err_vmake (const char *func_name, int errnum, va_list ptr);
err_info *err_copy (err_info *err);
int err_systemf (const char *cmd, ...);

#define ERRFN __PRETTY_FUNCTION__

void err_fix_filename (const char *in, char *out);

char *util_bytes_to_user (char *buff, unsigned bytes);

#define UNUSED(c) (c)=(c)

#endif
