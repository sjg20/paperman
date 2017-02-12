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


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"

#include <QDebug>

#include "config.h"

#include "err.h"
#include "desktopmodel.h"
#include "desktopwidget.h"
#include "desk.h"
#include "file.h"
#include "filemax.h"
#include "paperstack.h"
#include "qscanner.h"
#include "qxmlconfig.h"



Paperstack::Paperstack (QString stackName, QString pageName, bool jpeg)
   {
   _page = 0;
//    _pages.setAutoDelete (true);  (not available in QList)
   _stackName = stackName;
   _pageName = pageName;
   _scanning = false;
//    _file = 0;
   _blankPolicy = record;
   _blankThreshold = 500;
   _jpeg = jpeg;
   _scanning = true;
   _cleared = 0;
   }


Paperstack::~Paperstack ()
   {
   while (!_pages.isEmpty())
      delete _pages.takeFirst();
//    qDeleteAll (_pages);
//    _pages.clear();
   }


void Paperstack::cancel (void)
   {
   _scanning = false;
   }


/*QByteArray *Paperstack::byteArray (void)
   {
   return _page->byteArray ();
   }*/


void Paperstack::debug (void)
   {
   qDebug ("(stack %d pages, plus scanning %p", _pages.size (), _page);
   for (int i = 0; i < _pages.size (); i++)
      qDebug () << "   " << i << _pages [i];
   qDebug () << "   stack)";
   }


 const PPage *Paperstack::curPage (void)
   {
   return _page;
   }


bool Paperstack::clearPage (int pagenum)
   {
   _pages [pagenum]->clear ();

   _cleared++;
//    qDebug () << "clearPage upto" << _cleared;

   // should delete the stack if all pages are clear
   if (!_scanning && _cleared == _pages.size ())
      return true;
   return false;
   }


err_info *Paperstack::confirm (void)
   {
   Q_ASSERT (_scanning);

   _scanning = false;
   return NULL;
   }


err_info *Paperstack::confirmImage (Filepage *&mp, QMutex &mutex)
   {
   bool mark_blank = false;
   mp = NULL;

   // check the blank page policy
   if (_blankPolicy != record && !_jpeg)
      {
      // if this page is blank and not required, skip
      bool blank = _page->isBlank ();

      if (blank)
         if (_blankPolicy == ignore
            || (_blankPolicy == ignoreIfBack && !_front))
            {
//            printf ("Skipped blank page\n");
            mark_blank = true;
//             cancelImage ();
//             return NULL;
            }
      }

   // This is passed back to the caller
   mp = new Filemaxpage;

   mp->setPaperstack (this);
   CALL (_page->confirm (_pageName, mark_blank, mp));

   if (_stackName.isEmpty ())
      _stackName = _pageName;
   incrementName (_pageName);
   mutex.lock ();
   _pages.append (_page);
   _page = 0;
   mutex.unlock ();
   return NULL;
   }


void Paperstack::setBlankPolicy (t_blankPolicy policy, int blank_threshold)
   {
   _blankPolicy = policy;
   _blankThreshold = blank_threshold;
   }


QString Paperstack::getStackName (void)
   {
   return _stackName;
   }


#if 0
int Paperstack::getSize (void)
   {
   return _model ? _model->getScanSize () : 0;
   }
#endif


int Paperstack::pageCount (void)
   {
   return _pages.count ();
   }


int Paperstack::addImage (int width, int height, int depth, int stride, bool front, bool jpeg)
   {
   assert (!_page);
   _page = new PPage (_pages.size (), width, height, depth, stride, _jpeg, _blankThreshold);
   _front = front;
   if (!jpeg)
      return _page->size ();

   // rough hueristics for JPEG page size
   if (depth == 8)
      return  _page->size () / 20;
   return  _page->size () / 40;
   }


bool Paperstack::addImageBytes (unsigned char *buf, int size)
   {
   assert (_page);
   _page->addBytes (buf, size);
   return true;
   }


void Paperstack::incrementName (QString &name)
   {
   util_incrementFilename (name);
//   printf ("new name %s\n", name.latin1 ());
   }


void Paperstack::cancelImage (void)
   {
   // this will be done later in pageAdded()3
//    delete _page;
   _page = 0;
   }


QString Paperstack::coverageStr ()
   {
   if (_page)
      return _page->coverageStr ();
   return "";
   }


