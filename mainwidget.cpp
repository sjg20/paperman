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


#include <stdarg.h>
#include <stdlib.h>

#include "config.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QtGlobal>
#include <QShortcut>
#if QT_VERSION >= 0x050000
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#else
#include <QPrintDialog>
#include <QPrinter>
#endif

#include "qapplication.h"
#include "qdatetime.h"
#include "qdevicesettings.h"
#include "qdir.h"
#include "qmessagebox.h"
#include "qpainter.h"
#include "qsanestatusmessage.h"
#include "qscandialog.h"
#include "qscanner.h"
#include "qscannersetupdlg.h"
#include "qtimer.h"
#include "qxmlconfig.h"
#include "previewwidget.h"

#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopwidget.h"

#include "err.h"
#include "mainwidget.h"
#include "ui_mainwindow.h"
#include "desk.h"
#include "maxview.h"
#include "op.h"
#include "options.h"
#include "paperstack.h"
#include "pagewidget.h"
#include "printopt.h"
#include "pscan.h"
#include "resource.h"
#include "ui_printopt.h"

Mainwidget::Mainwidget (QWidget *parent, const char *name)
      : QStackedWidget (parent)
   {
   setObjectName(name);
   if (!xmlConfig)
      new QXmlConfig();
   xmlConfig->setVersion(VERSION);
   xmlConfig->setFilePath(QDir::homePath()+
                      "/.maxview/maxview_config.xml");

   QDir dir;

   qRegisterMetaType<SANE_Status>("SANE_Status");

   dir.mkdir (QDir::homePath()+ "/.maxview");

   xmlConfig->setCreator("MaxView");

   QScannerSetupDlg::initConfig ();

   xmlConfig->readConfigFile();
   _smooth = xmlConfig->boolValue ("DISPLAY_SMOOTH");

   setMinimumSize (200, 200);
   _desktop = new Desktopwidget (this);
   _page = new Pagewidget (_desktop->getModelconv (), "mainwidget/", this);
   _page->init ();
//    _page->setSmoothing (_smooth);
   _page->setSmoothing (false);
   _scanner = 0;
   _scanDialog = 0;
   _pscan = 0;
   _options = 0;
   _scanning = false;
//    _stack = 0;

   addWidget (_desktop);
   addWidget (_page);
   _contents = _desktop->getModel ();
   _view = _desktop->getView ();

   connect (_desktop, SIGNAL (showPage (const QModelIndex &)),
      this, SLOT (showPage (const QModelIndex &)));

   connect (_page->_returnToDesktop, SIGNAL (triggered()), this, SLOT (showDesktop ()));

   _watchButtons = false;
   _buttonTimer = new QTimer (this);
   connect (_buttonTimer, SIGNAL(timeout()), this, SLOT (checkButtons()));
   _buttonTimer->start (500); // check buttons twice each second
   }


Mainwidget::~Mainwidget ()
   {
   delete _desktop;
   if (_scanner)
      delete _scanner;
   if (_scanDialog)
      delete _scanDialog;
   }


void Mainwidget::closing (void)
   {
   _desktop->closing ();
   _page->closing ();
   if (_pscan)
      _pscan->closing ();
   }


void Mainwidget::checkButtons (void)
   {
   int buttons;

   if (!_watchButtons || !_scanner)
      return;
   buttons = _scanner->checkButtons ();
   if (buttons > 0 && (buttons & 1)) // scan
      scan ();
   }


void Mainwidget::saveSettings (void)
   {
   if (_scanner)
      {
      QDeviceSettings ds(_scanner);

      ds.saveDeviceSettings("Last settings");
      }
   }


