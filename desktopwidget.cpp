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
#include <QCheckBox>
#include <QFileDialog>
#include <QKeyEventTransition>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QItemSelectionModel>
#include <QToolBar>
#include <QToolButton>

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
#include "mainwindow.h"
#include "maxview.h"
#include "pagewidget.h"
#include "senddialog.h"
#include "utils.h"
#include "ui_search.h"

Desktopwidget::Desktopwidget (QWidget *parent)
      : QSplitter (parent)
   {
   _model = new Dirmodel ();
//    _model->setLazyChildCount (true);
   _dir = new Dirview (this);

   _dir_proxy = new Dirproxy();
   _dir_proxy->setSourceModel(_model);
   _dir->setModel(_dir_proxy);

   _contents = new Desktopmodel (this);

   _toolbar = new Toolbar();

   connect(_toolbar->pPrev, SIGNAL(clicked()), this, SLOT(pageLeft()));
   connect(_toolbar->pNext, SIGNAL(clicked()), this, SLOT(pageRight()));
   connect(_toolbar->prev, SIGNAL(clicked()), this, SLOT(stackLeft()));
   connect(_toolbar->next, SIGNAL(clicked()), this, SLOT(stackRight()));

   connect(_toolbar->pressed_esc, SIGNAL(triggered()),
           this, SLOT(resetFilter()));
   connect(_toolbar->cancelFilter, SIGNAL(clicked()),
           this, SLOT(resetFilter()));

   connect(_toolbar->match, SIGNAL(textChanged(const QString&)),
           this, SLOT(matchChange(const QString &)));

   QWidget *group = new QWidget(this);

   _view = new Desktopview (group);
   QVBoxLayout *lay = new QVBoxLayout (group);
   lay->setContentsMargins (0, 0, 0, 0);
   lay->setSpacing (2);
   lay->addWidget (_toolbar);
   lay->addWidget (_view);

   connect (_view, SIGNAL (itemPreview (const QModelIndex &, int, bool)),
         this, SLOT (slotItemPreview (const QModelIndex &, int, bool)));
   connect(_view, SIGNAL(escapePressed()), this, SLOT(exitSearch()));

   _contents_proxy = new Desktopproxy (this);
   _contents_proxy->setSourceModel (_contents);
   _view->setModel (_contents_proxy);
//    printf ("contents=%p, proxy=%p\n", _contents, _proxy);

   // set up the model converter
   _modelconv = new Desktopmodelconv (_contents, _contents_proxy);

   // setup another one for Desktopmodel, which only allows assertions
   _modelconv_assert = new Desktopmodelconv (_contents, _contents_proxy, false);

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

   _main = static_cast<Mainwidget *>(parent);

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
   connect (_dir->_search, SIGNAL(triggered()), this, SLOT(searchInFolders()));
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
   connect(_dir->_refresh_cache, SIGNAL(triggered ()), this,
           SLOT(slotRefreshCache()));

   connect(_toolbar->exitSearch, SIGNAL(clicked()), this, SLOT(exitSearch()));

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
   addAction (_act_locate, "&Locate folder",  SLOT(locateFolder ()), "Ctrl+Shift+L");
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
//   addAction (_act_send, "&Send stacks", SLOT (send ()), "Ctrl+S");
//   addAction (_act_deliver_out, "&Delivery outgoing", SLOT (deliverOut ()), "");
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
      QModelIndex src_ind = _model->index(dirname);
      index = _dir_proxy->mapFromSource(src_ind);
      selectDir(index);
      }
   else
      err = err_make (ERRFN, ERR_directory_could_not_be_added1,
                       qPrintable(in_dirname));
   return err;
   }


void Desktopwidget::selectDir(const QModelIndex &target, bool forceChange)
   {
//    int count = _model->rowCount (QModelIndex ());

   /* use the second directory if there is nothing supplied, since the first
      is 'Recent items' */
   QModelIndex ind = target;

   if (ind == QModelIndex())
      ind = _dir_proxy->index(1, 0, QModelIndex());

   //QModelIndex src_ind = _dir_proxy->mapToSource(ind);
   //qDebug () << "Desktopwidget::selectDir" << _model->data(src_ind, Dirmodel::FilePathRole).toString ();
   _dir->setCurrentIndex(ind);
   _dir->setExpanded(ind, true);
    dirSelected(ind, false, forceChange);
   }


