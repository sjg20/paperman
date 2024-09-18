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

   // true if waiting for the user to confirm directory creation
   bool _awaiting_user;

   // true if we are currently searching for folders
   bool _searching;

   void setMainwidget(Mainwidget *main);

   /** Search for folders which match a string */
   void searchForFolders(const QString& match);

   //! Scan into the selected folder; returns true if scan was handled
   bool scan();

public slots:
   void keypressFromFolderList(QKeyEvent *evt);

    //! hide or show the folders list
    void checkFolders();

protected slots:
    /**
     * @brief Select a directory from the folder list
     * @param target  Item in the folder list to select
     *
     * The selected directory is shown in the Dirview
     */
    void selectDir(const QModelIndex& target);

signals:
   void keypressReceived(QKeyEvent *event);
   void selectItem(const QModelIndex&);

protected:
   virtual void keyPressEvent(QKeyEvent *event) override;
   virtual void mousePressEvent(QMouseEvent *e) override;

   /** Show the folders list, sizing it correctly */
   void showFolders();

   // Return the currently selected item, or -1 if none
   QModelIndex selected();

   /**
     * @brief Create a directory from the _missing list, return true if done
     * @param item   Index within _missing of the directory to create
     * @param fname  Returns filename of dir created
     * @param ind    Returns Dirmodel index of the created directory
     * @return true if done (i.e. user confirmed it), false if not
     */
   bool createMissingDir(int item, QString& fname, QModelIndex& ind);

   Foldersel *_foldersel;
   QStandardItemModel *_model;
   Mainwidget *_main;
   QWidget *_parent;

   // timer for checking whether we should close the folder list
   QTimer *_timer;

   // directory path for the folder list
   QString _path;

   // possible missing directories shown to the user
   QStringList _missing;

   // true if the folders list has been set up
   bool _valid;
};

#endif // FOLDERLIST_H
