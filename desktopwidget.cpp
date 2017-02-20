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

#include <QtGui>
#include <QSettings>
#include <QItemSelectionModel>

#include "qlistview.h"
#include "qapplication.h"
#include "qcursor.h"
#include "qdir.h"
#include "qfileinfo.h"
#include "qpointer.h"
#include "qlistview.h"
#include "qinputdialog.h"
#include "qevent.h"
#include "qlabel.h"
#include "qlayout.h"
#include "qpoint.h"
#include "qsplitter.h"
#include "qtimer.h"
#include <QDropEvent>
#include <QPixmap>

#include "err.h"

#include "desktopdelegate.h"
#include "desktopmodel.h"
#include "desktopview.h"
#include "desktopundo.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "dirview.h"
#include "op.h"
#include "desk.h"
#include "maxview.h"
#include "pagewidget.h"
#include "senddialog.h"
#include "utils.h"


// define this to use a proxy model which provides fast filtering
#define USE_PROXY


// static QFont *contents_font = 0;

Desktopwidget::Desktopwidget (QWidget *parent)
      : QSplitter (parent)
   {
   _model = new Dirmodel ();
//    _model->setLazyChildCount (true);
   _dir = new Dirview (this);
   _dir->setModel (_model);

   _contents = new Desktopmodel (this);

   QWidget *group = createToolbar();

   _view = new Desktopview (group);
   QVBoxLayout *lay = new QVBoxLayout (group);
   lay->setContentsMargins (0, 0, 0, 0);
   lay->setSpacing (2);
   lay->addWidget (_toolbar);
   lay->addWidget (_view);

   connect (_view, SIGNAL (itemPreview (const QModelIndex &, int, bool)),
         this, SLOT (slotItemPreview (const QModelIndex &, int, bool)));

#ifdef USE_PROXY
   _proxy = new Desktopproxy (this);
   _proxy->setSourceModel (_contents);
   _view->setModel (_proxy);
//    printf ("contents=%p, proxy=%p\n", _contents, _proxy);

   // set up the model converter
   _modelconv = new Desktopmodelconv (_contents, _proxy);

   // setup another one for Desktopmodel, which only allows assertions
   _modelconv_assert = new Desktopmodelconv (_contents, _proxy, false);
#else
   _proxy = 0;
   _view->setModel (_contents);
   _modelconv = new Desktopmodelconv (_contents);

   // setup another one for Desktopmodel, which only allows assertions
   _modelconv_assert = new Desktopmodelconv (_contents, false);
#endif

   _view->setModelConv (_modelconv);

   _contents->setModelConv (_modelconv_assert);

   _delegate = new Desktopdelegate (_modelconv, this);
   _view->setItemDelegate (_delegate);
   connect (_delegate, SIGNAL (itemClicked (const QModelIndex &, int)),
         this, SLOT (slotItemClicked (const QModelIndex &, int)));
   connect (_delegate, SIGNAL (itemPreview (const QModelIndex &, int, bool)),
         this, SLOT (slotItemPreview (const QModelIndex &, int, bool)));
   connect (_delegate, SIGNAL (itemDoubleClicked (const QModelIndex &)),
      this, SLOT (openStack (const QModelIndex &)));

   connect (_contents, SIGNAL (undoChanged ()),
      this, SIGNAL (undoChanged ()));
   connect (_contents, SIGNAL (dirChanged (QString&, QModelIndex&)),
      this, SLOT (slotDirChanged (QString&, QModelIndex&)));
   connect (_contents, SIGNAL (beginningScan (const QModelIndex &)),
      this, SLOT (slotBeginningScan (const QModelIndex &)));
   connect (_contents, SIGNAL (endingScan (bool)),
      this, SLOT (slotEndingScan (bool)));
   connect (_contents, SIGNAL(updateRepositoryList (QString &, bool)),
            this, SLOT(slotUpdateRepositoryList (QString &, bool)));

    // position the items when the model is reset, otherwise things
    // move and look ugly for a while
    connect (_contents, SIGNAL (modelReset ()), _view, SLOT (setPositions ()));

   createPage();

   // and when there are no selected items
   connect (_view, SIGNAL (pageLost()), _page, SLOT (slotReset ()));

   _parent = parent;
   _pendingMatch = QString::null;
   _updating = false;

   // setup the preview timer
   _timer = new QTimer ();
   _timer->setSingleShot (true);
   connect (_timer, SIGNAL(timeout()), this, SLOT(updatePreview()));

   connect (_dir, SIGNAL (clicked (const QModelIndex&)),
            this, SLOT (dirSelected (const QModelIndex&)));
   connect (_dir, SIGNAL (activated (const QModelIndex&)),
            this, SLOT (dirSelected (const QModelIndex&)));
   connect (_model, SIGNAL(droppedOnFolder(const QMimeData *, QString &)),
            this, SLOT(slotDroppedOnFolder(const QMimeData *, QString &)));

   /* notice when the current directory is fully displayed so we can handle
      any pending action */
   connect (_contents, SIGNAL (updateDone()), this, SLOT (slotUpdateDone()));

   // connect signals from the directory tree
   connect (_dir->_new, SIGNAL (triggered ()), this, SLOT (newDir ()));
   connect (_dir->_rename, SIGNAL (triggered ()), this, SLOT (renameDir ()));
   connect (_dir->_delete, SIGNAL (triggered ()), this, SLOT (deleteDir ()));
   connect (_dir->_refresh, SIGNAL (triggered ()), this, SLOT (refreshDir ()));
   connect (_dir->_add_recent, SIGNAL (triggered ()), this,
            SLOT (addToRecent ()));
   connect (_dir->_add_repository, SIGNAL (triggered ()), this,
            SLOT (slotAddRepository ()));
   connect (_dir->_remove_repository, SIGNAL (triggered ()), this,
            SLOT (slotRemoveRepository ()));

   setStretchFactor(indexOf(_dir), 0);

   QList<int> size;

   if (!getSettingsSizes ("desktopwidget/", size))
      {
      size.append (200);
      size.append (1000);
      size.append (400);
      }
   setSizes (size);

   connect (_view, SIGNAL (popupMenu (QModelIndex &)),
         this, SLOT (slotPopupMenu (QModelIndex &)));

   // allow top level to see our view messages
   connect (_view, SIGNAL (newContents (QString)), this, SIGNAL (newContents (QString)));

   addActions();

   /* unfortunately when we first run maxview it starts with the main window
      un-maximised. This means that scrollToLast() doesn't quite scroll far
      enough for the maximised view which appears soon afterwards. As a hack
      for the moment, we do another scroll 1 second after starting up */
   QTimer::singleShot(1000, _view, SLOT (scrollToLast()));
   }


