/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
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
*/

#include <algorithm>

#include <QBuffer>
#include <QPainter>
#include <QTimer>

#include "camera.h"

#ifdef Q_OS_LINUX
#include "camera_v4l2.h"
#endif

//! the fake cameras on offer, while they are in use
static QList<FakeCamera::Spec> fake_specs;
static bool fake_in_use;

//! the fake cameras which exist, for FakeCamera::push()
static QList<FakeCamera *> fake_live;

//! how often a fake camera sends a small picture, in milliseconds
static const int FAKE_PREVIEW_MS = 30;


Camera::Camera (QObject *parent)
      : QObject (parent)
   {
   }


Camera::~Camera ()
   {
   }


QList<CameraInfo> Camera::list (void)
   {
   if (FakeCamera::inUse ())
      return FakeCamera::cameras ();
#ifdef Q_OS_LINUX
   return V4l2Camera::cameras ();
#else
   return QList<CameraInfo> ();
#endif
   }


bool Camera::present (void)
   {
   if (FakeCamera::inUse ())
      return !FakeCamera::cameras ().isEmpty ();
#ifdef Q_OS_LINUX
   return V4l2Camera::present ();
#else
   return false;
#endif
   }


Camera *Camera::create (const QString &id, QObject *parent)
   {
   if (FakeCamera::inUse ())
      return FakeCamera::make (id, parent);
#ifdef Q_OS_LINUX
   for (const CameraInfo &info : V4l2Camera::cameras ())
      if (info.id == id)
         return new V4l2Camera (id, parent);
#endif
   return nullptr;
   }


//! true if two sizes have the same shape, to within a hundredth
static bool same_shape (const QSize &a, const QSize &b)
   {
   qint64 lhs = (qint64)a.width () * b.height ();
   qint64 rhs = (qint64)b.width () * a.height ();

   return qAbs (lhs - rhs) * 100 <= qMax (lhs, rhs);
   }


QSize Camera::previewSize (const QList<QSize> &sizes, const QSize &still)
   {
   QSize shaped, wide, largest;

   for (const QSize &size : sizes)
      {
      if (size.isEmpty ())
         continue;
      if (!largest.isValid () || size.width () > largest.width ())
         largest = size;
      if (size.width () < PreviewWidth)
         continue;
      if (!wide.isValid () || size.width () < wide.width ())
         wide = size;
      if (still.isValid () && same_shape (size, still)
          && (!shaped.isValid () || size.width () < shaped.width ()))
         shaped = size;
      }
   return shaped.isValid () ? shaped : wide.isValid () ? wide : largest;
   }


/***************************** JPEG pictures *****************************/

// the markers of a JPEG which matter here
enum
   {
   M_SOF0 = 0xc0,   // the picture's size: SOF0 to SOF15, but for the next 3
   M_DHT = 0xc4,    // Huffman tables
   M_JPG = 0xc8,
   M_DAC = 0xcc,
   M_SOF15 = 0xcf,
   M_RST0 = 0xd0,   // these, to RST7, and the next two have no length
   M_SOI = 0xd8,    // start of the file
   M_EOI = 0xd9,    // end of the file
   M_SOS = 0xda,    // start of the picture's data
   M_TEM = 0x01,
   };

/* The Huffman tables a Motion-JPEG picture means when it has none, which
   are the ones the JPEG standard gives as typical, in its section K.3.
   Each is the number of codes of each length from 1 to 16 bits, then the
   value each code stands for */
static const unsigned char dc_lum_bits [16] =
   {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};

static const unsigned char dc_chrom_bits [16] =
   {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};

static const unsigned char dc_vals [12] =
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static const unsigned char ac_lum_bits [16] =
   {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};

static const unsigned char ac_lum_vals [162] =
   {
   0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
   0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
   0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
   0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
   0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
   0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
   0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
   0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
   0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
   0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
   0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
   0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
   0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
   0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
   0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
   0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
   0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
   0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
   0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
   0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
   0xf9, 0xfa
   };

static const unsigned char ac_chrom_bits [16] =
   {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};

