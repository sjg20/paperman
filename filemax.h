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
   File:       filemax.h
   Started:    26/6/09

   This file implmenents a max stack, which is a file containing a set
   of scanned images and other information
*/


#include <QDateTime>

#include "file.h"
#include "utils.h"

struct decode_info;
struct debug_info;


class Filemaxpage;


typedef struct debug_info
   {
   // debugging things which control the code
   int level;       // debug level to us
   int max_steps;   // number of decode steps to do (normally INT_MAX)
   int start_tile;  // start tile to decode
   int num_tiles;   // number of tiles to decode (normally INT_MAX)
   FILE *logf;      // file to send debug ascii data to
   FILE *compf;     // file to which compression info is written

   // from here on, debug status info
   int step;        // current step
   } debug_info;


typedef struct cache_info
   {
   QByteArray buff;
//    byte *buff;
//   int alloced;   //!< amount alloced to cache
//   int size;      //!< size of cache
   int pos;       //!< file position of data in cache
   } cache_info;


typedef struct tile_info
   {
   byte *buf;  // buffer to hold data
   int size;  // size of tile data
   int code;  // unknown
   int other;  // unknown
   } tile_info;


typedef struct part_info
   {
   int start;
   int size;      /* size in header, as adjusted by us */
   int raw_size;  /* size as indicated by header */
   byte *buf;    // the actual data
   } part_info;


typedef struct chunk_info
   {
   int start;
   int size;
   int chunkid;   // all chunks related to a particular page have the same chunkid
   int flags;
   int textflag;
   int titletype;
   int type;
   bool used;
   cpoint image_size;    // image size in pixels
   cpoint preview_size;  // preview size in pixels
   cpoint dpi;           // dots per inch
   int bits;      // bits per pixel
   int channels;  // number of channels
   cpoint tile_size;  // size of this tile
   cpoint tile_extent;  // number of tiles across and down
   int tile_count;      // total number of tiles
   int tile_line_bytes; // number of bytes in a tile's line

   // line_bytes is the number of bytes in an image line
   int line_bytes;      // total number of bytes in a line
   tile_info *tile;     // info about tiles
   int image_start;  /* offset from start+0x20 to preview image */
   int code;         // tile code
//   int part_count;
//    part_info *part;
   QVector<part_info> parts;
   int after_part;  /* file pointer after part directory */
   byte *image;   /* binary image data */
   int image_bytes;  // number of bytes in image
   byte *preview;  // preview image
   int preview_bytes;  // number of bytes in preview data
   byte *buf;       // buffer which holds the chunk data
   bool loaded;   //!< true if chunk exists in memory
   bool saved;    //!< true if chunk has been save to file
   } chunk_info;


typedef struct page_info
   {
   int chunkid;
   int roswell;

   int have_roswell; // true if the following fields are valid
   int image;
   int noti1, noti2, title, text;
   QDateTime timestamp;

   QString titlestr;     //!< page title
   bool title_loaded;    //!< true if page title string has been loaded
   bool title_saved;     //!< false if page title has been updated since loading
   } page_info;



