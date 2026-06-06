#ifndef TEST_DIRMODEL_H
#define TEST_DIRMODEL_H

class QAbstractItemModel;
class QAbstractProxyModel;

#include <QObject>

#include "suite.h"

class Dirmodel;

class TestDirmodel: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testBase();
   void testProxy();
   void testModel();
   void testAddDir();
   void testAddFiles();

   //! Check that the cache has files in it
   void testCacheFiles();

   //! refresh() must not lose the directory's existing subdirectories
   void testRefreshKeepsSubdirs();

   //! Backend failure during populate is surfaced and can be retried
   void testBackendErrorSurfacing();

   //! addRemoteRepository wires a Dirmodel to a live paperman-server
   void testRemoteRepository();

   //! Expanding a child node (not just the root) fires another /browse
   void testRemoteRepositoryExpandChild();

   //! Same as testRemoteRepositoryExpandChild but accessed through
   //! the Dirproxy the GUI uses, to catch any proxy-caching surprises.
   void testRemoteRepositoryExpandChildThroughProxy();

   //! The FilePathRole exposed for a remote root must match the key
   //! Dirmodel::backendForRoot uses (i.e. Diritem::dir()).  If they
   //! diverge, Desktopmodel::showDir can't find the backend and Desk
   //! falls back to QDir, producing "dir not found" on selection.
   void testRemoteRootBackendLookup();

private:
   /**
    * @brief  Set up a new Dirmodel with test data
    * @param  add_files  true to add some files, false for just directories
    * @return Dirmodel, ready for tests
    */
   Dirmodel *setupModel(bool add_file = false);

   /**
    * @brief Run checks on a Dirmodel
    * @param model     Model to check
    * @param dirmodel  Dirmodel object, if available, else nullptr
    * @param proxy     Dirproxy object, if available, else nullptr
    */
   void checkModel(const QAbstractItemModel *model, const Dirmodel *dirmodel,
                   const QAbstractProxyModel *proxy);

   // Read the .papertree file from a path and return it as a string
   QString getPaperTree(QString &path);
};

#endif // TEST_DIRMODEL_H
