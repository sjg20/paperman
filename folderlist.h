#ifndef FOLDERLIST_H
#define FOLDERLIST_H

#include <QTableView>

class Foldersel;

class Folderlist : public QTableView
{
   Q_OBJECT

public:
   Folderlist(Foldersel *foldersel, QWidget *parent);
   ~Folderlist();

   // Return the currently selected item, or -1 if none
   QModelIndex selected();

public slots:
   void keypressFromFolderList(QKeyEvent *evt);

signals:
   void keypressReceived(QKeyEvent *event);
   void selectItem(const QModelIndex&);

protected:
   virtual void keyPressEvent(QKeyEvent *event) override;
   virtual void mousePressEvent(QMouseEvent *e) override;

   Foldersel *_foldersel;
};

//! Set up a new folderlist
Folderlist *setupFolderList(Foldersel *foldersel, QWidget *parent);

#endif // FOLDERLIST_H