PPage::PPage (int pagenum, int width, int height, int depth, int stride,
      bool jpeg, int blank_threshold)
   {
   _width = width;
   _height = height;
   _depth = depth;
   _stride = stride;
   _size = stride * height;
   _pagenum = pagenum;
   _jpeg = jpeg;
   _jpeg_created = false;
//   printf ("stride=%d, %dx%dx%d, size=%d\n", stride, width, height, depth, _size);

   /* in the case of JPEG, we create a smaller data buffer, but also create
      an image buffer that we can decompress into */
   if (_jpeg)
      {
      _decomp.reserve (_size);
      memset (_decomp.data (), _size, '\0');
      _size /= 2;
      }

   _data.reserve (_size);
   memset (_data.data (), _size, '\0');

   _blank = true;
   _blankThreshold = blank_threshold;
   _nonblankPixels = _pixels = 0;
   _pixelTarget = width * height;
   if (blank_threshold)
      _pixelTarget /= blank_threshold;
//    _model = model;
//printf ("pixel target %d of %d\n", _pixelTarget, width * height);
   if (_jpeg)
      setupJpeg ();
   }


PPage::~PPage ()
   {
   clear ();
   }


/*QByteArray *PPage::byteArray (void)
   {
   return &_data;
   }*/


void PPage::getDetails (int &width, int &height, int &depth, int &stride) const
   {
   width = _width;
   height = _height;
   depth = _depth;
   stride = _stride;
   }


bool PPage::getData (const char *&data, int &size) const
   {
   /* for JPEG we supply the decompressed data */
   const QByteArray &ba = _jpeg ? _decomp : _data;

   data = ba.constData ();
   size = ba.size ();

   /* hack to get around a fault in QByteArray(). Calling resize() causes the
      data to be realloced (which we don't want) even when we have called
      reserve() previously */
   if (_jpeg)
      size = _decomp_avail;

   return true;
   }


void PPage::clear (void)
   {
   _data.clear ();
   _decomp.clear ();
   if (_jpeg)
      finishJpeg ();
   }


bool PPage::isBlank (void)
   {
   return _blank;
   }


static void init_source(j_decompress_ptr cinfo)
   {
   cinfo = cinfo;
   }


static boolean fill_input_buffer (j_decompress_ptr cinfo)
   {
   UNUSED (cinfo);
   // we have no more data to give!
   return FALSE;
   }


void PPage::skip_input_data (long num_bytes)
   {
   if (num_bytes > (int)_source.pub.bytes_in_buffer)
      {
      _to_be_skipped = num_bytes - _source.pub.bytes_in_buffer;
      num_bytes = _source.pub.bytes_in_buffer;
      }
   _source.pub.next_input_byte += (size_t) num_bytes;
   _source.pub.bytes_in_buffer -= (size_t) num_bytes;
   }


static void skip_input_data (j_decompress_ptr cinfo, long num_bytes)
   {
   jpeg_source_info *src = (jpeg_source_info *)cinfo->src;
   PPage *page = src->page;

   page->skip_input_data (num_bytes);
   }


static void term_source (j_decompress_ptr cinfo)
   {
   cinfo = cinfo;
   }


static void my_error_exit (j_common_ptr cinfo)
   {
   struct my_error_mgr* myerr = (struct my_error_mgr*) cinfo->err;
   char buffer[JMSG_LENGTH_MAX];

   (*cinfo->err->format_message)(cinfo, buffer);
   qDebug () << "** JPEG error" << buffer;
   myerr->err = 1;
   }


void PPage::setupJpeg (void)
   {
   int i;

   jpeg_create_decompress (&_cinfo);
   _jpeg_created = true;
   _cinfo.err = jpeg_std_error (&_jerr.mgr);

   _cinfo.src = &_source.pub;
   _source.page = this;
   _source.pub.init_source = init_source;
   _source.pub.fill_input_buffer = ::fill_input_buffer;
   _source.pub.skip_input_data = ::skip_input_data;
   _source.pub.resync_to_restart = jpeg_resync_to_restart;/* use default method */
   _source.pub.term_source = term_source;
   _source.pub.bytes_in_buffer = 0;   /* forces fill_input_buffer on first read */
   _source.pub.next_input_byte = NULL;/* until buffer loaded */

   _cinfo.err = jpeg_std_error (&_jerr.mgr);
   _jerr.err = 0;
   _jerr.mgr.error_exit = my_error_exit;
   _to_be_skipped = 0;
   _upto = 0;
   _decomp_avail = 0;

   /* Make a sample array as required by the jpeg library */
   _buffer = new JSAMPROW [_height];
   for (i = 0; i < _height; i++)
      _buffer [i] = (JSAMPLE *)(_decomp.data () + _stride * i);

   _state = State_read_header;
   }


