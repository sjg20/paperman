#ifndef TEST_DIRVIEW_H
#define TEST_DIRVIEW_H

#include <QObject>
#include <QTemporaryDir>

#include "suite.h"

class Dirmodel;
class Dirproxy;
class Dirview;

class TestDirview: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   //! Check that selecting an item updates the context
   void testSelectContext();

   //! Check that menuGetPath() and menuGetName() return the right values
   void testMenuGetters();

   //! Check that expanding a directory shows children
   void testExpandDir();

   //! Check that selectContextItem() emits clicked()
   void testSelectEmitsClicked();

   //! Refreshing a directory in the view must not lose its subdirectories
   void testRefreshKeepsSubdirsInView();

private:
   /**
    * @brief  Set up a Dirview with a Dirmodel and Dirproxy
    * @param  model    Returns the Dirmodel
    * @param  proxy    Returns the Dirproxy
    * @return Dirview, ready for tests
    *
    * Two separate repositories are created: the first from setupRepo()
    * containing main/ and two from a second temp directory with its own
    * structure.
    */
   Dirview *setupView(Dirmodel *&model, Dirproxy *&proxy);

   QTemporaryDir *_tempDir2;
};

#endif // TEST_DIRVIEW_H
