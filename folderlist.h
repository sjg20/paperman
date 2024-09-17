#ifndef FOLDERLIST_H
#define FOLDERLIST_H

#include <QTableView>

class QStandardItemModel;
class Foldersel;

class Folderlist : public QTableView
{
   Q_OBJECT

public:
   Folderlist(Foldersel *foldersel, QWidget *parent);
   ~Folderlist();

   // Return the currently selected item, or -1 if none
   QModelIndex selected();

   /** Show the folders list, sizing it correctly */
   void showFolders();

   // possible missing directories shown to the user
   QStringList _missing;

   // directory path for the folder list
   QString _path;

   // true if the folders list has been set up
   bool _valid;

public slots:
   void keypressFromFolderList(QKeyEvent *evt);

signals:
   void keypressReceived(QKeyEvent *event);
   void selectItem(const QModelIndex&);

protected:
   virtual void keyPressEvent(QKeyEvent *event) override;
   virtual void mousePressEvent(QMouseEvent *e) override;

   Foldersel *_foldersel;
   QWidget *_parent;
   QStandardItemModel *_model;
};

#endif // FOLDERLIST_H