void Mainwidget::showPage (const QModelIndex &index, bool delay_smoothing)
   {
   // TODO: Work out whether delay_smoothing is needed
   //delay_smoothing = delay_smoothing;
   UNUSED(delay_smoothing);
#if 0
   if (!f->max)
      {
      err_info *err;

      err = maxdesk->ensureMax (f);
      if (err)
         QMessageBox::warning (0, "File Unreadable",
               err->errstr);
      }
   else
   switch (max_get_type (f->max))
      {
      case FILET_max :
         raiseWidget (1);
         _page->showPage (maxdesk, f, delay_smoothing);
         break;

      case FILET_pdf :
      default :
         maxdesk->viewFile (f);
         break;
      }
#endif
   setCurrentIndex(1);
//    _page->showPage (index.model (), index, delay_smoothing);
   _page->showPages (index.model (), index, 0, -1, 2);
   }


void Mainwidget::showDesktop ()
   {
   setCurrentIndex(0);
   }


void Mainwidget::pageLeft (void)
   {
   if (currentWidget() == _desktop)
      _desktop->pageLeft ();
   else
      _page->pageLeft ();
   }


void Mainwidget::pageRight (void)
   {
   if (currentWidget () == _desktop)
      _desktop->pageRight ();
   else
      _page->pageRight ();
   }


void Mainwidget::stackLeft (void)
   {
   QModelIndex index;

   _desktop->stackLeft ();
   if (currentWidget () == _page && _desktop->getCurrentFile (index))
      showPage (index, true);
   }


void Mainwidget::stackRight (void)
   {
   QModelIndex index;

   _desktop->stackRight ();
   if (currentWidget () == _page && _desktop->getCurrentFile (index))
      showPage (index, true);
   }


bool Mainwidget::ensureScanner (void)
   {
   bool ok;

   if (!_scanner)
      {
      QScannerSetupDlg ssd(_scanner, 0);
      ok = ssd.setupLast ();
      if (ok)
         _scanner = ssd.scanner ();
      else
         {
         ssd.show ();
         ssd.exec ();
         _scanner = ssd.scanner ();
         if (!_scanner->openDevice ())
            {
            delete _scanner;
            _scanner = 0;
            }
         }
      if (_pscan)
         _pscan->scannerChanged (_scanner);
      if (_scanner)
         {
         connectScanner ();

         // not supported by Fujitsu backend? Our code doesn't handle it either
         //_scanner->setIOMode (SANE_TRUE);
         }
      else
         {
         QMessageBox::warning (0, "Scanner Problem", "Scanner not selected");
         return false;
         }
      }
   _watchButtons = true;
   return true;
   }


static QString getDefaultStackName (void)
   {
   return QString ("");
   }


static QString getDefaultPageName (void)
   {
   return QDate::currentDate ().toString ("d_MMMM_yyyy_");
   }


void Mainwidget::updatePscan ()
   {
   if (_pscan)
      _pscan->checkEnabled (_scanning);
   }


