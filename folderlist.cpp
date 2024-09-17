/* Implementation of a list of folders */

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStandardItemModel>
#include "folderlist.h"
#include "foldersel.h"

Folderlist::Folderlist(Foldersel *foldersel, QWidget *parent)
    : QTableView(parent), _foldersel(foldersel)
{
   _parent = parent;
   _model = new QStandardItemModel(1, 1, parent);
   setModel(_model);
   _valid = false;

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
}

Folderlist::~Folderlist()
{
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