void PPage::continueJpeg ()
   {
   int nread;

//    qDebug () << "continueJpeg state" << _state << "avail" << _data.size () << "upto" << _upto;
   _source.pub.next_input_byte = (const JOCTET *)(_data.data () + _upto);
   _source.pub.bytes_in_buffer = _data.size () - _upto;;

   switch (_state)
      {
      case State_read_header:
         if (jpeg_read_header (&_cinfo, TRUE) == JPEG_SUSPENDED)
            break;
         _state = State_start;
         // no break

      case State_start:
         if (!jpeg_start_decompress (&_cinfo))
            break;
         _state = State_read_lines;
         // no break

      case State_read_lines :
         while (_cinfo.output_scanline < _cinfo.output_height)
            {
            nread = jpeg_read_scanlines (&_cinfo, _buffer + _cinfo.output_scanline,
               _cinfo.output_height - _cinfo.output_scanline);
//             qDebug () << "nread" << nread << _cinfo.output_scanline << "of" << _cinfo.output_height;
            if (!nread)
               break;
            }
         _decomp_avail = _cinfo.output_scanline * _stride;
         if (_cinfo.output_scanline < _cinfo.output_height)
            break;

         _state = State_finish;
         // no break

      case State_finish :
//          qDebug () << "finish";
         jpeg_finish_decompress (&_cinfo);
          // JPEG library seems to return false even when it has all the data
//           break; so we don't check the return value
         _state = State_done;
         // no break

      case State_done :
         break;
      }

   _upto = (char *)_source.pub.next_input_byte - _data.data ();
//    qDebug () << "finished: _upto" << _upto;
   }


void PPage::finishJpeg (void)
   {
   if (_jpeg_created)
      {
      jpeg_destroy_decompress (&_cinfo);
      if (_jerr.err)
         qDebug () << "JPEG error" << _jerr.err;
      delete _buffer;
      _jpeg_created = false;
      }
   }


#define RGB_THRESHOLD 240
#define GREY_THRESHOLD 251


bool PPage::checkBlank (const unsigned char *buf, int size)
   {
   // number of bits set in a 4-bit number
   static char bit_count [16] =
      {
      0, 1, 1, 2, 1, 2, 2, 3,
      1, 2, 2, 3, 2, 3, 3, 4
      };
   int count = 0, pixels;
   const unsigned char *end = buf + size;

   // scan the buffer counting the number of non-blank pixels
   switch (_depth)
      {
      case 1 :
         for (; buf < end; buf++)
            count += bit_count [*buf & 0xf] + bit_count [*buf >> 4];
         break;

      case 8 :
         for (; buf < end; buf++)
            if (*buf < GREY_THRESHOLD)
               count++;
         break;

      case 24 :
         for (; buf < end; buf += 3)
            if (buf [0] < RGB_THRESHOLD
               || buf [1] < RGB_THRESHOLD
               || buf [2] < RGB_THRESHOLD)
               count++;
         break;
      }

   // work out total pixels in this block
   pixels = size * 8 / _depth;

   // if more than 1 in COVERAGE pixels are blank, consider it blank
//   printf ("count = %d, pixels = %d\n", count, pixels);

   _nonblankPixels += count;
   _pixels += pixels;
//printf ("pixels = %d, non blank = %d\n",_pixels,  _nonblankPixels);
   if (_nonblankPixels < _pixelTarget)
      return true;

   // otherwise not blank
   return false;
   }


bool PPage::addBytes (const unsigned char *buf, int size)
   {
   if (_data.size () + size <= _size)
      {
      _data.append (QByteArray ((const char *)buf, size));
      if (_jpeg)
         {
         int avail = _decomp_avail;

         continueJpeg ();
         if (!checkBlank ((const unsigned char *)_decomp.data () + avail, _decomp_avail - avail))
            _blank = false;
         }
      else if (!checkBlank (buf, size))
         _blank = false;
      return true;
      }
   return false;
   }


