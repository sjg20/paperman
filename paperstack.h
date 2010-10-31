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
   File:       paperstack.h
   Started:    early on

   This file contains an implementation of a paper stack in the making.
   This is used to handle scanning, where a stack it built up one page
   at a time until it it complete.

   A threaded scanning engine is provided also, which can be told to
   start scanning, and will thereafter emit various signals as it
   progresses.
*/


#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include "qstring.h"
#include <setjmp.h>

#include "sane/sane.h"

extern "C" {
#include "jpeglib.h"
   };

#include "err.h"


class Desktopmodel;
class Filepage;
class PPage;
class QScanner;

struct max_page_info;
//struct file_info;


/******************************** jpeg decompression stuff *****************************/

typedef struct jpeg_source_info
   {
   struct jpeg_source_mgr pub;  /* public fields */
//   max_info *max;
//   chunk_info *chunk;
   PPage *page;
   } jpeg_source_info;


typedef struct jpeg_dest_info
   {
   struct jpeg_destination_mgr pub;  /* public fields */
//   max_info *max;
//   chunk_info *chunk;
   QByteArray data;
   } jpeg_dest_info;


struct my_error_mgr
   {
   struct jpeg_error_mgr mgr;
   jmp_buf setjmp_buffer;
   int err;
   };


/* holds information about a paperstack page
 */

class PPage
   {
   friend class Paperscan;
   friend class Paperstack;

public:
   /** member function which handles the JPEG call of the same name */
   void skip_input_data (long num_bytes);

private:
   /** constructor for page

      \param pagenum  the page number that this will represent when finished
      \param width    width of image
      \param height   height of image
      \param depth    depth of image in bits per pixel (1, 8 or 24)
      \param stride   bytes per line
      \param jpeg     true if data being added with addBytes is JPEG compressed
      \param blank_threshold  blank threshold 1:n */
   PPage (int pagenum, int width, int height, int depth, int stride,
         bool jpeg, int blank_threshold);
   ~PPage ();
   bool addBytes (const unsigned char *buf, int size);


   /*************************** JPEG decompression *************************/

   /** which state we are currently in */
   enum State
      {
      State_read_header,   //!< we need to read the header
      State_start,         //!< we need to start
      State_read_lines,    //!< we are reading scan lines
      State_finish,        //!< we need to finish
      State_done           //!< we are finished
      };

   /** set up ready for jpeg decompression */
   void setupJpeg (void);

   /** continue JPEG decompression with any new data we have */
   void continueJpeg (void);

   /** we have finished, so uninit */
   void finishJpeg (void);

   /** returns details about a page

      \param width    width of image
      \param height   height of image
      \param depth    depth of image in bits per pixel (1, 8 or 24)
      \param stride   bytes per line */
   void getDetails (int &width, int &height, int &depth, int &stride) const;

   /** confirm that a page will be stored

      \param pageName   page name to give this page */
   struct err_info *confirm (QString &pageName, bool mark_blank, Filepage *);

   /** compress a new page and add it to the given maxdesk */
   struct err_info *compressPage (Filepage *mp, bool mark_blank);

   /** returns the raw size of the page in bytes */
   int size (void);

   /** returns true if paper is blank */
   bool isBlank (void);

   /** returns a string representing the page coverage */
   QString coverageStr ();

   /** clear the page's buffer, releasing any memory used */
   void clear (void);

   /** returns a pointer to the page's byte array */
//    QByteArray *byteArray (void);

   /** returns the data currently available for the page

      \param data    returns pointer to data
      \param size    returns size of data */
   bool getData (const char *&data, int &size) const;

   /** returns the page number of the page

      \returns page number (from 0) */
   int pagenum (void) const { return _pagenum; }

private:
   /** return true if the given bytes indicate a blank page */
   bool checkBlank (const unsigned char *buf, int size);

private:
   int _size;     //!< size of image in bytes
   int _width;    //!< width of image
   int _height;   //!< height of image
   int _depth;    //!< depth of image
   int _stride;   //!< number of bytes per line
//   unsigned char *_buf;    //!< buffer containing image
//   int _upto;     //!< position in the buffer that we are up to
   QString _name;  //!< name of page
   bool _blank;   //!< true if page is blank
   int _blankThreshold;         //!< threshold for blank pages 1:n
   bool _jpeg;    //!< true if page is JPEG compressed by scanner (else raw data)
   int _nonblankPixels;     //!< number of non-white pixels
   int _pixels;         //!< total number of pixels
   int _pixelTarget;    //!< number of non-white pixels we need to have a non-blank page
   bool _mark_blank;    //!< true to mark page blank
//   Desktopmodel *_model;   //!< model that this page is destined for
   QByteArray _data;    //!< data bytes
   int _pagenum;        //!< the page number for this page

   /** variables to handle jpeg decompression */
   struct jpeg_decompress_struct _cinfo;
   JSAMPARRAY _buffer;/* Output row buffer */
   jpeg_source_info _source;
   struct my_error_mgr _jerr;
   int _upto;                    //!< which byte we are up to in _data
   int _to_be_skipped;           //!< number of bytes to skip
   State _state;                 //!< the current state of play
   QByteArray _decomp;           //!< destination image buffer to use for decompression
   bool _jpeg_created;           //!< true if we have created a JPEG decompressor
   int _decomp_avail;            //!< number of image bytes available from decompressor
   };


