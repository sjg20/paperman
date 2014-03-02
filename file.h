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
   File:       file.h
   Started:    26/6/09

   This file contains information about a single file on the desktop.
   It could potentially also hold information about a directory of files
   that are shown as a single stack on the desktop, if that file format
   didn't support multiple images.
*/


#ifndef __file_h
#define __file_h


#include <QBitArray>
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QStringList>


#include "err.h"


class QTextStream;

class Desk;
class Operation;
class Filepage;
class Paperstack;


/** information about a single file on the desktop */

class File : public QObject
   {
   Q_OBJECT

public:
   /** file types we support. Each of these has a subclass Filexxx. If you
       want to implement multiple image types within a type, consider
       subtype instead of creating a new type */
   enum e_type
      {
      Type_other,     // generic file
      Type_max,      // max file
      Type_pdf,      // pdf file
//       Type_tiff,     // tiff file  (to be implemented)
      Type_jpeg,     // JPEG file  (to be implemented)
//       Type_djvu,     // djvu file  (to be implemented)
      // other?

      Type_count     // numer of types
      };

   File (const QString &dir, const QString &fname, Desk *desk, e_type type);
   File (File *orig, Desk *new_desk);
   ~File ();

   //! given a filename, deduce its type from its extension
   static e_type typeFromName (const QString &fname);

   static bool decodePageNumber (const QString &fname, QString &base,
                                 int &pagenum, QString &ext);

   void setup (void);

   // annotation types

   enum e_annot
      {
      Annot_author,
      Annot_title,
      Annot_keywords,
      Annot_notes,

      Annot_count,
      Annot_none = Annot_count
      };

   // envelope types
   enum e_env
      {
      Env_from,      //!< username / group/ device this came from, e.g sglass
      Env_to,        //!< usernames / groups this is addressed to , e.g sglass,admin
      Env_subject,   //!< subject of file
      Env_notes,     //!< notes attached by sender

      Env_count
      };

   static File *createFile (const QString &dir, const QString fname,
         Desk *desk, e_type type);

   static Filepage *createPage (e_type type);

   err_info *copyTo (File *fnew, int odd_even, Operation &op, bool verbose = false);

   /** converts a name into an e_env value */
   static e_env envFromName (const QString &name);

   /** converts an e_env value into a name */
   static QString envToName (e_env env);

   /*********** functions which the base class should implement **********/
public:
   // load / save / create
   virtual err_info *load (void) = 0;  // was desk->ensureMax

   /** create the file (on the filesystem). It doesn't need to be written to
       as we will call flush() later for that */
   virtual err_info *create (void) = 0;

   virtual err_info *flush (void) = 0;

   virtual err_info *remove (void) = 0;

   /**
    * Check if this filename belongs within the existing file
    *
    * This handles the case where we see additional pages for an existing
    * File, for those File types which don't support multiple pages such as
    * JPEG. We look for a page marker
    *
    * If found, then the file is added as a new page
    *
    * \param base_fname      Filename to check (stripped of ext and page number)
    * \param pagenum         Page number for file (0..n-1)
    * \return true if found, false if not
    */
   virtual bool claimFileAsNewPage (QString fname, QString &base_fname,
                                    int pagenum);

   // accessing and changing metadata

   virtual int pagecount (void) = 0;

   virtual err_info *getPageTitle (int pagenum, QString &title) = 0;

   virtual err_info *getAnnot (e_annot type, QString &text)= 0;

   virtual err_info *putAnnot (QHash<int, QString> &updates) = 0;

   virtual err_info *putEnvelope (QStringList &env) = 0;

   virtual err_info *getPageText (int pagenum, QString &str) = 0;

   /** gets the total size of a file in bytes. this should include data not
       yet flushed to the filesystem */
   virtual int getSize (void) = 0;

   virtual err_info *renamePage (int pagenum, QString &name) = 0;

   virtual err_info *getImageInfo (int pagenum, QSize &size,
         QSize &true_size, int &bpp, int &image_size, int &compressed_size,
         QDateTime &timestamp) = 0;

   virtual err_info *getPreviewInfo (int pagenum, QSize &Size, int &bpp) = 0;




   // image related
   virtual QPixmap pixmap (bool recalc = false) = 0;

   /** returns the preview image for a particular page.

     If 'blank' then the image should be returned blank, either by using
     colour_image_for_blank() or setting the palette.

      \param pagenum    page number within stack
      \param pixmap     returns pixmap
      \param do_scale always false, not used (TODO: remove?)
      \param blank      true to show image as 'blank'
      \returns error, or NULL if none */
   virtual err_info *getPreviewPixmap (int pagenum, QPixmap &pixmap, bool blank) = 0;

   /** returns the image for a particular page.

     If 'blank' then the image should be returned blank, either by using
     colour_image_for_blank() or setting the palette.

      \param pagenum    page number within stack
      \param do_scale   always false, not used (TODO: remove?)
      \param image      returns image result
      \param Size     returns original size of image (image may be slightly
                           larger to accomodate padding margins)
      \param trueSize returns actual size of image (including padding margins)
      \param bpp      returns number of bits per pixel in orginal image
      \param blank      true to show image as 'blank'
      \returns error, or NULL if none */
   virtual err_info *getImage (int pagenum, bool do_scale,
               QImage &image, QSize &Size, QSize &trueSize, int &bpp, bool blank) = 0;



   // operations on files

   //FIXME: should remove 'flush' as we can just call flush()
   virtual err_info *addPage (const Filepage *mp, bool flush) = 0;

//    virtual err_info *unstack (int pagenum, bool remove,
//             QString &newname, QString &pagename, File **dest);

   virtual err_info *removePages (QBitArray &pages,
         QByteArray &del_info, int &count) = 0;

   virtual err_info *restorePages (QBitArray &pages,
      QByteArray &del_info, int count) = 0;

   /** unstack a range of pages from a stack, creating them as a new stack.

      \param pagenum    first page number to unstack
      \param pagecount  number of pages to unstack
      \param remove     true to remove the pages (otherwise the old pages are left alone)
      \param dest       pointer to newly create file to add pages to
      \returns error, or NULL if ok */
   virtual err_info *unstackPages (int pagenum, int pagecount, bool remove,
               File *dest) = 0;

   /** stack one stack onto of this one. The stacks must be the same type.

      \param src  source stack
      \returns error, or NULL If ok */
   virtual err_info *stackStack (File *src) = 0;

   /** duplicate a file */
   virtual err_info *duplicate (File *&fnew, File::e_type type, const QString &uniq,
      int odd_even, Operation &op, bool &supported) = 0;


   /*********** end of functions which the base class should implement ******/

   /** returns the type name */
   static QString typeName (e_type type);

   /** returns the type name of this file (JPEG, PDF, etc.) */
   QString typeName (void);

   /** returns the extension of file type including the . (for example
       Type_pdf is .pdf) */
   static QString typeExt (e_type type);

   bool extMatchesType (const QString &ext);

   QString getAnnotName (e_annot type);

   /** sets the file position in the viewer */
   void setPos (QPoint pos);

   QString pageTitle (int pagenum);

   e_type type (void);

   /** sets the current page number */
   void setPagenum (int pagenum);

   /** sets the number of pages */
   void setPagecount (int count);

   void setPreviewMaxsize (QSize size);
   QSize previewMaxsize (void);

   void setTitleMaxsize (QSize size);
   QSize titleMaxsize (void);

   void setPagenameMaxsize (QSize size);
   QSize pagenameMaxsize (void);

   void decodeFile (QString &line, bool read_sizes);

   void encodeFile (QTextStream &stream);

   QString &filename (void);
   QString &basename (void);
   QString &leaf (void);

   /** get file extension, including leading . */
   QString &ext (void);

   /** \returns true if the file rect intersects the given rect */
   bool intersects (QRect &base);

   QPoint &pos (void);

   int size (void ) { return _size; }

   QString pathname (void);

   int order (void);
   void setOrder (int order);

   void setTime (QDateTime dt);

   QDateTime time (void);

   bool valid (void);
   void setValid (bool valid);

   int pagenum (void);

   err_info *err (void);

   /** Set the error for this file

     \param err   Error to set, or NULL To clear error
     \returns the error passed in */
   err_info *setErr (err_info *err);

   static void colour_image_for_blank (QImage &image);

   err_info *copyFile (QString from, QString to);

   void updateFilename (const QString &fname);

   Desk *desk (void);
   void setDesk (Desk *desk);

//    err_info *moveToTrash (QString &trashname);

   /** move or copy a file into a new directory, ensuring tha the name
       is unique. If moved then you are expected to delete this file
       object as it is no longer valid

      \param newDir     directory to move/copy to
      \param newName    returns new name given to the file
      \param copy       true to copy, else will be moved
      \return error, or NULL if ok */
   err_info *move (QString &newDir, QString &newName, bool copy);

   err_info *rename (QString &fname, bool auto_rename);

   static err_info *not_impl (void);

   /** makes a copy of a file and puts it in the provided desk. If 'type' is
      Type_other then a bytewise copy will be made, otherwise the file will be
      converted to the supplied type. The leaf of the filename to use it in
      uniq - it is known to be unique with the directory being used.

      \param desk    desk to put the new file in. If NULL then the file is
      created in the tmp directory
      \param type    required file type to convert to, also normally determines ext
      \param uniq    leaf of required filename (so for /path/to/file.ext would be 'file')
      \param odd_even   which pages to duplicate: 1 = even, 2 = odd, 3 = both
      \param op      operation to use
      \param fnew    returns the newly created file
      \returns error, or NULL If ok */
   err_info *duplicateToDesk (Desk *desk, File::e_type type, QString &uniq,
      int odd_even, Operation &op, File *&fnew);

   /** as above but bases uniq on the existing file's name with _copy appended,
       and puts the file in the same desk */
   err_info *duplicateAny (e_type type, int odd_even, Operation &op, File *&fnew);

   /** if compatible, stack one stack onto another. After doing some checks
       this calls the appropriate stackStack() method

      \param src  source stack to add to this one
      \returns error, or NULL if ok */
   err_info *stackItem (File *src);

   /** unstack a range of pages from a stack, creating them as a new stack.

      \param pagenum    first page number to unstack
      \param pagecount  number of pages to unstack
      \param remove     true to remove the pages (otherwise the old pages are left alone)
      \param uniq       unique filename for new stack, including extension
      \param dest       returns pointer to newly create file
      \param itemnum    returns item number of the new file within the desk
      \param seq        sequence number of operation, used to offset stacks from parent
      \returns error, or NULL if ok */
   err_info *unstackItems (int pagenum, int pagecount, bool remove,
               QString fname, File *&dest, int &itemnum, int seq = -1);

protected:
   QPixmap unknownPixmap (void);

protected:
   e_type _type;        //!< file type
   int subtype;         //!< subtype information (where type refers to multiple file types)
   Desk *_desk;         //!< desk that this file belongs to, or 0 if none
   QString _filename;   //!< filename within directory (not full path)
   QString _basename;   //!< displayed filename (with or without extension)
   QString _leaf;       //!< leaf name (filename without extension)
   QString _ext;        //!< extension
   QString _dir;        //!< directory
   QString _pathname;   //!< full path name
   QPoint _pos;         //!< current position on desktop
   int _pagenum;        //!< current page visible
//   int _pagecount;      //!< number of pages
   int _size;       //!< file size
   QPixmap _pixmap;     //!< preview image for this file
   QSize _preview_maxsize;     //!< max preview size for file (this is the preview pixmap)
   QSize _title_maxsize;     //!< max pixel size for file (stack) title
   QSize _pagename_maxsize;     //!< max pixel size for all page titles
//   struct max_info *_max;  //!< max file info
//   time_t _time;             //!< last modification time
   QDateTime _timestamp;
   int _order;               //!< numerical order in list
//   QPoint _ipos;   //!< position of image
   err_info _serr;  //!< place to put error
   err_info *_err;  //!< last error which occured with this item
   bool _valid;      //!< true if we have scanned this file and know what it contains

   // annotation data
   bool _annot_loaded;               // TRUE if data has been loaded
   QStringList _annot_data;   // the annotation data that was loaded

   // envelope data
   bool _env_loaded;               // TRUE if data has been loaded
   QStringList _env_data;   // the envelope data that was loaded
   File *_ref_to;          //!< file that this one is a reference to, for virtual file
   };


