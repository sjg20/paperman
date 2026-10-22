/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
*/

#include <functional>

#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QToolButton>
#include <QtTest/QtTest>

#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopwidget.h"
#include "dirview.h"
#include "folderlist.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pagewidget.h"
#include "pageview.h"
#include "paperstack.h"
#include "pscan.h"
#include "qxmlconfig.h"
#include "test_fakescan.h"
#include "test_video.h"

static const QSize kWindowSize (1440, 900);


void TestVideo::init ()
   {
   _repo = qEnvironmentVariable ("PAPERMAN_VIDEO_REPO");
   _sheets = qEnvironmentVariable ("PAPERMAN_VIDEO_SHEETS");
   _out = qEnvironmentVariable ("PAPERMAN_VIDEO_DIR");
   if (_repo.isEmpty () || _sheets.isEmpty () || _out.isEmpty ())
      QSKIP ("set PAPERMAN_VIDEO_REPO, PAPERMAN_VIDEO_SHEETS and "
             "PAPERMAN_VIDEO_DIR to play the video");
   if (!Fakescan::available ())
      QSKIP ("the video scans with the fake scanner, built only on Linux");
   }


void TestVideo::cue (const QString &caption)
   {
   _cues.append (QJsonObject {
      {"ms", QDateTime::currentMSecsSinceEpoch ()},
      {"caption", caption}});
   }


void TestVideo::pause (int ms)
   {
   QTest::qWait (ms);
   }


void TestVideo::glide (QWidget *widget, QPoint pos, int ms)
   {
   QPoint from = QCursor::pos ();
   QPoint to = widget->mapToGlobal (pos);
   int steps = qMax (1, ms / 16);

   for (int i = 1; i <= steps; i++)
      {
      // start and finish gently, as a hand does
      double t = double (i) / steps;

      t = t * t * (3 - 2 * t);
      QCursor::setPos (from + (to - from) * t);
      QTest::qWait (16);
      }
   }


void TestVideo::click (QWidget *widget, QPoint pos, bool twice)
   {
   glide (widget, pos);
   pause (200);
   if (twice)
      QTest::mouseDClick (widget, Qt::LeftButton, Qt::NoModifier, pos);
   else
      QTest::mouseClick (widget, Qt::LeftButton, Qt::NoModifier, pos);
   pause (400);
   }


void TestVideo::type (QWidget *widget, const QString &text)
   {
   for (QChar ch : text)
      {
      QTest::keyClicks (widget, QString (ch));
      QTest::qWait (110);
      }
   }


/* Call @func with the first dialog shown, other than the scan panel, once
   one is: it is run from a dialog's own loop, as a person answers it */
static void whenDialog (QObject *context, std::function<void (QDialog *)> func,
                        int tries = 100)
   {
   QTimer::singleShot (100, context, [context, func, tries] ()
      {
      for (QWidget *widget : QApplication::topLevelWidgets ())
         {
         QDialog *diag = qobject_cast<QDialog *> (widget);

         if (diag && diag->isVisible () && !qobject_cast<Pscan *> (diag))
            {
            func (diag);
            return;
            }
         }
      if (tries > 1)
         whenDialog (context, func, tries - 1);
      });
   }


//! the stack named @p name in the stack pane, as the view knows it
static QModelIndex stackIndex (Desktopwidget *desktop, const QString &name)
   {
   Desktopview *view = desktop->getView ();
   QAbstractItemModel *model = view->model ();

   for (int row = 0; row < model->rowCount (view->rootIndex ()); row++)
      {
      QModelIndex ind = model->index (row, 0, view->rootIndex ());

      if (ind.data ().toString ().startsWith (name))
         return ind;
      }
   return QModelIndex ();
   }