err_info *PPage::confirm (QString &pageName, bool mark_blank, Filepage *mp)
   {
   if (pageName.right (1) == "_")
      pageName.truncate (pageName.length () - 1);
   _name = pageName;
   _mark_blank = mark_blank;
//   printf ("confirmed %s\n", _name.latin1 ());
//   printf ("page complete: %dx%dx%d @%d, size %d/%d, short %d\n", _width, _height,
//      _depth, _stride, _upto, _size, _size - _upto);
   return compressPage (mp, mark_blank);
   }


int PPage::size (void)
   {
   return _size;
   }


err_info *PPage::compressPage (Filepage *mp, bool mark_blank)
   {
//   printf ("final count = %d, pixels = %d\n", _nonblankPixels, _pixels);
   mp->addData (_width, _height, _depth, _stride, _name, _jpeg, mark_blank, _pagenum, _data,
      _jpeg ? -1 : _size);
   return mp->compress ();
#if 0
   mp->width = _width;
   mp->height = _height;
   mp->depth = _depth;
   mp->stride = _stride;
   mp->titlestr = _name.isNull () ? NULL : _name.latin1 ();
   mp->jpeg = _jpeg;
   mp->mark_blank = mark_blank;
   mp->pagenum = _pagenum;
//    CALL (max_compress_page (mp, _buf, _jpeg ? _upto : _size));
   CALL (max_compress_page (mp, (byte *)_data.constData (), _jpeg ? _data.size () : _size));
#endif

   // we shouldn't do this until the receiving thread has acknowledged it
//    _data.clear ();
//    delete _buf;
//    _buf = 0;
   return NULL;
   }


QString PPage::coverageStr ()
   {
   double cov;

   // can't work out coverage from JPEG data
//    if (_jpeg)
//       return "";

   // work out coverage
   cov = (double)_nonblankPixels / _pixels;
   if (_nonblankPixels == 0)
      return "0";

   // if more than 0.1%, use x.y% notation
   if (cov >= 0.001)
      return QString ("%1%").arg (cov * 100, 0, 'f', 1);

   cov = 1 / cov;

#define COVERAGE_MAX 10000
   // if more than 1:COVERAGE_MAX use that
   if (cov > 10000)
      return "<1:10000";

   return QString ("1:%1").arg ((int)cov);
   }


Paperscan::Paperscan (QObject *parent)
      : QThread (parent)
   {
   _stack = 0;
   _scanner = 0;
   _cancel = false;
   }


Paperscan::~Paperscan ()
   {
   }


void Paperscan::ensureStack (QString &stack_name, QString &page_name,
      SANE_Parameters &parameters)
   {
   if (!_stack)
      {
      _mutex.lock ();
      // start a new stack
#ifdef CONFIG_sane_jpeg
      _stack = new Paperstack (stack_name, page_name, parameters.format == HACK_SANE_FRAME_JPEG);
#else
      _stack = new Paperstack (stack_name, page_name, FALSE);
#endif
      _mutex.unlock ();

      _stack->setBlankPolicy ((Paperstack::t_blankPolicy)xmlConfig->intValue("SCAN_BLANK"),
               xmlConfig->intValue("SCAN_BLANK_THRESHOLD"));
      emit stackNew (stack_name);
      }
   }


