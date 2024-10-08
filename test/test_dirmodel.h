#ifndef TEST_DIRMODEL_H
#define TEST_DIRMODEL_H

class QAbstractItemModel;

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

private:
   // Set up a new Dirmodel with test data
   Dirmodel *setupModel();

   /**
    * @brief Run checks on a Dirmodel
    * @param model   Model to check
    */
   void checkModel(const QAbstractItemModel *model);
};

#endif // TEST_DIRMODEL_H