void Desktopwidget::slotDroppedOnFolder(const QMimeData *data, QString &dir)
   {
   QByteArray encodedData = data->data("application/vnd.text.list");
   QDataStream stream(&encodedData, QIODevice::ReadOnly);
   QStringList newItems;

   while (!stream.atEnd()) {
      QString text;
      stream >> text;
      newItems << text;
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
   QModelIndex src_ind = _dir_proxy->mapToSource(index);
   if (_model->findIndex(src_ind) != -1)
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

      QModelIndex src_parent = _model->parent(src_ind);

//       if (!_model->setData (index, QVariant (text)))
      path.truncate (path.length () - oldName.length () - 1);
      fullPath = path + "/" + text;
      if (dir.rename (path + "/" + oldName, fullPath))
         _model->refresh(src_parent);
//          _dir->refreshItemRename (text);  // indicates current item has new children
      else
         QMessageBox::warning (0, "Maxview", "Could not rename directory");
      }
   }


void Desktopwidget::refreshDir()
   {
   QModelIndex index = _dir->menuGetModelIndex ();
   QModelIndex src_ind = _dir_proxy->mapToSource(index);

   // update the model with this new directory
   _model->refresh(src_ind);

   /* Now refresh the Desktopview  */
   QModelIndex sind = _contents->refresh(_path);

   QModelIndex ind = sind;
   _modelconv->indexToProxy(ind.model (), ind);
   _view->setRootIndex(ind);
   }


void Desktopwidget::addToRecent ()
   {
   QModelIndex index = _dir->menuGetModelIndex ();
   QModelIndex src_ind = _dir_proxy->mapToSource(index);

   // update the model with this new directory
   _model->addToRecent(src_ind);
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

void Desktopwidget::slotRefreshCache()
{
   QModelIndex index = _dir->menuGetModelIndex();
   QModelIndex src_ind = _dir_proxy->mapToSource(index);
   QModelIndex root = _model->findRoot (src_ind);

   Operation op ("Refreshing cache", 0, this);
   _model->refreshCache(root, &op);
}

void Desktopwidget::deleteDir ()
   {
   QMessageBox::warning (0, "Paperman",
                         "Please use the file manager to delete files, then "
                         "use the refresh option here");
   return;

   QString path = _dir->menuGetPath ();
   QString fullPath;
   int ok;
   QString oldName = _dir->menuGetName ();
   QModelIndex index = _dir->menuGetModelIndex ();
   QModelIndex src_ind = _dir_proxy->mapToSource(index);

   // find out how many files are in the directory
   QString str = _model->countFiles(src_ind, 10);

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

      qDebug () << "remove dir" << _model->filePath(src_ind);
      err = _model->rmdir(src_ind);
      if (err)
          QMessageBox::warning (0, "Maxview", err->errstr);
      }
   }

QStringList Desktopwidget::findFolders(const QString& text, QString& dirPath,
                                       QStringList& missing)
{
   dirPath = getRootDirectory();
   if (dirPath.isEmpty())
      return QStringList();
   QModelIndex root = getRootIndex();

   Operation op ("Scanning folders", 0, this);
   return _model->findFolders(text, dirPath, root, missing, &op);
}

void Desktopwidget::startSearch(const QString& path, const QString& match)
{
   // Create a 'virtual' maxdesk which holds files from a number different dirs
   _contents_proxy->setFilterFixedString ("");

   QModelIndex root = getRootIndex();
   QString root_path = _model->data(root, QDirModel::FilePathRole).toString ();

   Operation op("Scanning folders", 0, this);
   QStringList matches;
   matches = _model->findFiles(match, path, root, &op);

   QModelIndex sind = _contents->finishFileSearch(path, root_path, matches);

   //delete op;
   QModelIndex ind = sind;
   _modelconv->indexToProxy(ind.model (), ind);
   _view->setRootIndex(ind);

   // Set the focus so that Escape works
   _view->setFocus();

   _view->scrollToTop();
}