static const unsigned char ac_chrom_vals [162] =
   {
   0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
   0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
   0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
   0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
   0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
   0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
   0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
   0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
   0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
   0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
   0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
   0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
   0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
   0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
   0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
   0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
   0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
   0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
   0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
   0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
   0xf9, 0xfa
   };


/** add a Huffman table to some JPEG data, as a segment of its own

   \param out     the data to add to
   \param id      the table's class and number: 0x00 and 0x01 are the DC
                  tables for brightness and colour, 0x10 and 0x11 the AC
   \param bits    how many codes there are of each length
   \param vals    the value of each code
   \param count   the number of values */
static void add_table (QByteArray &out, int id, const unsigned char *bits,
                       const unsigned char *vals, int count)
   {
   int len = 2 + 1 + 16 + count;

   out.append ((char)0xff);
   out.append ((char)M_DHT);
   out.append ((char)(len >> 8));
   out.append ((char)(len & 0xff));
   out.append ((char)id);
   out.append ((const char *)bits, 16);
   out.append ((const char *)vals, count);
   }


//! the standard Huffman tables, in the order a JPEG encoder writes them
static QByteArray standard_tables (void)
   {
   QByteArray out;

   add_table (out, 0x00, dc_lum_bits, dc_vals, sizeof (dc_vals));
   add_table (out, 0x10, ac_lum_bits, ac_lum_vals, sizeof (ac_lum_vals));
   add_table (out, 0x01, dc_chrom_bits, dc_vals, sizeof (dc_vals));
   add_table (out, 0x11, ac_chrom_bits, ac_chrom_vals,
              sizeof (ac_chrom_vals));
   return out;
   }


/** look through a JPEG's headers

   \param jpeg     the JPEG
   \param size     returns the size of the picture
   \param depth    returns its bits per pixel
   \param has_dht  returns true if it has Huffman tables
   \param sos      returns where the picture's data starts: the offset
                   of its start-of-scan marker
   \param end      returns the offset just past the end marker
   \returns an empty string if OK, else what is wrong */
static QString parse_jpeg (const QByteArray &jpeg, QSize &size, int &depth,
                           bool &has_dht, int &sos, int &end)
   {
   const unsigned char *data = (const unsigned char *)jpeg.constData ();
   int total = jpeg.size ();
   int pos = 2;

   size = QSize ();
   depth = 0;
   has_dht = false;
   sos = end = -1;
   if (total < 4 || data [0] != 0xff || data [1] != M_SOI)
      return QObject::tr ("The picture is not a JPEG");

   while (sos < 0)
      {
      // a marker is 0xff, perhaps several, then what kind it is
      if (pos + 2 > total || data [pos] != 0xff)
         return QObject::tr ("The picture's JPEG headers are damaged");
      while (pos + 1 < total && data [pos + 1] == 0xff)
         pos++;
      if (pos + 2 > total)
         return QObject::tr ("The picture's JPEG headers are cut short");

      int marker = data [pos + 1];

      if (marker == M_TEM || (marker >= M_RST0 && marker <= M_SOI))
         {
         pos += 2;
         continue;
         }
      if (marker == M_EOI)
         return QObject::tr ("The JPEG has no picture in it");
      if (pos + 4 > total)
         return QObject::tr ("The picture's JPEG headers are cut short");

      int len = data [pos + 2] << 8 | data [pos + 3];

      if (len < 2 || pos + 2 + len > total)
         return QObject::tr ("The picture's JPEG headers are cut short");
      if (marker == M_DHT)
         has_dht = true;
      else if (marker == M_SOS)
         sos = pos;
      else if (marker >= M_SOF0 && marker <= M_SOF15 && marker != M_JPG
               && marker != M_DAC)
         {
         if (len < 8)
            return QObject::tr ("The picture's JPEG headers are damaged");

         const unsigned char *sof = data + pos + 4;
         int comps = sof [5];

         if (sof [0] != 8 || (comps != 1 && comps != 3))
            return QObject::tr ("The picture is a kind of JPEG which "
                                "cannot be kept");
         size = QSize (sof [3] << 8 | sof [4], sof [1] << 8 | sof [2]);
         depth = comps * 8;
         }
      pos += 2 + len;
      }
   if (size.isEmpty ())
      return QObject::tr ("The JPEG does not give the picture's size");

   // the end marker cannot appear within the picture's data
   static const char eoi [] = {(char)0xff, (char)M_EOI};
   int at = jpeg.lastIndexOf (QByteArray::fromRawData (eoi, 2));

   if (at < pos)
      return QObject::tr ("The picture is cut short: only part of it "
                          "arrived");
   end = at + 2;
   return QString ();
   }


