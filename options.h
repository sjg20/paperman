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


#ifndef __options_h
#define __options_h

#include <QPrinter>


#include "ui_options.h"

class Mainwidget;
class Ui_printopt;

class Options : public QDialog, public Ui::Options
{
    Q_OBJECT

public:
    Options(QWidget* parent = 0, const char* name = 0, bool modal = false, Qt::WindowFlags fl = Qt::WindowFlags());
    ~Options();

    virtual void setMainwidget( Mainwidget * main );

public slots:
    virtual void ok_clicked();
    virtual void cancel_clicked();

protected:
    Mainwidget *_main;

protected slots:
    virtual void languageChange();

private:
    virtual void init();

private slots:
    virtual void updateThreshold( int );

};

#endif