void Mainwidget::scan (void)
   {
   SANE_Status status;
   SANE_Parameters parameters;
   QString stack_name = getDefaultStackName ();
   QString page_name = getDefaultPageName ();

   // cannot support more than one scan job at a time (yet)
   if (_scanning)
      return;

   if (!ensureScanner ())
      return;

   // check resolution, etc.
   status = _scanner->getParameters(&parameters);
   if (status != SANE_STATUS_GOOD)
      {
      QMessageBox::warning (0, "Scanner Problem", "Could not get parameters");
      return;
      }
   _watchButtons = false;

//    progress ("Starting...");
//    progressSize (0);
//    info ("");

   // get stack/page names from _pscan if available
   if (_pscan)
      {
      QString str;

      str = _pscan->getStackName ();
      if (!str.isEmpty ())
         stack_name = str;
      str = _pscan->getPageName ();
      if (!str.isEmpty ())
         page_name = str;
      }

   Paperscan scan;

   connect (&scan, SIGNAL (stackNew (const QString &)), this, SLOT (slotStackNew (const QString &)));
   connect (&scan, SIGNAL (stackNewPage (const Filepage *, const QString &, const QString &)),
         this, SLOT (slotStackNewPage (const Filepage *, const QString &, const QString &)));
   connect (&scan, SIGNAL (stackConfirm (void)), this, SLOT (slotStackConfirm (void)));
   connect (&scan, SIGNAL (stackCancel (void)), this, SLOT (slotStackCancel (void)));
   connect (&scan, SIGNAL (scanComplete (SANE_Status, const QString &, const err_info *)),
         this, SLOT (slotScanComplete (SANE_Status, const QString &, const err_info *)));
   connect (&scan, SIGNAL (progress (const QString &)), this, SLOT (progress (const QString &)));
   connect (&scan, SIGNAL (stackPageStarting (int, const PPage *)),
      this, SLOT (slotStackPageStarting (int, const PPage *)));
   connect (&scan, SIGNAL (stackPageProgress (const PPage *)),
      this, SLOT (slotStackPageProgress (const PPage *)));

   scan.setup (_scanner, stack_name, page_name);
   scan.start ();
   _scan = &scan;
   _scanning = true;
   _scan_cancelling = false;
   updatePscan ();

   while (_scanning)
      /* note this will not allow the scanning thread to get events, but
         at the moment it doesn't have an event loop anyway */
      qApp->processEvents ();
   _scan = 0;
//    qDebug () << "scan complete";
   _watchButtons = true;
   updatePscan ();
   }


void Mainwidget::slotStackNew (const QString &stack_name)
   {
   err_info *err;

//    qDebug () << "slotStackNew" << stack_name;
   if (!_scan_cancelling)
      {
      err = _contents->beginScan (_view->rootIndexSource (), stack_name);
      if (err)
         _scan->cancelScan (err);
      }
   }


void Mainwidget::stopScan (bool cancel)
   {
   if (_scanning)
      {
      if (cancel)
         {
         _scan_cancelling = true;
         _scan->cancelScan (err_make (ERRFN, ERR_scan_cancelled));
         }
      else
         _scan->endScan ();
      }
   }


void Mainwidget::slotScanComplete (SANE_Status status, const QString &msg, const err_info *err)
   {
   _desktop->scanComplete ();

//    qDebug () << "slotScanComplete" << status << msg << err;

   // don't report end of documents if we have managed to scan some
   if (status)
      {
      QSaneStatusMessage fred (status, this);

      printf ("status = %d\n", status);
      fred.exec ();
      }

   progress (msg);

   if (err && err->errnum != ERR_scan_cancelled)
      QMessageBox::warning (0, "Error", err->errstr);
   _scanning = false;
   }


void Mainwidget::slotStackNewPage (const Filepage *mp, const QString &coverageStr,
      const QString &infostr)
   {
   err_info *err;

   if (!_scan_cancelling)
      {
   //    qDebug () << "slotStackNewPage";
      if (!infostr.isEmpty ())
         info (infostr);
      err = _contents->addPageToScan (mp, coverageStr);
      _scan->pageAdded (mp);
      progressSize (_contents->getScanSize ());
      if (err)
         _scan->cancelScan (err);
      }
   }


void Mainwidget::slotStackConfirm (void)
   {
//    qDebug () << "slotStackConfirm";
   if (!_scan_cancelling)
      {
      err_info *err = _contents->confirmScan ();

      //FIXME: should we cancel in this case?
      if (err)
         _scan->cancelScan (err);
      }
   }


void Mainwidget::slotStackCancel (void)
   {
   /* the scan system has requested that we cancel the scan - this might be
      in response to our request */
//    qDebug () << "slotStackCancel";
   err_info *err = _contents->cancelScan ();
   if (err)
      _scan->cancelScan (err);
   }



void Mainwidget::progress (const QString &str)
   {
//    qDebug () << "progress" << str;
   if (_pscan)
      _pscan->progress (str.toLatin1 ().constData());
   else
      emit newContents (str);
   }


