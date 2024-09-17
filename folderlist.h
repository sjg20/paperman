#ifndef FOLDERLIST_H
#define FOLDERLIST_H

#include <QTableView>

class Folderlist : public QTableView
{
   Q_OBJECT

   public:
   Folderlist(QWidget *parent);
   ~Folderlist();

   // Return the currently selected item, or -1 if none
   QModelIndex selected();

   signals:
   void keypressReceived(QKeyEvent *event);
   void selectItem(const QModelIndex&);

   protected:
   virtual void keyPressEvent(QKeyEvent *event) override;
   virtual void mousePressEvent(QMouseEvent *e) override;
};

//! Set up a new folderlist
Folderlist *setupFolderList(QWidget *parent);

#endif // FOLDERLIST_H