void Desktopwidget::createPage(void)
   {
   _page = new Pagewidget (_modelconv, "desktopwidget/", this);
   _page->setSmoothing (false);

   // allow top level to see our preview messages
   connect (_page, SIGNAL (newContents (QString)), this, SIGNAL (newContents (QString)));

   connect (_page, SIGNAL (modeChanging (int, int)),
      this, SLOT (slotModeChanging (int, int)));

   _page->init ();

   // alert the page widget whenever a new page is finished scanning
   connect (_contents, SIGNAL (newScannedPage (const QString &, bool)),
      _page, SLOT (slotNewScannedPage (const QString &, bool)));

   // alert the page widget whenever we start to scan a new page
   connect (_contents, SIGNAL (beginningPage ()),
      _page, SLOT (slotBeginningPage ()));

   // and when we have a new preview image fragment for the page being scanned
   connect (_contents, SIGNAL (newScaledImage (const QImage &, int)),
      _page, SLOT (slotNewScaledImage (const QImage &, int)));

   // and when we change a stack
   connect (_contents, SIGNAL (dataChanged (const QModelIndex &, const QModelIndex &)),
      _page, SLOT (slotStackChanged (const QModelIndex &, const QModelIndex &)));

   // and when we delete any stacks
   connect (_contents, SIGNAL (rowsRemoved (const QModelIndex &, int, int)),
      _page, SLOT (slotReset ()));

   // and when we want to commit the stack
   connect (_contents, SIGNAL (commitScanStack ()),
      _page, SLOT (slotCommitScanStack ()));
   }


void Desktopwidget::addActions(void)
   {
   // use translatable version of keys
   addAction (_act_duplicate, "&Duplicate", SLOT(duplicate ()), "Ctrl+D");
   addAction (_act_locate, "&Locate folder",  SLOT(locateFolder ()), "Ctrl+L");
   addAction (_act_delete, "D&elete stack",  SLOT(deleteStacks ()), "Delete");

   addAction (_act_stack, "&Stack", SLOT(stackPages()), "Ctrl+G");
   addAction (_act_unstack_page, "Unstack &page", SLOT(unstackPage()), "Ctrl+I");
   addAction (_act_unstack_all, "&Unstack all", SLOT(unstackStacks ()), "Ctrl+U");
   addAction (_act_rename_stack, "&Rename stack", SLOT(renameStack ()), "F2");  //"F2,Ctrl+R");
   addAction (_act_rename_page, "Re&name page", SLOT (renamePage ()), "Shift+F2");

   addAction (_act_duplicate_page, "Duplicate p&age", SLOT (duplicatePage ()), "Ctrl+Shift+I");
   addAction (_act_duplicate_max, "as &Max", SLOT (duplicateMax ()), "Ctrl+Shift+D");
   addAction (_act_duplicate_pdf, "as &PDF", SLOT (duplicatePdf ()), "Ctrl+Shift+P");
   //    addAction (_act_duplicate_tiff, "as &Tiff", SLOT (duplicateTiff ()), "Ctrl+Shift+T");
   addAction (_act_duplicate_odd, "&odd pages only", SLOT (duplicateOdd ()), "");
   addAction (_act_duplicate_even, "&even pages only", SLOT (duplicateEven ()), "");
   addAction (_act_duplicate_jpeg, "as &JPEG", SLOT (duplicateJpeg ()), "Ctrl+Shift+J");

   addAction (_act_email, "&Files", SLOT (email ()), "Ctrl+E");
   addAction (_act_email_pdf, "as &PDF", SLOT (emailPdf ()), "Ctrl+Shift+E");
   addAction (_act_email_max, "as &Max", SLOT (emailMax ()), "Ctrl+Alt+E");
   addAction (_act_send, "&Send stacks", SLOT (send ()), "Ctrl+S");
   addAction (_act_deliver_out, "&Delivery outgoing", SLOT (deliverOut ()), "");
   }