/* holds information about a single stack of paper, consisting of a number
of pages */

class Paperstack
   {
public:
   Paperstack (QString stackName, QString pageName, bool jpeg);
   ~Paperstack ();

   enum t_blankPolicy
      {
      record,
      ignoreIfBack,
      ignore
      };

   /** set the blank policy and threshold */
   void setBlankPolicy (t_blankPolicy policy, int blank_threshold);

   /** adds a new image with the given parameters - data will come later.
       The image depth also determines the number of colours.

      \param width    width of image
      \param height   height of image
      \param depth    depth of image in bits per pixel (1, 8 or 24)
      \param stride   bytes per line
      \param front    true if a front page (else back)
      \param jpeg     true to estimate size based on JPEG compression
      \returns approximate size of image in bytes, or 0 if error */
   int addImage (int width, int height, int depth, int stride, bool front, bool jpeg);

   /** adds more bytes to the current image */
   bool addImageBytes (unsigned char *buf, int size);

   /** confirm an image - add it to the stack - call this when there is
       no more data

      \return pointer to page data, or 0 if the page was blank */
   struct err_info *confirmImage (Filepage *&mp, QMutex &mutex);

   /** cancel an image - discard it - call this is a problem prevents
       the image being added to the stack */
   void cancelImage (void);

   /** returns number of pages in stack */
   int pageCount (void);

   /** compresses pages and writes a stack into a max file */
//   void compressMax (const char *fname);

   /** increment the number at the end of a string */
   void incrementName (QString &name);

   /** returns the stack name */
   QString getStackName (void);

   /** inits a paper stack, tentatively creating it in the given model */
   struct err_info *init (void);

   /** confirm a paper stack (this adds it to the viewer, etc.) */
   struct err_info *confirm (void);

   /** gets the size of a paper stack in bytes */
   int getSize (void);

   /** returns a string representing the page coverage for the currently active page */
   QString coverageStr ();

   bool isScanning (void) { return _scanning; }

   /** clears the given page, releasing all memory

      \returns    true if all pages are now cleared and the stack can be discarded */
   bool clearPage (int pagenum);

   /** returns the byte array for the current page

      \returns pointer to byte array */
//    QByteArray *byteArray (void);

   /** returns a pointer to the current page

      \returns page  current page */
   const PPage *curPage (void);

   /** cancel the stack */
   void cancel (void);

   void debug (void);

private:
   PPage *_page;              //!< current page being read
   QList<PPage *> _pages;       //!< all pages
   QString _stackName;   //!< name to give to this stack
   QString _pageName;    //!< name to give to this page
// Desk *_desk;   //!< maxdesk used for this stack
//    struct file_info *_file;
//   Desktopmodel *_model;  //!< model this stack relates to
   bool _scanning;              //!< are we scanning at the moment?
   t_blankPolicy _blankPolicy;  //!< what to do with blank pages
   int _blankThreshold;         //!< threshold for blank pages 1:n
   bool _front;                 //!< true if this is a front page (else back)
   bool _jpeg;                  //!< true if we are doing JPEG compression (else raw data)

   /** number of pages which have been cleared so far - when all done we can
       delete the stack */
   int _cleared;
   };


