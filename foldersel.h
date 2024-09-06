#ifndef FOLDERSEL_H
#define FOLDERSEL_H

#include <QLineEdit>

class Foldersel : public QLineEdit
{
   Q_OBJECT

protected:
   virtual void focusOutEvent(QFocusEvent *e) override;

public:
   Foldersel(QWidget* parent);
   ~Foldersel();
};

#endif // FOLDERSEL_H