/** this holds information about a page image which can be added to a file */

class Filepage
   {
public:
   Filepage ();
   virtual ~Filepage ();

   void setPaperstack (Paperstack *stack);

   void addData (int width, int height, int depth, int stride,
      QString &name, bool jpeg, bool blank, int pagenum, QByteArray data, int size);

   bool markBlank (void) const;

   Paperstack *stack (void) const;

   int pagenum (void) const;

   static void getImageFromLines (const char *data, int width, int height, int depth,
      int stride, QImage &image, bool restride32 = false, bool blank = false);

   static void getImageFromLines (char *data, int width, int height, int depth,
      int stride, QImage &image, bool restride32, bool blank, bool invert);

   QImage getThumbnail (bool invert) const;

   /** returns the thumbnail as a raw data block

      \param word_align   true to word-align the start of each scan line,
                          false to pack to nearest byte */
   QByteArray getThumbnailRaw (bool invert, QImage &image, bool
         word_align = true) const;

public:
   // functions which should be implemented by the subclass

   /** compress the page, store the compressed data internally within the class */
   virtual err_info *compress (void);

   QString name (void) { return _name; }

   void getImage (QImage &image) const;

   /* copy the data out of an image, packing it down so that lines
      do not necessarily start on a word boundary (but on a byte
      boundary) */
   QByteArray copyData (bool invert, bool force_24bpp) const;

public:
   int _width;   //!< width in pixels
   int _height;  //!< height in pixels
   int _depth;   //!< depth in bpp
   int _stride;  //!< bytes per line
   int _size;       //!< size of data
   QString _name; //!< the page name
   int _jpeg;           //!< true if page is already JPEG compressed
   int _mark_blank;     //!< true if the page should be marked blank (to be deleted)
   int _pagenum;        //!< page number (used by Paperstack)
   Paperstack *_stack;  //!< the Paperstack which is creating this page
   QDateTime _timestamp;   //!< timestamp for page
   QByteArray _data;    //!< the image data
   };

#endif

