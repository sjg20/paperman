#ifndef FOLDERLIST_H
#define FOLDERLIST_H

#include <QTableView>

class QStandardItemModel;
class Foldersel;
class Mainwidget;

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

   void setMainwidget(Mainwidget *main);

   /** Search for folders which match a string */
   void searchForFolders(const QString& match);

public slots:
   void keypressFromFolderList(QKeyEvent *evt);

signals:
   void keypressReceived(QKeyEvent *event);
   void selectItem(const QModelIndex&);

protected:
   virtual void keyPressEvent(QKeyEvent *event) override;
   virtual void mousePressEvent(QMouseEvent *e) override;

   Foldersel *_foldersel;
   QStandardItemModel *_model;
   Mainwidget *_main;
   QWidget *_parent;
};

#endif // FOLDERLIST_H
