/***************************************************************************
                          qimageioext.cpp  -  description
                             -------------------
    begin                : Mon Mar 26 2001
    copyright            : (C) 2001 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QImageReader>
#include <QImageWriter>
#include <QTextStream>

#include "qimageioext.h"

#include <qcolor.h>
#include <qfile.h>
#include <qimage.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_LIBTIFF

#include <tiffio.h>

//16 bit images are currently not supported
void writeTiff(QImageIO* iio)
{
  QImage image;
  TIFF* tiffile;
  QRgb rgb;
  char *data;
  int y;
  int channels;
  int quality;
  int compression;
  int width;
  int bytes;
  int depth;
  bool color;
  bool has_alpha;
  Q_UINT16  extra_samples[1];
  QString params;
  float x_res,y_res;
  //PACKBITS is supported for all formats and lossless; use it as default, if the user
  //didn't request a specific format.
  compression = COMPRESSION_PACKBITS;
  if(iio->parameters())
  {
    params = iio->parameters();
    if(params.contains("COMPRESSION_NONE"))
      compression = COMPRESSION_NONE;
    else if(params.contains("COMPRESSION_CCITTRLE")) //Huffman
      compression = COMPRESSION_CCITTRLE;
    else if(params.contains("COMPRESSION_CCITTFAX3")) //group3 fax
      compression = COMPRESSION_CCITTFAX3;
    else if(params.contains("COMPRESSION_CCITTFAX4")) //group4 fax
      compression = COMPRESSION_CCITTFAX4;
    else if(params.contains("COMPRESSION_PACKBITS"))  //packed bits
      compression = COMPRESSION_PACKBITS;
    else
    {
      bool ok;
      int i = params.toInt(&ok);
      if(ok)
      {
        if(i>=0 && i<=100)
        {
          compression = COMPRESSION_JPEG;
          quality = i;
        }
      }
    }
  }
  image = iio->image();
  width = image.width();
  depth = image.depth();
  has_alpha = image.hasAlphaBuffer();
  if(depth == 32) depth = 8;
  if(depth == 8)
  {
    if((compression ==  COMPRESSION_CCITTRLE) ||
       (compression ==  COMPRESSION_CCITTFAX3) ||
       (compression ==  COMPRESSION_CCITTFAX4))
    {
      iio->setStatus(1);//compression type unsupported for this format
      return;
    }
  }
  else if(depth == 1)
  {
    if(compression ==  COMPRESSION_JPEG)
    {
      iio->setStatus(1);//compression type unsupported for this format
      return;
    }
  }

  if(image.depth()>1 && !image.allGray())
  {
    channels = 3;
    if(has_alpha)
      channels += 1;
    color = true;
  }
  else
  {
    channels = 1;
    color = false;
  }

  //For non-lineart images, bytes per channel is always one
  bytes = 1;

  if((image.depth() == 1) && (image.numColors() == 2))
  {
	  if(qGray(image.color(0)) > qGray(image.color(1)))
    {
	    // 0=dark/black, 1=light/white - invert
	    image.detach();
	    for(int y=0; y<image.height(); y++ )
      {
		    uchar *p = image.scanLine(y);
		    uchar *end = p + image.bytesPerLine();
		    while ( p < end )
		      *p++ ^= 0xff;
	    }
	  }
  }

  tiffile = TIFFOpen(QFile::encodeName(iio->fileName()), "w");
qDebug("TIFF: try to open %s",iio->fileName().latin1());
  if(!tiffile)
  {
    iio->setStatus(1);
    return ;
  }

  TIFFSetField(tiffile, TIFFTAG_IMAGEWIDTH, image.width());
  TIFFSetField(tiffile, TIFFTAG_IMAGELENGTH, image.height());
  TIFFSetField(tiffile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE, channels);
  TIFFSetField(tiffile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tiffile, TIFFTAG_COMPRESSION, compression);
  TIFFSetField(tiffile, TIFFTAG_SOFTWARE, "QuiteInsane");

  if((image.dotsPerMeterX() > 0) && (image.dotsPerMeterY() > 0))
  {
    TIFFSetField(tiffile, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    x_res = float(image.dotsPerMeterX())/(100.0/2.54);
    y_res = float(image.dotsPerMeterY())/(100.0/2.54);
    TIFFSetField(tiffile, TIFFTAG_XRESOLUTION,x_res);
    TIFFSetField(tiffile, TIFFTAG_YRESOLUTION,y_res);
  }

  if(compression == COMPRESSION_JPEG)
  {
    TIFFSetField(tiffile, TIFFTAG_JPEGQUALITY, quality);
    TIFFSetField(tiffile, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RAW);
  }

  if(color)
  {
    TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE,8);
    if(has_alpha)
      TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL,4);
    else
      TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL,3);
  }
  else
  {
    if(depth == 1) //lineart
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
      TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE,1);
      TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL,1);
    }
    else // grayscale
    {
      TIFFSetField(tiffile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      TIFFSetField(tiffile, TIFFTAG_BITSPERSAMPLE,8);
      TIFFSetField(tiffile, TIFFTAG_SAMPLESPERPIXEL,1);
    }
  }

  if(has_alpha)
  {
    extra_samples [0] = EXTRASAMPLE_ASSOCALPHA;
    TIFFSetField (tiffile, TIFFTAG_EXTRASAMPLES, 1, extra_samples);
  }
  TIFFSetField(tiffile,TIFFTAG_ROWSPERSTRIP,TIFFDefaultStripSize(tiffile,0));

  if(image.depth() > 1)
    data = new char[image.width() * channels * bytes];
  else
    data = new char[(image.width()+7)/8];

  if(data)
  {
    for(y = 0; y < image.height(); y++)
    {
      if(image.depth() > 1)
      {
        for(int i=0;i<image.width();i++)
        {
          rgb = image.pixel(i,y);
          if(channels == 3)
          {
            data[i*3] = qRed(rgb);
            data[i*3+1] = qGreen(rgb);
            data[i*3+2] = qBlue(rgb);
          }
          else if(channels == 4)//alpha
          {
            int alpha = qAlpha(rgb);
            data[i*4] = Q_UINT8(qRed(rgb)*alpha/255);
            data[i*4+1] = Q_UINT8(qGreen(rgb)*alpha/255);
            data[i*4+2] = Q_UINT8(qBlue(rgb)*alpha/255);
            data[i*4+3] = Q_UINT8(alpha);
          }
          else
          {
            data[i] = qGray(rgb);
          }
        }
        if(TIFFWriteScanline(tiffile,data, y, 0) == -1) //error
        {
          TIFFClose(tiffile);
          delete [] data;
          iio->setStatus(1);
          return;
        }
      }
      else
      {
        if(TIFFWriteScanline(tiffile,(char*)image.scanLine(y), y, 0) == -1) //error
        {
          TIFFClose(tiffile);
          delete [] data;
          iio->setStatus(1);
          return;
        }
      }
    }
    iio->setStatus(0);
    delete [] data;
  }
  else
  {
    //could not allocate data
    iio->setStatus(1);
  }
  TIFFClose(tiffile);
}

void readTiff(QImageIO* iio)
{
  QImage image;
  int depth;
  int i;
  float x_resolution, y_resolution;
  int y;
  TIFF *tiff;
  Q_UINT16 extra;
  Q_UINT16* extra_types;
  Q_UINT16 compress_tag;
  Q_UINT32 height,width;
  Q_UINT16 bits_per_sample;
  Q_UINT16 interlace;
  Q_UINT16 max_sample_value;
  Q_UINT16 min_sample_value;
  Q_UINT16 photometric;
  Q_UINT16 samples_per_pixel;
  Q_UINT16 units;
  bool color;
  bool has_alpha = false;
  bool min_is_white = false;
  tiff=TIFFOpen(QFile::encodeName(iio->fileName()),"r");
  if(tiff == (TIFF *) 0L)
  {
    iio->setStatus(1);
    return;
  }
  TIFFGetField(tiff,TIFFTAG_IMAGEWIDTH,&width);
  TIFFGetField(tiff,TIFFTAG_IMAGELENGTH,&height);
  TIFFGetFieldDefaulted(tiff,TIFFTAG_BITSPERSAMPLE,&bits_per_sample);
  TIFFGetFieldDefaulted(tiff,TIFFTAG_PHOTOMETRIC,&photometric);
  TIFFGetField(tiff,TIFFTAG_SAMPLESPERPIXEL,&samples_per_pixel);
  TIFFGetField(tiff,TIFFTAG_EXTRASAMPLES,&extra,&extra_types);
  TIFFGetFieldDefaulted(tiff,TIFFTAG_PLANARCONFIG,&interlace);

  if(extra>0 && (extra_types[0] == EXTRASAMPLE_ASSOCALPHA))
  {
    has_alpha = true;
  }
  //The formats we can read. If the tiff has been written by QuiteInsane,
  //then we can read it this way. For other formats, we fall back to
  //RGBA - at least for the moment.
  if((!TIFFIsTiled(tiff)) &&
     (interlace == PLANARCONFIG_CONTIG) &&
     ((bits_per_sample == 1)   ||
      (bits_per_sample == 8)   ||
      (bits_per_sample == 16)) &&
     ((photometric == PHOTOMETRIC_MINISWHITE) ||
      (photometric == PHOTOMETRIC_MINISBLACK) ||
      (photometric == PHOTOMETRIC_RGB)        ||
      (photometric == PHOTOMETRIC_PALETTE)))
  {
    if((bits_per_sample == 8) || (bits_per_sample == 16))
    {
      if(photometric == PHOTOMETRIC_RGB)
      {
        color = true;
        depth = 32;
        if(!image.create(width,height,depth))
        {
          iio->setStatus(1);
          return;
        }
        image.setAlphaBuffer(has_alpha);
      }
      else
      {
        depth = 8;
        if(!image.create(width,height,depth))
        {
          iio->setStatus(1);
          return;
        }
        image.setNumColors(256);
        image.setAlphaBuffer(has_alpha);
        if(photometric == PHOTOMETRIC_PALETTE)
        {
          color = true;
          Q_UINT16* blue_colormap;
          Q_UINT16* green_colormap;
          Q_UINT16* red_colormap;

          TIFFGetField(tiff,TIFFTAG_COLORMAP,&red_colormap,&green_colormap,
                       &blue_colormap);
          for(i=0; i < 256; i++)
          {
            image.setColor(i,qRgb(red_colormap[i]/257,
                                  green_colormap[i]/257,
                                  blue_colormap[i]/257));

          }
        }
        else if(photometric == PHOTOMETRIC_MINISWHITE)
        {
          color = false; //grayscale
          min_is_white = true;
        }
        else
        {
          color = false;
        }
      }
    }
    else if(bits_per_sample == 1) //lineart, B&W
    {
      depth = 1;
      if(!image.create(width,height,depth,2,QImage::BigEndian))
      {
        iio->setStatus(1);
        return;
      }
    }
    if(depth == 1)
    {
      image.setColor(0,qRgb(255,255,255));
      image.setColor(1,qRgb(0,0,0));
    }
    TIFFGetFieldDefaulted(tiff,TIFFTAG_RESOLUTIONUNIT,&units);
    TIFFGetFieldDefaulted(tiff,TIFFTAG_XRESOLUTION,&x_resolution);
    TIFFGetFieldDefaulted(tiff,TIFFTAG_YRESOLUTION,&y_resolution);
    if(units == 1)
    {
      image.setDotsPerMeterX(0);
      image.setDotsPerMeterY(0);
    }
    else if(units == 2)
    {
      image.setDotsPerMeterX(int(x_resolution*100.0/2.54));
      image.setDotsPerMeterY(int(y_resolution*100.0/2.54));
    }
    else if(units == 3)
    {
      image.setDotsPerMeterX(int(x_resolution*100.0));
      image.setDotsPerMeterY(int(y_resolution*100.0));
    }
    TIFFGetFieldDefaulted(tiff,TIFFTAG_COMPRESSION,&compress_tag);
    TIFFGetFieldDefaulted(tiff,TIFFTAG_MINSAMPLEVALUE,&min_sample_value);
    TIFFGetFieldDefaulted(tiff,TIFFTAG_MAXSAMPLEVALUE,&max_sample_value);
    TIFFGetFieldDefaulted(tiff,TIFFTAG_SAMPLESPERPIXEL,&samples_per_pixel);

    Q_UINT8* line;
    line = 0;
    if(depth == 32)
    {
      if(bits_per_sample == 16)
      {
        if(has_alpha)
          line = new Q_UINT8 [width*8];
        else
          line = new Q_UINT8 [width*6];
      }
      else
      {
        if(has_alpha)
          line = new Q_UINT8 [width*4];
        else
          line = new Q_UINT8 [width*3];
      }
    }
    if(depth == 8)
    {
      if(bits_per_sample == 16)
        line = new Q_UINT8 [width*2];
      else
        line = new Q_UINT8 [width];
    }
    if(depth == 1)
      line = new Q_UINT8 [TIFFScanlineSize(tiff)];

    if(line)
    {
      for(y=0; y < (int) image.height(); y++)
      {
        if(TIFFReadScanline(tiff,(unsigned char*)line,y,0) == -1)
        {
          delete [] line;
          TIFFClose(tiff);
          iio->setStatus(1);
          return;
        }
        if(depth == 32)
        {
          Q_UINT16* line16;
          line16 = (Q_UINT16*) line;
          int Qt::red,Qt::green,Qt::blue,alpha;
          for(i=0;i<image.width();i++)
          {
            if(bits_per_sample == 16)
            {
              if(has_alpha)
              {
                Qt::red = int(line16[i*4])/257;
                Qt::green = int(line16[i*4+1])/257;
                Qt::blue = int(line16[i*4+2])/257;
                alpha = int(line16[i*4+3]/257);
                if(alpha > 0)
                {
                  Qt::red = (Qt::red*255)/alpha;
                  Qt::green = (Qt::green*255)/alpha;
                  Qt::blue = (Qt::blue*255)/alpha;
                }
                image.setPixel(i,y,qRgba(Qt::red,Qt::green,Qt::blue,alpha));
              }
              else
                image.setPixel(i,y,qRgb(int(line16[i*3]),
                                        int(line16[i*3+1]),
                                        int(line16[i*3+2])));
            }
            else
            {
              if(has_alpha)
              {
                Qt::red = int(line[i*4]);
                Qt::green = int(line[i*4+1]);
                Qt::blue = int(line[i*4+2]);
                alpha = int(line[i*4+3]);
                if(alpha > 0)
                {
                  Qt::red = (Qt::red*255)/alpha;
                  Qt::green = (Qt::green*255)/alpha;
                  Qt::blue = (Qt::blue*255)/alpha;
                }
                image.setPixel(i,y,qRgba(Qt::red,Qt::green,Qt::blue,alpha));
              }
              else
                image.setPixel(i,y,qRgb(int(line[i*3]),
                                        int(line[i*3+1]),
                                        int(line[i*3+2])));
            }
          }
        }
        if(depth == 8)
        {
          if(bits_per_sample == 16)
          {
            Q_UINT16* line16;
            line16 = (Q_UINT16*) line;
            for(i=0;i<image.width();i++)
            {
              int Qt::gray = line16[i]/257;
              if(color)
              {
                image.setPixel(i,y,Qt::gray);
              }
              else
              {
                if(min_is_white)
                {
                  Qt::gray = 255 - Qt::gray;
                  image.setPixel(i,y,Qt::gray);
                  image.setColor(Qt::gray,qRgb(Qt::gray,Qt::gray,Qt::gray));
                }
                else
                {
                  image.setPixel(i,y,Qt::gray);
                  image.setColor(Qt::gray,qRgb(Qt::gray,Qt::gray,Qt::gray));
                }
              }
            }
          }
          else if(bits_per_sample == 8)
          {
            for(i=0;i<image.width();i++)
            {
              if(color)
              {
                image.setPixel(i,y,line[i]);
              }
              else
              {
                image.setPixel(i,y,line[i]);
                if(min_is_white)
                {
                  image.setPixel(i,y,line[i]);
                  image.setColor(line[i],qRgb(int(line[i]^0xff),
                                              int(line[i]^0xff),
                                              int(line[i]^0xff)));
                }
                else
                {
                  image.setPixel(i,y,line[i]);
                  image.setColor(line[i],qRgb(int(line[i]),
                                              int(line[i]),
                                              int(line[i])));
                }
              }
            }
          }
        }
        if(depth == 1)
        {
          memcpy(image.scanLine(y),line,TIFFScanlineSize(tiff));
        }
      }
      delete [] line;
      TIFFClose(tiff);
    }
    else //error
    {
      TIFFClose(tiff);
      iio->setStatus(1);
      return;
    }
  }
//otherwise, try to read an RGBA image
  else
  {
    unsigned int i;
    unsigned int i2;
    unsigned char* idat;
    uint32 w, h;
    size_t npixels;
    uint32* raster;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);
    npixels = w * h;
    raster = (uint32*) _TIFFmalloc(npixels * sizeof (uint32));
    if(raster)
    {
      if(TIFFReadRGBAImage(tiff, w, h, raster, 0))
      {
        if(!image.create(w,h,32))
        {
          iio->setStatus(1);
          _TIFFfree(raster);
          return;
        }
        image.setAlphaBuffer(true);
        //we finaly can transfer the data to our image
        idat = (unsigned char*)raster;
        for(i=0;i<h;i++)
        {
          for(i2=0;i2<w;i2++)
          {
            image.setPixel(i2,i,qRgba((int)*(idat),
                                      (int)*(idat+1),
                                      (int)*(idat+2),
                                      (int)*(idat+3)));
            idat += 4;
          }
        }
      }
      _TIFFfree(raster);
      image = image.mirror(false,true);
    }
    else //couldn't allocate raster
    {
      TIFFClose(tiff);
      iio->setStatus(1);
      return;
    }
    TIFFClose(tiff);
  }
  iio->setImage(image);
  iio->setStatus(0);
}

void initTiffIO()
{
  QImageIO::defineIOHandler("TIF","^[MI][MI][\\x01*][\\x01*]",0,
                            readTiff,writeTiff);
}
#endif

#if 0

/*****************************************************************************
  PBM/PGM/PPM (ASCII and RAW) image read/write functions
 *****************************************************************************/