void Mainwidget::slotStackPageStarting (int total, const PPage *page)
   {
   if (!_scan_cancelling)
      {
      _contents->pageStarting (*_scan, page);
      _progressTotal = total;
      }
   }


void Mainwidget::info (const QString &str)
   {
   if (_pscan)
      _pscan->info (str.toLatin1 ().constData());
   }


void Mainwidget::slotStackPageProgress (const PPage *page)
   {
   if (!_scan_cancelling)
      {
      const char *data;
      int size;

      _contents->pageProgress (*_scan, page);
      if (_pscan && _scan->getData (page, data, size))
         _pscan->progress (size * 100 / _progressTotal);
      }
   }


void Mainwidget::progressSize (int size)
   {
   if (_pscan)
      {
      char str [30];

      util_bytes_to_user (str, size);
      _pscan->progressSize (str);
      }
   }


void Mainwidget::updateScanDialog (void)
   {
   QScanner::format_t format;

   // set up JPEG parameter
   format = _scanDialog->getFormat ();
   _scanDialog->setFormat (format, xmlConfig->boolValue ("SCAN_USE_JPEG"));
   }


void Mainwidget::pscan (void)
   {
   if (!_pscan)
      {
      _pscan = new Pscan (this, "pscan", false, Qt::WindowStaysOnTopHint);
      _pscan->setMainwidget (this);
      updatePscan ();
      }
   if (!ensureScanner ())
      return;
   _pscan->scannerChanged (_scanner);
   setupScanDialog ();
   _pscan->show ();
   }


void Mainwidget::options (void)
   {
   if (!_options)
      {
      _options = new Options (0, 0);
      _options->setMainwidget (this);
      }
   _options->exec ();

   // tell the scan dialog about it, if open
   if (_scanDialog)
      updateScanDialog ();
   }


void Mainwidget::setupScanDialog (void)
   {
   _scanDialog = new QScanDialog (_scanner, 0);
   connect (_scanDialog, SIGNAL (closed ()), this, SLOT (scanDialogClosed ()));
   connect (_scanDialog, SIGNAL (signalPendingDone ()), this, SLOT (slotPendingDone ()));
   connect (_scanner, SIGNAL (signalScanDone()),
                  _scanDialog, SLOT (slotDoPendingChanges()));
   connect (_scanDialog, SIGNAL (warning (QString&)), this, SLOT (slotWarning (QString &)));
   _preview = _scanDialog->getPreview ();
   _pscan->setPreviewWidget (_preview);
   _pscan->setScanDialog (_scanDialog);
   updateScanDialog ();
   }

   
void Mainwidget::slotWarning (QString &str)
   {
   if (_pscan)
       _pscan->warning (str);
   }


void Mainwidget::slotPendingDone(void)
   {
//   printf ("reload\n");
   if (_pscan)
      _pscan->pendingDone ();
   }


void Mainwidget::slotReloadOptions(void)
   {
//   printf ("reload\n");
   if (_pscan)
      _pscan->scannerChanged (_scanner);
   }


void Mainwidget::slotSetOption (int)
   {
//   printf ("change %d\n", num);
   if (_pscan)
      _pscan->scannerChanged (_scanner);
   }


void Mainwidget::connectScanner (void)
   {
   connect (_scanner, SIGNAL (signalReloadOptions()),
            this, SLOT (slotReloadOptions()));
   connect (_scanner, SIGNAL (signalSetOption(int)),
            this, SLOT (slotSetOption(int)));
   }


