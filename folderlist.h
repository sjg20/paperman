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

   void setMainwidget(Mainwidget *main);

   //! Scan into the selected folder; returns true if scan was handled
   bool scan();

   //! Indicates that a scan is starting
   void scanStarting();

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

    void foldersel_textChanged(const QString &text);

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
     * @param ind    Returns Dirmodel index of the created directory
     * @return true if done (i.e. user confirmed it), false if not
     */
   bool createMissingDir(int item, QModelIndex& ind);

   /** Search for folders which match a string */
   void searchForFolders(const QString& match);

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

   // true if we are currently searching for folders
   bool _searching;

   // string which we saw while in the middle of searching
   QString _next_search;

   // true if waiting for the user to confirm directory creation
   bool _awaiting_user;
};

#endif // FOLDERLIST_H