//Why do we need this?
//Because the relevant function is buggy in all versions of
//Qt2.2.x, at least up to version 2.2.4
//This leads to random crashes when one tries to save an 8 bit
//image in PGM format.
//There's also a problem with saving 1 bit images in PBM format
//in the original code, which leads to a conversion
//to 8 bit and therefore to bloated files in the wrong format.
//This reader can also read images in 16bit raw format. It
//converts the data to 8bit/channel.
//This code is based on the functions
//static void read/write_pbm_image( QImageIO *iio ) in qimage.cpp

void qis_get_dots_per_meter(QIODevice *d,int* dots_m_x,int* dots_m_y)
{
  QString qs;
  bool ok;
  QTextStream ts(d);
  qs = ts .readLine();
  for(int i=0;i<3;i++)
  {
    qs = ts .readLine();
    if(qs.contains("#DOTS_PER_METER_X"))
    {
      qs = qs.right(qs.length()-17);
      qs = qs.stripWhiteSpace();
      *dots_m_x = qs.toInt(&ok);
      if(!ok)
        *dots_m_x = 0;
    }
    else if(qs.contains("#DOTS_PER_METER_Y"))
    {
      qs = qs.right(qs.length()-17);
      qs = qs.stripWhiteSpace();
      *dots_m_y = qs.toInt(&ok);
      if(!ok)
        *dots_m_y = 0;
    }
  }
  //if one value is invalid, set both values to 0
  if((*dots_m_x == 0) || (*dots_m_y == 0))
  {
    *dots_m_x = 0;
    *dots_m_y = 0;
  }
  d->reset();
}