void TestVideo::testVideo ()
   {
   Mainwindow me;
   Desktopwidget *desktop = me.getDesktop ();
   Desktopview *view = desktop->getView ();
   Dirview *dirs = desktop->_dir;

   QVERIFY (!desktop->addDir (_repo));
   me.resize (kWindowSize);
   me.showMaximized ();
   QVERIFY (QTest::qWaitForWindowExposed (&me));
   me.activateWindow ();
   QVERIFY (QTest::qWaitForWindowActive (&me));
   QCursor::setPos (me.mapToGlobal (QPoint (720, 450)));

   auto dirPoint = [&] (const QString &rel) {
      QModelIndex ind = desktop->getDirIndex (_repo + "/" + rel + "/");

      dirs->scrollTo (ind);
      return dirs->visualRect (ind).center ();
   };

   desktop->selectDir (desktop->getDirIndex (_repo + "/"));
   pause (1500);

   // browsing: the folders, a month's bills and a page of one
   cue ("Your papers, in folders you already know");
   click (dirs->viewport (), dirPoint ("bills"), true);
   click (dirs->viewport (), dirPoint ("bills/2026"), true);
   click (dirs->viewport (), dirPoint ("bills/2026/10oct"));
   pause (800);

   QModelIndex bill = stackIndex (desktop, "power bill");

   QVERIFY (bill.isValid ());
   click (view->viewport (), view->visualRect (bill).center ());
   desktop->_page->slotZoomFit ();
   pause (1500);

   Pageview *pages = desktop->_page->_pageview;
   QModelIndex second = pages->model ()->index (1, 0, pages->rootIndex ());

   QVERIFY (second.isValid ());
   click (pages->viewport (), pages->visualRect (second).center ());
   pause (2000);

   // scanning: two sheets into the travel folder
   cue ("Scan straight into a folder");

   QStringList sheets = QDir (_sheets).entryList ({"*.png"}, QDir::Files,
                                                 QDir::Name);

   QVERIFY (!sheets.isEmpty ());
   for (const QString &name : sheets)
      QVERIFY (Fakescan::loadSheet (QImage (_sheets + "/" + name), QImage (),
                                    150));
   Fakescan::setSideTime (900);

   if (!xmlConfig)
      new QXmlConfig ();
   xmlConfig->setStringValue ("LAST_DEVICE", FAKESCAN_DEVICE);

   // the sheets are scanned both sides, and their blank backs left out
   xmlConfig->setIntValue ("SCAN_BLANK", Paperstack::ignoreIfBack);
   // open the scan panel from its button in the main toolbar
   QToolButton *panel = nullptr;

   for (QToolButton *button : me.findChildren<QToolButton *> ())
      if (button->defaultAction () == me.actionPscan && button->isVisible ())
         panel = button;
   QVERIFY (panel);
   click (panel, panel->rect ().center ());

   Pscan *pscan = nullptr;

   QTRY_VERIFY ((pscan = [] () -> Pscan * {
      for (QWidget *widget : QApplication::topLevelWidgets ())
         if (qobject_cast<Pscan *> (widget) && widget->isVisible ())
            return qobject_cast<Pscan *> (widget);
      return nullptr;
   } ()));
   pscan->move (me.mapToGlobal (QPoint (me.width () - pscan->width () - 30,
                                        90)));
   pscan->activateWindow ();
   QVERIFY (QTest::qWaitForWindowActive (pscan));
   pause (800);

   int item = pscan->preset->findText ("Auto 200dpi duplex");

   if (item >= 0)
      {
      pscan->preset->setCurrentIndex (item);
      pscan->on_preset_activated (item);
      }
   click (pscan->stackName, pscan->stackName->rect ().center ());
   type (pscan->stackName, "flight itinerary");
   click (pscan->folderName, pscan->folderName->rect ().center ());
   type (pscan->folderName, "trav");
   pause (900);

   // pick the folder from those which match
   Folderlist *matches = pscan->_folders;
   QModelIndex travel;

   QTRY_VERIFY (matches->isVisible ());
   for (int row = 0; row < matches->model ()->rowCount (); row++)
      {
      QModelIndex ind = matches->model ()->index (row, 0);

      if (ind.data ().toString () == "travel")
         travel = ind;
      }
   QVERIFY (travel.isValid ());
   click (matches->viewport (), matches->visualRect (travel).center ());
   pause (900);
   click (pscan->scan, pscan->scan->rect ().center ());
   QTRY_VERIFY_WITH_TIMEOUT (Fakescan::sheetsLeft () == 0, 60000);
   QTRY_VERIFY_WITH_TIMEOUT (
      QFile::exists (_repo + "/travel/flight itinerary.max"), 30000);
   pause (3500);
   pscan->hide ();
   pause (800);

   // searching: for a word, then the manual it is in
   cue ("Find any page by the words on it");

   // from the top, so as to search all the papers
   QModelIndex top = desktop->getDirIndex (_repo + "/");

   dirs->scrollTo (top);
   click (dirs->viewport (), dirs->visualRect (top).center ());
   pause (600);

   bool searched = false;

   whenDialog (this, [this, &searched] (QDialog *diag)
      {
      QRadioButton *byText = diag->findChild<QRadioButton *> ("byText");
      QLineEdit *words = diag->findChild<QLineEdit *> ("stackName");
      QDialogButtonBox *box = diag->findChild<QDialogButtonBox *> ();

      pause (600);
      click (byText, QPoint (8, byText->height () / 2));
      click (words, words->rect ().center ());
      type (words, "warranty");
      pause (600);

      QPushButton *ok = box->button (QDialogButtonBox::Ok);

      searched = true;
      click (ok, ok->rect ().center ());
      });
   /* as Ctrl+S does; the display has no window manager to make the window
      active, which a shortcut needs */
   desktop->activateSearch ();
   QVERIFY (searched);
   pause (1500);

   QModelIndex manual = stackIndex (desktop, "dishwasher");

   QVERIFY (manual.isValid ());
   click (view->viewport (), view->visualRect (manual).center ());
   desktop->_page->slotZoomFit ();
   pause (3000);

   // filing: a stack from the inbox to where it belongs
   cue ("File a stack by typing part of a folder's name");
   QTest::keyClick (view, Qt::Key_Escape);
   pause (800);
   click (dirs->viewport (), dirPoint ("inbox"));
   pause (1000);

   // a reminder from the dentist, scanned without a name
   QModelIndex first;

   for (int row = 0; row < view->model ()->rowCount (view->rootIndex ());
        row++)
      {
      QModelIndex ind = view->model ()->index (row, 0, view->rootIndex ());

      if (ind.data ().toString () == "20-Oct-26")
         first = ind;
      }
   QVERIFY (first.isValid ());
   click (view->viewport (), view->visualRect (first).center ());

   bool moved = false;

   whenDialog (this, [this, &moved] (QDialog *diag)
      {
      QLineEdit *folder = diag->findChild<QLineEdit *> ("folderName");

      pause (600);
      type (folder, "med");
      pause (1200);
      moved = true;
      QTest::keyClick (diag->focusWidget () ? diag->focusWidget () : folder,
                       Qt::Key_Return);
      });

   /* the stack's menu waits in a loop of its own, so choose from it on a
      timer, as for a dialog */
   std::function<void (int)> choose = [&] (int tries)
      {
      for (QWidget *top : QApplication::topLevelWidgets ())
         {
         QMenu *menu = qobject_cast<QMenu *> (top);

         if (menu && menu->isVisible ())
            {
            for (QAction *action : menu->actions ())
               if (action->text ().remove ('&') == "Move to folder")
                  {
                  pause (700);
                  click (menu, menu->actionGeometry (action).center ());
                  return;
                  }
            menu->close ();
            return;
            }
         }
      if (tries > 1)
         QTimer::singleShot (100, this, [&choose, tries] {
            choose (tries - 1); });
      };
   QTimer::singleShot (100, this, [&choose] { choose (100); });

   QPoint at = view->visualRect (first).center ();
   QContextMenuEvent menuEvent (QContextMenuEvent::Mouse, at,
                                view->viewport ()->mapToGlobal (at));

   QApplication::sendEvent (view->viewport (), &menuEvent);
   QVERIFY (moved);
   pause (1500);
   click (dirs->viewport (), dirPoint ("medical"));
   pause (2500);

   cue ("Paperman: free and open source, at getpaperman.com");
   pause (3500);

   QFile file (_out + "/cues.json");

   QVERIFY (file.open (QIODevice::WriteOnly));
   file.write (QJsonDocument (_cues).toJson ());
   }
