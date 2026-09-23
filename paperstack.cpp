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
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"

#include <QDir>
#include <QFile>
#include <QDateTime>
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

/* A pixel counts towards coverage when it is dark enough that the
   lineart conversion would make it black, so a page reports a similar
   coverage whether scanned in colour, grey or mono. This matches the
   default lineart threshold (mid-grey). */
#define COVERAGE_THRESHOLD 128

/* A pixel is coloured when the spread between its strongest and weakest
   of red, green and blue is at least a fifth of the strongest, so a dark
   colour counts as much as a bright one, and it is not so dark that the
   spread is just noise. A page counts as colour when at least
   COLOUR_FRACTION of its pixels are: scanned text and line art give under
   0.5% by this measure, from colour fringes at edges and the JPEG the
   scanner sends, and anything with a coloured mark on it several percent */
#define SATURATION_DIVISOR 5
#define SATURATION_MIN_BRIGHT 40
#define COLOUR_FRACTION 0.01

/* That measure alone cannot find a small mark, such as a stamp with a
   name and address on it: the fringes along the edges of ordinary black
   print come to 0.7% of a page by themselves, so nothing under a
   percent can be believed. A fringe is a pixel or two wide, though,
   while ink laid on the page is coloured through and through, so count
   as well the pixels with colour all round them, within COLOUR_RADIUS.
   Ink showing through from the back of the sheet is mottled and does
   not hold its colour over a whole neighbourhood, so it is left out
   with the fringes: pages of print give none of this at all, and the
   back of the stamped sheet 0.018%, against 0.17% for the stamp
   itself */
#define COLOUR_RADIUS INTERIOR_RADIUS
#define COLOUR_SOLID_FRACTION 0.0005

/* Both of those measures look at the page as a whole, and a note
   written on it is a few thin strokes in one corner: too little of the
   page to tell from the fringes of the print, and too thin to hold its
   colour all round the way a stamp pressed on the page does. What
   marks writing out is what it is made of. Ink laid on the page is
   strongly coloured, dark as ink is, and the paper beside it is white,
   within COLOUR_PAPER_RADIUS. Ink which has soaked through from the
   back of the sheet is strongly coloured too, but it has stained the
   paper it came through, so there is no white beside it, and the
   fringe along the edge of black print is neither strong nor dark.

   Count those pixels in each COLOUR_TILE-square tile of the sheet and
   take the densest, since writing is in one place rather than spread
   over the page: a tile with a few pen strokes across it gives around
   1%, a page of print none at all, and the back of a sheet with a blue
   stamp on it under a tenth of that */
#define COLOUR_STRONG 60
#define COLOUR_INK_LUM 100
#define COLOUR_PAPER_RADIUS INTERIOR_RADIUS
#define COLOUR_TILE 128
#define COLOUR_TILE_FRACTION 0.003

/* Without colour, a page is grey rather than mono when enough of it lies
   in the mid-tones inside filled regions: pixels with ink (darker than
   INK_THRESHOLD, so print showing through from the back of the sheet
   does not count) whose whole (2 * INTERIOR_RADIUS + 1)-square
   neighbourhood has ink too, and which are themselves no darker than
   MID_THRESHOLD. Text is thin strokes, so almost none of its pixels lie
   inside a region, and a bold heading is solid black rather than
   mid-toned, while a photograph qualifies across its area: 7% of a book
   page for an illustration, 3% for a small one, against under 0.2% for
   text. The scanner leaves a shadow of the paper edge along the top and
   bottom of the page, a mid-grey band that would count, so the first and
   last EDGE_ROWS rows are left out. The window is wider than the sheet
   and its backing is mid-grey too, with a soft edge where the sheet
   ends, which on its own gives a text page five times the interior
   pixels its text does, so each row is counted only between its
   paper-bright edges, less SHEET_INSET to clear the shade of that edge */
#define INK_THRESHOLD 200
#define PALE_THRESHOLD COVERAGE_THRESHOLD
#define MID_THRESHOLD 64
#define INTERIOR_RADIUS 2
#define INTERIOR_ROWS (2 * INTERIOR_RADIUS + 1)
#define EDGE_ROWS 32
#define SHEET_INSET 16
#define GREY_FRACTION 0.01

/* A picture is a solid area of ink, whatever its tones: a photograph
   has mid-tones and is found by the rule below, but an engraving or a
   line drawing is ink and paper with nothing in between and reads like
   print. Print is thin strokes, though, so a block FILL_CELL square
   with ink in every pixel of it is not print. Counting those blocks
   over the whole page would lose a small picture among the white around
   it, so the page is divided into tiles FILL_TILE blocks square and the
   densest tile decides: a page of print fills under a twelfth of a
   tile, an engraving three fifths of one and a photograph all of it */
#define FILL_CELL 8
#define FILL_TILE 16
#define FILL_TILE_CELLS (FILL_TILE * FILL_TILE)
#define FILL_FRACTION 0.25

/* Handwriting in pencil, and a faded stamp, are pale all through, and
   storing the page as mono would throw them away: everything lighter
   than COVERAGE_THRESHOLD is dropped. A page is grey as well, then,
   when enough of it is ink that mono drops with nothing it keeps within
   INTERIOR_RADIUS to stand in for it. Print never gives this, however
   fine: its pale pixels are the edges of strokes whose middle is
   solid. A page of text gives under 0.002% of itself, a pencil note
   across the head of a page 0.06%.

   A run of pale ink too long to be a stroke is not writing at all: it
   is a printed rule, or the shade the edge of the sheet casts, which
   lies across the window wherever the back end is not cutting the page
   down to the sheet. Handwriting stays well within a hundredth of the
   window, so longer runs are left out. That shade is patchy, breaking
   into pieces a stroke's length, so a gap of up to a four-hundredth of
   the window does not break a run: joined up again, the pieces show
   themselves for what they are while writing is untouched */
#define SOFT_FRACTION 0.0001
#define SOFT_MAX_RUN(width) ((width) / 100)
#define SOFT_GAP(width) ((width) / 400)

/* The scanner scans the full width of its window, so a sheet narrower
   than the window leaves the backing showing beyond its edge. The
   backing is a shade of grey and never reaches paper white, while the
   sheet is paper-bright somewhere along every line of it, even a blank
   one, so the brightest pixel of a line says whether the sheet is still
   there. Keep at least this much of the window, in case a page really
   is dark to its edges */
#define PAPER_BRIGHT 250
#define PAPER_MIN_FRACTION 0.25

/* How much of a column has to be ink for the sheet to be there where
   it is not bright. A cover is often printed right to the edge of the
   sheet, leaving nothing paper-bright to find it by, while the backing
   beyond it holds no ink at all: on the covers of three books not one
   column of backing came near this, and the sheet found this way is
   the same width as the body pages of the book */
#define PAPER_INK_FRACTION 0.1

/* How much of a line across the sheet has to be paper-bright for the
   sheet to be there. Anything beyond its edge can still catch the odd
   bright patch, so a single pixel is not enough; the edge itself is
   sharp, going from nothing to almost the whole line within about
   fifteen lines, so asking for half loses only a few of them */
#define PAPER_COLUMN_FRACTION 0.5

/* A sheet which went through the feeder a little askew has its corners
   out beyond the rest of it, in columns which hold only a few lines of
   paper each: asking for half a column of paper there cuts the corners
   off. Once the edge has been found, follow it outwards while the next
   column still holds some paper, which walks along the corner and
   stops where the sheet does. A speck on the backing cannot pull the
   edge out to itself, since the walk stops at the first column with no
   paper in it at all */
#define PAPER_EDGE_FRACTION 0.02
#define PAPER_EDGE_MOST 0.05



