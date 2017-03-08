/***************************************************************************
                          ruler.cpp  -  description
                             -------------------
    begin                : Mon Jul 17 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

//class name changed from QRuler to Ruler on Mon Jan 28 2002

#include <math.h>

#include "ruler.h"
#include <qfont.h>
#include <qfontmetrics.h>
#include <qapplication.h>
#include <qpainter.h>
#include <qmatrix.h>

Ruler::Ruler( QWidget * parent, const char * name,Qt::Orientation o, Qt::WindowFlags f)
      :QFrame(parent, f)
{
    setObjectName(name);
	mOrientation = o;
}
Ruler::~Ruler()
{
}
/**  */
void Ruler::drawContents(QPainter* painter)
{
//The ruler looks like this:
//
// |....+....|....+....|
// or
// |....|....|....|....|
// or
// | + | + | + | + | + |
// or
// |  |  |  |  |  |  |  |
//
// where
// | == longmark
// + == mediummark
// . == shortmark
//
// The layout depends on the pixel distance between marks.
// We try to ensure, that the distance between marks is at least 5
// pixel. Otherwise the ruler might look bad due to rounding errors.

	int mark_number;//number of marks between longmarks
	double mark_dist;//distance between marks
	int mc;//mark counter
  int w = width();
  int h = height();
  bool draw_mm,draw_lm,draw_sm;
  bool draw_mm_text;
  draw_lm = false;
  draw_mm = false;
  draw_sm = false;

  //scalefactor
  double f;
	if(mOrientation == Qt::Horizontal)
    f = double(w)/(mMaxRange-mMinRange);
  else
    f = double(h)/(mMaxRange-mMinRange);
  //drawing offset
  double x_off;
  x_off = mMinRange*f;

  QString qs;
  int textheight;
  int textwidth;

  QFont qf(qApp->font());
  QFontMetrics qfm(qf );

  int dignum = 1;
  qs = "0";
  //find number of digits for max val
  while(int(mMaxRange)/dignum > 10)
  {
    qs += "0";
    dignum *=10;
  }
	textwidth = qfm.boundingRect(qs).width();
	textheight=qfm.boundingRect(qs).height();

  int lm_text_step;
  int lm_text_cnt;
  int draw_lm_text_cnt;
  int draw_lm_text = 0;

	if(mOrientation == Qt::Horizontal)
	{
    if((textheight + 12) > 30)
      setFixedHeight(textheight + 12);
    else
      setFixedHeight(30);
    int cnt = 1;
    for(cnt=1;cnt<int(mMaxRange);cnt *= 10)
    {
      if(double(cnt) * f > 20.0)
        break;
    }
    mark_dist = double(cnt) * f;
    lm_text_step = cnt;
    if(mMinRange > 0.0)
      lm_text_cnt = 1 + int(mMinRange)/lm_text_step;
    else
      lm_text_cnt = 0;
    //if cnt > 1, then it might be possible to draw text on the medium markers
    //find pixel distance between longmark values
    draw_lm_text_cnt = 0;
    while(draw_lm_text_cnt*int(mark_dist) < textwidth + 10)
      ++draw_lm_text_cnt;
    if((cnt > 1) && (int(mark_dist) > 2*(textwidth + 10)))
      draw_mm_text = true;
    if((cnt % 2) != 0)
      draw_mm_text = false;

    if(int(mark_dist) < 2*textwidth +10)
      draw_mm_text = false;
    if(mark_dist >= 50.0)
    {
      //draw 1 mediummark and 8 shortmarks between longmarks
      mark_dist = mark_dist/10.0;
      mark_number = 10;
    }
    else if(mark_dist >= 25.0)
    {
      //draw 5 shortmarks between longmarks
      mark_dist = mark_dist/5.0;
      mark_number = 5;
    }
    else if(mark_dist >= 10.0)
    {
      //draw 1 mediummark between longmarks
      mark_dist = mark_dist/2.0;
      mark_number = 2;
    }
    else
      mark_number = 0;

    double map_max;

    map_max = mMaxRange*f;
    mc = int(ceil(double(x_off)/mark_dist));

    double d;
		for(d=x_off;d<=map_max;d+=0.1)
		{
      switch(mark_number)
      {
        case 0:
          if(d >= mark_dist*double(mc))
            draw_lm = true;
          break;
        case 2:
          if((d >= mark_dist*double(mc)) && (mc % 2 == 0))
            draw_lm = true;
          else if(d >= (mark_dist*double(mc)))
            draw_mm = true;
          break;
        case 5:;
          if((d>= mark_dist*double(mc)) && (mc % 5 == 0))
            draw_lm = true;
          else if(d >= mark_dist*double(mc))
            draw_sm = true;
          break;
        case 10:;
          if((d >= (mark_dist*double(mc)) ) && (mc % 10 == 0))
            draw_lm = true;
          else if((d >= (mark_dist*double(mc)) ) && (mc % 5 == 0))
            draw_mm = true;
          else if(d >= (mark_dist*double(mc)) )
            draw_sm = true;
          break;
        default:;
      }
      if(draw_lm)
      {
				painter->drawLine((int)(d-x_off),height(),(int)(d-x_off),height()-12);//4*height()/7);					
        draw_lm = false;
				if(draw_lm_text == 0)
				{
					 qs.sprintf("%i",int(lm_text_cnt * lm_text_step));
//p         	 drawText ((int)(d-x_off+2),height()-12,qs);//height()/2, qs );
				}
        ++draw_lm_text;
        if(draw_lm_text >= draw_lm_text_cnt)
          draw_lm_text = 0;
        lm_text_cnt += 1;
        ++mc;
      }
      else if(draw_mm)
      {
				painter->drawLine((int)(d-x_off),height(),(int)(d-x_off),height()-6);//2*height()/3);					
        draw_mm = false;
				if(draw_mm_text)
				{
					 qs.sprintf("%i",int(lm_text_cnt * lm_text_step -
                               lm_text_step/2));
//p         	 drawText ((int)(d-x_off+2),height()-12,qs);//height()/2, qs );
				}
        ++mc;
      }
      else if(draw_sm)
      {
				painter->drawLine((int)(d-x_off),height(),(int)(d-x_off),height()-3);//4*height()/5);					
        draw_sm = false;
        ++mc;
      }
    }
  }
	else
	{
    if((textwidth + 12) > 30)
      setFixedWidth(textwidth + 12);
    else
      setFixedWidth(30);
    int cnt = 1;
    for(cnt=1;cnt<int(mMaxRange);cnt *= 10)
    {
      if(double(cnt) * f > 20.0)
        break;
    }
    mark_dist = double(cnt) * f;
    lm_text_step = cnt;
    if(mMinRange > 0.0)
      lm_text_cnt = 1 + int(mMinRange)/lm_text_step;
    else
      lm_text_cnt = 0;
    //if cnt > 1, then itmight be possible to draw text on the medium markers
    //find pixel distance between longmark values
    draw_lm_text_cnt = 0;
    while(draw_lm_text_cnt*int(mark_dist) < textwidth + 10)
      ++draw_lm_text_cnt;
    if((cnt > 1) && (int(mark_dist) > 2*(textwidth + 10)))
      draw_mm_text = true;
    if((cnt % 2) != 0)
      draw_mm_text = false;


    if(int(mark_dist) < 2*textwidth +10)
      draw_mm_text = false;
    if(mark_dist >= 50.0)
    {
      //draw 1 mediummark and 8 shortmarks between longmarks
      mark_dist = mark_dist/10.0;
      mark_number = 10;
    }
    else if(mark_dist >= 25.0)
    {
      //draw 5 shortmarks between longmarks
      mark_dist = mark_dist/5.0;
      mark_number = 5;
    }
    else if(mark_dist >= 10.0)
    {
      //draw 1 mediummark between longmarks
      mark_dist = mark_dist/2.0;
      mark_number = 2;
    }
    else
      mark_number = 0;

    double map_max;

    map_max = mMaxRange*f;
    mc = int(ceil(double(x_off)/mark_dist));

    double d;
		for(d=x_off;d<=map_max;d+=0.1)
		{
      switch(mark_number)
      {
        case 0:
          if(d >= mark_dist*double(mc))
            draw_lm = true;
          break;
        case 2:
          if((d >= mark_dist*double(mc)) && (mc % 2 == 0))
            draw_lm = true;
          else if(d >= (mark_dist*double(mc)))
            draw_mm = true;
          break;
        case 5:;
          if((d>= mark_dist*double(mc)) && (mc % 5 == 0))
            draw_lm = true;
          else if(d >= mark_dist*double(mc))
            draw_sm = true;
          break;
        case 10:;
          if((d >= (mark_dist*double(mc)) ) && (mc % 10 == 0))
            draw_lm = true;
          else if((d >= (mark_dist*double(mc)) ) && (mc % 5 == 0))
            draw_mm = true;
          else if(d >= (mark_dist*double(mc)) )
            draw_sm = true;
          break;
        default:;
      }
      if(draw_lm)
      {
				painter->drawLine(width(),(int)(d-x_off),width()-12,(int)(d-x_off));					
        draw_lm = false;
				if(draw_lm_text == 0)
				{
					 qs.sprintf("%i",int(lm_text_cnt * lm_text_step));
//p         	 drawText(1,(int)(d-x_off + textheight + 2), qs );
				}
        ++draw_lm_text;
        if(draw_lm_text >= draw_lm_text_cnt)
          draw_lm_text = 0;
        lm_text_cnt += 1;
        ++mc;
      }
      else if(draw_mm)
      {
				painter->drawLine(width(),(int)(d-x_off),width()-6,(int)(d-x_off));					
        draw_mm = false;
				if(draw_mm_text)
				{
					 qs.sprintf("%i",lm_text_cnt * lm_text_step -
                               lm_text_step/2);
//p         	 drawText(1,(int)(d-x_off + textheight + 2), qs );
				}
        ++mc;
      }
      else if(draw_sm)
      {
				painter->drawLine(width(),(int)(d-x_off),width()-3,(int)(d-x_off));					
        draw_sm = false;
        ++mc;
      }
    }
  }
}
/** No descriptions */
void Ruler::setRange(double min,double max)
{
  mMinRange = min;
  mMaxRange = max;
  update();
}
/** No descriptions */
void Ruler::setMinRange(double min)
{
  mMinRange = min;
  update();
}
/** No descriptions */
void Ruler::setMaxRange(double max)
{
  mMaxRange = max;
  update();
}