int qis_read_pbm_int( QIODevice *d )
{
  int	  c;
  int	  val = -1;
  bool  digit;
  const int buflen = 100;
  char  buf[buflen];

  while(true)
  {
    if( (c=d->getch()) == EOF )		// end of file
      break;
    digit = isdigit(c);
    if ( val != -1 )
    {
      if ( digit )
      {
  	     val = 10*val + c - '0';
  	     continue;
      }
      else
      {
  	     if ( c == '#' )			// comment
  	       d->readLine( buf, buflen );
  	     break;
      }
    }
    if ( digit )				// first digit
	   val = c - '0';
    else if ( isspace(c) )
	   continue;
	  else if ( c == '#' )
	   d->readLine( buf, buflen );
	  else
	   break;
  }
  return val;
}

QImage *qis_read_pbm_image( QImageReader *iio )	// read PBM image data
{
  const int	buflen = 300;
  char	buf[buflen];
  QIODevice  *d = iio->device();
  int		w, h, nbits, mcc, y;
  int		pbm_bpl;
  int red,green,blue;
  char	type;
  bool	raw;
  bool is_16_bit = false;
  QImage	image;
  int dotsmx;
  int dotsmy;
  QString params;
  int word_size;
  bool big_endian;

  qSysInfo (&word_size,&big_endian);
  qis_get_dots_per_meter(d,&dotsmx,&dotsmy);

  if ( d->readBlock( buf, 3 ) != 3 )			// read P[1-6]<white-space>
  	return NULL;
  if ( !(buf[0] == 'P' && isdigit(buf[1]) && isspace(buf[2])) )
  	return NULL;
  switch ( (type=buf[1]) )
  {
  	case '1':				// ascii PBM
  	case '4':				// raw PBM
	    nbits = 1;
	    break;
	  case '2':				// ascii PGM
	  case '5':				// raw PGM
	    nbits = 8;
	    break;
	  case '3':				// ascii PPM
	  case '6':				// raw PPM
	    nbits = 32;
	    break;
	  default:
    	return NULL;
  }
  raw = type >= '4';
  w = qis_read_pbm_int( d );			// get image width
  h = qis_read_pbm_int( d );			// get image height
  if ( nbits == 1 )
	  mcc = 0;				// ignore max color component
  else
	  mcc = qis_read_pbm_int( d );		// get max color component
  if ( w <= 0 || w > 32767 || h <= 0 || h > 32767 || mcc < 0)
  	return NULL;

  int maxc = mcc;
  if ( maxc > 255 )
	  maxc = 255;
  if(mcc == 65535)
    is_16_bit = true;
  image.create( w, h, nbits, 0,
          		  nbits == 1 ? QImage::BigEndian :  QImage::IgnoreEndian );
  if ( image.isNull() )
  	return NULL;

  //set resolution
  image.setDotsPerMeterX(dotsmx);
  image.setDotsPerMeterY(dotsmy);

  pbm_bpl = (nbits*w+7)/8;			// bytes per scanline in PBM

  if ( raw )
  {				// read raw data
	  if ( nbits == 32 )
    {			// type 6
      if(is_16_bit)
        pbm_bpl = 6*w;
      else
  	    pbm_bpl = 3*w;
	    uchar *buf24 = new uchar[pbm_bpl], *b;
	    QRgb  *p;
	    QRgb  *end;
	    for ( y=0; y<h; y++ )
      {
		    if ( d->readBlock( (char *)buf24, pbm_bpl ) != pbm_bpl )
        {
		      delete[] buf24;
          return NULL;
		    }
	    	p = (QRgb *)image.scanLine( y );
    		end = p + w;
    		b = buf24;
    		while ( p < end )
        {
          if(is_16_bit)
          {
            if(!big_endian)
            {
              red = ((int(b[0]) << 8) + b[1]) / 256;
              green = ((int(b[2]) << 8) + b[3]) / 256;
              blue = ((int(b[4]) << 8) + b[5]) / 256;
            }
            else
            {
              red = ((int(b[1]) << 8) + b[0]) / 256;
              green = ((int(b[3]) << 8) + b[2]) / 256;
              blue = ((int(b[5]) << 8) + b[4]) / 256;
            }
		        *p++ = qRgb(red,green,blue);
		        b += 6;
          }
          else
          {
		        *p++ = qRgb(b[0],b[1],b[2]);
		        b += 3;
          }
		    }
	    }
	    delete[] buf24;
	  }
    else
    {				// type 4,5
      uchar* buf16 = 0;
      uchar* b;
      uchar* p;
      uchar* end;
      if(is_16_bit && type == '5')
      {
        pbm_bpl = 2*w;
   	    buf16 = new uchar[pbm_bpl];
        b = buf16;
      }
      for ( y=0; y<h; y++ )
      {
        if(is_16_bit && type == '5')
        {
		      if(d->readBlock((char*)buf16,pbm_bpl ) != pbm_bpl)
          {
		        delete[] buf16;
  	        return NULL;
		      }
	    	  p = (uchar *)image.scanLine( y );
    		  end = p + w;
    		  b = buf16;
    		  while ( p < end )
          {
            if(is_16_bit)
            {
              if(!big_endian)
                red = ((int(b[0]) << 8) + b[1]) / 256;
              else
                red = ((int(b[1]) << 8) + b[0]) / 256;
		          *p++ = (uchar)red;
		          b += 2;
            }
          }
        }
        else
        {
		      if ( d->readBlock( (char *)image.scanLine(y), pbm_bpl )
			         != pbm_bpl )
          	return NULL;
	      }
      }
      if(buf16)
        delete [] buf16;
	  }
  }
  else
  {					// read ascii data
	  register uchar *p;
	  int n;
	  for ( y=0; y<h; y++ )
    {
	    p = image.scanLine( y );
	    n = pbm_bpl;
	    if ( nbits == 1 )
      {
		    int b;
		    while ( n-- )
        {
		      b = 0;
		      for ( int i=0; i<8; i++ )
			      b = (b << 1) | (qis_read_pbm_int(d) & 1);
		      *p++ = b;
		    }
	    }
      else if ( nbits == 8 )
      {
		    if ( mcc == maxc )
        {
		      while ( n-- )
          {
			      *p++ = qis_read_pbm_int( d );
		      }
		    }
        else
        {
		      while ( n-- )
          {
			      *p++ = qis_read_pbm_int( d ) * maxc / mcc;
		      }
		    }
	    }
      else
      {				// 32 bits
		    n /= 4;
		    int r, g, b;
		    if ( mcc == maxc )
        {
		      while ( n-- )
          {
       			r = qis_read_pbm_int( d );
       			g = qis_read_pbm_int( d );
       			b = qis_read_pbm_int( d );
       			*((QRgb*)p) = qRgb( r, g, b );
       			p += 4;
		      }
		    }
        else
        {
		      while ( n-- )
          {
       			r = qis_read_pbm_int( d ) * maxc / mcc;
       			g = qis_read_pbm_int( d ) * maxc / mcc;
       			b = qis_read_pbm_int( d ) * maxc / mcc;
       			*((QRgb*)p) = qRgb( r, g, b );
       			p += 4;
  		    }
	     	}
	    }
	  }
  }

  if ( nbits == 1 )
  {				// bitmap
	  image.setNumColors( 2 );
	  image.setColor( 0, qRgb(255,255,255) ); // white
	  image.setColor( 1, qRgb(0,0,0) );	// black
  }
  else if ( nbits == 8 )
  {			// graymap
	  image.setNumColors( maxc+1 );
	  for ( int i=0; i<=maxc; i++ )
	    image.setColor( i, qRgb(i*255/maxc,i*255/maxc,i*255/maxc) );
  }

  return new QImage (image);
}