QWidget *Desktopwidget::createToolbar(void)
   {
   QWidget *group = new QWidget (this);

   //TODO: Move this to use the designer
   /* create the desktop toolbar. We are doing this manually since we can't
      seem to get Qt to insert a QLineEdit into a toolbar */
   _toolbar = new QToolBar (group);
//   _toolbar = new QWidget (group);
//   _toolbar = group;
   addAction (_actionPprev, "Previous page", SLOT(pageLeft ()), "", _toolbar, "pprev.xpm");
   addAction (_actionPprev, "Next page", SLOT(pageRight ()), "", _toolbar, "pnext.xpm");
   addAction (_actionPprev, "Previous stack", SLOT(stackLeft ()), "", _toolbar, "prev.xpm");
   addAction (_actionPprev, "Next stack", SLOT(stackRight ()), "", _toolbar, "next.xpm");

   QWidget *findgroup = new QWidget (_toolbar);

   QHBoxLayout *hboxLayout2 = new QHBoxLayout();
   hboxLayout2->setSpacing(0);
   hboxLayout2->setContentsMargins (0, 0, 0, 0);
   hboxLayout2->setObjectName(QString::fromUtf8("hboxLayout2"));
   findgroup->setLayout (hboxLayout2);

   QLabel *label = new QLabel (findgroup);
   label->setText(QApplication::translate("Mainwindow", "Filter:", 0,
                                          QApplication::UnicodeUTF8));
   label->setObjectName(QString::fromUtf8("label"));

   hboxLayout2->addWidget(label);

   _match = new QLineEdit (findgroup);
   _match->setObjectName ("match");
   QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Fixed);
   sizePolicy2.setHorizontalStretch(1);
   sizePolicy2.setVerticalStretch(0);
   sizePolicy2.setHeightForWidth(_match->sizePolicy().hasHeightForWidth());
   _match->setSizePolicy(sizePolicy2);
   _match->setMinimumSize(QSize(50, 0));
   //_match->setDragEnabled(true);

   connect (_match, SIGNAL (returnPressed()),
        this, SLOT (matchUpdate ()));
   connect (_match, SIGNAL (textChanged(const QString&)),
        this, SLOT (matchChange (const QString &)));
   //_reset_filter = new QAction (this);
   //_reset_filter->setShortcut (Qt::Key_Escape);
   //connect (_reset_filter, SIGNAL (triggered()), this, SLOT (resetFilter()));
   //_match->addAction (_reset_filter);
   //_match->installEventFilter (this);

   // When ESC is pressed, clear the field
   QStateMachine *machine = new QStateMachine (this);
   QState *s1 = new QState (machine);

//   QSignalTransition *pressed_esc = new QSignalTransition(_match,
//                                       SIGNAL(textChanged(const QString&)));
   QKeyEventTransition *pressed_esc = new QKeyEventTransition(_match,
                           QEvent::KeyPress, Qt::Key_Escape);
   s1->addTransition (pressed_esc);
   connect(pressed_esc, SIGNAL(triggered()), this, SLOT(resetFilter()));
   machine->setInitialState (s1);
   machine->start ();
#if 0
  QPushButton *test = new QPushButton (findgroup);
  test->setText ("hello");
  hboxLayout2->addWidget (test);

   QStateMachine *test_machine = new QStateMachine (this);
   QState *test_s1 = new QState (test_machine);

   QSignalTransition *trans = new QSignalTransition(test, SIGNAL(clicked()));
   test_s1->addTransition (trans);
   connect(trans, SIGNAL(triggered()), this, SLOT(resetFilter()));
   test_machine->setInitialState (test_s1);
   test_machine->start ();
#endif
    // and change the state
   hboxLayout2->addWidget(label);

   hboxLayout2->addWidget (_match);

   addAction (_find, "Filter stacks", SLOT(findClicked ()), "", findgroup, "find.xpm");
   QToolButton *find = new QToolButton (findgroup);
   find->setDefaultAction (_find);
   hboxLayout2->addWidget (find);
