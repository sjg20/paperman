/* Implementation of a list of folders */

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTimer>
#include "folderlist.h"
#include "foldersel.h"
#include "mainwidget.h"

Folderlist::Folderlist(Foldersel *foldersel, QWidget *parent)
    : QTableView(parent), _foldersel(foldersel),
    _parent(parent)
{
   _parent = parent;
   _main = nullptr;   // not known yet
   _model = new QStandardItemModel(1, 1, parent);
   setModel(_model);
   _valid = false;
   _awaiting_user = false;
   _searching = false;

   horizontalHeader()->hide();
   verticalHeader()->hide();

   // Set the default row height to 0 so that it will be as small as possible
   // while still making the text visible
   verticalHeader()->setDefaultSectionSize(0);
   setShowGrid(false);

   setSelectionBehavior(QTableView::SelectRows);
   setSelectionMode(QTableView::SingleSelection);
   setEditTriggers(QTableView::NoEditTriggers);

   hide();

   connect(this, SIGNAL(keypressReceived(QKeyEvent *)),
           this, SLOT(keypressFromFolderList(QKeyEvent *)));
   connect(this, SIGNAL(selectItem(const QModelIndex&)),
           this, SLOT(selectDir(const QModelIndex&)));
   connect(_foldersel, SIGNAL(textChanged(const QString&)),
           this, SLOT(foldersel_textChanged(const QString&)));

   _timer = new QTimer (this);
   connect (_timer, SIGNAL(timeout()), this, SLOT(checkFolders()));
   _timer->start(200); // check 5 times a second
}

Folderlist::~Folderlist()
{
}

void Folderlist::setMainwidget(Mainwidget *main)
{
   _main = main;
}

void Folderlist::keypressFromFolderList(QKeyEvent *evt)
{
   QApplication::postEvent(_foldersel, evt);
}

void Folderlist::keyPressEvent(QKeyEvent *old)
{
   bool send = true;     // send to folderName field
   bool pass_on = old->key() != Qt::Key_Tab;  // pass on to QTableView

   if (send) {
      QKeyEvent *evt = new QKeyEvent(old->type(), old->key(), old->modifiers(),
                                     old->text(), old->isAutoRepeat(),
                                     old->count());

      // Send the keypress to the folderName field
      emit keypressReceived(evt);
   }

   if (pass_on)
      QTableView::keyPressEvent(old);
}

void Folderlist::mousePressEvent(QMouseEvent *evt)
{
   QTableView::mousePressEvent(evt);

   // show the folder
   QPoint point = evt->pos() + QPoint(horizontalScrollBar()->value(),
                                      verticalScrollBar()->value());
   QModelIndex ind = indexAt(point);
   if (ind != QModelIndex())
      emit selectItem(ind);
}

QModelIndex Folderlist::selected()
{
   QModelIndexList sel = selectedIndexes();

   if (sel.size())
      return sel[0];

   return QModelIndex();
}

void Folderlist::showFolders()
{
   // Get a rectangle the same size as folderName but starting below it
   QRect rect = _foldersel->geometry();
   rect.translate(QPoint(0, rect.height()));

   // extend it to the bottom of the dialog
   rect.setBottom(_parent->geometry().bottom());

   // place the folders list in the right place
   move(rect.topLeft());
   resize(rect.size());
   horizontalHeader()->resizeSection(0, rect.width());
   show();
}

void Folderlist::searchForFolders(const QString& match)
{
   QStringList folders;
   bool valid = false;
   int row;

   // We want at least three characters for a match
   if (match.length() >= 3) {
      folders = _main->findFolders(match, _path, _missing);
      if (_path.isEmpty())
         return;
      valid = true;
   }

   // put the folder list into the model
   _model->removeRows(0, _model->rowCount(QModelIndex()), QModelIndex());

   for (row = 0; row < _missing.size(); row++) {
      _model->insertRows(row, 1, QModelIndex());
      _model->setData(_model->index(row, 0, QModelIndex()),
                      QString("<Add: %1>").arg(_missing[row]));
   }
   for (int i = 0; i < folders.size(); i++) {
      _model->insertRows(row + i, 1, QModelIndex());
      _model->setData(_model->index(row + i, 0, QModelIndex()),
                      folders[i]);
   }

   if (folders.size()) {
      selectRow(0);
   } else {
      _model->insertRows(0, 1, QModelIndex());
      _model->setData(_model->index(0, 0, QModelIndex()),
                      valid ? "<no match>" : "<too short>");
   }

   // make sure the column is wide enough
   resizeColumnToContents(0);

   showFolders();

   if (folders.size())
      setFocus();

   _valid = folders.size() > 0;
}

void Folderlist::checkFolders()
{
   // close the folder list if the focus is in anything other than the two
   // fields dealing with the list of folders
   if (!_awaiting_user && QApplication::focusWidget() != _foldersel &&
       QApplication::focusWidget() != this && isVisible()) {
      hide();
      if (_parent->focusWidget() == this)
         _foldersel->setFocus(Qt::OtherFocusReason);
   }

   // show the folder list if focus is in folderName
   if (_valid && QApplication::focusWidget() == _foldersel &&
       !_main->isScanning()) {
      showFolders();
      setFocus(Qt::OtherFocusReason);
   }
}

bool Folderlist::createMissingDir(int item, QModelIndex& ind)
{
   const QString& fname = _missing[item];
   bool ok;

   // Adding a directory
   _awaiting_user = true;
   ok = QMessageBox::question(
            0,
            tr("Confirmation -- Paperman"),
            tr("Do you want to add directory %1").arg(fname),
            QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
   _awaiting_user = false;
   if (!ok)
      return false;

   _main->newDir(_path + "/" + fname, ind);

   // regenerate the folder list
   _searching = true;
   searchForFolders(_foldersel->text());
   _searching = false;

   return true;
}

void Folderlist::selectDir(const QModelIndex& target)
{
   if (target.row() < _missing.size()) {
      QModelIndex ind;
      QString fname;

      createMissingDir(target.row(), ind);
   } else {
      QString dir_path = _path + "/" + _model->data(target).toString();

      _main->selectDir(dir_path);
   }
}

bool Folderlist::getSelected(QModelIndex& ind, bool hiddenOk)
{
   if (_awaiting_user)
      return false;

   bool ok = true;
   ind = QModelIndex();
   if (_valid && (hiddenOk || isVisible())) {
      QModelIndex sel_ind = selected();

      if (sel_ind != QModelIndex()) {
         if (sel_ind.row() < _missing.size()) {
            ok = createMissingDir(sel_ind.row(), ind);
         } else {
            QString dir_path;

            dir_path = _path + "/" + _model->data(sel_ind).toString();
            ind = _main->getDirIndex(dir_path);
         }
      }
   }

   return ok;
}

void Folderlist::foldersel_textChanged(const QString &text)
{
   if (_searching) {
      _next_search = text;
      return;
   }

   QString match = text;
   do {
      _searching = true;
      searchForFolders(match);
      _searching = false;

      match = _next_search;
      _next_search = QString();
   } while (match.size());
}

void Folderlist::scanStarting()
{
   // close the folders list
   if (!_awaiting_user)
      hide();
}

Foldersel::Foldersel(QWidget* parent)
    : QLineEdit(QString(), parent)
{
}

Foldersel::~Foldersel()
{
}

void Foldersel::focusOutEvent(QFocusEvent *)
{
   // Do nothing here, so that any selected text remains selected
}