//Format PNM is supported directly; i. e. the image is
//converted to the correct sub-format.

bool qis_write_pbm_image( QImageWriter *iio, QImage &image )
{
  QIODevice * out = iio->device();
  Q3CString str;

  Q3CString format = iio->format();
  format = format.left(3);			// ignore RAW part
  bool gray = false;
  bool detached = false;
  if(image.isNull())
    return false;
  if( format == "PBM" )
  {
    if(image.depth() > 1)
    {
      image.detach();
      image = image.convertDepth(1);
      detached = true;
    }
  }
  else if(format == "PGM")
  {
    gray = true;
    if(image.depth() < 8)
    {
      image.detach();
      image = image.convertDepth(8);
      detached = true;
    }
  }
  else if(format == "PPM")
  {
    gray = false;
    if(image.depth() < 8)
    {
      image.detach();
      image = image.convertDepth(8);
      detached = true;
    }
  }
  else if (format == "PNM")
  {
    if((image.depth() == 32) || (image.depth() == 8))
    {
      if(image.isGrayscale())
      {
        format = "PGM";
        gray = true;
      }
      else
      {
        format = "PPM";
      }
    }
    else
    {
      format = "PBM";
      if(image.depth() > 1)
      {
        image.detach();
	      image = image.convertDepth(1);
        detached = true;
      }
    }
  }
  else
    return false;

  if ( image.depth() == 1 && image.numColors() == 2 )
  {
	  if ( qGray(image.color(0)) < qGray(image.color(1)) )
    {
	    // 0=dark/black, 1=light/white - invert
      if(!detached)
	      image.detach();
	    for ( int y=0; y<image.height(); y++ )
      {
		    Q_UINT8* p = image.scanLine(y);
		    Q_UINT8* end = p + image.bytesPerLine();
		    while ( p < end )
		      *p++ ^= 0xff;
	    }
	  }
  }

  int w = image.width();
  int h = image.height();

  str.sprintf("P\n%d %d\n", w, h);

  switch (image.depth())
  {
	  case 1:
    {
	    str.insert(1, '4');
	    if (out->writeBlock(str, str.length()) != str.length())
		    return false;
	    w = (w+7)/8;
	    for (int y=0; y<h; y++)
      {
		    Q_UINT8* line = image.scanLine(y);
		    if ( w != (int)out->writeBlock((char*)line,w) )
		      return false;
	    }
	  }
	  break;

	  case 8:
    {
	    str.insert(1, gray ? '5' : '6');
	    str.append("255\n");
	    if (out->writeBlock(str, str.length()) != str.length())
		    return false;
      Q_UINT8* buf;
      int bpl;
      if(gray)
        bpl = w;
      else
        bpl = w*3;
      buf = new Q_UINT8 [bpl];
      if(buf)
      {
  	    for(int y=0; y<h; y++)
        {
          for(int x=0;x<w;x++)
          {
        		QRgb  rgb = image.pixel(x,y);
        		if(gray)
            {
              buf[x] = (Q_UINT8) qGray(rgb);
      		  }
            else
            {
           		buf[x*3] = (Q_UINT8) qRed(rgb);
           		buf[x*3+1] = (Q_UINT8) qGreen(rgb);
           		buf[x*3+2] = (Q_UINT8) qBlue(rgb);
     		    }
       		}
    	  	if ( bpl != (int)out->writeBlock((char*)buf, bpl) )
          {
            delete [] buf;
    		    return false;
    		  }
  	    }
  	    delete [] buf;
      }
      else
        return false;
	  }
	  break;

  	case 32:
    {
      str.insert(1, gray ? '5' : '6');
      str.append("255\n");
      if (out->writeBlock(str, str.length()) != str.length())
  		  return false;
      QRgb rgb;
      Q_UINT8* buf;
      int bpl;
      if(gray)
        bpl = w;
      else
        bpl = w*3;
      buf = new Q_UINT8 [bpl];
      if(buf)
      {
        for(int y=0; y<h; y++)
        {
          for(int x=0;x<w;x++)
          {
        		rgb = image.pixel(x,y);
        		if(gray)
            {
              buf[x] = (Q_UINT8)qGray(rgb);
      		  }
            else
            {
           		buf[x*3] = (Q_UINT8) qRed(rgb);
           		buf[x*3+1] = (Q_UINT8) qGreen(rgb);
           		buf[x*3+2] = (Q_UINT8) qBlue(rgb);
     		    }
       		}
    	  	if ( bpl != (int)out->writeBlock((char*)buf, bpl) )
          {
            delete [] buf;
    		    return false;
    		  }
    	  }
    	  delete [] buf;
      }
      else
        return false;
  	}
  }
  return true;
}