//    connect (_find, SIGNAL (activated ()), this, SLOT (findClicked ()));

   QSpacerItem *spacerItem = new QSpacerItem(16, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

   hboxLayout2->addItem (spacerItem);

   _global = new QCheckBox("Subdirs", findgroup);
   _global->setObjectName(QString::fromUtf8("global"));

   hboxLayout2->addWidget(_global);

   addAction (_reset, "Reset", SLOT(resetFilter ()), "", findgroup);
   QToolButton *reset = new QToolButton (findgroup);
   reset->setDefaultAction (_reset);
   hboxLayout2->addWidget (reset);

   _toolbar->addWidget (findgroup);

#ifndef QT_NO_TOOLTIP
   _match->setToolTip(QApplication::translate("Mainwindow", "Enter part of the name of the stack to search for", 0, QApplication::UnicodeUTF8));
   _find->setToolTip(QApplication::translate("Mainwindow", "Search for the name", 0, QApplication::UnicodeUTF8));
   _global->setToolTip(QApplication::translate("Mainwindow", "Enable this to search all subdirectories also", 0, QApplication::UnicodeUTF8));
   _reset->setToolTip(QApplication::translate("Mainwindow", "Reset the search string", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_WHATSTHIS
   _match->setWhatsThis(QApplication::translate("Mainwindow", "The filter feature can be used in two ways. To filter out unwanted stacks, type a few characters from the stack name that you are looking for. Everything that does not match will be removed from view. To go back, just delete characters from the filter.\n"
"\n"
"There is also a 'global' mode which allows searching of all subdirectories. To use this, select the 'global' button, then type your filter string. Press return or click 'find' to perform the search. This might take a while.\n"
"\n"
"To reset the filter, click the 'reset' button.", 0, QApplication::UnicodeUTF8));
   _find->setWhatsThis(QApplication::translate("Mainwindow", "Click this button to perform a search when in global mode", 0, QApplication::UnicodeUTF8));
   _global->setWhatsThis(QApplication::translate("Mainwindow", "The filter feature can be used in two ways. To filter out unwanted stacks, type a few characters from the stack name that you are looking for. Everything that does not match will be removed from view. To go back, just delete characters from the filter.\n"
"\n"
"There is also a 'global' mode which allows searching of all subdirectories. To use this, select the 'global' button, then type your filter string. Press return or click 'find' to perform the search. This might take a while.\n"
"\n"
"To reset the filter, click the 'reset' button.", 0, QApplication::UnicodeUTF8));
   _reset->setWhatsThis(QApplication::translate("Mainwindow", "Press this button to reset the filter string and display stacks in the current directory", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS
   return group;
   }

Desktopwidget::~Desktopwidget ()
   {
   delete _timer;
   delete _contents;
   delete _modelconv;
   delete _modelconv_assert;
   delete _dir;
   }


void Desktopwidget::closing (void)
   {
   QList<int> size = sizes ();

   setSettingsSizes ("desktopwidget/", size);
   _page->closing ();
   }


void Desktopwidget::slotModeChanging (int new_mode, int old_mode)
   {
//    qDebug () << "slotModeChanging" << new_mode << old_mode;

   // get the current sizes and save them
   if (old_mode != Pagewidget::Mode_none)
      {
      QList<int> size = sizes ();
      QString str = QString ("desktopwidget/mode%2/").arg (old_mode);

      setSettingsSizes (str, size);
      }

   if (new_mode != Pagewidget::Mode_none)
      {
      QList<int> size;
      QString str = QString ("desktopwidget/mode%2/").arg (new_mode);

      if (getSettingsSizes (str, size))
         setSizes (size);
      }
   }


/***************************** scanning *********************************/

void Desktopwidget::slotBeginningScan (const QModelIndex &sind)
   {
//    qDebug () << "slotBeginningScan";
   _modelconv->assertIsSource (0, &sind, 0);

   // convert to a proxy index, since that is what _view uses
   QModelIndex ind = sind;
   _modelconv->indexToProxy (ind.model (), ind);

   // scroll so the new stack is visible
   _view->scrollTo (ind);

   // select this new stack
   _view->setSelectionRange (ind.row (), 1);

   // advise the page widget that we are starting a scan
   _page->beginningScan (ind);
   }


void Desktopwidget::slotEndingScan (bool cancel)
   {
//    qDebug () << "slotEndingScan";
   _page->endingScan (cancel);
   }


void Desktopwidget::scanComplete (void)
   {
   _page->scanComplete ();
   }


/***************************************************************************/

void Desktopwidget::addAction (QAction *&act, const char *text, const char *slot, const QString &shortcut,
      QWidget *parent, const char *image)
   {
   if (!parent)
      parent = _view;
   act = new QAction (tr (text), parent);
   if (!shortcut.isEmpty ())
      act->setShortcut (tr (shortcut.toLatin1()));
   if (image)
      {
      QIcon icon;
      QString str = QString (":/images/images/%1").arg (image);

      icon.addPixmap (QPixmap(str), QIcon::Normal, QIcon::Off);
      act->setIcon (icon);
//       act->setIconSize(QSize(24, 24));
      act->setAutoRepeat (true);
      }
   parent->addAction (act);
   connect (act, SIGNAL (triggered()), this, slot);
   }


bool Desktopwidget::getCurrentFile (QModelIndex &index)
   {
   index = _view->getSelectedItem ();

   return index.isValid ();
   }


err_info *Desktopwidget::addDir (QString in_dirname, bool ignore_error)
   {
   err_info *err = NULL;

   QDir dir (in_dirname);

//    dirname = dir.absPath () + "/";
   QString dirname = dir.canonicalPath ();
   if (dirname.isEmpty ())
      {
      dirname = in_dirname;
      if (dirname.endsWith ("/"))
         dirname.chop (1);
      err = err_make (ERRFN, ERR_directory_not_found1,
                       qPrintable(dirname));
      }

   // Check that the dirname isn't overlapping another
   CALL (_model->checkOverlap (dirname, in_dirname));
   dirname += "/";

   QModelIndex index = _model->index (dirname, 0);

   if (index != QModelIndex ())
      return err_make (ERRFN, ERR_directory_is_already_present_as2,
                       qPrintable(in_dirname), qPrintable(dirname));
   else if (err && !ignore_error)
      ;
   else if (_model->addDir (dirname, ignore_error))
      {
      QModelIndex index = _model->index (dirname);
      selectDir (index);
      }
   else
      err = err_make (ERRFN, ERR_directory_could_not_be_added1,
                       qPrintable(in_dirname));
   return err;
   }


void Desktopwidget::selectDir (QModelIndex &index)
   {
//    int count = _model->rowCount (QModelIndex ());

   /* use the second directory if there is nothing supplied, since the first
      is 'Recent items' */
   if (index == QModelIndex ())
      index = _model->index (1, 0, QModelIndex ());

   //qDebug () << "Desktopwidget::selectDir" << _model->data (index, Qt::DisplayRole).toString ();
   _dir->setCurrentIndex (index);
   _dir->setExpanded (index, true);
    dirSelected (index, false);
   }


void Desktopwidget::slotDroppedOnFolder(const QMimeData *data, QString &dir)
   {
   QByteArray encodedData = data->data("application/vnd.text.list");
   QDataStream stream(&encodedData, QIODevice::ReadOnly);
   QStringList newItems;
   int rows = 0;

   while (!stream.atEnd()) {
      QString text;
      stream >> text;
      newItems << text;
      ++rows;
   }

   QModelIndexList list = _contents->listFromFilenames (newItems, _view->rootIndexSource ());

   dir += "/";
   QStringList sl;
   _contents->moveToDir (list, _view->rootIndexSource (), dir, sl);
//    event->acceptAction ();
   }


void Desktopwidget::renameDir ()
   {
   QString path = _dir->menuGetPath ();
   QString fullPath;
   bool ok;
   QString oldName = _dir->menuGetName ();
   QModelIndex index;

   index = _dir->menuGetModelIndex ();
   if (_model->findIndex (index) != -1)
      {
      QMessageBox::warning (0, "Maxview", "You cannot rename a root directory");
      return;
      }
   QString text = QInputDialog::getText(
            this, "Maxview", "Enter new directory name:", QLineEdit::Normal,
            oldName, &ok);
   if ( ok && !text.isEmpty() && text != oldName)
      {
      QDir dir;

      QModelIndex index = _dir->menuGetModelIndex ();
      QModelIndex parent = _model->parent (index);

//       if (!_model->setData (index, QVariant (text)))
      path.truncate (path.length () - oldName.length () - 1);
      fullPath = path + "/" + text;
      if (dir.rename (path + "/" + oldName, fullPath))
         _model->refresh (parent);
//          _dir->refreshItemRename (text);  // indicates current item has new children
      else
         QMessageBox::warning (0, "Maxview", "Could not rename directory");
      }
   }


void Desktopwidget::refreshDir ()
   {
   QModelIndex index = _dir->menuGetModelIndex ();

   // update the model with this new directory
   _model->refresh (index);
   }


void Desktopwidget::addToRecent ()
   {
   QModelIndex index = _dir->menuGetModelIndex ();

   // update the model with this new directory
   _model->addToRecent (index);
   }

void Desktopwidget::updateSettings ()
   {
   int count = _model->rowCount (QModelIndex ());
   QSettings qs;

   qs.remove ("repository");
   qs.beginWriteArray ("repository");
   for (int i = 1; i < count; i++)
      {
      QModelIndex index = _model->index (i, 0, QModelIndex ());

      qs.setArrayIndex (i - 1);
      qs.setValue ("path", _model->data (index, Dirmodel::FilePathRole));
      }
   qs.endArray ();
   }

void Desktopwidget::slotUpdateRepositoryList (QString &dirname, bool add_not_delete)
   {
   err_info *err = NULL;

   if (add_not_delete)
      err = addDir (dirname);
   else
      {
      QModelIndex index = _model->index (dirname, 0);

      if (index != QModelIndex ())
         {
         _contents->removeDesk (dirname);
         _model->removeDirFromList (index);
         _contents->resetDirPath ();
         }
      else
         qDebug () << "slotUpdateRepositoryList: Could not find dirname"
               << dirname << "in model index: ";
      }
   if (!err_complain (err))
      updateSettings ();
   }

void Desktopwidget::slotAddRepository ()
   {
   QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select folder to use as a new repository"));

   if (!dir.isEmpty ())
      _contents->addRepository (dir);
   }

void Desktopwidget::slotRemoveRepository ()
   {
   QString dir = _dir->menuGetPath ();

   _contents->removeRepository (dir);
   }

void Desktopwidget::deleteDir ()
   {
   QString path = _dir->menuGetPath ();
   QString fullPath;
   int ok;
   QString oldName = _dir->menuGetName ();
   QModelIndex index = _dir->menuGetModelIndex ();

   // find out how many files are in the directory
   QString str = _model->countFiles (index, 10);

   ok = QMessageBox::question(
            this,
            tr("Confirmation -- maxview"),
            tr("Do you want to delete directory %1 (which contains %2)?")
               .arg (path).arg (str),
            QMessageBox::Ok, QMessageBox::Cancel);
   if ( ok == QMessageBox::Ok)
      {
      printf ("delete dir\n");
      err_info *err;
      QDir dir;

      qDebug () << "remove dir" << _model->filePath (index);
      err = _model->rmdir (index);
      if (err)
          QMessageBox::warning (0, "Maxview", err->errstr);
      }
   }


void Desktopwidget::newDir ()
   {
   QString path = _dir->menuGetPath ();
   QString fullPath;
   bool ok;

   QString text = QInputDialog::getText(
            this, "Maxview", "Enter new subdirectory name:", QLineEdit::Normal,
            QString::null, &ok);
   if ( ok && !text.isEmpty() )
      {
      QModelIndex index = _dir->menuGetModelIndex ();

//       printf ("mkdir  %s\n", _model->filePath (index).latin1 ());
      index = _model->mkdir (index, text);
//       printf ("   - got '%s'\n", _model->filePath (index).latin1 ());
      if (index == QModelIndex ())
         QMessageBox::warning (0, "Maxview", "Could not make directory " + fullPath);
      }
   }


void Desktopwidget::dirSelected (const QModelIndex &index, bool allow_undo)
   {
   QString path = _model->data (index, QDirModel::FilePathRole).toString ();
   QModelIndex root = _model->findRoot (index);
   QString root_path = _model->data (root, QDirModel::FilePathRole).toString ();

//    printf ("dirSelected %s, %s\n", path.latin1 (), _contents->getDirPath ().latin1 ());

   // clear the page preview
   _page->slotReset ();

   // if we have are actually changing directory, do so
   if (path != _contents->getDirPath ())
      {
      _path = path;
      _contents->changeDir (path, root_path, allow_undo);
      }

   /* otherwise just clear the current selection. This avoid confusion with
      keyboard shortcuts which might operate in the directory view and
      item view */
   else
      {
      QItemSelectionModel *sel = _view->selectionModel ();

      sel->clear ();
      }
   }


void Desktopwidget::slotDirChanged (QString &dirPath, QModelIndex &deskind)
   {
   QModelIndex index = _model->index (dirPath);

//    qDebug () << "Desktopwidget::slotDirChanged" << _dir->currentIndex () << index;
   _dir->setCurrentIndex (index);

   _modelconv->assertIsSource (0, &deskind, 0);
   QModelIndex ind = deskind;
   _modelconv->indexToProxy (ind.model (), ind);

   _view->setRootIndex (ind);

   // ensure that the correct item is displayed
   // the filename of the required item is held in _scroll_to
   QModelIndex scroll_ind = _contents->index (_scroll_to, deskind);
   _modelconv->indexToProxy (scroll_ind.model (), scroll_ind);
   _scroll_to = "";  // so we don't do the same next time

   if (scroll_ind != QModelIndex ())
      {
      _view->setSelectionRange (scroll_ind.row (), 1);
      _view->scrollTo (scroll_ind);
      }

   // if no particular 'scroll to' item is specified, just scroll to the last item
   else
      _view->scrollToLast ();
   }


void Desktopwidget::resetFilter (void)
   {
   qDebug () << "resetFilter";
   _match->setText ("");
   _global->setChecked (false);
   matchUpdate ("", false, true);
   }


void Desktopwidget::matchChange (const QString &)
   {
   if (!_global->isChecked ())
      matchUpdate (_match->text (), false);
   }


void Desktopwidget::findClicked (void)
   {
   if (_global->isChecked ())
      matchUpdate (_match->text (), _global->isChecked ());
   else
      emit tr ("To filter, just type into the filter field. To search all subdirectories, select global, enter filter and click this 'find' button");
   }


void Desktopwidget::matchUpdate (void)
   {
   matchUpdate (_match->text (), _global->isChecked ());
   }


/** update the match string and perform a new search */

void Desktopwidget::matchUpdate (QString match, bool subdirs, bool reset)
   {
   QModelIndex index = _model->index (_path);
   Operation *op = 0;

   if (!_proxy)
      return;

// printf ("path = %s\n", _path.latin1 ());
   // if we're already updating, schedule a later update (no subdirs allowed)
   if (_updating)
      {
      _pendingMatch = match;
//      printf ("pending '%s'\n", match.latin1 ());

      // tell the viewer to stop updating
      _contents->stopUpdate ();
      return;
      }

   /* if we are doing a global search, we need to recreate the maxdesk. Here
      we are creating a 'virtual' maxdesk which holds files from a number
      of different directories */
//   printf ("update\n");
   if (subdirs || reset)
      {
      _proxy->setFilterFixedString ("");
//       _updating = true;
      if (subdirs) // this might take a long time
         op = new Operation ("Searching", 100, this);
      QModelIndex root = _model->findRoot (index);
      QString root_path = _model->data (root, QDirModel::FilePathRole).toString ();
      QModelIndex sind = _contents->refresh (_path, root_path,
            match.isEmpty () ? true : false, match, subdirs, op);
      if (op)
         delete op;
      QModelIndex ind = sind;
      _modelconv->indexToProxy (ind.model (), ind);
      _view->setRootIndex (ind);
      }

   /* but otherwise we can just use the proxy model and tell it to change
      the filter */
   else
      {
      QModelIndex ind;

      // update the proxy
      qDebug () << "match" << match;
      _proxy->setFilterFixedString (match);

      // scroll to the first match
      ind = _proxy->index (0, 0, _view->rootIndex ());
      if (ind != QModelIndex ())
         _view->scrollTo (ind);
//      _view->setRootIndex (index);  // is this ok in the normal case?
      }
   }


void Desktopwidget::slotUpdateDone ()
   {
   QString str;

   emit updateDone ();

//   printf ("update done\n");
   // start a pending search if there is one
   _updating = false;
   if (_pendingMatch.length ())
      {
      str = _pendingMatch;
      _pendingMatch = QString::null;
      matchUpdate (str, false);
      }
   }


void Desktopwidget::openStack (const QModelIndex &index)
   {
   emit showPage (index);
   }


void Desktopwidget::updatePreview (void)
   {
   QModelIndex index = _update_index;

//    _page->showPage (_update_index.model (), index);
   _page->showPages (_update_index.model (), index, 0, -1, -1);
   }


void Desktopwidget::slotPopupMenu (QModelIndex &index)
   {
   _view->setContextIndex (index);
//    _contents->slotNewContextEvent (index);
   QMenu *context_menu = new QMenu (this);
/*   QLabel *caption = new QLabel( "<font color=darkblue><u><b>"
       "Stack</b></u></font>", context_menu);
   caption->setAlignment( Qt::AlignCenter );*/
//s   contextMenu->insertItem( caption );

   // get ready to call isSelection()
   _view->getSelectionSummary ();

   bool at_least_one = _view->isSelection (Desktopview::SEL_at_least_one);
   context_menu->addAction (_act_locate);
   _act_locate->setEnabled (_contents->searchedSubdirs ());

   context_menu->addAction (_act_stack);
   _act_stack->setEnabled (_view->isSelection (Desktopview::SEL_more_than_one));

   context_menu->addAction (_act_unstack_page);
   _act_unstack_page->setEnabled (_view->isSelection (Desktopview::SEL_one_multipage));

   context_menu->addAction (_act_unstack_all);
   _act_unstack_all->setEnabled (_view->isSelection (Desktopview::SEL_at_least_one_multipage));

   context_menu->addAction (_act_duplicate);
   _act_duplicate->setEnabled (at_least_one);

   context_menu->addAction (_act_delete);
   _act_delete->setEnabled (at_least_one);

   context_menu->addAction (_act_rename_stack);
   _act_rename_stack->setEnabled (at_least_one);

   context_menu->addAction (_act_rename_page);
   _act_rename_page->setEnabled (_view->isSelection (Desktopview::SEL_one_multipage));

   QMenu *submenu = context_menu->addMenu (tr ("&Duplicate..."));
   submenu->addAction (_act_duplicate_page);
   _act_duplicate_page->setEnabled (at_least_one);

   submenu->addAction (_act_duplicate_max);
   _act_duplicate_max->setEnabled (at_least_one);

   submenu->addAction (_act_duplicate_pdf);
   _act_duplicate_pdf->setEnabled (at_least_one);

   submenu->addAction (_act_duplicate_even);
   _act_duplicate_even->setEnabled (at_least_one);

   submenu->addAction (_act_duplicate_odd);
   _act_duplicate_odd->setEnabled (at_least_one);

   submenu->addAction (_act_duplicate_jpeg);
   _act_duplicate_jpeg->setEnabled (at_least_one);

//  submenu->insertItem( "as &Tiff", this, SLOT(duplicateTiff()), Qt::CTRL+Qt::Key_T );
   
   submenu = context_menu->addMenu (tr ("&Email..."));
   submenu->addAction (_act_email);
   _act_email->setEnabled (at_least_one);
   submenu->addAction (_act_email_max);
   _act_email_max->setEnabled (at_least_one);
   submenu->addAction (_act_email_pdf);
   _act_email_pdf->setEnabled (at_least_one);

   submenu = context_menu->addMenu (tr ("&Send..."));
   submenu->addAction (_act_send);
   _act_send->setEnabled (at_least_one);

   submenu->addAction (_act_deliver_out);
   _act_deliver_out->setEnabled (true);

   context_menu->exec (QCursor::pos());
   delete context_menu;
   _view->setContextIndex (QModelIndex ());
   }


void Desktopwidget::pageLeft (const QModelIndex &index)
   {
   QAbstractItemModel *model = (QAbstractItemModel *)index.model ();
   int pagenum = model->data (index, Desktopmodel::Role_pagenum).toInt ();
   QVariant v = pagenum - 1;

   model->setData (index, v, Desktopmodel::Role_pagenum);
   }


void Desktopwidget::pageRight (const QModelIndex &index)
   {
   QAbstractItemModel *model = (QAbstractItemModel *)index.model ();
   int pagenum = model->data (index, Desktopmodel::Role_pagenum).toInt ();
   QVariant v = pagenum + 1;

   model->setData (index, v, Desktopmodel::Role_pagenum);
   }


void Desktopwidget::slotItemClicked (const QModelIndex &index, int which)
   {
   QAbstractItemModel *model = (QAbstractItemModel *)index.model ();
   bool changed = false;

   if (index != QModelIndex ())
      {
      switch (which)
         {
         case Desktopdelegate::Point_left : // left
            pageLeft (index);
            changed = true;
            break;

         case Desktopdelegate::Point_right : // right
            pageRight (index);
            changed = true;
            break;

         case Desktopdelegate::Point_page : // page button
            {
            int pagenum = model->data (index, Desktopmodel::Role_pagenum).toInt ();
            int pagecount = model->data (index, Desktopmodel::Role_pagecount).toInt ();
            int num;
            bool ok;

            num = QInputDialog::getInteger (this, "Select page number", "Page",
                       pagenum + 1, 1, pagecount, 1, &ok);
            if (ok)
               {
               QVariant v = num - 1;

               model->setData (index, v, Desktopmodel::Role_pagenum);
               changed = true;
               }
            break;
            }

         default :
            break;
         }

      if (changed)
         {
         QString str = index.model ()->data (index, Desktopmodel::Role_message).toString ();
         emit newContents (str);
         }
      }
   }


void Desktopwidget::slotItemPreview (const QModelIndex &index, int, bool now)
   {
   if (index != QModelIndex ())
      {
      // preview now if requested
      if (now)
         _page->showPages (index.model (), index, 0, -1, -1);
//          _page->showPage (index.model (), index, false);
      else

      // otherwise give the user time to click again
         {
         _update_index = index;
         _timer->start (300);
         }
      }
   }


/************************** action slots *****************************/

void Desktopwidget::complete (QModelIndex parent, err_info *err)
   {
   // need to work out first item added and number added
   int start, count;

   // select the newly created items
   if (_contents->itemsAdded (parent, start, count))
      _view->setSelectionRange (start, count);
   err_complain (err);
   }


void Desktopwidget::duplicate (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   complete (parent, _contents->duplicate (slist, parent));
   }


void Desktopwidget::duplicateMax (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   _contents->duplicateMax (slist, parent);
   complete (parent, NULL);
   }


void Desktopwidget::duplicatePdf (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   _contents->duplicatePdf (slist, parent);
   complete (parent, NULL);
   }


void Desktopwidget::duplicateJpeg (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   _contents->duplicateJpeg (slist, parent);
   complete (parent, NULL);
   }


void Desktopwidget::duplicateEven (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   _contents->duplicateMax (slist, parent, 2);
   complete (parent, NULL);
   }


void Desktopwidget::duplicateOdd (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   _contents->duplicateMax (slist, parent, 1);
   complete (parent, NULL);
   }

   
void Desktopwidget::send (void)
{
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();

   // bring up a dialogue allowing user to enter information
   Senddialog send (this);

   if (!err_complain (send.setup (_contents, parent, slist)))
      {
      if (send.exec () == QDialog::Accepted)
         {
         // send it
         err_complain (send.doSend ());
         }
      }
}


void Desktopwidget::deliverOut (void)
   {
   qDebug () << "deliverOut";
   }


void Desktopwidget::email (void)
{
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();
   
   err_complain (_contents->opEmailFiles (parent, slist, File::Type_other));
}


void Desktopwidget::emailMax (void)
{
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();
   
   err_complain (_contents->opEmailFiles (parent, slist, File::Type_max));
}


void Desktopwidget::emailPdf (void)
{
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList slist = _view->getSelectedListSource ();
   
   err_complain (_contents->opEmailFiles (parent, slist, File::Type_pdf));
}


void Desktopwidget::locateFolder ()
   {
   QModelIndex ind = _view->getSelectedItem ();
   QString filename, pathname;

   if (ind.isValid ())
      {
      filename = ind.model ()->data (ind, Desktopmodel::Role_filename).toString ();
      pathname = ind.model ()->data (ind, Desktopmodel::Role_pathname).toString ();
      QString dir = pathname;
      dir.truncate (dir.length () - filename.length ());

      // once the folder has finished refreshing, we want to ensure that this item is visible
      _scroll_to = filename;

      QModelIndex dirindex = _model->index (dir);
      selectDir (dirindex);
      }
   }


void Desktopwidget::deleteStacks (void)
   {
   QModelIndexList list = _view->getSelectedListSource ();
   int ok;

   ok = QMessageBox::question(
            this,
            tr("Confirmation -- maxview"),
            tr("Do you want to delete %n stack(s)?", "", list.size ()),
            QMessageBox::Ok, QMessageBox::Cancel);
   if ( ok == QMessageBox::Ok)
      _contents->trashStacks (list, _view->rootIndexSource ());
   }


void Desktopwidget::unstackStacks (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndexList list = _view->getSelectedListSource ();
   int ok = QMessageBox::Ok;
   int start, count;
   err_info *err;

   if (list.size () > 1)
      ok = QMessageBox::question(
            this,
            tr("Confirmation -- maxview"),
            tr("Do you want to unstack %n stack(s)?", "", list.size ()),
            QMessageBox::Ok, QMessageBox::Cancel);
   if (ok == QMessageBox::Ok)
      {
      err = _contents->unstackStacks (list, parent);

      // select the newly created items
      if (_contents->itemsAdded (parent, start, count))
         _view->setSelectionRange (start, count);
      err_complain (err);
      }
   }


void Desktopwidget::unstackPage (void)
   {
   QModelIndex parent = _view->rootIndexSource ();
   QModelIndex index = _view->getSelectedItem (true);
   int start, count;

   Q_ASSERT (parent == index.parent ());
   _contents->unstackPage (index, -1, true);
   if (_contents->itemsAdded (parent, start, count))
      _view->setSelectionRange (start, count);
   }


void Desktopwidget::duplicatePage (void)
   {
   QModelIndex index = _view->getSelectedItem (true);

   _contents->unstackPage (index, -1, false);
   }


void Desktopwidget::stackPages (void)
   {
   QModelIndexList list = _view->getSelectedListSource ();
   QModelIndex dest;

   // pick the first item as the destination
   if (list.size () >= 2)
      {
      dest = list [0];
      list.removeAt (0);
      err_complain (_contents->stackItems (dest, list, 0));
      }
   }


void Desktopwidget::renameStack (void)
   {
   QModelIndex index = _view->getSelectedItem ();

   _view->setCurrentIndex (index);
   _view->renameStack (index);
   }


void Desktopwidget::renamePage (void)
   {
   QModelIndex index = _view->getSelectedItem ();

   _view->setCurrentIndex (index);
   _view->renamePage (index);
   }


void Desktopwidget::stackLeft (void)
   {
   QModelIndex ind = _view->getSelectedItem ();

   if (!ind.isValid ())
      return;

   QModelIndex parent = ind.parent ();
   int count = ind.model ()->rowCount (parent);
   int row = ind.row ();

   if (row > 0)
      row--;
   else if (count)
      row = count - 1;
   if (count)
      {
      _view->setSelectionRange (row, 1);
      _view->scrollTo (ind.model ()->index (row, 0, parent));
      }
   }


void Desktopwidget::stackRight (void)
   {
   QModelIndex ind = _view->getSelectedItem ();

   if (!ind.isValid ())
      return;

   QModelIndex parent = ind.parent ();
   int count = ind.model ()->rowCount (parent);
   int row = ind.row ();

   if (row < count - 1)
      row++;
   else if (count)
      row = 0;
   if (count)
      {
      _view->setSelectionRange (row, 1);
      _view->scrollTo (ind.model ()->index (row, 0, parent));
      }
   }


void Desktopwidget::pageLeft (void)
   {
   QModelIndex ind = _view->getSelectedItem ();

   if (ind.isValid ())
      {
      pageLeft (ind);
      _view->scrollTo (ind);
      }
   }


void Desktopwidget::pageRight (void)
   {
   QModelIndex ind = _view->getSelectedItem ();

   if (ind.isValid ())
      {
      pageRight (ind);
      _view->scrollTo (ind);
      }
   }

void Desktopwidget::activateFind ()
   {
   _match->clear ();
   _match->setFocus (Qt::OtherFocusReason);
   _global->setChecked (true);
   }

#if 0
bool Desktopwidget::eventFilter (QObject *watched_object, QEvent *e)
   {
   bool filtered = false;

   if (e->type () == QEvent::KeyPress)
      {
      QKeyEvent* k = (QKeyEvent*) e;

      if (k->key () == Qt::Key_Escape)
         {
         qDebug() << "here";
         filtered = true; //eat event
         }
      }
  return filtered;
  }
#endif