class Filemax : public File
   {
   Q_OBJECT

public:
   Filemax (const QString &dir, const QString &filename, Desk *desk);
   ~Filemax ();

   /*********** functions which the base class should implement **********/

   // load / save / create

   virtual err_info *load (void);  // was desk->ensureMax

   virtual err_info *create (void);

   virtual err_info *flush (void);

   virtual err_info *remove (void);


   // accessing and changing metadata

   virtual int pagecount (void);

   virtual err_info *getPageTitle (int pagenum, QString &title);

   virtual err_info *getAnnot (e_annot type, QString &text);

   virtual err_info *putAnnot (QHash<int, QString> &updates);

   virtual err_info *putEnvelope (QStringList &env);

   virtual err_info *getPageText (int pagenum, QString &str);

   virtual int getSize (void);

   virtual err_info *renamePage (int pagenum, QString &name);

   err_info *getImageInfo (int pagenum, QSize &size,
         QSize &true_size, int &bpp, int &image_size, int &compressed_size,
         QDateTime &timestamp);

   virtual err_info *getPreviewInfo (int pagenum, QSize &Size, int &bpp);



   // image related
   virtual QPixmap pixmap (bool recalc = false);

   virtual err_info *getPreviewPixmap (int pagenum, QPixmap &pixmap, bool blank);

   virtual err_info *getImage (int pagenum, bool do_scale,
               QImage &image, QSize &Size, QSize &trueSize, int &bpp, bool blank);


   // operations on files
   virtual err_info *addPage (const Filepage *mp, bool flush);

   virtual err_info *removePages (QBitArray &pages,
         QByteArray &del_info, int &count);

   virtual err_info *restorePages (QBitArray &pages,
      QByteArray &del_info, int count);

   virtual err_info *unstackPages (int pagenum, int pagecount, bool remove,
               File *dest);

   virtual err_info *stackStack (File *src);

   virtual err_info *duplicate (File *&fnew, File::e_type type, const QString &uniq,
      int odd_even, Operation &op, bool &supported);


   /** rebuild missing greyscale previews in the file */
   err_info *rebuildPreviews ();

   /*********** end of functions which the base class should implement ******/

public:
   static void chunk_init (chunk_info &chunk);

   static void part_resize (QVector<part_info> &parts, int count);

   static void part_init (part_info &part);

   static void page_init (page_info &page);
   static void page_resize (QVector<page_info> &pages, int count);

   static void chunk_resize (QVector<chunk_info> &chunks, int count);

private:
   void debug_page (page_info *page);

   void debug_pages (void);

   /** check if any error has been recorded against the max file. If so, clear
   it and return it */
   err_info *max_checkerr (void);

   /** read a 32-bit signed word from the max file at the given position

      \param max      the max file to read from
      \param pos      the position to read
      \param *wordp   returns the word read

      \returns err_info containing erro or NULL if ok */
   err_info *getworde (int pos, int *wordp);

   /** read a 32-bit word from a max file at a given position

      If an error occurs, this is recorded in the max structure, and this
      read and all subsquent ones will return -1, until max_checkerr()
      is called to clear the error.

      \param max      the max file to read from
      \param pos      the position to read

      \returns the word read, or -1 on error */
   int getword (int pos);

   /** read a 16-bit unsigned halfword from the max file at the given position

      \param max      the max file to read from
      \param pos      the position to read
      \param *wordp   returns the word read

      \returns err_info containing errno or NULL if ok */
   err_info *gethwe (int pos, int *valuep);

   /** read a 16-bit unsigned halfword from a max file at a given position

      If an error occurs, this is recorded in the max structure, and this
      read and all subsquent ones will return -1, until max_checkerr()
      is called to clear the error.

      \param max      the max file to read from
      \param pos      the position to read

      \returns the halfword read, or -1 on error */
   int gethw (int pos);

   err_info *getbytee (int pos, int *bytep);

   err_info *merr_make (const char *func_name, int errnum, ...);

   /** read data into the cache and return a pointer to it. This data will
   survive until the next max_cache_data() call */
   err_info *max_cache_data (cache_info &cache, int pos,
         int size, int min, byte **datap);

   /** checks the cache for the required data (a single word) and returns it if
   available. If not, it loads the cache with the next 4k of data starting at
   that position, and returns the required data */
   byte *max_check_scache (int pos);

   /* read data into the given buffer */
   err_info *max_read_data (int pos, byte *buf, int size);

   /* write data to a max file:

      \param max    the max file
      \param pos    file position to write to
      \param data   data to write
      \param size   number of bytes to write */
   err_info *max_write_data (int pos, byte *data, int size);

   void max_clear_cache (cache_info &cache, int pos = 0, int size = -1);

   /** read a header for an image 'part' - images consist of a number of parts
   each containing different types of data. This function reads the position
   and size of a particular part

      \param max     max file being processed
      \param part    place to put returned information
      \param pos     position of part header info in file

      \returns err_info error, or NULL if ok */
   err_info *read_part (part_info &part, int pos);

   unsigned getbits (struct decode_info &decode, int count);

   int update_table (struct decode_info &decode, byte *inptr, int width,
                         short *table);

   // decode an uncompressed line
   void do_uncomp (struct decode_info &decode, int bits_used);

   err_info *decomp (struct decode_info &decode, byte *inptr, int bits_used, int width,
                   short *table_prev, short *table, int *bits_usedp);

   /* takes a table of (start, end) positions and sets pixels in the line between each pair of positions */
   void output (short *table, byte *destptr, int width);

   err_info *do_compressed (struct decode_info &decode);

   void free_tables (struct decode_info &decode);

   void setup_huffman_13 (unsigned *src, unsigned *src2, unsigned *tab);

   void setup_huffman_7 (unsigned *in, unsigned *out);

   err_info *setup_tables (struct decode_info &decode);

   void do_single (struct decode_info &decode);

   err_info *decode_compressed_tile (struct decode_info &decode, int size);

   err_info *decode_tileinfo (chunk_info &chunk);

   err_info *decode_tile (chunk_info &chunk,
            struct decode_info &decode, int code, int pos, int size, byte *ptr,
            cpoint &tile_size);

   err_info *decode_tiledata (chunk_info &chunk,
                     decode_info &decode, byte *&imagep, QSize *image_size);

   err_info *decode_preview (chunk_info &chunk, int flip,
            byte **previewp);

   /** decode an image from a chunk into the given byte buffer. If image_size is
       not NULL then it specifies the image size of the destination buffer, which
       may be smaller in the x dimension only */
   err_info *decode_image (chunk_info &chunk, byte *&imagep, QSize *image_size);

   err_info *decode_init (decode_info &decode, chunk_info &chunk,
                        byte *data, byte *image, int stride, cpoint &tile_size);

   const char *check_chunk (int start);

   err_info *alloc_part (chunk_info &chunk);

   void chunk_free (chunk_info &chunk);

   err_info *read_chunk_buf (chunk_info &chunk);

   void write_bermuda (byte *buf);

   /** read the header of the chunk at the given file position. This does not
   read all the chunk's data, but just the header information, and information
   about the parts inside, if any

      \param chunk   chunk to read from
      \param pos     start position to read from

      \returns error (NULL for ok) */
   err_info *read_chunk (chunk_info &chunk, int pos);

   err_info *chunk_ensure (chunk_info &chunk);

   /** find a chunk at the given position. If tempp, then it may be loaded temporarily
   in which case *tempp will be set to true on exit


      chunkp   set to point to the chunk, or NULL If not found */
   err_info *chunk_find (int pos, chunk_info *&chunkp, bool *tempp);

   void max_set_debug (struct debug_info *debug);

   void max_set_debug_compf (struct debug_info *debug, FILE *compf);

   err_info *update_titlestr (page_info *page, const QString &title);

   err_info *ensure_all_chunks (void);

   err_info *page_read_roswell (page_info &page);

   err_info *setup_max (void);

   char *max_get_shell_filename (void);

   // Open the max file
   err_info *max_openf();

   void max_free (void);

   /** find the chunk assocated with a page image and return it. If it has not
   been loaded from the file yet, and tempp is non-NULL, then temporarily
   load it, without loading all other chunks. This is an optimisation to
   allow previews to be loaded quickly

      \param pagenum     pagenum to find
      \param chunkp      returns chunk
      \param tempp       on entry, non-NULL to allow temporary loading, or
                        NULL to force full loading of max file
                        on exit, value indicates whether chunk is
                        temporary, and thus should be freed (with free-chunk())
                        when it is finished with. Only value if err returns NULL

      \param err   returns err_info, or NULL if no error */
   err_info *find_page_chunk (int pagenum,
               chunk_info *&chunkp, bool *tempp, page_info **pagep);

   err_info *find_page (int pagenum, page_info* &page);

   err_info *max_get_text (int pagenum, char **textp);

   err_info *load_annot (void);

   //! load the envelope data
   err_info *load_envelope (void);

   err_info *max_get_annot (int type, char **strp);

   err_info *max_put_annot (char *str_list []);

   err_info *max_get_preview (int pagenum, char **titlestr,
            cpoint *size, int *bpp, byte **previewp);

   err_info *max_get_preview_info (int pagenum, char **titlestr,
            cpoint *size, int *bpp);

   err_info *max_get_image (int pagenum, cpoint *size, cpoint *true_size,
                        int *bpp, byte **imagep, int *image_size);

   err_info *max_free_image (int pagenum);

   bool max_get_preview_maxsize (cpoint *size);

   //! Ensure that the file is open
   err_info *ensure_open();

   //! Ensure that the file is closed
   void ensure_closed();

   err_info *max_open_file();

   err_info *dodump (FILE *f, byte *ptr, int start, int count);

   err_info *dump (FILE *f, int start, int count);

   err_info *dumpc (FILE *f, int start);

   err_info *dump_block (FILE *f, const char *name, int pos);

   void chunk_dump (int num, chunk_info &chunk);
   void chunks_dump (void);

   err_info *show_file (FILE *f);

   /** replace a page

      \param max     max file containing page
      \param page    page to replace
      \param mp      new maxpage to replace it with */
   err_info *max_replace_page (page_info &page, Filemaxpage &mp);

   /** compress a page image and store the info in mp->chunk

      \param mp   holds information about width, height, etc.
      \param buf  image buffer to compress
      \param size size of image buffer */
   err_info *max_compress_page (Filepage *mp, byte *buf, int size);

   err_info *max_update_page_image (int pagenum, int width, int height,
                 int depth, int stride, byte *buf, int size);

   void write_max_header (void);

   /** converts the information in the various chunk->... variables into
      a buffer of chunk data in chunk->buf. What is actually written there
      depends on the type of the chunk


      \param chunk    chunk to build
      \param force    true to build the buf even if it already exists
                      otherwise we will just update a few flags */
   err_info *build_chunk (chunk_info &chunk, bool force = false);

   err_info *remove_chunk (chunk_info &chunk);

   err_info *remove_chunknum (int pos);

   err_info *restore_chunknum (int pos);

   err_info *alloc_chunk (int chunk_size,
            chunk_info **chunkp);

   /* insert a new chunk into a max file. If posp, then *posp should contain
   the current position of the chunk, or 0 if it doesn't current exist in
   the max file. It will then return the position allocated to the new chunk.
   It may need to move if the new size is larger than the old.

   new_chunk->size must be set up correctly
   new_chunk->start will be set up by this function

      \param max        max file to add chunk to
      \param new_chunk  new chunk to add
      \param posp       pointer to old chunk position / returns new position

      \returns NULL if ok, else err_info * */
   err_info *insert_chunk (chunk_info &new_chunk, int *posp);

   err_info *create_bermuda (void);

   err_info *create_annot (void);

   err_info *create_envelope (void);

   err_info *create_roswell (page_info &page);

   err_info *create_title (page_info &page);

   err_info *page_add (int chunkid, const QString &titlestr,
            page_info *&pagep);

   err_info *write_max (FILE *f);

   /** write a set of pages to a given filename

      Note that this will destroy maxpage in the process, so the call does
         not need to free it

      \param fname filename to write to
      \param maxpage info on each page
      \param count number of pages */
   err_info *max_write (const char *fname, Filepage *maxpage, int count);

   err_info *merge_chunk (Filemax *src, int chunk_pos, int *new_pos);

   err_info *flush_chunks (void);

   err_info *flush_pages (void);

   err_info *ensure_titlestr (int pagenum, page_info &page);

   err_info *merge_chunks (Filemax *src,
            page_info &dstpage, page_info &srcpage);

   err_info *max_rename_page (int pagenum, char *newname);

   err_info *free_page (page_info &page);

   err_info *restore_page (page_info &page);

   err_info *remove_page (page_info *page);

   err_info *remove_pages (int pagenum, int count);

   err_info *remove_page_list (int count, const int *del, void *in_del_info);

   err_info *restore_page_list (int count, const int *del, const void *in_del_info);

   err_info *max_setup_page (page_info &page, const Filemaxpage &maxpage);

   err_info *max_add_page (const Filemaxpage *maxpage, bool do_flush);

   err_info *chunk_unload (int pos);

   err_info *max_unload (int pagenum);

private:
//    char *_fname;                                               /* file name */
   FILE *_fin;     // the open file
//   byte *data;                                              /* file data */

   int _version;         // version number
   // the main cache
   cache_info _cache;    // main cache, for large reads
   cache_info _scache;   // small cache, for byte/halfword/word reads

   // the 4k cache (for word, halfword and byte reads
//    byte *_cache_4k;
//   int _cache_4k_pos;       //!< file position of data in cache

//    int _size;                                                  /* file size */
   int _chunk0_start;
   QVector <chunk_info> _chunks;
//   int _page_count;
//   int _page_alloced;
//   int chunk_count;     // number of chunks
//   int chunk_alloced;   // number of chunks actually allocated (space)
//   int start_pagenum;
   int _signature; // file signature
   int _bermuda, _tunguska, _annot, _trail;
   int _envelope;
   int _b0, _b4;
   QVector <page_info> _pages;
//   int _type;   // type of file (FILET_...)
   QByteArray _hdr;
//    byte *_hdr;
//    int _hdr_size;
   int _chunkid_next;   // the next chunkid value to allocate in the file
   bool _hdr_updated;   //!< the header has been updated since last saved
   struct debug_info *_debug;  // debug info
   bool _version_a;  //!< true if this is an old version A file
//   err_info *_err;    //!< the last error that occurred
   bool _all_chunks_loaded;  //!< true if all chunk data has been loaded
   };


class Filemaxpage : public Filepage
   {
public:
   Filemaxpage (void);
   ~Filemaxpage (void);

   /** compress the page */
   err_info *compress (void);

public:
   // debug stuff
   char *_maxdata;  //!< maxdata for this page
   struct chunk_info _chunk;   //!< the chunk data
   struct debug_info _debug;
   };
