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

private:
   // Set up a new Dirmodel with test data
   Dirmodel *setupModel();

   /**
    * @brief Run checks on a Dirmodel
    * @param model     Model to check
    * @param dirmodel  Dirmodel object, if available, else nullptr
    * @param proxy     Dirproxy object, if available, else nullptr
    */
   void checkModel(const QAbstractItemModel *model, const Dirmodel *dirmodel,
                   const QAbstractProxyModel *proxy);

};

#endif // TEST_DIRMODEL_H
