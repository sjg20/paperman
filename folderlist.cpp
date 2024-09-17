/* Implementation of a list of folders */

#include <QHeaderView>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStandardItemModel>
#include "folderlist.h"
#include "foldersel.h"

Folderlist::Folderlist(QWidget *parent)
    : QTableView(parent)
{
}

Folderlist::~Folderlist()
{
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

Folderlist *setupFolderList(QWidget *parent)
{
   QStandardItemModel *model;
   Folderlist *folders;

   folders = new Folderlist(parent);
   model = new QStandardItemModel(1, 1, parent);
   folders->setModel(model);
   folders->horizontalHeader()->hide();
   folders->verticalHeader()->hide();

   // Set the default row height to 0 so that it will be as small as possible
   // while still making the text visible
   folders->verticalHeader()->setDefaultSectionSize(0);
   folders->setShowGrid(false);

   folders->setSelectionBehavior(QTableView::SelectRows);
   folders->setSelectionMode(QTableView::SingleSelection);
   folders->setEditTriggers(QTableView::NoEditTriggers);

   folders->hide();

   return folders;
}
