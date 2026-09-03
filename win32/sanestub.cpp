/*
 * Stand-in for libsane on platforms where it is not available (Windows)
 *
 * The scanner code is written against the SANE API throughout, so rather
 * than guard every call site this provides the handful of entry points
 * paperman uses. There are never any devices, so every real operation
 * reports SANE_STATUS_UNSUPPORTED; the built-in simulated scanner in
 * qscanner.cpp bypasses these functions and keeps working.
 */

#include <cstddef>

#include <sane/sane.h>

extern "C"
{

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
   static const SANE_Device *none = NULL;

   *device_list = &none;
   return SANE_STATUS_GOOD;
   }

SANE_Status sane_open (SANE_String_Const, SANE_Handle *handle)
   {
   *handle = NULL;
   return SANE_STATUS_UNSUPPORTED;
   }

void sane_close (SANE_Handle)
   {
   }

const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle,
                                                          SANE_Int)
   {
   return NULL;
   }

SANE_Status sane_control_option (SANE_Handle, SANE_Int, SANE_Action, void *,
                                 SANE_Int *info)
   {
   if (info)
      *info = 0;
   return SANE_STATUS_UNSUPPORTED;
   }

SANE_Status sane_get_parameters (SANE_Handle, SANE_Parameters *)
   {
   return SANE_STATUS_UNSUPPORTED;
   }

SANE_Status sane_start (SANE_Handle)
   {
   return SANE_STATUS_UNSUPPORTED;
   }

SANE_Status sane_read (SANE_Handle, SANE_Byte *, SANE_Int, SANE_Int *length)
   {
   if (length)
      *length = 0;
   return SANE_STATUS_UNSUPPORTED;
   }

void sane_cancel (SANE_Handle)
   {
   }

SANE_Status sane_set_io_mode (SANE_Handle, SANE_Bool)
   {
   return SANE_STATUS_UNSUPPORTED;
   }

SANE_Status sane_get_select_fd (SANE_Handle, SANE_Int *fd)
   {
   if (fd)
      *fd = -1;
   return SANE_STATUS_UNSUPPORTED;
   }

SANE_String_Const sane_strstatus (SANE_Status status)
   {
   switch (status)
      {
      case SANE_STATUS_GOOD: return "Success";
      case SANE_STATUS_UNSUPPORTED: return "Operation not supported";
      case SANE_STATUS_CANCELLED: return "Operation was cancelled";
      case SANE_STATUS_DEVICE_BUSY: return "Device busy";
      case SANE_STATUS_INVAL: return "Invalid argument";
      case SANE_STATUS_EOF: return "End of file reached";
      case SANE_STATUS_JAMMED: return "Document feeder jammed";
      case SANE_STATUS_NO_DOCS: return "Document feeder out of documents";
      case SANE_STATUS_COVER_OPEN: return "Scanner cover is open";
      case SANE_STATUS_IO_ERROR: return "Error during device I/O";
      case SANE_STATUS_NO_MEM: return "Out of memory";
      case SANE_STATUS_ACCESS_DENIED: return "Access to resource denied";
      }
   return "Unknown SANE status";
   }

}
