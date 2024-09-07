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


#ifndef __desk_h
#define __desk_h


#include <list>
#include <vector>

#include "qpoint.h"
#include "qsize.h"
#include "qstring.h"
#include <QPixmap>

#include "err.h"


class File;
class Operation;
class QFileInfo;
class QPixmap;
class QPoint;
class QImage;
class Paperstack;
struct max_page_info;



struct max_info;

#if 0
/** information about a single file on the desktop */

typedef struct file_info
   {
   QString filename;   //!< filename within directory (not full path)
   QString pathname;   //!< full path name
   QPoint pos;         //!< current position on desktop
   int pagenum;        //!< current page visible
   int pagecount;      //!< number of pages
   int size;       //!< file size
   QPixmap *pixmap;    //!< preview image for this file
   QSize preview_maxsize;     //!< max preview size for file (this is the preview pixmap)
   QSize title_maxsize;     //!< max pixel size for file (stack) title
   QSize pagename_maxsize;     //!< max pixel size for all page titles
   struct max_info *max;  //!< max file info
   time_t time;             //!< last modification time
   int order;               //!< numerical order in list
   QPoint ipos;   //!< position of image
   err_info serr;  //!< place to put error
   err_info *err;  //!< last error which occured with this item
   } file_info;


typedef struct filelist_info
   {
   std::vector <file_info> file;
   int last;
   } filelist_info;
#endif