void Mainwidget::selectScanner (void)
   {
   bool ok;

   _watchButtons = false;
   if (_scanDialog)
      {
      delete _scanDialog;
      _scanDialog = 0;
      _pscan->setScanDialog (_scanDialog);
      }
/*
   if (_pscan)
      {
      delete _pscan;
      _pscan = 0;
      }
*/
   QScannerSetupDlg ssd(_scanner, 0);

   /* TODO: tidy up this wierdness - it should be possible to cancel but
      at present this casues a crash */
   _scanner = 0;
   ssd.show ();
   ok = ssd.exec () == QDialog::Accepted;

   if (ok)
      _scanner = ssd.scanner ();
   if (_scanner && _scanner->isOpen ())
      {
      connectScanner ();
      if (_pscan)
         _pscan->scannerChanged (_scanner);
      setupScanDialog ();
      _watchButtons = true;
      }
   else
      {
      if (_pscan)
         delete _pscan;
      _pscan = 0;
      ssd.uninitScanner();
      _scanner = 0;
      }
   }


void Mainwidget::scannerSettings (void)
   {
   if (!ensureScanner ())
      return;

   if (_scanDialog)
      _scanDialog->show ();
//   sd.exec ();
   }


void Mainwidget::scanDialogClosed (void)
   {
     //   delete _scanDialog;
     //   _scanDialog = 0;
   _scanDialog->hide ();
//   printf ("closed\n");
   }


#if 0
bool Mainwidget::selectedFile (Desk *&maxdesk, file_info *&file)
   {
   return visibleWidget () == _desktop
      ? _desktop->getCurrentFile (maxdesk, file)
      : _page->getCurrentFile (maxdesk, file);
   }
#endif



void Mainwidget::drawTextOnWhite (int flags, QString text, int linenum, bool do_bold)
   {
   QString part1, part2;

   QFont font = _painter->font ();
   QFont bfont = font;
   bfont.setBold (!font.bold ());
   QFontMetrics fm (font);
   QFontMetrics fmb (bfont);

   part2 = text;
   if (do_bold)
      {
      QStringList list = text.split ('\n');
      if (list.size () > 1)
         {
         part1 = list [0] + ": ";
         part2 = list [1];
         }
      }

//    QFontMetrics fmb = _painter->fontMetrics ();
   int ypos = _printable.bottom() - (linenum + 1) * fm.height(); //_painter->fontMetrics().descent() - 1;

   QRect rect = QRect (_printable.left (), ypos,
         _printable.width (), fm.height());

   int width1 = fmb.horizontalAdvance (part1);
   int width = width1 + fm.horizontalAdvance (part2);
   int xpos = rect.left ();
   if (flags == Qt::AlignHCenter)
      xpos = (rect.right () - width) / 2;
   else if (flags == Qt::AlignRight)
      xpos = rect.right () - width;
   QRect bound = QRect (xpos, rect.top (), width, rect.height ());
   bound.adjust (-fm.averageCharWidth (), 0, fm.averageCharWidth (), 0);
   _painter->fillRect (bound, _opt->_fillColour);
   _painter->setPen (QColor (_opt->_textColour));
   _painter->setFont (font);
   if (!part1.isEmpty ())
      {
      _painter->setFont (bfont);
      _painter->drawText (xpos, _printable.bottom() - fm.descent() - 1 - linenum * fmb.height (), part1);
      _painter->setFont (font);
      xpos += width1;
      }
   _painter->drawText (xpos, _printable.bottom() - fm.descent() - 1 - linenum * fm.height (), part2);
   }


