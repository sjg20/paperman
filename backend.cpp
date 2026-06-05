/*
License: GPL-2
*/

#include "backend.h"

#include <QFileInfo>


QString Backend::contentTypeForPath(const QString &path)
{
   QString ext = QFileInfo(path).suffix().toLower();
   if (ext == "pdf")                        return "application/pdf";
   if (ext == "jpg" || ext == "jpeg")       return "image/jpeg";
   if (ext == "tif" || ext == "tiff")       return "image/tiff";
   if (ext == "png")                        return "image/png";
   /* .max and anything else: opaque bytes. */
   return "application/octet-stream";
}