class Desk
   {
public:
   /** read the maxdesk info from the given directory

      \param dir           the directory to read
      \param rootdir       the directory to contain the trash dir
      \param readDesk   true to read the maxdesk.ini file
      \param read_sizes    read the size information (otherwise it is
                           ignored and will be recalculated */
   Desk (const QString &dir, const QString &rootdir,
         bool do_readDesk = true, bool read_sizes = true,
         bool writeDesk = true);

   /** set up an empty temporary desk */
   Desk (void);

   //! destructor
   ~Desk ();

   void clear (void);

   /** add matching files in a directory to the maxdesk.

      \param dirPath     the directory to search
      \param match       the string to match
      \param subdirs     true to also search subdirectories
      \param operation   operation to update */
   void addMatches (const QString &dirPath, const QString &match,
         bool subdirs = false, Operation *op = 0);

   /** read the maxdesk.ini file which contains positional and size
      information for each stack in the directory

      \param read_sizes    read the size information (otherwise it is
                           ignored and will be recalculated */
   void readDesk (bool read_sizes = true);

   /** write the maxdesk.ini file */
   bool writeDesk (void);

   // allocate and zero a new file structure
//    file_info *alloc_file (void);

   // operations we can do on files
   enum operation_t
      {
      op_rotate,
      op_hflip,
      op_vflip,

      op_count
      };

   /** create a new file of the correct type

      \param dir     directory
      \param fname   file name (the file extension determines the type) */
   File *createFile (const QString &dir, const QString fname);

   //! add a new file to the structure (if not already present) and position it
   void addFile (QFileInfo &file, const QString &dir);

   //! add an existing file to a desk - should only be used from Desktopmodel
   void addFile (File *f);

   /** given a filename, try to make it unique by adding numbers, etc.

      \param fname    the original filename (excluding extension)
      \param dir      the directory to check (null for current)
      \param ext      the file extension with dot - e.g. ".max"
      \param returns  the unique filename (without extension or directory), or
                      null if nothing unique can be found (probably a
                      filesystem fault) */
   QString findNextFilename (QString fname, QString dir = QString(), QString ext = QString());

   /** adds a new file to a desk. The file is normally positioned
       at the bottom of the view, but it is possible to place it next
       to an existing file. In this case, fbase and seq should be provided
       to indicate the existing file (for positioning only) and the sequence
       number of this new file relative to it. Sequence number 0 will be on
       top of the existing file, 1 will be offset slightly down and to the
       right, etc.

      \param fp         the file to add
      \param fbase      base file to position against (0 to add at bottom)
      \param seq        sequence number relative to fbase
      \returns the row number that the file was added to (normally the last one!) */
   int newFile (File *fnew, File *fbase = 0, int seq = -1);

   void removeRows (int row, int count);

   /** get the directory this maxdesk relates to (full path)

     \returns directory */
   QString &dir (void);
   QString &rootDir (void) { return _rootDir; }

   File *takeAt (int row);

   /** similar to the above but also returns the file position */
   File *findFile (QString fileName, int &pos);

   File *getFile (int itemnum);

   int rowCount (void);          //!< our 'official' record of row count
   void updateRowCount (void);   //!< update our official row count record

   QString trashdir (void);

   void arrangeBy (int type);

   /** rescan a file after it has been changed */
//    err_info *rescanFile (file_info *f);

   /** dispose of a file, but don't remove it from the list */
//    void disposeFile (file_info *f);

   //! start iterating and return the first file in the list
//    file_info *first (void);

   //! return the next file in the list
//    file_info *next (void);

   //! returns the n'th item in the list
//    file_info *item (int n);

   //! returns the number of files in the list
   int fileCount (void);
#if 0
   /** read a preview pixmap for page 'pagenum' of the file

      \returns 0 if all ok, else -ve error value */
   struct err_info *getPreviewPixmap (file_info *f, int pagenum, QString &title,
          QPixmap &pixmap, int &bpp, bool blank);

   /** read an image pixmap for page 'pagenum' of the file

      \param f        file to read
      \param pagenum  page number to read
      \param pixmapp  returns a new pixmap
      \param Size     returns display size of image (pixmap may be slightly
                           larger)
      \param bpp      returns number of bits per pixel in orginal image.
                      Note that the pixmap will generally be the same depth
                      as the current screen mode, i.e. normally 24bpp
      \param smooth   true if smoothing should be used to give a better
                      result (but slow!)

      \returns true if all ok */
   err_info *getImagePixmap (file_info *f, int pagenum,
             QPixmap **pixmapp, QSize &Size, QSize &scaledSize, int &bpp, bool smooth = false);

   struct err_info *getImageQImage (file_info *f, int pagenum, bool do_scale,
               QImage **imagep, QSize &Size, QSize &trueSize, int &bpp);

   err_info *getImageQImageRef (file_info *f, int pagenum, bool do_scale,
            QImage &image, QSize &Size, QSize &trueSize, int &bpp, bool blank = false);

   err_info *get_image_qimage_ref (max_info *max, int pagenum, bool do_scale,
            QImage &image, QSize &Size, QSize &trueSize, int &bpp, bool blank);

   err_info *get_image_qimage (max_info *max, int pagenum, bool do_scale,
            QImage **imagep, QSize &Size, QSize &trueSize, int &bpp);

   /** returns information about a page from a file

      \param f        file to read
      \param pagenum  page number to read
      \param Size     returns display size of image (pixmap may be slightly
                           larger)
      \param trueSize returns pixmap size of image (includes required margins)
      \param bpp      returns number of bits per pixel in orginal image.
                      Note that the pixmap will generally be the same depth
                      as the current screen mode, i.e. normally 24bpp
      \param csize    returns compressed size of page image
      \param dt       return page timestamp

      \returns       error, or NULL if none */
   struct err_info *getImageInfo (file_info *f, int pagenum,
         QSize &Size, QSize &trueSize, int &bpp, int &csize, QDateTime &dt);

   /** returns information about a preview of a page from a file

      \param f        file to read
      \param pagenum  page number to read
      \param Size     returns display size of preview
      \param bpp      returns number of bits per pixel in preview

      \returns       error, or NULL if none */
   struct err_info *getPreviewInfo (file_info *f, int pagenum,
         QSize &Size, int &bpp);

   /** reads the OCR text from a page

      \param f       file to read
      \param pagenum page number to read
      \param str     returns string containing OCR text */
   struct err_info *getPageText (file_info *f, int pagenum, QString &str);

   struct err_info *ensureMax (file_info *f);
#endif

   /** decode all images in a directory, checksum them, and write the checksums to
       a 'checksums' file

       \\return true if all ok */
   struct err_info *checksum (void);

   /** adds a new stack to the maxdesk, returns true if ok

      \param stack_name    name of new stack
      \param fnew          returns pointer to new file
      \param item          returns item number of new file in the desk */
   err_info *addPaperstack (const QString &stack_name, File *&fnew, int &item);
#if 0

   /** simple test that compression and decompression work ok */
   err_info *test_compare_with_tiff (QString &fname);
   err_info *test1 (QString &fname);
   err_info *test_decomp_comp (QString &fname);

   /** stack the pages from fsrc into fdest */
   struct err_info *stackItem (file_info *fdest, file_info *fsrc);

   /** unstack the given page from the file, creating it as a new stack.

       The position of this new stack will simply be at the end, unless 'seq'
       is specified, in which case it will be relative to f, and 'seq'
       positions offset from it

      \param f          stack to operate on
      \param pagenum    page number to unstack
      \param remove     true to remove the page (otherwise the old page is left alone)
      \param fnewp      returns pointer to new stack
      \param newname    returns the new filename chosen
      \param seq        sequence number we are up to (for positioning), -1 for none */
   struct err_info *unstackPage (file_info *f, int pagenum, bool remove,
         file_info **fnewp, QString &newname, int seq = -1);

   /** unstack a range of pages from a stack, creating them as a new stack.

       The position of this new stack will simply be at the end, unless 'seq'
       is specified, in which case it will be relative to f, and 'seq'
       positions offset from it

      \param f          stack to operate on
      \param pagenum    first page number to unstack
      \param remove     true to remove the pages (otherwise the old pages are left alone)
      \param fnewp      returns pointer to new stack
      \param newname    name to use for new stack, returns name chosen
      \param pagecount  number of pages to unstack */
   err_info *unstackPages (file_info *f, int pagenum, bool remove,
            file_info **fnewp, QString &newname, int pagecount);

   /** remove a number of pages from a stack

      The data returned in del_info can be passed to restorePages

      \param f          stack to operate on
      \param pages      page numbers to remove, bit n true means remove page n
      \param del_info   a place to put information about the deleted pages
      \param count      returns number of pages removed */
   err_info *removePages (file_info *f, QBitArray &pages,
      QByteArray &del_info, int &count);

   /** restore a number of pages to a stack

      The data in del_info must have been created from removePages()

      \param f          stack to operate on
      \param pages      page numbers to restore, bit n true means remove page n
                        see Desktopmodel::opUndeletePages() for more info
      \param del_info   a place with information about the deleted pages
      \param count      number of pages to restore */
   err_info *restorePages (file_info *f, QBitArray &pages,
         QByteArray &del_info, int count);

   /** duplicate a stack, creating a new one just next to it

      \param f       stack to duplicate
      \param *fnewp  returns the duplicate
      \param newname the name given to the new file
      \returns       error, or NULL if none */
   err_info *duplicate (file_info *f, file_info **fnewp, QString &newname);

   QString removeExtension (QString &fname, QString &ext);

   err_info *copyFile (QString from, QString to);

   /** remove a stack

      \param f       stack to remove
      \returns       error, or NULL if none */
   err_info *remove (file_info *f);

   /** move a stack to the trash. It may need to be renamed.

      \param f       stack to move to trash
      \param trashname  returns its new filename in the trash
      \returns       error, or NULL if none */
   err_info *moveToTrash (file_info *f, QString &trashname);
#endif

   err_info *deleteFromTrash (QString &trashname);
   
   err_info *deleteFromDir (QString &src, QString &trashname);

   /** move a stack from the trash. It may need to be renamed

      \param trashname  filename to move
      \param filename   desired new filename (updated on return)
      \param fnew       returns the resulting stack
      \returns       error, or NULL if none */
   err_info *moveFromTrash (QString &trashname, QString &filename,
         File *&fnew);

#if 0 //p
   /** move a stack to a new directory. Note that the stack may need to be
       renamed if a stack with the same name already exists in that
       directory

      \param f       stack to move
      \param newDir  new directory to send to (QString() for trash)
      \param newName returns the new name
      \returns       error, or NULL if none */
   err_info *move (file_info *f, QString &newDir, QString &newName);
#endif

   /** move a stack from the a source directory. It may need to be renamed

      \param src        source directory
      \param trashname  filename to move
      \param filename   desired new filename (updated on return)
      \param fnew       returns the resulting stack
      \returns       error, or NULL if none */
   err_info *moveFromDir (QString &src, QString &trashname, QString &filename,
      File *&fnew);

#if 0//p
   err_info *viewFile (file_info *f);

   /** duplicate a file f as a .max file, returning it in fnewp
      \param f          file to duplicate
      \param *fnewp     returns point to the duplicate created
      \returns      error, or NULL if none */
   err_info *duplicateMax (file_info *f, int odd_even, file_info **fnewp,
         Operation &op, QString &newname);

   /** duplicate a file f as a .pdf file, returning it in fnewp */
   err_info *duplicatePdf (file_info *f, file_info **fnewp, Operation &op, QString &newname);

   /** duplicate a file f as a .tiff file, returning it in fnewp */
   err_info *duplicateTiff (file_info *f, file_info **fnewp, Operation &op, QString &newname);

   /** add a page to a file

       \param flush   flush the max file to disc */
   err_info *addPage (file_info *file, const struct max_page_info *mp, bool flush);

   /** flush an item, ensuring it is updated on disc */
   err_info *flushItem (file_info *file);

   /** rename an item

      \param f    item to rename
      \param name new name, including extension (but not directory)
      \param auto_rename   automatically rename the file if there is already
                           one in the directory by the same name */
   err_info *rename (file_info *f, QString &name, bool auto_rename = false);

   /** rename a page

      \param f       item to rename
      \param pagenum page number to rename
      \param name new name, including extension (but not directory) */
   err_info *renamePage (file_info *f, int pagenum, QString &name);

   /** convert a file to .pdf format, leaving the old file there */
   err_info *convertPdf (QString &old);

   /** convert a file to .max format, and check that it is correct
      \param odd_even   which pages to duplicate: 1 = even, 2 = odd, 3 = both */
   err_info *convertMax (QString &old, QString newf, bool verbose,
			 bool force, bool reloc, int odd_even = 3);

   err_info *convert_to_pdf (max_info *max, QString fname, Operation *op);

   /** dump information about a file */
   err_info *dumpInfo (QString &fname, int debug_level);

   /** perform an operation on a file */
   err_info *operation (file_info *f, int pagenum, operation_t type, int ival);

   err_info *do_convert (QImage *image, QImage &newimage,
			 operation_t type, int ival);

   err_info *convert_to_jpeg (max_info *max, QString fname,
                       int pagenum, Operation *op);

   err_info *convertJpeg (QString &old);

//    err_info *buildIndex (QString fname, QString dir);
#endif
   /** sets the debug level to use for newly created max files */
   void setDebugLevel (int level);

#if 0
   /** returns the file before 'old' in the list */
   file_info *findPrevFile (file_info *old);

   /** returns the file after 'old' in the list */
   file_info *findNextFile (file_info *old);

   /** run some tests on Desk */
   void runTests (void);
#endif
   /** set whether to allow dispose or not */
   void setAllowDispose (bool allow);

   /** set whether to allow dispose or not */
   bool getAllowDispose (void);
#if 0 //p
   /** get strings from the annotation block

      \param f    file
      \param type annotation type (MAXA_...) */
   QString getAnnot (file_info *f, int type);

   /** returns the title of the a page

      \param pagenum    page number to look up (-1 for current)
   */
   QString getPageTitle (file_info *f, int pagenum = -1);

   /** figure out the next position to place a stack */
   void detectNextPosition (void);
#endif

   /** indicate that the desk is dirty and must be written back to the
       filesystem */
   void dirty (void);

   /** flush the desk to the filesystem */
   void flush (void);

   /** advance the current position to the next free space */
   void advance (void);

   // Re-read the directory and .paperdesk file
   void refresh();

private:
   /** check if a file exists in a maxdesk

     \param fileName       File to search for
     \returns pointer to file info if found, else NULL */
   File *findFile (QString fileName);

   //! set up some things for a new desk
   void setup (void);

   /** advance the current position by one space */
   void advanceOne (void);

   /** reset the position to the start */
   void resetPos (void);

   /** returns true if the given position clashes with any existing item */
   bool clashes (QPoint &pos);

   bool addToExistingFile (QString &fname);


#if 0
   err_info *scan_file (QString dir_name, QFileInfo *fi,
			filelist_info &old, filelist_info &file);

   file_info *find_file (filelist_info &fl, QString &fname);

   err_info *scan_dir (QString dir_name, filelist_info &old,
			     filelist_info &file);

   /** internal test: that incrementing the first string results in the second */
   void testInc (QString in, QString out);

   /** colour the image slighty to show it as blue

      \param image   image to colour*/
   void colour_image_for_blank (QImage &image);
#endif // 0

private:
   QList<File *> _files;
   int _row_count;     //!< number of files last time we checked
//    std::list<file_info *> _file;
//    std::list<file_info *>::iterator _it;
   QString _dir;       //!< directory the files are in
   QString _rootDir;   //!< root directory for this model (where trash is)
   QPoint _pos;        //!< next position for a file to appear
   int _rightMargin;   //!< right margin for items
   int _debug_level;     //!< the debug level to use for max debugging
   struct debug_info *_debug;   //!< debug settings to pass max decoder
   bool _do_writeDesk; //!< true to write back the maxdesk.ini file
   bool _allow_dispose;  //!< whether to allow disposal of this maxdesk (just a flag for clients)

   /* these fields help with positioning of a number of newly created files
      with respect to an existing base file. Here we keep track of the
      sequence number we are up to, and the position */
   QPoint _fbase_pos;   //!< current position to use with respect to base file
   int _fbase_seq;      //!< current sequence number with respect to base file (0 for none)
   QString _trash_dir;  //!< current trash directory
   bool _dirty;         //!< true if the desk has been changed and we must write it back
   };



#endif