err_info *Mainwidget::printPage (int seq, QModelIndex &ind,
         int pnum, int numpages)
   {
   err_info *e = 0;
   QImage image;
   bool print;
   QSize size, trueSize;
   int bpp;
   bool is_blank = pnum == numpages;

   print = seq >= _from_page;
   if (_to_page != -1 && seq > _to_page)
      print = false;

   // check printing of odd/even pages
   if (!_opt->_printOdd && (seq & 1))
      print = false;
   if (!_opt->_printEven && !(seq & 1))
      print = false;

   if (!print)
      return NULL;   // skip this page
//            sprintf (msg, "Printing page %d of %d", page, file->pagecount);
//            emit newContents (msg);

   if (_new_page)
      {
      _printer->newPage();
      _new_page = false;
      }
   if (!is_blank)
      e = _contents->getImage (ind, pnum, false,
                        image, size, trueSize, bpp);
//    _painter->fillRect (_printable, QBrush (Qt::blue)); for debug
   if (!e)
      {
      _painter->save ();
      double xscale = (double)_body.width () / size.width ();
      double yscale = (double)_body.height () / size.height ();
      double scale = qMin (xscale, yscale);

      scale = qMin (scale, _opt->_expandFit ? 8 : 1.0);

      _painter->scale (scale, scale);
      //               printf ("scale %d x %d: %1.2lf,%1.2lf: %1.2lf\n", size.x (), size.y (),
      //                    xscale, yscale, scale);
      if (!image.isNull ())
         _painter->drawImage (0, 0, image);
      _painter->restore ();
      }


   // print page numbers
//    ypos -= bottom;

   // draw a white background for the text
//   QRect textrect = QRect (_printable.left(),
//         _printable.bottom() - _painter->fontMetrics().height() - 1,
//         _printable.width (), _painter->fontMetrics().height() + 1);
//    QRect newrect = textrect;
//    newrect.translate (0, -100);

// we could fill the whole line, but now we just fill behind each item of text
//    if (!is_blank
//       && (_opt->printNumbers || _opt->printNames || _opt->printSeq || e))
//       _painter->fillRect (textrect, QBrush (QColor (240, 240, 240)));
//    painter->fillRect (newrect, QBrush (Qt::green));

   QString info = _contents->imageInfo (ind, pnum, false);
   QString timestamp = _contents->imageTimestamp (ind, pnum);

   QString left, mid, right;

   if (!is_blank && _opt->_printNumbers)
      right = QString ("Page %1 of %2").arg (pnum + 1).arg (numpages);
   if (!is_blank && _opt->_printNames)
      {
      QString title = ind.model ()->data (ind, Qt::DisplayRole).toString ();
      QString page = ind.model ()->data (ind, Desktopmodel::Role_pagename).toString ();

      mid = QString ("%1\n%2").arg (title).arg (page);
      }

   if (is_blank || _opt->_printSeq || e)
      {
      if (_opt->_printSeq)
         left = QString ("Seq %1 of %2").arg (seq + 1).arg (_pagecount);
      if (is_blank)
         left = " (this page intentionally blank)";
      if (e)
         left += QString (", ERROR: %1").arg (e->errstr);
      e = NULL;
      }

   // if we have info, try to fit it somewhere
   if (!is_blank && _opt->_printImageInfo)
      {
      bool clear = true;

      if (left.isEmpty ())
         left = info;
      else if (mid.isEmpty ())
         mid = info;
      else if (right.isEmpty ())
         right = info;

      // no luck, will have to place it elsewhere
      else
         clear = false;
      if (clear)
         info = "";
      }

   // same with timestamp
   if (!is_blank && _opt->_printTimestamp)
      {
      bool clear = true;

      if (left.isEmpty ())
         left = timestamp;
      else if (mid.isEmpty ())
         mid = timestamp;
      else if (right.isEmpty ())
         right = timestamp;

      // no luck, will have to place it elsewhere
      else
         clear = false;
      if (clear)
         timestamp = "";
      }

   if (!right.isEmpty ())
      drawTextOnWhite (Qt::AlignRight, right);
   if (!mid.isEmpty ())
      drawTextOnWhite (Qt::AlignHCenter, mid, 0, true);
   if (!left.isEmpty ())
      drawTextOnWhite (Qt::AlignLeft, left);

   if (!is_blank && !info.isEmpty ())
      drawTextOnWhite (Qt::AlignLeft, info, 1);
   if (!is_blank && !timestamp.isEmpty ())
      drawTextOnWhite (Qt::AlignHCenter, timestamp, 1);

   _new_page = true;
   return NULL;
   }


void Mainwidget::savePrintSettings (void)
   {
   _saved = true;
   _dialog->accept ();
   }