Paperstack::Paperstack (QString stackName, QString pageName, bool jpeg)
   {
   _page = 0;
   _page_back = 0;
//    _pages.setAutoDelete (true);  (not available in QList)
   _stackName = stackName;
   _pageName = pageName;
   _scanning = false;
//    _file = 0;
   _blankPolicy = record;
   _blankThreshold = 500;
   _autoColour = false;
   _autoSize = false;
   _sideways = Sideways_no;
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
   qDebug ("(stack %d pages, plus scanning %p", (int)_pages.size (), _page);
   for (int i = 0; i < _pages.size (); i++)
      qDebug () << "   " << i << _pages [i];
   qDebug () << "   stack)";
   }


 const PPage *Paperstack::curPage (void)
   {
   return _page;
   }


 const PPage *Paperstack::curPageBack (void)
   {
   return _page_back;
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

   /* check the blank page policy. The pixels behind it are counted as
      the page arrives, the scanner's JPEG decompressed as it comes, so
      this works for a colour page too: it used not to, back when the
      JPEG was stored without ever being looked at */
   if (_blankPolicy != record)
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


err_info *Paperstack::confirmImageBack (Filepage *&mp, QMutex &mutex)
   {
   bool mark_blank = false;
   mp = NULL;

   /* mirrors confirmImage() but for the back-side page in a progressive
    * duplex scan. ignoreIfBack always treats this as a back page. */
   if (_blankPolicy != record)
      {
      bool blank = _page_back->isBlank ();
      if (blank)
         if (_blankPolicy == ignore || _blankPolicy == ignoreIfBack)
            mark_blank = true;
      }

   mp = new Filemaxpage;
   mp->setPaperstack (this);
   CALL (_page_back->confirm (_pageName, mark_blank, mp));

   if (_stackName.isEmpty ())
      _stackName = _pageName;
   incrementName (_pageName);
   mutex.lock ();
   _pages.append (_page_back);
   _page_back = 0;
   mutex.unlock ();
   return NULL;
   }


void Paperstack::setAutoColour (bool on)
   {
   _autoColour = on;
   }


void Paperstack::setSideways (t_sideways how)
   {
   _sideways = how;
   }


void Paperstack::setAutoSize (bool on)
   {
   _autoSize = on;
   }


PPage::Rotate Paperstack::rotationFor (bool front) const
   {
   if (_sideways == Sideways_no)
      return PPage::Rotate_none;

   /* the top of the page is at one side of the front and, the sheet being
      seen from behind, at the other side of the back. With the top at the
      left, a quarter turn clockwise brings it to the top */
   bool top_left = (_sideways == Sideways_top_left) == front;

   return top_left ? PPage::Rotate_cw : PPage::Rotate_ccw;
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
   _front = front;
   _page = new PPage (_pages.size (), width, height, depth, stride, _jpeg,
                      _blankThreshold, _autoColour, _autoSize,
                      rotationFor (front));
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


int Paperstack::addImageBack (int width, int height, int depth, int stride, bool jpeg)
   {
   assert (_page);       /* must be paired with a front image */
   assert (!_page_back);
   /* assign sequential page numbers so the back lands right after the
    * front in the stack list. */
   _page_back = new PPage (_pages.size () + 1, width, height, depth, stride,
                           _jpeg, _blankThreshold, _autoColour, _autoSize,
                           rotationFor (false));
   if (!jpeg)
      return _page_back->size ();
   if (depth == 8)
      return _page_back->size () / 20;
   return _page_back->size () / 40;
   }


int Paperstack::restartBack (int width, int height, int depth, int stride,
                             bool jpeg)
   {
   assert (_page_back);
   assert (_page_back->_data.isEmpty ());
   delete _page_back;
   _page_back = NULL;
   return addImageBack (width, height, depth, stride, jpeg);
   }


bool Paperstack::addImageBytesBack (unsigned char *buf, int size)
   {
   assert (_page_back);
   _page_back->addBytes (buf, size);
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


QString Paperstack::kindStr ()
   {
   if (_page)
      return _page->kindStr ();
   return "";
   }


QString Paperstack::coverageStrBack ()
   {
   if (_page_back)
      return _page_back->coverageStr ();
   return "";
   }


PPage::PPage (int pagenum, int width, int height, int depth, int stride,
      bool jpeg, int blank_threshold, bool auto_colour, bool auto_size,
      Rotate rotate)
   {
   _autoColour = auto_colour;
   _autoSize = auto_size;
   _rotate = rotate;
   _colourPixels = 0;
   _fill_max = 0;
   _colour_rows = 0;
   _col = 0;
   _bright_cols.clear ();
   _ink_cols.clear ();
   _colourBand [0] = _colourBand [1] = _colourBand [2] = 0;
   _row_x = _row_skip = _rows_done = _partial_len = 0;
   _row_gap = -INTERIOR_ROWS;
   /* a back end that finds the foot of the sheet as it scans (the fujitsu
      backend with ald) cannot say the height when the page starts and
      reports -1. Size the buffers for a letter-shaped page for now; the
      real height comes from the JPEG header, or for raw data from how
      much arrives before the end of the page */
   _height_known = height > 0;
   if (!_height_known)
      height = width * 13 / 10;
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
      memset (_decomp.data (), '\0', _size);
      _size /= 2;
      }

   _data.reserve (_size);
   memset (_data.data (), '\0', _size);

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
   return false;
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


/* A scanner which stops at the foot of the sheet sends a page shorter
   than the height its JPEG promised, and the decoder fills what is
   left with grey rather than saying it has finished. It does say so in
   a warning, though, and where it says it is where the page ends */
static void my_emit_message (j_common_ptr cinfo, int msg_level)
   {
   if (msg_level < 0 && cinfo->is_decompressor)
      {
      j_decompress_ptr dinfo = (j_decompress_ptr)cinfo;
      jpeg_source_info *src = (jpeg_source_info *)dinfo->src;
      char buffer [JMSG_LENGTH_MAX];

      (*cinfo->err->format_message) (cinfo, buffer);
      if (src && src->page)
         src->page->dataRanOut (dinfo->output_scanline);
      qCDebug (logErr, "JPEG: %s", buffer);
      }
   }


void PPage::dataRanOut (int lines)
   {
   if (_data_lines < 0)
      _data_lines = lines;
   }


static void my_error_exit (j_common_ptr cinfo)
   {
   struct my_error_mgr* myerr = (struct my_error_mgr*) cinfo->err;
   char buffer[JMSG_LENGTH_MAX];

   (*cinfo->err->format_message)(cinfo, buffer);
   qDebug () << "** JPEG error" << buffer;
   myerr->err = 1;

   /* the library says this must not come back, and means it: what it
      does afterwards is undefined, and it decoded a page of nonsense
      when it was let to carry on */
   longjmp (myerr->setjmp_buffer, 1);
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
   _jerr.mgr.emit_message = my_emit_message;
   _to_be_skipped = 0;
   _data_lines = -1;
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

   if (_state == State_done || _jerr.err)
      return;

   /* A marker can say to skip more than has arrived so far, and the
      rest of that skip has to come out of what arrives next: without
      it the decoder starts again in the middle of the marker, reads
      the page as one long run of nonsense and stops part way down */
   if (_to_be_skipped)
      {
      int skip = qMin ((int)_to_be_skipped, _data.size () - _upto);

      _upto += skip;
      _to_be_skipped -= skip;
      if (_to_be_skipped)
         return;     // still inside it: nothing to decode yet
      }

//    qDebug () << "continueJpeg state" << _state << "avail" << _data.size () << "upto" << _upto;
   _source.pub.next_input_byte = (const JOCTET *)(_data.data () + _upto);
   _source.pub.bytes_in_buffer = _data.size () - _upto;;

   /* the error handler comes back here rather than returning into the
      library, whose state is not to be relied on after one */
   if (setjmp (_jerr.setjmp_buffer))
      {
      qWarning () << "page" << _pagenum << ": the scanner's JPEG data is"
                  << "damaged; keeping the" << _decomp_avail / qMax (_stride, 1)
                  << "lines which came out of it";
      _state = State_done;
      return;
      }

   switch (_state)
      {
      case State_read_header:
         if (jpeg_read_header (&_cinfo, true) == JPEG_SUSPENDED)
            break;
         if (!_height_known)
            setHeight (_cinfo.image_height);
         _state = State_start;
         Q_FALLTHROUGH();

      case State_start:
         if (!jpeg_start_decompress (&_cinfo))
            break;
         _state = State_read_lines;
         Q_FALLTHROUGH();

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
         Q_FALLTHROUGH();

      case State_finish :
//          qDebug () << "finish";
         jpeg_finish_decompress (&_cinfo);
          // JPEG library seems to return false even when it has all the data
//           break; so we don't check the return value
         _state = State_done;
         Q_FALLTHROUGH();

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
      delete[] _buffer;
      _jpeg_created = false;
      }
   }




bool PPage::checkBlank (const unsigned char *buf, int size)
   {
   // number of bits set in a 4-bit number
   static char bit_count [16] =
      {
      0, 1, 1, 2, 1, 2, 2, 3,
      1, 2, 2, 3, 2, 3, 3, 4
      };
   int count = 0, pixels = 0;
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
            if (*buf < COVERAGE_THRESHOLD)
               count++;
         break;

      case 24 :
         {
         /* use luminance, as the greyscale/lineart conversion does, so
            the three modes agree. Count the coloured pixels in the same
            pass, for kind(), and hand the ink flags on by row for the
            filled-region test */
         int colour = 0;

         while (buf < end)
            {
            /* the tail of a row is padding when the stride exceeds it */
            if (_row_skip)
               {
               int skip = qMin (_row_skip, (int)(end - buf));

               buf += skip;
               _row_skip -= skip;
               continue;
               }

            /* raw data arrives in chunks of any length, so a pixel can be
               split across two: carry its first bytes over, or the rest
               of the page is read a byte or two out of step and every
               edge looks coloured */
            const unsigned char *px;

            if (_partial_len)
               {
               while (_partial_len < 3 && buf < end)
                  _partial [_partial_len++] = *buf++;
               if (_partial_len < 3)
                  break;
               px = _partial;
               _partial_len = 0;
               }
            else if (end - buf < 3)
               {
               while (buf < end)
                  _partial [_partial_len++] = *buf++;
               break;
               }
            else
               {
               px = buf;
               buf += 3;
               }
            int lum = (px [0] * 77 + px [1] * 150 + px [2] * 29) >> 8;
            int mx = qMax (px [0], qMax (px [1], px [2]));
            int mn = qMin (px [0], qMin (px [1], px [2]));

            if (lum < COVERAGE_THRESHOLD)
               count++;
            /* the sheet is paper-bright somewhere in every line of it,
               even a blank one; the backing beyond its edge never is.
               A cover printed to its edges is not bright there, but it
               has ink where the backing has none */
            if (lum >= PAPER_BRIGHT || lum < INK_THRESHOLD)
               {
               if (_bright_cols.isEmpty ())
                  {
                  _bright_cols = QVector<int> (_width, 0);
                  _ink_cols = QVector<int> (_width, 0);
                  }
               if (lum >= PAPER_BRIGHT)
                  _bright_cols [_col]++;
               else
                  _ink_cols [_col]++;
               }
            if (++_col >= _width)
               _col = 0;
            bool coloured = (mx - mn) * SATURATION_DIVISOR >= mx
                  && mx >= SATURATION_MIN_BRIGHT;

            if (coloured)
               {
               colour++;
               _colourBand [lum < 100 ? 0 : lum < 200 ? 1 : 2]++;
               }
            if (_autoColour)
               inkPixel (lum, coloured,
                         mx - mn >= COLOUR_STRONG && lum < COLOUR_INK_LUM);
            pixels++;
            }
         _colourPixels += colour;
         break;
         }
      }

   // work out total pixels in this block
   if (_depth != 24)
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


void PPage::setHeight (int height)
   {
   int i;


   _height = height;
   _height_known = true;
   int size = _stride * height;
   if (_jpeg)
      {
      /* the decompressor's row pointers go into _decomp, which may move
         when it grows, so rebuild them */
      if (size > _decomp.capacity ())
         {
         _decomp.reserve (size);
         memset (_decomp.data (), '\0', size);
         }
      if (_jpeg_created)
         {
         delete[] _buffer;
         _buffer = new JSAMPROW [height];
         for (i = 0; i < height; i++)
            _buffer [i] = (JSAMPLE *)(_decomp.data () + _stride * i);
         }
      size /= 2;
      }
   _size = qMax (size, _data.size ());

   /* Never move the buffer the data arrives in while a JPEG is being
      decoded out of it. This is called by the decoder itself, from
      inside the header it is reading, once that header says how big
      the page really is; making room in that buffer moves the bytes
      the decoder is part way through, and it carries on reading the
      memory they used to be in. The buffer grows by itself as more
      data arrives, which happens between decodes and is safe */
   if (!_jpeg)
      _data.reserve (_size);
   _pixelTarget = _width * height;
   if (_blankThreshold)
      _pixelTarget /= _blankThreshold;
   }


bool PPage::addBytes (const unsigned char *buf, int size)
   {
   /* raw data of a height not yet known: keep room for whatever comes */
   if (!_height_known && !_jpeg && _data.size () + size > _size)
      {
      _size = qMax (_size * 2, _data.size () + size);
      _data.reserve (_size);
      }
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
   /* raw data of a height the back end did not know: it is however many
      lines arrived */
   if (!_height_known)
      {
      _height = _stride ? _data.size () / _stride : 0;
      _size = _height * _stride;
      _height_known = true;
      }

   /* A scanner asked to stop at the foot of the sheet ends the page
      early, and what it has already said about the page stands: the
      JPEG header promises the lines of the whole window and the data
      stops short of them. Store the page at the length which arrived,
      so that what is in the file is what can be read back out of it */
   if (_jpeg && _stride)
      {
      int lines = _decomp_avail / _stride;

      /* the decoder fills the lines the scanner never sent, so where
         it said the data ran out is a better answer than how many
         lines it went on to produce */
      if (_data_lines > 0 && _data_lines < lines)
         lines = _data_lines;
      if (lines > 0 && lines < _height)
         _height = lines;
      }
//   printf ("confirmed %s\n", _name.latin1 ());
//   printf ("page complete: %dx%dx%d @%d, size %d/%d, short %d\n", _width, _height,
//      _depth, _stride, _upto, _size, _size - _upto);
   return compressPage (mp, mark_blank);
   }


int PPage::size (void)
   {
   return _size;
   }


QByteArray PPage::convert (const unsigned char *src, int height, int depth,
                           int &width_out, int &height_out,
                           int &stride_out) const
   {
   bool turn = _rotate != Rotate_none;
   /* The sheet is narrower than the window the scanner was given, and
      what lies beyond its edge is the backing. A sideways feed puts the
      page's height along the scanner, where nothing trims it, so the
      page is left with the backing at its foot; an upright one has it
      down the sides. Either way, cut it off at the edge of the sheet */
   int lo = 0, hi = _width - 1;
   int paper_lo, paper_hi;

   if (_autoSize && sheetEdges (height, paper_lo, paper_hi))
      {
      /* leave a margin: a sheet goes through slightly skewed, and the
         very edge of a page printed to its margins is dark rather than
         paper-bright, so the sheet reaches a little beyond where it
         last looks like paper. The margin is white on a mono or grey
         page, since the backing is lighter than the ink threshold */
      int margin = _width / 100;

      lo = qMax (0, paper_lo - margin);
      hi = qMin (_width - 1, paper_hi + margin);
      }
   int ow = turn ? height : hi - lo + 1;
   int oh = turn ? hi - lo + 1 : height;
   int ostride = depth == 24 ? ow * 3 : depth == 8 ? ow : (ow + 7) / 8;
   QByteArray out (ostride * oh, '\0');
   unsigned char *dst = (unsigned char *)out.data ();

   for (int dy = 0; dy < oh; dy++, dst += ostride)
      for (int dx = 0; dx < ow; dx++)
         {
         int sx, sy;

         /* where this output pixel comes from: a quarter turn clockwise
            brings the left edge to the top, anticlockwise the right */
         switch (_rotate)
            {
            case Rotate_cw :
               sx = lo + dy;
               sy = height - 1 - dx;
               break;
            case Rotate_ccw :
               sx = hi - dy;
               sy = dx;
               break;
            default :
               sx = lo + dx;
               sy = dy;
               break;
            }

         const unsigned char *in = src + sy * _stride;
         int r, g, b;

         switch (_depth)
            {
            case 24 :
               in += sx * 3;
               r = in [0];
               g = in [1];
               b = in [2];
               break;
            case 8 :
               r = g = b = in [sx];
               break;
            default :   // 1 is black, as SANE has it
               r = g = b = in [sx >> 3] & (0x80 >> (sx & 7)) ? 0 : 255;
               break;
            }

         int lum = (r * 77 + g * 150 + b * 29) >> 8;

         switch (depth)
            {
            case 24 :
               dst [dx * 3] = r;
               dst [dx * 3 + 1] = g;
               dst [dx * 3 + 2] = b;
               break;
            case 8 :
               dst [dx] = lum;
               break;
            default :
               if (lum < COVERAGE_THRESHOLD)
                  dst [dx >> 3] |= 0x80 >> (dx & 7);
               break;
            }
         }
   width_out = ow;
   height_out = oh;
   stride_out = ostride;
   return out;
   }


err_info *PPage::compressPage (Filepage *mp, bool mark_blank)
   {
   Kind kind = _autoColour ? this->kind () : Kind_colour;
   int depth = kind == Kind_grey ? 8 : kind == Kind_mono ? 1 : _depth;

   mp->_rotate = _rotate == Rotate_cw ? 90 : _rotate == Rotate_ccw ? 270 : 0;

   /* a colour page that turned out to need no colour is stored as grey
      or, with no mid-tones either, as mono, and a page fed sideways is
      turned upright: either is done from the decoded pixels */
   if (depth != _depth || _rotate != Rotate_none)
      {
      const unsigned char *src = (const unsigned char *)
         (_jpeg ? _decomp.constData () : _data.constData ());
      int lines = _jpeg ? _decomp_avail / _stride : _data.size () / _stride;
      int width, height, stride;
      QByteArray out = convert (src, qMin (lines, _height), depth, width,
                                height, stride);

      mp->addData (width, height, depth, stride, _name, false, mark_blank,
                   _pagenum, out, out.size ());
      return mp->compress ();
      }

   /* The page is stored as the scanner sent it, so cutting the backing
      off the sides means cutting up the JPEG, which can be done by
      moving its blocks about rather than by decoding and encoding it
      again: the pixels which are kept are the ones the scanner sent */
   if (_autoSize && _jpeg && _depth == 24)
      {
      int paper_lo, paper_hi;

      if (sheetEdges (_height, paper_lo, paper_hi))
         {
         int margin = _width / 100;
         int lo = qMax (0, paper_lo - margin);
         int hi = qMin (_width - 1, paper_hi + margin);
         QByteArray out;

         if (lo || hi < _width - 1)
            {
            int at = jpegCrop ((const byte *)_data.constData (),
                               _data.size (), lo, hi, out);

            if (at >= 0)
               {
               _data = out;
               _width = hi + 1 - at;
               _stride = _width * 3;
               }
            }
         }
      }

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


/* Take the next pixel of the page, in row order, for the filled-region
   count. Ink flags are kept for the last INTERIOR_ROWS rows, each row
   already eroded sideways by INTERIOR_RADIUS; once a row completes, the
   mid-tone pixels of the middle row of the ring whose column has ink in
   every row of it are the interior ones. Each row's count is held for
   EDGE_ROWS rows before it is added, so the last EDGE_ROWS rows of the
   page never are, and the first EDGE_ROWS are skipped */
void PPage::inkPixel (int lum, bool coloured, bool strong)
   {
   if (_rows.isEmpty ())
      {
      _rows = QByteArray (INTERIOR_ROWS * _width, '\0');
      _delay = QByteArray (EDGE_ROWS * _width, '\0');
      _interior_cols = QVector<int> (_width, 0);
      _soft_cols = QVector<int> (_width, 0);
      _colour_cols = QVector<int> (_width, 0);
      _colour_cells = QVector<int> (_width / COLOUR_TILE + 1, 0);
      _colour_tiles = QVector<int> (_width / COLOUR_TILE + 1, 0);
      _cell_ink = QVector<int> (_width / FILL_CELL + 1, 0);
      _tile_solid = QVector<int> (_width / (FILL_CELL * FILL_TILE) + 1, 0);
      }

   if (!_row_x)
      _row_gap = -INTERIOR_ROWS;   // no gap behind the start of a row

   if (lum < PALE_THRESHOLD)
      _cell_ink [_row_x / FILL_CELL]++;

   char *row = _rows.data () + (_rows_done % INTERIOR_ROWS) * _width;
   char flag = lum >= INK_THRESHOLD ? Ink_none
         : lum >= PALE_THRESHOLD ? Ink_pale
         : lum >= MID_THRESHOLD ? Ink_mid : Ink_dark;

   if (coloured)
      flag |= Ink_colour;
   if (strong)
      flag |= Ink_strong;
   if (lum >= PAPER_BRIGHT)
      flag |= Ink_paper;

   /* a pixel is kept only if it and its INTERIOR_RADIUS neighbours each
      side have ink: mark it now, unmark the neighbours behind it that a
      gap unmarks, and unmark it if a gap just behind it, or the edge of
      the image, leaves it too close to one */
   row [_row_x] = flag;
   if ((flag & Ink_mask) == Ink_none)
      {
      for (int i = qMax (0, _row_x - INTERIOR_RADIUS); i < _row_x; i++)
         row [i] |= Ink_gap;
      _row_gap = _row_x;
      }
   else if (_row_x < INTERIOR_RADIUS
            || _row_x - _row_gap <= INTERIOR_RADIUS)
      row [_row_x] |= Ink_gap;
   if (++_row_x < _width)
      return;

   /* row complete: the last INTERIOR_RADIUS pixels have no right-hand
      neighbours, then count the middle row of the ring */
   for (int i = qMax (0, _width - INTERIOR_RADIUS); i < _width; i++)
      row [i] |= Ink_gap;
   _row_x = 0;
   _row_skip = _stride - _width * 3;
   _rows_done++;

   /* a row of cells is complete: the cells with ink right through are
      the filled ones, and the densest tile of them says whether the
      page holds a picture */
   if (_rows_done % FILL_CELL == 0)
      {
      for (int cell = 0; cell < _cell_ink.size (); cell++)
         {
         if (_cell_ink [cell] == FILL_CELL * FILL_CELL)
            _tile_solid [cell / FILL_TILE]++;
         _cell_ink [cell] = 0;
         }
      if (_rows_done % (FILL_CELL * FILL_TILE) == 0)
         for (int tile = 0; tile < _tile_solid.size (); tile++)
            {
            _fill_max = qMax (_fill_max, _tile_solid [tile]);
            _tile_solid [tile] = 0;
            }
      }
   if (_rows_done < INTERIOR_ROWS)
      return;

   const char *rows [INTERIOR_ROWS];
   int mid = (_rows_done - 1 - INTERIOR_RADIUS) % INTERIOR_ROWS;

   for (int r = 0; r < INTERIOR_ROWS; r++)
      rows [r] = _rows.constData () + r * _width;

   /* a row's marks wait EDGE_ROWS rows before they are added to their
      columns, so the last EDGE_ROWS rows of the page never are, and
      the first EDGE_ROWS are skipped */
   int done = _rows_done - INTERIOR_ROWS;
   char *slot = _delay.data () + (done % EDGE_ROWS) * _width;

   if (done >= 2 * EDGE_ROWS)
      {
      /* a tile row is complete: keep the most each tile column held */
      if (_colour_rows == COLOUR_TILE)
         {
         for (int tile = 0; tile < _colour_cells.size (); tile++)
            {
            _colour_tiles [tile] = qMax (_colour_tiles [tile],
                                         _colour_cells [tile]);
            _colour_cells [tile] = 0;
            }
         _colour_rows = 0;
         }
      _colour_rows++;
      for (int x = 0; x < _width; x++)
         {
         if (slot [x] & Mark_interior)
            _interior_cols [x]++;
         if (slot [x] & Mark_soft)
            _soft_cols [x]++;
         if (slot [x] & Mark_colour)
            _colour_cols [x]++;
         if (slot [x] & Mark_pen)
            _colour_cells [x / COLOUR_TILE]++;
         }
      }
   memset (slot, Mark_none, _width);
   for (int x = 0; x < _width; x++)
      {
      char centre = rows [mid][x] & Ink_mask;
      char mark = Mark_none;

      /* ink of a strong colour with the paper still white beside it is
         a pen mark: ink which soaked through from the back has stained
         the paper it came through, and has no white left beside it */
      if (rows [mid][x] & Ink_strong)
         {
         bool paper = false;

         for (int r = 0; r < INTERIOR_ROWS && !paper; r++)
            for (int dx = -COLOUR_PAPER_RADIUS;
                 dx <= COLOUR_PAPER_RADIUS && !paper; dx++)
               {
               int nx = x + dx;

               paper = nx >= 0 && nx < _width
                     && (rows [r][nx] & Ink_paper) != 0;
               }
         if (paper)
            mark |= Mark_pen;
         }

      /* ink laid on the page is coloured all through, while the fringe
         along the edge of black print is a pixel or two of colour with
         white or black beside it */
      if (rows [mid][x] & Ink_colour)
         {
         bool all = x >= COLOUR_RADIUS && x < _width - COLOUR_RADIUS;

         for (int r = 0; r < INTERIOR_ROWS && all; r++)
            for (int dx = -COLOUR_RADIUS; dx <= COLOUR_RADIUS && all; dx++)
               all = (rows [r][x + dx] & Ink_colour) != 0;
         if (all)
            mark |= Mark_colour;
         }
      if (centre == Ink_none)
         {
         slot [x] = mark;
         continue;
         }

      /* a mid-tone pixel every one of whose neighbours has ink is
         inside a filled region: a photograph rather than a stroke */
      if (centre != Ink_dark)
         {
         bool all = true;

         for (int r = 0; r < INTERIOR_ROWS && all; r++)
            all = rows [r][x] != Ink_none && !(rows [r][x] & Ink_gap);
         if (all)
            mark |= Mark_interior;
         }

      /* ink too pale for mono to keep, with nothing it would keep
         anywhere near it, is writing that mono would lose */
      if (centre == Ink_pale)
         {
         bool kept = false;

         for (int r = 0; r < INTERIOR_ROWS && !kept; r++)
            for (int dx = -INTERIOR_RADIUS; dx <= INTERIOR_RADIUS && !kept;
                 dx++)
               {
               int nx = x + dx;
               /* not "near": Windows takes that name for itself */
               char beside = nx >= 0 && nx < _width
                     ? rows [r][nx] & Ink_mask : Ink_none;

               kept = beside == Ink_dark || beside == Ink_mid;
               }
         if (!kept)
            mark |= Mark_soft;
         }
      slot [x] = mark;
      }

   /* drop the pale ink that lies in runs too long to be writing */
   int start = -1;   // where the run being measured began
   int last = -1;    // the last pale pixel of it

   for (int x = 0; x <= _width; x++)
      {
      if (x < _width && (slot [x] & Mark_soft))
         {
         if (start < 0)
            start = x;
         last = x;
         continue;
         }
      if (start < 0)
         continue;
      if (x < _width && x - last <= SOFT_GAP (_width))
         continue;       // near enough to be the same run
      if (last - start + 1 > SOFT_MAX_RUN (_width))
         for (int i = start; i <= last; i++)
            slot [i] &= ~Mark_soft;
      start = -1;
      }
   }


/* Find the edges of the sheet across the scanner window, from the
   paper-bright pixels counted in each column

   \param height  rows of the page
   \param lo, hi  return the first and last column holding the sheet
   \param printed true to take a column with ink on it for the sheet as
            well, which finds the edge of a cover printed right up to
            it. The shade the edge of the sheet casts on the backing is
            ink by this measure, so a count of what is on the page
            passes false and keeps to the paper-bright columns
   \returns true if the window holds something that looks like a sheet */

bool PPage::sheetBounds (int height, int &lo, int &hi, bool printed) const
   {
   int paper_lo = -1, paper_hi = -1;

   if (!_bright_cols.isEmpty ())
      {
      int need = (int)(height * PAPER_COLUMN_FRACTION);
      int ink = (int)(height * PAPER_INK_FRACTION);

      for (int x = 0; x < _width; x++)
         if (_bright_cols [x] >= need
             || (printed && _ink_cols [x] >= ink))
            {
            if (paper_lo < 0)
               paper_lo = x;
            paper_hi = x;
            }
      }
   if (paper_lo < 0 || paper_hi - paper_lo + 1 < _width * PAPER_MIN_FRACTION)
      return false;

   lo = paper_lo;
   hi = paper_hi;
   return true;
   }


/* Where to cut a page down to the sheet, which is not quite where the
   sheet is reckoned to be for reading what is on it.

   A sheet which went through the feeder a little askew has its corners
   out beyond the rest of it, in columns holding only a few lines of
   paper each, and cutting the page where half a column is paper takes
   the corners off with the backing. Follow the edge outwards from
   there while the next column still holds some paper: that walks along
   the corner and stops where the sheet does, and a speck out on the
   backing cannot pull the edge to itself, since the walk stops at the
   first column with no paper at all.

   Only paper counts here, not the ink which stands in for it when the
   sheet is being found: the shade the edge of a sheet casts on the
   backing is dark enough to read as ink, and following that leads out
   across the whole window. A corner reaches out by the length of the
   sheet times how far it is off square, so there is a limit on how far
   this goes as well */
bool PPage::sheetEdges (int height, int &lo, int &hi) const
   {
   if (!sheetBounds (height, lo, hi, true))
      return false;

   /* what the backing itself gives, measured at the edge of the
      window, where there is no sheet: the window can take in more than
      the sheet in both directions, and a page whose sheet ends part
      way down is bright below it right across the window */
   int edge = qMax (1, (int)(height * PAPER_EDGE_FRACTION));
   int most = (int)(_width * PAPER_EDGE_MOST);

   for (int i = 0; i < most && lo > 0
        && _bright_cols [lo - 1] >= _bright_cols [0] + edge; i++)
      lo--;
   for (int i = 0; i < most && hi < _width - 1
        && _bright_cols [hi + 1] >= _bright_cols [_width - 1] + edge; i++)
      hi++;
   return true;
   }


/* Add up the ink marks of the columns that lie on the sheet. The sheet
   is not as wide as the window and its edge casts a shade along the
   whole length of the page, which is mid-toned and would otherwise
   count for more than anything printed on the page, so the count stops
   SHEET_INSET short of each edge

   \param interior  returns mid-tone pixels inside a filled region
   \param soft      returns the pale ink with nothing mono keeps near it
   \param colour    returns the pixels with colour all round them */

void PPage::inkTotals (int &interior, int &soft, int &colour) const
   {
   int lo = 0, hi = _width - 1;

   interior = soft = colour = 0;
   if (_interior_cols.isEmpty ())
      return;
   if (sheetBounds (_rows_done, lo, hi, false))
      {
      lo = qMax (0, lo + SHEET_INSET);
      hi = qMin (_width - 1, hi - SHEET_INSET);
      }
   for (int x = lo; x <= hi; x++)
      {
      interior += _interior_cols [x];
      soft += _soft_cols [x];
      colour += _colour_cols [x];
      }
   }


/* The densest tile of pen ink lying on the sheet. Tiles which are not
   wholly inside the edges of the sheet are left out: the step from the
   sheet to the backing beyond it comes back from the scanner with a
   coloured fringe along it, as any sharp edge does */
int PPage::colourTile (void) const
   {
   int lo = 0, hi = _width - 1;
   int most = 0;

   if (_colour_tiles.isEmpty ())
      return 0;
   if (sheetBounds (_rows_done, lo, hi, false))
      {
      lo = qMax (0, lo + SHEET_INSET);
      hi = qMin (_width - 1, hi - SHEET_INSET);
      }
   for (int tile = 0; tile < _colour_tiles.size (); tile++)
      {
      if (tile * COLOUR_TILE < lo || (tile + 1) * COLOUR_TILE - 1 > hi)
         continue;
      most = qMax (most, qMax (_colour_tiles [tile], _colour_cells [tile]));
      }
   return most;
   }


PPage::Kind PPage::kind (void) const
   {
   if (_depth != 24 || !_pixels)
      return Kind_colour;
   if ((double)_colourPixels / _pixels >= COLOUR_FRACTION)
      return Kind_colour;

   int interior, soft, colour;

   inkTotals (interior, soft, colour);
   if ((double)colour / _pixels >= COLOUR_SOLID_FRACTION)
      return Kind_colour;
   if ((double)colourTile () / (COLOUR_TILE * COLOUR_TILE)
       >= COLOUR_TILE_FRACTION)
      return Kind_colour;
   if ((double)_fill_max / FILL_TILE_CELLS >= FILL_FRACTION)
      return Kind_grey;
   if ((double)interior / _pixels >= GREY_FRACTION
       || (double)soft / _pixels >= SOFT_FRACTION)
      return Kind_grey;
   return Kind_mono;
   }


/* what the page-kind test decided, for a line the user reads */
static const char *kind_name (PPage::Kind kind)
   {
   switch (kind)
      {
      case PPage::Kind_grey: return "grey";
      case PPage::Kind_mono: return "mono";
      default: return "colour";
      }
   }


/* what coverageStr() adds when a colour page is stored as something less */
static const char *kind_suffix (PPage::Kind kind)
   {
   switch (kind)
      {
      case PPage::Kind_grey: return " grey";
      case PPage::Kind_mono: return " mono";
      default: return "";
      }
   }


/* What the auto-colour test saw: the counts it is built on, as
   percentages of the page, and what it made of them. The thresholds
   these are compared against are at the head of this file */
QString PPage::kindStr ()
   {
   int interior, soft, solid;
   int lo = 0, hi = _width - 1;
   QString sheet = "sheet not found";

   inkTotals (interior, soft, solid);
   if (sheetEdges (_rows_done, lo, hi))
      sheet = QString ().asprintf ("sheet %d..%d (%d wide, %.0f%%)",
                                   lo, hi, hi - lo + 1,
                                   100.0 * (hi - lo + 1) / _width);
   return QString ().asprintf (
            "%dx%d depth %d%s pixels %d, colour %d "
            "(%.3f%%: dark %d mid %d light %d, solid %d %.4f%%, "
            "pen %d %.3f%%), interior %d (%.3f%%), soft %d (%.4f%%), %s -> %s",
            _width, _height, _depth, _jpeg ? " jpeg" : "",
            _pixels, _colourPixels,
            _pixels ? 100.0 * _colourPixels / _pixels : 0,
            _colourBand [0], _colourBand [1], _colourBand [2],
            solid, _pixels ? 100.0 * solid / _pixels : 0,
            colourTile (),
            100.0 * colourTile () / (COLOUR_TILE * COLOUR_TILE),
            interior, _pixels ? 100.0 * interior / _pixels : 0,
            soft, _pixels ? 100.0 * soft / _pixels : 0,
            qPrintable (sheet), kind_name (kind ()));
   }


QString PPage::coverageStr ()
   {
   double cov;
   QString suffix = _autoColour ? kind_suffix (kind ()) : "";

   /* PAPERMAN_KIND_DEBUG shows what the page-kind test saw, for tuning
      its thresholds against real scans */
   const char *debug = getenv ("PAPERMAN_KIND_DEBUG");

   if (debug && QDir (debug).exists ())
      {
      /* a directory: keep the page's data as well, the scanner's JPEG or
         the raw pixels as a PPM, to run the test on outside the scan */
      QFile f (QString ("%1/page%2.%3").arg (debug).arg (_pagenum)
               .arg (_jpeg ? "jpg" : "ppm"));

      if (f.open (QIODevice::WriteOnly))
         {
         if (!_jpeg && _depth == 24)
            f.write (QString ("P6\n%1 %2\n255\n").arg (_width)
                     .arg (_data.size () / _stride).toLatin1 ());
         f.write (_data);
         }
      }
   if (debug)
      fprintf (stderr, "page %d: %s\n", _pagenum, qPrintable (kindStr ()));

   // can't work out coverage from JPEG data
//    if (_jpeg)
//       return "";

   // work out coverage
   cov = (double)_nonblankPixels / _pixels;
   if (_nonblankPixels == 0)
      return "0" + suffix;

   // if more than 0.1%, use x.y% notation
   if (cov >= 0.001)
      return QString ("%1%").arg (cov * 100, 0, 'f', 1) + suffix;

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
   /* set up here, not in scan(): endScan() may be called as soon as the
      thread is started, before scan() would have run */
   _end = false;
   _draining = false;
   _stop_tried = false;
   _sides_done = 0;
   _t_start = _t_read = _t_data = _t_confirm = 0;
   _cpu_seconds = 0;
   _max_waiting = -1;
   _progress_clock.start ();
   }


Paperscan::~Paperscan ()
   {
   }


/* A misfeed is the user's to clear, at the scanner, and the pages still
   in the hopper are meant to follow it. Say what happened, through the
   scan window and the status bar, then wait for the scan to be taken up
   again rather than ending it and leaving the user to answer a dialog
   first.

   The scanner's own scan button says the user has cleared the paper
   path and wants to go on.  A back end that does not offer the button
   leaves nothing to wait for, so the scan is started again every
   RESUME_POLL_MS to see whether the way is clear.  Either way Stop ends
   the scan at once, and so does RESUME_WAIT_MS of nothing happening,
   which is what an unattended scan wants */

#define RESUME_POLL_MS 1000
#define RESUME_WAIT_MS (30 * 1000)

bool Paperscan::isMisfeed (SANE_Status status)
   {
   return status == SANE_STATUS_JAMMED || status == SANE_STATUS_COVER_OPEN;
   }


/* Did the scanner stop in the middle of a batch? A misfeed leaves this
   scanner in a state where it drops the connection rather than saying
   what is wrong, and the back end can only report that as an I/O error,
   which is true and useless: what the user has to do about it is clear
   the paper path and scan again

   \param status  what the back end said
   \param pages   sides scanned before it said so */

bool Paperscan::stoppedMidBatch (SANE_Status status, int pages)
   {
   return isMisfeed (status)
       || (status == SANE_STATUS_IO_ERROR && pages > 0);
   }


SANE_Status Paperscan::waitForResume (SANE_Status status)
   {
   QString why = QString (sane_strstatus (status));
   int buttons = _scanner->checkButtons ();
   bool watch = buttons != INT_MIN;
   QElapsedTimer waited;

   /* end the frame, so that the back end is ready to start another once
      the paper path is clear */
   _scanner->cancel ();
   waited.start ();
   while (!isCancelled () && waited.elapsed () < RESUME_WAIT_MS)
      {
      /* say what is wanted, and keep saying it with the time left, so
         that a scan waiting on the user never looks like a scan that
         has hung */
      emit scanProblem (tr ("%1: clear the scanner to carry on, or press "
                            "Stop to end the scan (%2s)").arg (why)
                        .arg ((RESUME_WAIT_MS - waited.elapsed () + 999)
                              / 1000));
      msleep (RESUME_POLL_MS);
      if (watch)
         {
         buttons = _scanner->checkButtons ();
         if (buttons == INT_MIN)      // the back end has stopped telling us
            watch = false;
         else if (!(buttons & (1 << QScanner::BUT_scan)))
            continue;
         }
      _op = "sane_start";
      status = _scanner->start ();
      if (status == SANE_STATUS_GOOD)
         {
         emit scanProblem (QString ());
         emit progress (tr ("Carrying on after %1").arg (why));
         return status;
         }
      if (!isMisfeed (status))
         break;       // the hopper is empty, or the scanner has given up
      }
   emit scanProblem (QString ());

   return status;
   }


/* Start the next page, waiting out a scanner that says it is busy and a
   misfeed that the user has yet to clear

   \returns the status of the scan that has started */

SANE_Status Paperscan::startPage (void)
   {
   SANE_Status status = SANE_STATUS_DEVICE_BUSY;
   int busy_count;

   for (busy_count = 0; status == SANE_STATUS_DEVICE_BUSY
                        && busy_count < 30 && !isCancelled ();
        busy_count++)
      {
      if (busy_count)
         {
         usleep (10000);
         // Check for double-feed while waiting
         if (_scanner->checkDoubleFeed ())
            emit doubleFeedDetected ();
         }
      _op = "sane_start";
      qint64 t0 = QDateTime::currentMSecsSinceEpoch ();

      status = _scanner->start ();
      if (status == SANE_STATUS_INVAL && !busy_count)
         {
         // did we forget to cancel last time?
         _scanner->cancel ();
         _op = "sane_start";
         status = _scanner->start ();
         }
      _t_start += QDateTime::currentMSecsSinceEpoch () - t0;
      }
   if (isMisfeed (status) && !isCancelled ())
      status = waitForResume (status);

   return status;
   }


/* Is the back end cutting each page down to the sheet? That is what
   the auto-size checkbox turns on: the finet backend's auto-size, which
   crops the image to the paper, or the fujitsu backend's ald, which
   ends each image at the foot of the sheet. With it off the user has
   asked for the page size they set, so a page fed sideways is left as
   long as the window rather than cut at the edge of the sheet */

/* A scanner told to stop at the foot of the sheet does not know how
   long the page will be until it gets there, and says -1. The window it
   was given is the most it can send, though, so that is what the page
   is made ready for: a guess from the width instead means the buffers
   grow part way through a long page, which costs a copy of the page.

   The window can be far longer than any sheet - this scanner will take
   a window nearly three metres long, for a banner or a till roll - and
   a page made ready for all of it would hold hundreds of megabytes
   waiting for a sheet of paper. Past twice the width, which covers
   every ordinary paper size, the page starts smaller and grows if the
   sheet really does go on that long */
int Paperscan::expectedLines (const SANE_Parameters &parameters) const
   {
   if (parameters.lines > 0)
      return parameters.lines;

   int bry = _scanner->getBryOption ();
   int tly = _scanner->getTlyOption ();
   int dpi = _scanner->yResolutionDpi ();

   if (bry < 0 || dpi <= 0)
      return parameters.lines;    // no window to go on: let the page guess

   double mm = SANE_UNFIX (_scanner->saneWordValue (bry));

   if (tly >= 0)
      mm -= SANE_UNFIX (_scanner->saneWordValue (tly));
   if (mm <= 0)
      return parameters.lines;

   int lines = (int)(mm * dpi / 25.4);

   return lines > parameters.pixels_per_line * 2 ? parameters.lines : lines;
   }


bool Paperscan::autoSize (void) const
   {
   static const char *const name [] = { "auto-size", "ald" };

   for (unsigned i = 0; _scanner && i < sizeof (name) / sizeof (name [0]);
        i++)
      {
      int num = _scanner->findOption (name [i]);

      if (num >= 0)
         return _scanner->saneWordValue (num) != 0;
      }
   return false;
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
      _stack = new Paperstack (stack_name, page_name, false);
#endif
      _mutex.unlock ();

      _stack->setBlankPolicy ((Paperstack::t_blankPolicy)xmlConfig->intValue("SCAN_BLANK"),
               xmlConfig->intValue("SCAN_BLANK_THRESHOLD"));
      _stack->setAutoColour (xmlConfig->boolValue ("SCAN_AUTO_COLOUR"));
      _stack->setAutoSize (autoSize ());
      _stack->setSideways ((Paperstack::t_sideways)
                           xmlConfig->intValue ("SCAN_SIDEWAYS"));
      emit stackNew (stack_name);
      }
   }


void Paperscan::scan ()
   {
   int len, total, todo, size;
   SANE_Status status;
   unsigned char *buf;
   bool done = false;
   SANE_Parameters parameters;
   int numsides, side;
   int total_sides = 0;  // total number of sides scanned
   QElapsedTimer wall;   // how long the whole scan takes
   int total_blank = 0;  // total number of blank sides scanned
   int stack_count = 0;  // total number of stacks created
//    Paperstack *stack = 0;
   _stack = 0;
   bool adf;  // true if using an auto document feeder
   err_info *err;
   int bpp, image_bpp;
   bool is_jpeg;
   QString side0_str;
   int stack_limit; // maximum number of pages per stack

//    qDebug () << "scan start";

   wall.start ();
   if (!_scanner)
      return;

   // check resolution, etc.
   _op = "sane_get_parameters";
   status = _scanner->getParameters (&parameters);
   if (status != SANE_STATUS_GOOD)
      {
      _progress_str = failStr (status, 0);
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


   stack_limit = xmlConfig->intValue ("SCAN_STACK_COUNT");
   err = NULL;
   if (!err) do
      {
      // read value here, to allow user to change it during scan
      int single = xmlConfig->intValue ("SCAN_SINGLE");

      status = SANE_STATUS_GOOD;
      side0_str = "";

      // Progressive-duplex fast path: scanner is duplexing AND libsane
      // exposes sane_read_dup. We do one sane_start, drive both pages
      // simultaneously through readDup(), and emit progress for both
      // as data flows in. Falls through to the legacy per-side loop
      // below when not applicable. The path produces one sheet = 2
      // sides per pass, so SCAN_SINGLE must allow at least 2 more
      // sides before we'd hit the limit.
      bool dup_room = !single || (single - total_sides) >= 2;
      bool dup_path = numsides == 2 && dup_room
                      && _scanner->hasReadDup ();
      if (dup_path)
         {
         ensureStack (_stack_name, _page_name, parameters);
         emit progress (QString ("Scanning page %1+%2")
                        .arg (total_sides + 1).arg (total_sides + 2));

         status = startPage ();
         if (status != SANE_STATUS_GOOD || isCancelled ())
            break;

         /* the parameters are only final once the scan has started: a
            back end may adjust the size to what the scanner delivers */
         if (_scanner->getParameters (&parameters) == SANE_STATUS_GOOD)
            bpp = parameters.bytes_per_line * 8 / parameters.pixels_per_line;

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
         int expected_bytes_f = _stack->addImage (parameters.pixels_per_line,
                expectedLines (parameters), image_bpp,
                parameters.bytes_per_line, true, is_jpeg);
         emit stackPageStarting (expected_bytes_f, _stack->curPage ());
         int expected_bytes_b = _stack->addImageBack (parameters.pixels_per_line,
                expectedLines (parameters), image_bpp,
                parameters.bytes_per_line, is_jpeg);
         emit stackPageStarting (expected_bytes_b, _stack->curPageBack ());

         /* Need separate buffers for the two sides. The legacy `buf` is
          * 256 KB which is plenty for one side; allocate a matching one
          * for the back. */
         unsigned char *buf_back = (unsigned char *) malloc (size);
         long total_f = 0, total_b = 0;
         if (!buf_back)
            status = SANE_STATUS_NO_MEM;
         while (status == SANE_STATUS_GOOD && !isCancelled ())
            {
            SANE_Int flen = 0, blen = 0;
            _op = "sane_read_dup";
            status = _scanner->readDup (buf, buf_back, size, &flen, &blen);
            if (status == SANE_STATUS_UNSUPPORTED && !total_f && !total_b)
               {
               /* the back end has no sane_read_dup(): the sheet is already
                  started, so read it a side at a time instead. The next
                  sheet takes the per-side loop, since hasReadDup() is now
                  false */
               status = readSide (buf, size, false, total_f);
               if (status == SANE_STATUS_EOF)
                  {
                  _op = "sane_start";
                  status = _scanner->start ();

                  /* the back is its own image, so take its size from its
                     own start: a back end that crops each side to its
                     content makes it differ from the front by a few
                     pixels, and read at the front's width it would come
                     out sheared and cut short */
                  if (status == SANE_STATUS_GOOD
                      && _scanner->getParameters (&parameters)
                         == SANE_STATUS_GOOD)
                     {
                     _mutex.lock ();
                     _stack->restartBack (parameters.pixels_per_line,
                                          expectedLines (parameters),
                                          image_bpp,
                                          parameters.bytes_per_line, is_jpeg);
                     _mutex.unlock ();
                     }
                  }
               if (status == SANE_STATUS_GOOD)
                  status = readSide (buf_back, size, true, total_b);
               break;
               }
            if (status != SANE_STATUS_GOOD)
               break;
            if (flen)
               {
               _mutex.lock ();
               _stack->addImageBytes (buf, flen);
               _mutex.unlock ();
               total_f += flen;
               notifyProgress (_stack->curPage ());
               }
            if (blen)
               {
               _mutex.lock ();
               _stack->addImageBytesBack (buf_back, blen);
               _mutex.unlock ();
               total_b += blen;
               notifyProgress (_stack->curPageBack ());
               }
            checkStopFeed ();
            }
         /* drain any final state notifyProgress() held back, so the
          * preview shows the complete page */
         notifyProgress (_stack->curPage (), true);
         notifyProgress (_stack->curPageBack (), true);
         free (buf_back);

         if (status == SANE_STATUS_JAMMED && _scanner->checkDoubleFeed ())
            emit doubleFeedDetected ();

         if (status == SANE_STATUS_EOF && total_f && total_b
             && !isCancelled ())
            {
            QString cov_f = _stack->coverageStr ();
            QString cov_b = _stack->coverageStrBack ();
            _info_str = QString ("Coverage %1 / %2").arg (cov_f).arg (cov_b);

            Filepage *mp = NULL;
            err = _stack->confirmImage (mp, _mutex);
            if (!err && mp)
               {
               emit stackNewPage (mp, cov_f, _info_str);
               if (mp->markBlank ()) total_blank++;
               total_sides++;
               _sides_done = total_sides;
               }
            if (!err)
               {
               err = _stack->confirmImageBack (mp, _mutex);
               if (!err && mp)
                  {
                  emit stackNewPage (mp, cov_b, _info_str);
                  if (mp->markBlank ()) total_blank++;
                  total_sides++;
                  _sides_done = total_sides;
                  }
               }
            if (!err) status = SANE_STATUS_GOOD;
            }
         else
            {
            QMutexLocker locker (&_mutex);
            _stack->cancelImage ();
            /* cancel the back-side too: it has no public cancel, but
             * confirmImageBack with a freshly-NULLed back handle is fine
             * since cancelImage just zeroes the front pointer. Free the
             * back page directly. */
            }

         if (stack_limit && _stack && _stack->pageCount () >= stack_limit
             && !isCancelled ())
            {
            if (_stack->pageCount ())
               {
               _stack->confirm ();
               emit stackConfirm ();
               stack_count++;
               }
            _stack = 0;
            }
         /* skip the legacy per-side loop for this iteration */
         goto next_page;
         }

      for (side = 0; status == SANE_STATUS_GOOD && side < numsides && !isCancelled (); side++)
         {
         // create a paper stack if required
         ensureStack (_stack_name, _page_name, parameters);

//         printf ("side = %d\n", side);
         QString str;

         str = QString ("Scanning page %1").arg (total_sides + 1);

         /* say how far ahead the scanner is, if it can tell us: with a
            fast feeder that is what the display seems to lag behind */
         int waiting = _scanner->imagesWaiting ();
         if (waiting > 0)
            str += tr (", %1 waiting in the scanner").arg (waiting);
         _max_waiting = qMax (_max_waiting, waiting);
         if (stack_count > 0)
            str += QString (" stack %1").arg (stack_count + 1);
         emit progress (str);

         status = startPage ();
         if (status != SANE_STATUS_GOOD || isCancelled ())
            break;
         total = 0;
         if (_scanner->getParameters (&parameters) == SANE_STATUS_GOOD)
            bpp = parameters.bytes_per_line * 8 / parameters.pixels_per_line;
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
         int expected_bytes = _stack->addImage (parameters.pixels_per_line,
                expectedLines (parameters), image_bpp,
                 parameters.bytes_per_line, side == 0, is_jpeg);
         emit stackPageStarting (expected_bytes, _stack->curPage ());
//          qDebug () << "emit stackPageStarting page" << _stack->curPage ()->pagenum ();

         int i, steps = 0;
         for (i = 0; status == SANE_STATUS_GOOD && !isCancelled (); i++)
            {
            todo = size;
            _op = "sane_read";
            qint64 tr0 = QDateTime::currentMSecsSinceEpoch ();
            status = _scanner->read (buf, todo, &len);
            qint64 tr1 = QDateTime::currentMSecsSinceEpoch ();
            _t_read += tr1 - tr0;
//             printf ("status = %d, len = %d, total = %d\n", status, len, total + len);
            if (status != 0)
               break;

            _mutex.lock ();
            _stack->addImageBytes (buf, len);
            _mutex.unlock ();
            _t_data += QDateTime::currentMSecsSinceEpoch () - tr1;
            total += len;
            steps++;
//             qDebug () << "thread up to " << total;
            notifyProgress (_stack->curPage ());
            checkStopFeed ();
            }

         // Check if error was due to double-feed
         if (status == SANE_STATUS_JAMMED && _scanner->checkDoubleFeed ())
            emit doubleFeedDetected ();

         // end of file is ok - indicates we have an image
         if (status == SANE_STATUS_EOF && total && !isCancelled ())
            {
            notifyProgress (_stack->curPage (), true);
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
            qint64 tc0 = QDateTime::currentMSecsSinceEpoch ();
            err = _stack->confirmImage (mp, _mutex);
            _t_confirm += QDateTime::currentMSecsSinceEpoch () - tc0;
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
            _sides_done = total_sides;
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
   next_page:
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

      /* Stop means finish the batch, not abandon it. A feeder that runs
         ahead of us has sheets scanned that we have not read yet: if the
         backend can stop the feeder while keeping those, carry on until
         it runs dry. Otherwise stop here as before */
      if (!done && !err && !isCancelled ())
         checkStopFeed ();
      } while (!done && (!_end || _draining) && !err && !isCancelled ());

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
      str = tr ("Scanned %1 page%2 in %3").arg (total_sides)
            .arg (total_sides == 1 ? "" : "s").arg (utilTimeStr (wall.elapsed ()));
      if (total_blank)
         str += QString (tr (" (%1 blank)")).arg (total_blank);
      if (stack_count > 1)
         str += QString (tr (" into %1 stacks")).arg (stack_count);
      }

   // supress errors about running out of documents if we actually got some
   if (status == SANE_STATUS_NO_DOCS && numsides > 0)
      status = SANE_STATUS_GOOD;

   if (status != SANE_STATUS_GOOD && !isCancelled ())
      str += "\n" + failStr (status, total_sides + 1);

   if (isCancelled ())
      err = &_cancel_err;

   // if we are in the middle of a scan, cancel it
   if (_stack && _stack->isScanning ())
      {
      emit stackCancel ();
      _stack->cancel ();
      str = tr ("Scan cancelled - %1 page%2 discarded").arg (total_sides)
            .arg (total_sides == 1 ? "" : "s");
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
QString Paperscan::failStr (SANE_Status status, int page)
   {
   QString str = tr ("%1() on device '%2' returned '%3'").arg (_op)
      .arg (_scanner->name ()).arg (sane_strstatus (status));

   if (page)
      str += tr (" while scanning page %1").arg (page);
   return str;
   }


SANE_Status Paperscan::readSide (unsigned char *buf, int size, bool back,
                                 long &total)
   {
   SANE_Status status = SANE_STATUS_GOOD;
   SANE_Int len;

   while (status == SANE_STATUS_GOOD && !isCancelled ())
      {
      _op = "sane_read";
      qint64 tr0 = QDateTime::currentMSecsSinceEpoch ();
      status = _scanner->read (buf, size, &len);
      qint64 tr1 = QDateTime::currentMSecsSinceEpoch ();
      _t_read += tr1 - tr0;
      if (status != SANE_STATUS_GOOD)
         break;
      _mutex.lock ();
      if (back)
         _stack->addImageBytesBack (buf, len);
      else
         _stack->addImageBytes (buf, len);
      _mutex.unlock ();
      _t_data += QDateTime::currentMSecsSinceEpoch () - tr1;
      total += len;
      notifyProgress (back ? _stack->curPageBack () : _stack->curPage ());
      checkStopFeed ();
      }
   return status;
   }


void Paperscan::run (void)
   {
   struct timespec ts;

//    qDebug () << "run";

   // scan
   scan ();

   if (!clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts))
      _cpu_seconds = ts.tv_sec + ts.tv_nsec / 1e9;
   }


void Paperscan::cancelScan (err_info *err)
   {
   QMutexLocker locker (&_mutex);

   Q_ASSERT (err);
//    qDebug () << "cancelScan";
   _cancel = true;
   _cancel_err = *err;
   }


/* The main thread does not need every chunk: the message carries no data
   and the receiver looks at the page's current state when it gets to it,
   so one outstanding message covers everything that arrives before then.
   Sending one per chunk regardless swamps the display with repaints, a
   hundred a page, and lets it fall ever further behind a fast feeder */
void Paperscan::notifyProgress (const PPage *page, bool final)
   {
   const int PROGRESS_INTERVAL = 40;   // ms, so 25 updates a second at most

   if (!page)
      return;

   int pagenum = page->pagenum ();
   QMutexLocker locker (&_mutex);
   qint64 now = _progress_clock.elapsed ();

   if (_progress_pending.contains (pagenum)
       || (!final && now - _progress_time.value (pagenum, -PROGRESS_INTERVAL)
           < PROGRESS_INTERVAL))
      return;
   _progress_pending.insert (pagenum);
   _progress_time.insert (pagenum, now);
   locker.unlock ();
   emit stackPageProgress (page);
   }


void Paperscan::progressHandled (const PPage *page)
   {
   QMutexLocker locker (&_mutex);

   _progress_pending.remove (page->pagenum ());
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


/* Stop means finish the batch rather than abandon it: the feeder stops
   and the sheets already fed are read out. Ask for that the moment the
   button is pressed, not at the end of the side, since a side can take
   a long time to end when the paper misfeeds and until then the press
   shows no sign of having done anything. Only worth asking once: a back
   end that cannot stop the feeder says so, and the scan then ends after
   this side as it always did */
void Paperscan::checkStopFeed (void)
   {
   if (_stop_tried || _draining)
      return;
   _mutex.lock ();
   bool ending = _end;
   _mutex.unlock ();
   if (!ending)
      return;
   _stop_tried = true;
   _draining = _scanner->stopFeed ();
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

   if (!_stack
       || (page != _stack->curPage () && page != _stack->curPageBack ()))
      return false;

   return page->getData (data, size);
   }


int Paperscan::getPagenum (const PPage *page)
   {
   QMutexLocker locker (&_mutex);

   if (!_stack
       || (page != _stack->curPage () && page != _stack->curPageBack ()))
      return -1;
   return page->pagenum ();
   }


bool Paperscan::getPageDetails (const PPage *page, int &width, int &height,
      int &depth, int &stride)
   {
   QMutexLocker locker (&_mutex);

   if (!_stack
       || (page != _stack->curPage () && page != _stack->curPageBack ()))
      return false;
   page->getDetails (width, height, depth, stride);
   return true;
   }