/**
Threaded flow for Paperscan class:

- call start () to start scan
- call cancel () to cancel scan (will cancel asap and emit stackCancelled() and scanComplete()

- new paperscan created
- this creates one or more paperstacks as it continues

- when a stack is created emit stackNew()
- after each page emit stackNewPage()
    - this passes a pointer to the new scan
    - the gui should add the page and then call slotPageAdded() to confirm/free

- when stack complete emit stackConfirm()
- if cancelled then emit stackCancel()
- when all stacks complete (regardless of whether scanning done or cancelled) then emit scanComplete()

*/

class Paperscan : public QThread
   {
   Q_OBJECT
public:
   Paperscan (QObject *parent = 0);
   ~Paperscan ();

   void setup (QScanner *scanner, QString stack_name, QString page_name);

   /** cancel scanning

      \param err     error to return from the scan once cancelled */
   void cancelScan (err_info *err);

   /** stop scanning after the current page */
   void endScan (void);

   /** free a previously scanned page */
   void pageAdded (const Filepage *mp);

   /** returns the data currently available for the current page. Note that
       if the scanner has already moved on to the next page this will return
       false.

      \param page    page to retrieve from
      \param data    returns pointer to data
      \param size    returns size of data
      \returns true if the data can be retrieved, else false */
   bool getData (const PPage *page, const char *&data, int &size);

   /** returns the page number of the current page, or -1 if it has moved to
       the next already */
   int getPagenum (const PPage *page);

   /** returns details about the current page

      \param page     current page
      \param width    width of image
      \param height   height of image
      \param depth    depth of image in bits per pixel (1, 8 or 24)
      \param stride   bytes per line
      \returns true on success, false if scanning page has changed */
   bool getPageDetails (const PPage *page, int &width, int &height,
         int &depth, int &stride);

protected:
   /** our run loop */
   void run (void);

signals:
   /** indicate that a new stack is being started

      \param stackName  suggested stack name*/
   void stackNew (const QString &stackName);

   /** indicate that a new page has been scanned

       coverageStr is valid for every page and should be stored. infoStr is
       a user string and is only non-empty when the last page of a duplex pair
       is scanned.

      \param mp      the new page in compressed form ready for maxdesk
      \param coverageStr   information about page coverage
      \param infoStr information about the page(s) for the user (e.g. coverage) */
   void stackNewPage (const Filepage *mp, const QString &coverageStr,
         const QString &infoStr);

   /** indicate that a stack is now confirmed */
   void stackConfirm (void);

   /** indicate that a stack is cancelled and should be deleted */
   void stackCancel (void);

   /** indicate that scanning is complete

      \param status  Sane status - will be reported if there is an error
      \param msg     a final message to display
      \param err     an error to display (normally NULL) */
   void scanComplete (SANE_Status status, const QString &msg, const err_info *err);

   /** provide a progress message for the user

      \param str     progress message */
   void progress (const QString &str);

   /** indicate that a new page is starting, and provide an estimate of the
       total number of bytes to be read from the scanner

      \param expected_bytes   total expected bytes from scanner for this page */
   void stackPageStarting (int expected_bytes, const PPage *page);

   /** indicate that more bytes have arrived for the current page being scanned
       These are available through Paperstack->getData() */
   void stackPageProgress (const PPage *page);

   /** provide the user with the number of bytes so far read

      \param upto    number of bytes read from scanner so far for this page */
   void progressUpto (int upto);

private:
   /** make sure that we have an active stack to scan into. If not, create
       one

       \param stack_name      stack name to use for newly created stack
       \param page_name       page name to use for pages in the stack
       \param parameters      sane parameters for pages to be scanned */
   void ensureStack (QString &stack_name, QString &page_name,
      SANE_Parameters &parameters);

   void scan (void);

   /** check if the scan should be cancelled

      \returns true if the scan should be cancelled, false if all ok */
   bool isCancelled (void);

private:
   QScanner *_scanner;        //!< the scanner we are using
   QString _stack_name;       //!< suggested stack name
   QString _page_name;        //!< suggested page name
   QString _progress_str;     //!< place to put progress information
   QString _info_str;         //!< place to put information string
   Paperstack *_stack;        //!< current stack we are scanning into
   QMutex _mutex;             //!< mutex to protect our variables
//   QWaitCondition _condition; //!<
   bool _cancel;              //!< true to cancel the scan
   err_info _cancel_err;      //!< error to return from a cancel operation
   bool _end;                 //!< true to end the scan
   };


