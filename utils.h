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
   File:       utils.h
   Started:    5/6/09

   This file contains utility functions which don't belong anywhere else.
*/


#ifndef __utils_h
#define __utils_h


#include <QList>
#include <QString>

#include <QTextStream>
#include <QVariantList>

class QDate;
class QDropEvent;

typedef unsigned char byte;

typedef struct cpoint
   {
   int x, y;
   } cpoint;

// Based on simpletreemodel/treeitem.h Qt5 example
class TreeItem
{
public:
    explicit TreeItem(const QVector<QVariant> &data, TreeItem *parentItem = nullptr);
    ~TreeItem();

    void appendChild(TreeItem *child);

    TreeItem *child(int row);
    const TreeItem *childConst(int row) const;
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    QString dirName() const;
    int row() const;
    TreeItem *parentItem();
    void dump(int indent = 0) const;
    void write(QTextStream& stream, int level) const;
    static bool read(QTextStream& stream, TreeItem *parent, int level);

private:
    QVector<TreeItem*> m_childItems;
    QVector<QVariant> m_itemData;
    TreeItem *m_parentItem;
};

/** retrieves a list of sizes from the Qt settings file.

    The setting used is base + "splitter/n/size" where n runs from
    1 to however many it finds.

    This function returns true if any records can be read, and updates
    the size array with a list of the numbers read.

    \param base      base name for setting
    \param size      size array to add to */
bool getSettingsSizes (QString base, QList<int> &size);

/** writes the given size settings out to the Qt settings file.

    The setting used is base + "splitter/n/size" where n runs from
    1 to the number of elements in the list */
void setSettingsSizes (QString base, QList<int> &size);

/** make a thumbnail of a JPEG file (quickly)

   The preview is taken from the current image scalled by PREVIEW_SIZE in each direction. The output
   buffer is either a 1 byte per pixel greyscale preview, or a 3 byte per pixel colour preview. In
   the latter case, the order of red and green is swapped from normal, as required by maxview format.

   Preview data is inverted - that means that the bottom row of the image appears first.

   \param data      data buffer containing JPEG file
   \param insize    data buffer size (number of bytes of JPEG data)
   \param *destp    returns an allocated buffer containing the preview data
   \param *dest_sizep  returns the allocated buffer size
   \param *sizep    returns the preview size

   \returns the last valid row of JPEG data decoded (can be used to crop the image) */
int jpeg_thumbnail (byte *data, int insize, byte **destp, int *dest_sizep, cpoint *sizep);

/** decode JPEG data into an image

   \param data        the JPEG data to decode
   \param size        the size of the data buffer
   \param dest        destimation buffer (which must be big enough)
   \param line_bytes  number of bytes per line in the output
   \param max_width   if not -1, then this is the maximum width available in the destination */
void jpeg_decode (byte *data, int size, byte *dest, int line_bytes, int bpp,
                  int max_width, int max_height);

QString removeExtension (const QString &fname, QString &ext);

void jpeg_encode (byte *image, cpoint *tile_size, byte *outbuff, int *size,
            int bpp, int line_bytes, int quality);

void memtest (const char *name);

QImage utilConvertImageToGrey (QImage &image);

QImage util_smooth_scale_image (QImage &image, QSize size);

/** creates a temporary file and returns its name. You should allocate
PATH_MAX + 1 bytes for the input string

   \param tmp  place to put temporary file
   \returns error, or NULL if all ok */
struct err_info *util_get_tmp (char *tmp);

/** build a zip file from a list of files

   \param zip     zip file
   \param fnamelist list of filenames to add to the zip (without their path) */
err_info *util_buildZip (QString &zip, const QStringList &fnamelist);

#define UTIL_PAGE_PREFIX "_p"

/** given a filename, try to make it unique by adding numbers, etc.

   Any text from UTIL_PAGE_PREFIX onwards is ignored (this allows use to
   check for files with page number suffixes.

   \param fname    the original filename (excluding extension)
   \param dir      the directory to check
   \param ext      the file extension with dot - e.g. ".max"
   \param returns  the unique filename (without extension or directory), or
                     null if nothing unique can be found (probably a
                     filesystem fault) */
QString util_findNextFilename (QString fname, QString dir, QString ext);

/** increment a filename

   There are various rules. It will increment numbers on the end of a filename, but only if
   single digit, or preceeded by _. Otherwise it will append a number, or _ + number if a number
   already exists

   \param name   old name, returns new name
   \param useNum true to use a number on the end of the name, else append _ if a number is present */
void util_incrementFilename (QString &name, bool useNum = true);

/* given a filename, check that it is unique with the supplied directory. If 
not, mangle the name with numbers until if becomes unique. Keep the extension
the same */
QString util_getUnique (QString fname, QString dir, QString ext);

/** get the login name of the current user 

   \param userName   returns user name, or "whoami" if not found
   \returns error, or NULL if ok */
err_info *util_getUsername (QString &userName);

/* Remove single quotes ' at the start and end of a string if present.

  \param str   String to process
  \returns string without quotes, or unchanged if there are no quotes */
QString utilRemoveQuotes (QString str);

/**
 * @brief Find a year in a filename
 * @param fname    Filename to check
 * @param foundPos Position in fname where the match was found
 * @return year (e.g. 2024) or 0 if none found
 *
 * Supported options are:
 *
 * 1. a year number as yyyy
 * 2. mmmyy or yymmm where mmm is a 3-character month name, yy 2-digit year
 */
int utilDetectYear(const QString& fname, int& foundPos);

/**
 * @brief Find a month in a filename
 * @param fname   Filename to check
 * @param foundPos Position in fname where the match was found
 * @return month (1=January, 12=December) or 0 if none
 */
int utilDetectMonth(const QString& fname, int& foundPos);

/**
 * @brief Suggest possible existing directories and new ones to create
 * @param date     Date to use for searching
 * @param matches  Returns a sorted list of matches
 * @param missing  Returns a list of directories which could be created to make
 *                 a better match
 * @return List of matches in order of quality
 */
QStringList utilDetectMatches(const QDate& date, QStringList& matches,
                              QStringList& missing);

/**
 * @brief Check if a particular drop event is supported by Paperman
 * @param event         Event to check
 * @param allowedTypes  List of allowed types, e.g.
 *                      {"application/vnd.text.list", "text/uri-list"}
 * @return true if OK, false if not
 *
 * A warning is shown if the type is not supported
 */
bool utilDropSupported(QDropEvent *event, const QStringList& allowedTypes);

/**
 * @brief Scan a directory to discover files and subdirectories
 * @param dirPath  Path to scan, without training "/"
 * @return Tree, with the children containing the subdirectories and files in
 *     @dirPath
 */
TreeItem *utilScanDir(QString dirPath);

#endif
