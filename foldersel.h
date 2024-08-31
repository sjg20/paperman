#ifndef FOLDERSEL_H
#define FOLDERSEL_H

#include <QLineEdit>

class Foldersel : public QLineEdit
{
   Q_OBJECT

public:
   Foldersel(QWidget* parent);
   ~Foldersel();
};

#endif // FOLDERSEL_H
