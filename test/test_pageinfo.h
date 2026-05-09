#ifndef TEST_PAGEINFO_H
#define TEST_PAGEINFO_H

#include <QObject>

#include "suite.h"

class TestPageinfo: public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! updatePixmap() must not crash when _model is null
   void testUpdatePixmapAfterDisconnect();
};

#endif // TEST_PAGEINFO_H
