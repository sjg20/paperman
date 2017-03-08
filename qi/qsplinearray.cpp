/***************************************************************************
                          qsplinearray.cpp  -  description
                             -------------------
    begin                : Tue Oct 31 2000
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

#include "qsplinearray.h"
#include <QPolygon>
#include <math.h>
#include <qrect.h>

QSplineArray::QSplineArray()
{
  mSorted = false;
}
QSplineArray::~QSplineArray()
{
}
/**  */
QPolygon QSplineArray::spline()
{
  sortCoords();
  int a;
  int e;
  int index;
  a = boundingRect().left();
  e = boundingRect().right();
  //if there are less then 3 control points, draw a line
  if(size()<3) return(line());

  QPolygon parray;
  parray.resize(0);
  index=0;
  int i,j;
  double x,y,x0,x1,x2,x3;
  double y0,y1,y2,y3;
  //Catmull-Rom matrix
  double CSpline[4][4] =
  {
    {-0.5,    1.0,   -0.5,   0.0},
    { 1.5,   -2.5,    0.0,   1.0},
    {-1.5,    2.0,    0.5,   0.0},
    { 0.5,   -0.5,    0.0,   0.0},
  };

  int num_points = (e-a)*2;
  double curve[num_points][4];
  double u,u2,u3;

  parray.resize(e-a);

  for( i = 0; i < num_points; i++ )
  {
    u = double(i)/double(num_points);
    u2 = u*u;                    // u^2
    u3 = u2*u;                   // u^3
    for( j = 0; j < 4; j++ )
    {
      curve[i][j] = (u3 * CSpline[j][0]) + (u2 * CSpline[j][1]) +
                    (u * CSpline[j][2]) + CSpline[j][3];
    }
  }

  int m;
  for (m = 0; m < size() - 1; m++)
  {
    //control points
    x0 = (m == 0) ? point(m).x() : point((m - 1)).x();
    x1 = point(m).x();
    x2 = point((m + 1)).x();
    x3 = (m == (size() - 2)) ? point((size() - 1)).x() : point((m + 2)).x();
    y0 = (m == 0) ? point(m).y() : point((m - 1)).y();
    y1 = point(m).y();
    y2 = point((m + 1)).y();
    y3 = (m == (size() - 2)) ? point((size() - 1)).y() : point((m + 2)).y();
    for( i = 0; i < num_points; i++ )
    {
      for( j = 0; j < 4; j++ )
      {
        x = (x0 * curve[i][0]) + (x1 * curve[i][1]) +
            (x2 * curve[i][2]) + (x3 * curve[i][3]);
        y = (y0 * curve[i][0]) + (y1 * curve[i][1]) +
            (y2 * curve[i][2]) + (y3 * curve[i][3]);
        if(int(x) >= index + a)
        {
          if(y < 0.0) y = 0.0;
          if(y > 255.0) y = 255.0;
          if(index < int(parray.size()))
          {
            parray.setPoint(index,int(x),int(y));
            ++index;
          }
        }
      }
    }
  }
  return parray;
}

void QSplineArray::sortCoords()
{
//my crappy sort algorithm
  int z;
	int flag=0;
	int flag2=0;
//	double d;
  QPoint qp;
	while(flag==0)
	{
		for (z=0;z<(int)size()-1;z++)
		{
			if(point(z).x()>point(z+1).x())
			{
				qp = point(z);
        setPoint(z,point(z+1));
        setPoint(z+1,qp);
				flag2=1;
			}
		}
		if(flag2==1)
		{
			flag2=0;
		}
		else
		{
			flag=1;
		}
	}
  mSorted=true;
}

QPolygon QSplineArray::line()
{
  sortCoords();
  int a;
  int y;
  int z;
  QPolygon qpa;
  qpa.resize(0);
  for(z=0;z<int(size())-1;z++)
  {
    for(a=point(z).x();a<point(z+1).x();a++)
    {
      qpa.resize(qpa.size()+1);
      y=(int)( double(a-point(z).x()) *
               double(point(z+1).y()-point(z).y())/
               double(point(z+1).x()-point(z).x()));
      y+=point(z).y();
      qpa.setPoint(qpa.size()-1,a,y);
    }
  }
  return qpa;
}

