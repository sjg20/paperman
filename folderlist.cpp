/* Implementation of a list of folders */

#include <QKeyEvent>
#include <QScrollBar>
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