void Mainwidget::openPrintopt (void)
   {
   QDialog diag;
      
//    _opt->reparent (&diag);
   QVBoxLayout *vbox = new QVBoxLayout (&diag);
   QDialogButtonBox *buttons = new QDialogButtonBox (QDialogButtonBox::Ok | 
      QDialogButtonBox::Cancel);

   connect(buttons, SIGNAL(accepted()), &diag, SLOT(accept()));
   connect(buttons, SIGNAL(rejected()), &diag, SLOT(reject()));
   vbox->addWidget (_opt);
   vbox->addWidget (buttons);
   diag.setLayout (vbox);
   diag.resize (vbox->minimumSize ());
   if (!diag.exec ())
      _opt->reset ();  // load options again if user cancels

   // set the parent back, otherwise diag will destroy _opt when it is deleted
   _opt->setParent (this);
   }


void Mainwidget::print (void)
   {
   Desktopview *view = _desktop->getView ();

   // get source model indexes, which is what we need
   QModelIndexList list = view->getSelectedList (true, true);
//    Desk *maxdesk;
//    file_info *file = 0;
   int pagenum = -1;
   QModelIndex ind;

   // if we're on page view, just print the current page of the current stack
   if (currentWidget () != _desktop)
      {
      if (_page->getCurrentIndex (ind, true))
         {
         pagenum = _page->getCurrentPage ();
         list.clear ();

         list << ind;
         }
      else
         {
         emit newContents ("Please select a page to print");
         return;
         }
      }

   // otherwise print all the selected stacks

   // work out what to print - if nothing just quit
   if (!list.size ())
      return;

   // create the QPrinter object
   _printer = new QPrinter ();

   // set up the print options
   _opt = new Printopt (_printer, list);
   _pagecount = _opt->countPages ();

   _printer->setResolution (300);
   _printer->setFullPage (_opt->_shrinkFit);
   if (pagenum == -1)
      {
      _printer->setPrintRange (QPrinter::AllPages);
      _printer->setFromTo (1, _pagecount);
      }
   else
      {
      _printer->setPrintRange (QPrinter::PageRange);
      _printer->setFromTo (pagenum + 1, pagenum + 1);
      }
// void QAbstractPrintDialog::setOptionTabs ( const QList<QWidget *> & tabs )
   QPrintDialog dialog (_printer, this);

   _list = &list;
   _dialog = &dialog;

   QPushButton *save = new QPushButton ("Save");
   dialog.layout()->addWidget(save);
   connect (save, SIGNAL (clicked ()), this, SLOT (savePrintSettings ()));

   QList<QWidget*> tabs;

   tabs << _opt;
   dialog.setOptionTabs (tabs);
   _saved = false;

   int result = dialog.exec ();
   if (result)
      {
      _opt->save ();
      _opt->putxml ();
      }
   if (result && !_saved)
      {
      _pagecount = _opt->countPages ();
      _new_page = false;
      Operation op ("Printing", _pagecount, this);
      QPainter painter;
      if (!painter.begin (_printer))                // paint on printer
         {
         emit newContents ("Printing aborted");
         return;
         }

      _watchButtons = false;
      qApp->processEvents ();

      QPaintDevice *device = painter.device();
//       int dpiy = metrics.logicalDpiY();
      int margin = 0;  //(int) ( (0/2.54)*dpiy ); // 2 cm margins
//       QRect view( body );
      char msg [300];
      err_info *e = NULL;

/*   bool sepSheet;
   bool shrinkFit;*/

      _from_page = _printer->fromPage () - 1;
      _to_page = _printer->toPage () - 1;
      _body = QRect ( margin, margin, device->width() - 2*margin,
                      device->height() - 2*margin );
      _painter = &painter;

      if (_opt->_shrinkFit)
         _printable = _body;
      else
         {
#if QT_VERSION >= 0x040400
         QMarginsF margins = _printer->pageLayout().margins();
         qreal left = margins.left(), top = margins.top();
         qreal right = margins.right(), bottom = margins.bottom();

         _printable = QRect (left, top, _body.width () - left - right, _body.height () - top - bottom);
#else
         _printable = _printer->pageRect ();
#endif
         }

      // set up the font
      _painter->setFont (_opt->_font);

      _reverse = _printer->pageOrder () == QPrinter::LastPageFirst;

      // iterate through all stacks and pages
      int seq = _reverse ? _pagecount - 1 : 0;      // page sequence number

      for (int i = _reverse ? list.size () - 1 : 0; _reverse ? i >= 0 : i < list.size ();
          _reverse ? i-- : i++)
//       foreach (ind, list)
         {
         ind = list [i];
         int numpages = _contents->data (ind, Desktopmodel::Role_pagecount).toInt ();
         int blank = _opt->_sepSheet /*&& _printer->doubleSidedPrinting ()*/ && (numpages & 1)
            ? 1 : 0;    // add a blank page?

         for (int pnum = _reverse ? numpages + blank - 1 : 0;
             _reverse ? pnum >= 0 : pnum < numpages + blank; _reverse ? pnum-- : pnum++)
//          for (int pnum = 0; pnum < numpages + blank; pnum++)
            {
            if (op.setProgress (seq))
               CALLB (err_make (ERRFN, ERR_operation_cancelled1, "print"));
//             printf ("seq %d: row %d, %d of %d\n", seq, ind.row (), pnum, numpages);
            CALLB (printPage (_reverse ? seq-- : seq++, ind, pnum, numpages));
            }
         if (e)
            break;
         }

      painter.end ();

//printf ("w,h = %d, %d\n", body.width (), body.height ());

      if (e)
         {
         snprintf (msg, sizeof(msg), "Print error: %s", e->errstr);
         emit newContents (msg);
         }
      else
         emit newContents ("Printing completed");
      }
   else if (!result)
      emit newContents ("Printing aborted");
   _watchButtons = true;
   delete _opt;
   }