QString cameraJpegInfo (const QByteArray &jpeg, QSize &size, int &depth,
                        bool &has_dht)
   {
   int sos, end;

   return parse_jpeg (jpeg, size, depth, has_dht, sos, end);
   }


QString cameraStill (const QByteArray &frame, QByteArray &jpeg)
   {
   QSize size;
   int depth, sos, end;
   bool has_dht;
   QString err = parse_jpeg (frame, size, depth, has_dht, sos, end);

   jpeg.clear ();
   if (!err.isEmpty ())
      return err;
   if (has_dht)
      {
      // the same bytes, sharing them if nothing follows the picture
      jpeg = end == frame.size () ? frame : frame.left (end);
      return QString ();
      }

   QByteArray tables = standard_tables ();

   jpeg.reserve (end + tables.size ());
   jpeg.append (frame.constData (), sos);
   jpeg.append (tables);
   jpeg.append (frame.constData () + sos, end - sos);
   return QString ();
   }


/****************************** fake camera ******************************/

void FakeCamera::use (const QList<Spec> &specs)
   {
   fake_specs = specs;
   fake_in_use = true;
   }


void FakeCamera::useReal (void)
   {
   fake_specs.clear ();
   fake_in_use = false;
   }


bool FakeCamera::inUse (void)
   {
   return fake_in_use;
   }


QList<CameraInfo> FakeCamera::cameras (void)
   {
   QList<CameraInfo> list;

   for (const Spec &spec : fake_specs)
      {
      CameraInfo info;

      info.id = spec.id;
      info.name = spec.name;
      list << info;
      }
   return list;
   }


FakeCamera *FakeCamera::make (const QString &id, QObject *parent)
   {
   for (const Spec &spec : fake_specs)
      if (spec.id == id)
         return new FakeCamera (spec, parent);
   return nullptr;
   }


//! a picture of a page with a number on it
static QImage make_image (const QSize &size, int num)
   {
   QImage image (size, QImage::Format_RGB888);
   QPainter p;

   image.fill (QColor (250, 248, 240));
   if (p.begin (&image))
      {
      int unit = qMax (1, size.height () / 20);
      QFont font = p.font ();

      // lines of 'text' down the page, which differ with the number
      p.setPen (Qt::NoPen);
      p.setBrush (QColor (60, 60, 70));
      for (int row = 2; row < 18; row += 2)
         p.drawRect (unit, row * unit,
                     size.width () - unit * (2 + (row + num) % 5),
                     qMax (1, unit / 3));
      font.setPixelSize (qMax (8, unit * 6));
      p.setFont (font);
      p.setPen (QColor (180, 30, 30));
      p.drawText (image.rect (), Qt::AlignCenter, QString::number (num));
      p.end ();
      }
   return image;
   }


QByteArray FakeCamera::makeJpeg (const QSize &size, int num)
   {
   QByteArray jpeg;
   QBuffer buf (&jpeg);

   buf.open (QIODevice::WriteOnly);
   make_image (size, num).save (&buf, "JPG", 80);
   return jpeg;
   }


FakeCamera::FakeCamera (const Spec &spec, QObject *parent)
      : Camera (parent), _spec (spec)
   {
   _preview = new QTimer (this);
   _preview->setInterval (FAKE_PREVIEW_MS);
   connect (_preview, &QTimer::timeout, this, &FakeCamera::sendPreview);
   fake_live << this;
   }


FakeCamera::~FakeCamera ()
   {
   fake_live.removeAll (this);
   }


