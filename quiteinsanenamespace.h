/***************************************************************************
                          quiteinsanenamespace.h  -  description
                             -------------------
    begin                : Fri Sep 8 2000
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

#ifndef QUITEINSANENAMESPACE_H
#define QUITEINSANENAMESPACE_H


/**
  *@author M. Herder
  */

class QIN
{
public: 
  enum ScanMode
	{
    Temporary      = 0,
    SingleFile     = 1,
    OCR            = 2,
    CopyPrint      = 3,
    MultiScan      = 4,
    Direct         = 5
  };
  enum MetricSystem
	{
    Millimetre     = 0,
    Centimetre     = 1,
    Inch           = 2,
    NoMetricSystem = 3
  };
  enum Orientation
	{
    Vertical   = 0,
    Horizontal = 1
  };
  enum Layout
	{
    ScrollLayout      = 0,
    TabLayout         = 1,
    MultiWindowLayout = 2,
    ListLayout        = 3
  };
  enum Status
	{
    OpenFailed  = 0,
    ReadyToShow = 1,
    NoScanner   = 2,
    UserCancel  = 3,
    InitFailed  = 4
  };
};

#endif
