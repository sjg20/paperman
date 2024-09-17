/* Measurer for text bounding box */

#include <QFontMetrics>
#include <QString>

#include "measure.h"

Measure::Measure(QStyle *style, QFont font)
    : _style(style)
{
   _font = font;
}

Measure::~Measure()
{
}

int Measure::getWidth(const QString& text)
{
   QFontMetrics fm(_font);
   return fm.size(0, text).width();
}