//This format is for preview images only. This IO handler scales the image
//to a size that fits 80x60 pixel if neccessary. This format has been chosen
//because it can be read by the Gimp and other image manipulation programs
//(and viewers).
QImage *qis_read_pnm7_image(QImageReader *iio)
{
  QIODevice  *d = iio->device();
  QImage image;
  int	pos = -1;
  int width = 0;
  int height = 0;
  int colors = 0;

  QString s;
  QString s2;

  Q3TextStream t(d);
  s = t.readLine();
  if(s.left(2) != "P7") return NULL;
  if(!s.contains("332")) return NULL;
  //find size
  while(!t.eof())
  {
    s = t.readLine();       // line of text excluding '\n'
    if(s.left(1) == "#") continue;
    s = s.stripWhiteSpace();
    s = s.simplifyWhiteSpace();
    pos = s.find(" ");
    if(pos > -1)
    {
      s2 = s.left(pos);
      width = s2.toInt();
      s = s.right(s.length() - pos -1);
    }
    pos = s.find(" ");
    if(pos > -1)
    {
      s2 = s.left(pos);
      height = s2.toInt();
      s = s.right(s.length() - pos -1);
    }
    colors = s.toInt();
    break;
  }
  //check, whether the values are valid
  if( width <= 0 || width > 80 || height <= 0 || height > 60 ||
      (colors != 255))
  	return NULL;
  if(!image.create(width,height,32))
  	return NULL;

	char *buf = new char[width];
  unsigned char red,green,blue;
  QDataStream ds(d);
  for(int cnt = 0;cnt < image.height();cnt++)
  {
    if(!ds.eof())
      ds.readRawBytes(buf,width);
    for(int cnt2=0;cnt2<width;cnt2++)
    {
      red = (((unsigned char)buf[cnt2] >>5) & 0x7);
      green = (((unsigned char)buf[cnt2]>>2) & 0x7);
      blue = (((unsigned char)buf[cnt2]) & 0x3);
      image.setPixel(cnt2,cnt,qRgb(255*red/7,
                                   255*green/7,
                                   255*blue/3));
    }
  }
  //check whether image must be scaled, i.e. whether
  //its size is greater than 80x60
  int w = image.width();
  int h = image.height();
  if(( w > 80) || (h  > 60))
  {
    double scale_h,scale_v;
    //one of the scalefactors becomes greater than 1.0
    scale_h = double(image.width())/80.0;
    scale_v = double(image.height())/60.0;
    if(scale_h > scale_v)
    {
      w = 80;
      h = int(double(image.height())/scale_h);
    }
    else
    {
      w = int(double(image.width())/scale_v);;
      h = 60;
    }
    image = image.smoothScale(w,h);
  }
  return new QImage (image);
}