bool FakeCamera::push (const QString &id, const QByteArray &still,
                       const QByteArray &after)
   {
   bool sent = false;

   for (FakeCamera *cam : fake_live)
      {
      if (cam->_spec.id != id || !cam->_open || !cam->_preview->isActive ())
         continue;
      cam->_pushed << still;
      cam->_after << after;

      // it arrives in its own time, as anything from a camera does
      QTimer::singleShot (0, cam, &FakeCamera::sendPushed);
      sent = true;
      }
   return sent;
   }


void FakeCamera::sendPushed (void)
   {
   while (_open && !_pushed.isEmpty ())
      {
      QByteArray jpeg;
      QByteArray frame = _pushed.takeFirst ();
      QByteArray after = _after.takeFirst ();

      // one which did not arrive whole is dropped, with what follows it
      if (!cameraStill (frame, jpeg).isEmpty ())
         continue;
      _follow = true;
      _follow_with = after;
      emit stillPushed (jpeg);
      }
   _pushed.clear ();
   _after.clear ();
   }


QString FakeCamera::open (void)
   {
   if (!_spec.open_err.isEmpty ())
      return _spec.open_err;
   _open = true;
   return QString ();
   }


void FakeCamera::close (void)
   {
   _preview->stop ();
   _open = false;
   _capturing = false;
   _follow = false;
   _pushed.clear ();
   _after.clear ();
   }


QList<QSize> FakeCamera::stillSizes (void) const
   {
   QList<QSize> sizes = _spec.sizes;

   std::sort (sizes.begin (), sizes.end (),
              [] (const QSize &a, const QSize &b) {
      return (qint64)a.width () * a.height ()
            > (qint64)b.width () * b.height ();
   });
   return sizes;
   }


void FakeCamera::startPreview (const QSize &still)
   {
   if (!_open)
      {
      emit failed (tr ("The camera is not open"));
      return;
      }
   _preview_size = previewSize (_spec.sizes, still);
   _preview->start ();
   }


void FakeCamera::stopPreview (void)
   {
   _preview->stop ();
   }


void FakeCamera::capture (const QSize &size)
   {
   if (!_open)
      {
      emit failed (tr ("The camera is not open"));
      return;
      }
   if (_capturing)
      return;
   _capturing = true;
   _still_size = size;
   QTimer::singleShot (_spec.capture_ms, this, &FakeCamera::sendStill);
   }


void FakeCamera::sendPreview (void)
   {
   // a camera sends nothing else while it takes a still
   if (!_open || _capturing || !_preview_size.isValid ())
      return;
   _previews++;

   // the picture after a still it sent unasked is not for showing
   if (_follow)
      {
      QByteArray jpeg = _follow_with;

      if (jpeg.isEmpty ())
         jpeg = makeJpeg (_preview_size, _previews);
      _follow = false;
      emit pushFollowed (jpeg);
      return;
      }
   if (_spec.stills.isEmpty ())
      {
      emit previewFrame (make_image (_preview_size, _previews));
      return;
      }

   // what the next still will be, as a camera's view is
   int next = _stills % _spec.stills.size ();

   if (next != _shown || _view.size () != _preview_size)
      {
      QByteArray jpeg;

      cameraStill (_spec.stills [next], jpeg);
      _view = QImage::fromData (jpeg, "JPG")
            .scaled (_preview_size, Qt::IgnoreAspectRatio,
                     Qt::SmoothTransformation);
      _shown = next;
      }
   if (!_view.isNull ())
      emit previewFrame (_view);
   }


void FakeCamera::sendStill (void)
   {
   QByteArray frame, jpeg;
   QString err = _spec.capture_err;

   // it may have been closed since
   if (!_open || !_capturing)
      return;
   _capturing = false;
   if (err.isEmpty ())
      {
      if (_spec.stills.isEmpty ())
         frame = makeJpeg (_still_size, _stills + 1);
      else
         frame = _spec.stills [_stills % _spec.stills.size ()];
      _stills++;
      err = cameraStill (frame, jpeg);
      }
   if (err.isEmpty ())
      emit captured (jpeg);
   else
      emit failed (err);
   }
