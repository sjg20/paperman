#ifndef MEASURE_H
#define MEASURE_H

#include <QFont>
#include <QSize>

class QStyle;

/** measure text according to a predefined font */
class Measure {
public:
   /* Set up measurement with a given style and font */
   Measure(QStyle *style, QFont font);
   ~Measure();

   //! Get the width of a given string
   int getWidth(const QString& text);

private:
   QStyle *_style;
   QFont _font;
};

#endif // MEASURE_H