void Mainwidget::selectAll (void)
   {
    _view->selectAll ();
   }


/** arrange items by given type (t_arrangeBy...) */

void Mainwidget::arrangeBy (t_arrangeBy type)
   {
   _view->arrangeBy (type);
   }


/** recalculate the sizes of all the items in this directory */

void Mainwidget::resizeAll ()
   {
   _view->resizeAll ();
   }


err_info *Mainwidget::operation (Desk::operation_t type, int ival)
   {
   UNUSED(type);
   UNUSED(ival);
/*p   if (visibleWidget () == _desktop)
      return _viewer->operation (type, ival);
   else
      return _page->operation (type, ival);*/
   return 0;
   }


void Mainwidget::rotate (int degrees)
   {
   err_complain (operation (Desk::op_rotate, degrees));
   }


void Mainwidget::flip (bool horiz)
   {
   err_complain (operation (horiz ? Desk::op_hflip : Desk::op_vflip, 0));
   }


bool Mainwidget::isSmoothing (void)
   {
   return _smooth;
   }


void Mainwidget::setSmoothing (bool smooth)
   {
   _smooth = smooth;
   xmlConfig->setBoolValue ("DISPLAY_SMOOTH", smooth);
   if (currentWidget() == _page)
      {
      _page->setSmoothing (_smooth);
      return _page->redisplay ();
      }
   }


void Mainwidget::matchUpdate (QString match, bool subdirs)
   {
   if (currentWidget () == _desktop)
      _desktop->matchUpdate (match, subdirs);
   }


void Mainwidget::resetSearch (void)
   {
   if (currentWidget () == _desktop)
      _desktop->matchUpdate ("", false, true);
   }


void Mainwidget::swapDesktop ()
   {
   if (currentWidget () == _desktop)
      {
      QModelIndex index;

      if (_desktop->getCurrentFile (index))
         showPage (index, false);
      }
   else
      showDesktop ();
   }