void Desktopwidget::searchInFolders()
{
   Ui::Search ui;
   QDialog diag;

   _toolbar->setFilterEnabled(false);
   ui.setupUi(&diag);
   ui.stackName->setText(_search_text);
   ui.stackName->setSelection(0, _search_text.size());

   QModelIndex index = _dir->menuGetModelIndex ();
   QModelIndex src_ind = _dir_proxy->mapToSource(index);
   QString path = _model->filePath(src_ind);

   ui.folderPath->setText(path);
   diag.show();
   if (!diag.exec()) {
      _toolbar->setFilterEnabled(true);
      return;
   }

   _search_text = ui.stackName->text();
   startSearch(path, _search_text);
   _toolbar->setSearchEnabled(true);
   _view->setStyleSheet("QListView { background: lightblue; }");
   _dir->setEnabled(false);
   _main->getMainwindow()->setSearchEnabled(false);
}

void Desktopwidget::normalView()
{
   _view->setStyleSheet("QListView { background: lightgray; }");
   _dir->setEnabled(true);
   _toolbar->setFilterEnabled(true);
   _toolbar->setSearchEnabled(false);
   _main->getMainwindow()->setSearchEnabled(true);
}

void Desktopwidget::exitSearch()
{
   /* This can be called for the Escape key even if there is no search active */
   if (!_toolbar->searchEnabled())
      return;

   normalView();
   QModelIndex index = _model->index (_path);
   _contents_proxy->setFilterFixedString ("");
   QModelIndex root = _model->findRoot (index);
   QString root_path = _model->data (root, QDirModel::FilePathRole).toString ();
   QModelIndex sind = _contents->showDir(_path, root_path);
   QModelIndex ind = sind;
   _modelconv->indexToProxy (ind.model (), ind);
   _view->setRootIndex (ind);
}

void Desktopwidget::newDir ()
   {
   QString path = _dir->menuGetPath ();
   QString fullPath;
   bool ok;

   QString text = QInputDialog::getText(
            this, "Maxview", "Enter new subdirectory name:", QLineEdit::Normal,
            QString(), &ok);
   if ( ok && !text.isEmpty() )
      {
      QModelIndex index = _dir->menuGetModelIndex ();
      QModelIndex src_ind = _dir_proxy->mapToSource(index);

//       printf ("mkdir  %s\n", _model->filePath (index).latin1 ());
      QModelIndex new_ind = _model->mkdir(src_ind, text);
//       printf ("   - got '%s'\n", _model->filePath (index).latin1 ());
      if (new_ind == QModelIndex())
         QMessageBox::warning(0, "Maxview", "Could not make directory " +
            _model->data(src_ind, QDirModel::FilePathRole).toString() + "/" +
                         text);
      }
   }

QModelIndex Desktopwidget::findDir(const QString& dir_path)
{
   QModelIndex src_ind = _model->index(dir_path);
   QModelIndex ind = _dir_proxy->mapFromSource(src_ind);

   return ind;
}

bool Desktopwidget::newDir(const QString& dir_path, QModelIndex& index)
{
   QDir parent(dir_path);
   QString dirname = parent.dirName();

   if (!parent.cdUp()) {
      // This really cannot happen
      QMessageBox::warning(0, "Paperman", "Directory does not exist: " +
                           parent.path());
      return false;
   }

   qDebug() << "to_create" << dir_path;
   QModelIndex parent_ind = _model->index(parent.path(), 0);
   QModelIndex src_ind = _model->mkdir(parent_ind, dirname);
   if (src_ind == QModelIndex()) {
      QMessageBox::warning(0, "Maxview", "Could not make directory " +
                           parent.path() + "/" + dirname);
      return false;
   }
   index = _dir_proxy->mapFromSource(src_ind);

   // Select the directory, since Dirview::menuGetModelIndex() becomes invalid
   // when something is added to the proxy model
   selectDir(index);

   return true;
}