void Paperscan::scan ()
   {
   int len, total, todo, size;
   SANE_Status status;
   unsigned char *buf;
   bool done = FALSE;
   SANE_Parameters parameters;
   int numsides, side;
   int total_sides = 0;  // total number of sides scanned
   int total_blank = 0;  // total number of blank sides scanned
   int stack_count = 0;  // total number of stacks created
//    Paperstack *stack = 0;
   _stack = 0;
   bool adf;  // true if using an auto document feeder
   err_info *err;
   int bpp, busy_count, image_bpp;
   bool is_jpeg;
   QString side0_str;
   int stack_limit; // maximum number of pages per stack

//    qDebug () << "scan start";

   if (!_scanner)
      return;

   // check resolution, etc.
   status = _scanner->getParameters (&parameters);
   if (status != SANE_STATUS_GOOD)
      {
      _progress_str = "";
      emit scanComplete (status, _progress_str, 0);
      return;
      }

//    _watchButtons = false;
   adf = _scanner->useAdf ();
   numsides = adf && _scanner->duplex () ? 2 : 1;

   size = 256 * 1024;
   buf = (unsigned char *)malloc (size);
   bpp = parameters.bytes_per_line * 8 / parameters.pixels_per_line;

   _progress_str = "Starting...";
   emit progress (_progress_str);

   _end = false;

   stack_limit = xmlConfig->intValue ("SCAN_STACK_COUNT");
   err = NULL;
   if (!err) do
      {
      // read value here, to allow user to change it during scan
      int single = xmlConfig->intValue ("SCAN_SINGLE");

      status = SANE_STATUS_GOOD;
      side0_str = "";
      for (side = 0; status == SANE_STATUS_GOOD && side < numsides && !isCancelled (); side++)
         {
         // create a paper stack if required
         ensureStack (_stack_name, _page_name, parameters);

//         printf ("side = %d\n", side);
         QString str;

         str = QString ("Scanning page %1").arg (total_sides + 1);
         if (stack_count > 0)
            str += QString (" stack %1").arg (stack_count + 1);
         emit progress (str);

         status = SANE_STATUS_DEVICE_BUSY;
         for (busy_count = 0; status == SANE_STATUS_DEVICE_BUSY && busy_count < 30 && !isCancelled ();
                busy_count++)
            {
            if (busy_count)
               usleep (10000);
            status = _scanner->start ();
            if (status == SANE_STATUS_INVAL && !busy_count)
               {
               // did we forget to cancel last time?
               _scanner->cancel ();
               status = _scanner->start ();
               }
//            printf ("status = %d\n", status);
            }
         if (status != SANE_STATUS_GOOD || isCancelled ())
            break;
         total = 0;
         image_bpp = parameters.format == SANE_FRAME_RGB ? 24
                    : parameters.depth;
         is_jpeg = false;
#ifdef CONFIG_sane_jpeg
         if (parameters.format == HACK_SANE_FRAME_JPEG)
            {
            image_bpp = bpp;
            is_jpeg = true;
            }
#endif
         int expected_bytes = _stack->addImage (parameters.pixels_per_line, parameters.lines,
                image_bpp,
                 parameters.bytes_per_line, side == 0, is_jpeg);
         emit stackPageStarting (expected_bytes, _stack->curPage ());
//          qDebug () << "emit stackPageStarting page" << _stack->curPage ()->pagenum ();

         int i, steps = 0;
         for (i = 0; status == SANE_STATUS_GOOD && !isCancelled (); i++)
            {
            todo = size;
            status = _scanner->read (buf, todo, &len);
//             printf ("status = %d, len = %d, total = %d\n", status, len, total + len);
            if (status != 0)
               break;

            _mutex.lock ();
            _stack->addImageBytes (buf, len);
            _mutex.unlock ();
            total += len;
            steps++;
//             qDebug () << "thread up to " << total;
            emit stackPageProgress (_stack->curPage ());
            }

         // end of file is ok - indicates we have an image
         if (status == SANE_STATUS_EOF && total && !isCancelled ())
            {
            QString cov = _stack->coverageStr ();

            if (side == 0 && numsides > 1)
               side0_str = cov;
            else
               {
               _info_str = cov;
               if (_info_str.isEmpty ())
                  ;
               else if (numsides == 1)
                  _info_str = QString ("Coverage %1").arg (_info_str);
               else
                  _info_str = QString ("Coverage %1 / %2").arg (side0_str).arg (_info_str);
               }
            Filepage *mp;

            // mp is destroyed by the receive, we do not destroy it here
            err = _stack->confirmImage (mp, _mutex);
            if (err)
               break;

            if (mp)
               emit stackNewPage (mp, cov, _info_str);
               //    emit progressSize (_stack->getSize ());
            if (mp->markBlank ())
               // page not confirmed, so it is blank
               total_blank++;
            status = SANE_STATUS_GOOD;
            total_sides++;
            }
         else
            {
            QMutexLocker locker (&_mutex);

            _stack->cancelImage ();
            }
//         printf ("status=%d, total=%d\n", status, total);

         // if we've hit the stack limit, start a new one
         if (stack_limit && _stack->pageCount () >= stack_limit && !isCancelled ())
            {
            // if we managed to scan anything, add it to the viewer
            if (_stack->pageCount ())
               {
               _stack->confirm ();
               emit stackConfirm ();
               stack_count++;
               }

            // start a new stack next time
//             delete _stack;  (pageAdded() does this now)
            _stack = 0;
            }

         // don't scan the other side if we only want 1 page
         if (single && total_sides >= single)
             break;
         }
      if (status != SANE_STATUS_GOOD)
         {
//          printf ("not good status=%d\n", status);
         break;
         }

//       status = _scanner->getParameters(&parameters);
//      printf ("status=%d, last=%d\n", status, parameters.last_frame == SANE_TRUE);
//      done = parameters.last_frame == SANE_TRUE;

      // work out whether to scan more sheets
      done = !adf || (single && total_sides >= single);

      } while (!done && !_end && !err && !isCancelled ());

   _scanner->cancel ();

   // if we managed to scan anything, add it to the viewer
   if (_stack && _stack->pageCount () && !isCancelled ())
      {
      _stack->confirm ();
      emit stackConfirm ();
      stack_count++;
      }

   QString str;

   // if we did scan pages but they were all blank...
   if (total_sides == total_blank)
      str = tr ("All pages blank");
   else
      {
      str = QString (tr ("Scanned %n page(s)", "", total_sides));
      if (total_blank)
         str += QString (tr (" (%1 blank)")).arg (total_blank);
      if (stack_count > 1)
         str += QString (tr (" into %1 stacks")).arg (stack_count);
      }

   // supress errors about running out of documents if we actually got some
   if (status == SANE_STATUS_NO_DOCS && numsides > 0)
      status = SANE_STATUS_GOOD;

   if (isCancelled ())
      err = &_cancel_err;

   // if we are in the middle of a scan, cancel it
   if (_stack && _stack->isScanning ())
      {
      emit stackCancel ();
      _stack->cancel ();
      str = tr ("Scan cancelled - %n page(s) discarded", "", total_sides);
      }

//    delete _stack;  (pageAdded() does this now)
   _mutex.lock ();
   _stack = 0;
   _mutex.unlock ();

//    qDebug () << "emitting scanComplete";
   emit scanComplete (status, str, err);
   free (buf);

/*
   printf ("option count=%d\n", _scanner->optionCount ());
   for (int i = 0; i < _scanner->optionCount (); i++)
      {
      printf ("%d: %s\n", i, _scanner->getOptionName (i));
      }
*/
   }