bool qis_write_pnm7_image(QImageWriter *iio, QImage &image)
{
  QIODevice* out = iio->device();
  int w,h;
  bool alpha = false;

  if(image.hasAlphaBuffer())
    alpha = true;
  //check whether image must be scaled, i.e. whether
  //its size is greater than 80x60
  w = image.width();
  h = image.height();
  if(( w > 80) || (h  > 60))
  {
    double scale_h,scale_v;
    //one of the scalefactors becomes greater than 1.0
    scale_h = double(image.width())/80.0;
    scale_v = double(image.height())/60.0;
    if(scale_h > scale_v)
    {
      w = 80;
      h = int(double(image.height())/scale_h);
    }
    else
    {
      w = int(double(image.width())/scale_v);;
      h = 60;
    }
    image = image.smoothScale(w,h);
  }
  int i,j,x;
  int value;
//  short int
  Q_INT8 dither_red[2][16]=
  {
    {-16,  4, -1, 11,-14,  6, -3,  9,-15,  5, -2, 10,-13,  7, -4,  8},
    { 15, -5,  0,-12, 13, -7,  2,-10, 14, -6,  1,-11, 12, -8,  3, -9}
  };
  Q_INT8 dither_green[2][16]=
  {
    { 11,-15,  7, -3,  8,-14,  4, -2, 10,-16,  6, -4,  9,-13,  5, -1},
    {-12, 14, -8,  2, -9, 13, -5,  1,-11, 15, -7,  3,-10, 12, -6,  0}
  };
  Q_INT8 dither_blue[2][16]=
  {
    { -3,  9,-13,  7, -1, 11,-15,  5, -4,  8,-14,  6, -2, 10,-16,  4},
    {  2,-10, 12, -8,  0,-12, 14, -6,  3, -9, 13, -7,  1,-11, 15, -5}
  };
//  unsigned short
  Q_UINT8 blue_map[2][16][256];
  Q_UINT8 green_map[2][16][256];
  Q_UINT8 red_map[2][16][256];

  QString str;
  QString tmp_str;
  str = "P7 332\n";
  str += "#END_OF_COMMENTS\n";
  tmp_str.sprintf("%i %i %i\n",w,h,255);
  str += tmp_str;
  if ((uint)out->writeBlock(str, str.length()) != str.length())
	  return false;

  if(image.depth() < 8) image = image.convertDepth(8);
  if(image.isNull())
	  return false;

  // initialise dither maps
  for (i=0; i < 2; i++)
  {
    for (j=0; j < 16; j++)
    {
      for (x=0; x < 256; x++)
      {
        value=x-16;
        if (x < 48)
          value=x/2+8;
        value+=dither_red[i][j];
        red_map[i][j][x] = Q_UINT8
          ((value < 0) ? 0 : (value > 255) ? 255 : value);
        value=x-16;
        if (x < 48)
          value=x/2+8;
        value+=dither_green[i][j];
        green_map[i][j][x] = Q_UINT8
          ((value < 0) ? 0 : (value > 255) ? 255 : value);
        value=x-32;
        if (x < 112)
          value=x/2+24;
        value+=2*dither_blue[i][j];
        blue_map[i][j][x] = Q_UINT8
          ((value < 0) ? 0 : (value > 255) ? 255 : value);
      }
    }
  }
  i=0;
  j=0;

  QDataStream d(out);
  for(int h=0; h < image.height(); h++)
  {
    for(int w=0; w < image.width(); w++)
    {
      QRgb color = image.pixel(w,h);
      if(alpha && (qAlpha(color) < 255))
        color = qRgb(255,255,255);
      Q_UINT8 val;
      val=Q_UINT8(((red_map[i][j][qRed(color)] & 0xe0) |
                 ((green_map[i][j][qGreen(color)] & 0xe0) >> 3) |
                 ((blue_map[i][j][qBlue(color)] & 0xc0) >> 6)));
      d<<val;
      j++;
      if (j == 16) j=0;
    }
    i++;
    if (i == 2) i=0;
  }
  return true;
}

void initPnmIO()
{
#warning "port to qt4"
//s  QImageIO::defineIOHandler("PNM7","^P7 332","t",
//s	  		                	  qis_read_pnm7_image,qis_write_pnm7_image );
}
#endif