void Desktopwidget::dirSelected(const QModelIndex &index, bool allow_undo,
                                bool force_change)
   {
   QModelIndex src_ind = _dir_proxy->mapToSource(index);
   QString path = _model->data(src_ind, QDirModel::FilePathRole).toString();
   QModelIndex root = _model->findRoot(src_ind);
   QString root_path = _model->data(root, QDirModel::FilePathRole).toString();

   //qDebug() << "dirSelected" << path << _contents->getDirPath();

   // clear the page preview
   _page->slotReset ();

   // if we have are actually changing directory, do so
   if (force_change || path != _contents->getDirPath ())
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
   QModelIndex src_ind = _model->index (dirPath);
   QModelIndex index = _dir_proxy->mapFromSource(src_ind);

//    qDebug () << "Desktopwidget::slotDirChanged" << _dir->currentIndex () << index;
   _dir->setCurrentIndex(index);

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

void Desktopwidget::resetFilter()
{
   _toolbar->match->clear();
   _toolbar->match->setFocus();
}

void Desktopwidget::matchChange(const QString& match)
{
   if (!_contents_proxy)
      return;

   QModelIndex ind;

   // update the proxy
   // qDebug () << "match" << match;
   _contents_proxy->setFilterFixedString (match);

   // scroll to the first match
   ind = _contents_proxy->index(0, 0, _view->rootIndex ());
   if (ind != QModelIndex ())
      _view->scrollTo (ind);
}

void Desktopwidget::slotUpdateDone ()
   {
   emit updateDone ();
   }


void Desktopwidget::openStack (const QModelIndex &index)
   {
   emit showPage (index);
   }


void Desktopwidget::updatePreview (void)
   {
   QModelIndex index = _update_index;

//    _page->showPage (_update_index.model (), index);
   if (index != QModelIndex())
      _page->showPages (_update_index.model(), index, 0, -1, -1);
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
/* to be implemented
   submenu = context_menu->addMenu (tr ("&Send..."));
   submenu->addAction (_act_send);
   _act_send->setEnabled (at_least_one);

   submenu->addAction (_act_deliver_out);
   _act_deliver_out->setEnabled (true);
*/
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

            num = QInputDialog::getInt (this, "Select page number", "Page",
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

      QModelIndex src_ind = _model->index (dir);
      QModelIndex ind = _dir_proxy->mapFromSource(src_ind);

      normalView();

      selectDir(ind, true);
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

void Desktopwidget::activateSearch()
{
   if (!_toolbar->searchEnabled())
      searchInFolders();
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

const QString Desktopwidget::getRootDirectory()
{
   // Get the top-level dirname of the the root
   QModelIndex root = getRootIndex();
   if (root == QModelIndex())
      return "";
   QString root_path = _model->data(root, QDirModel::FilePathRole).toString();

   return root_path;
}

const QModelIndex Desktopwidget::getRootIndex()
{
   // Find out which directory is currently selected
   QModelIndex ind = _dir->menuGetModelIndex();

   if (ind == QModelIndex())
      return QModelIndex();

   QModelIndex src_ind = _dir_proxy->mapToSource(ind);

   // Get the top-level dirname of that
   QModelIndex root = _model->findRoot(src_ind);

   return root;
}

TreeItem *Desktopwidget::ensureCache()
{
   // Find out which directory is currently selected
   QModelIndex ind = _dir->menuGetModelIndex();

   if (ind == QModelIndex())
      return nullptr;

   QModelIndex src_ind = _dir_proxy->mapToSource(ind);

   // Get the top-level dirname of that
   QModelIndex root = _model->findRoot(src_ind);

   Operation op("Scanning folders", 0, this);
   return _model->ensureCache(root, &op);
}

QModelIndex Desktopwidget::getDirIndex(const QString dirname)
{
   QModelIndex src_ind = _model->index(dirname);
   QModelIndex ind = _dir_proxy->mapFromSource(src_ind);

   return ind;
}

void Desktopwidget::setDirFilter(bool active)
{
   _dir_proxy->setActive(active);
}

Toolbar::Toolbar(QWidget* parent, Qt::WindowFlags fl)
   : QFrame(parent, fl)
{
   setupUi(this);

   // When ESC is pressed, clear the field
   QStateMachine *machine = new QStateMachine(this);
   QState *s1 = new QState(machine);

   // We seem to get the key release for Esc but never the key press. This may
   // be due to a dialog consuming it
   pressed_esc = new QKeyEventTransition(match, QEvent::KeyRelease,
                                         Qt::Key_Escape);
   s1->addTransition(pressed_esc);
   machine->setInitialState(s1);
   machine->start();

   searching->hide();
}

Toolbar::~Toolbar()
{
}

void Toolbar::setSearchEnabled(bool enable)
{
   searching->setVisible(enable);
}

bool Toolbar::searchEnabled()
{
   return searching->isVisible();
}

void Toolbar::setFilterEnabled(bool enable)
{
   cancelFilter->setEnabled(enable);
   match->setEnabled(enable);
}