void Paperscan::setup (QScanner *scanner, QString stack_name, QString page_name)
   {
   QMutexLocker locker (&_mutex);

   _scanner = scanner;
   _stack_name = stack_name;
   _page_name = page_name;
   }


/* this is our main thread */
void Paperscan::run (void)
   {
//    qDebug () << "run";

   // scan
   scan ();

   }


void Paperscan::cancelScan (err_info *err)
   {
   QMutexLocker locker (&_mutex);

   Q_ASSERT (err);
//    qDebug () << "cancelScan";
   _cancel = true;
   _cancel_err = *err;
   }


void Paperscan::endScan (void)
   {
   QMutexLocker locker (&_mutex);

   _end = true;
   }


bool Paperscan::isCancelled (void)
   {
   QMutexLocker locker (&_mutex);

   return _cancel;
   }


void Paperscan::pageAdded (const Filepage *mp)
   {

//    qDebug () << "pageAdded" << mp->pagenum;
   Paperstack *stack = mp->stack ();

   // clear this page, and the whole stack if this was the last page
   if (stack->clearPage (mp->pagenum ()))
      {
//       qDebug () << "deleting stack";
      delete stack;
      }
   delete mp;
   }


bool Paperscan::getData (const PPage *page, const char *&data, int &size)
   {
   QMutexLocker locker (&_mutex);

   if (!_stack || page != _stack->curPage ())
      return false;
//    qDebug ("getData page = %p", page);
//    _stack->debug ();

   return page->getData (data, size);
   }


int Paperscan::getPagenum (const PPage *page)
   {
   QMutexLocker locker (&_mutex);

   if (!_stack || page != _stack->curPage ())
      return -1;
   return page->pagenum ();
   }


bool Paperscan::getPageDetails (const PPage *page, int &width, int &height,
      int &depth, int &stride)
   {
   QMutexLocker locker (&_mutex);

   if (!_stack || page != _stack->curPage ())
      return false;
   page->getDetails (width, height, depth, stride);
   return true;
   }
