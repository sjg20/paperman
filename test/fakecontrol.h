#ifndef FAKECONTROL_H
#define FAKECONTROL_H

#include <functional>
#include <QStringList>

#include "scancontrol.h"

/** A scanner for the scan panel to work through in a test, which holds
    its settings and nothing more, so that the panel can be tried on any
    platform with no scanner at all.

    Like a fujitsu scanner it has a feeder, duplex, auto-size and
    straightening, and a brightness and contrast from -127 to 127 which
    mono has as an exposure from 0 to 255.

    Once a setting changes, changed() is called, which a test sets to
    Pscan::refresh() just as the main window refreshes the panel when the
    scanner says an option has changed */
class FakeControl : public ScanControl
{
public:
   bool _present = true;
   bool _settable = true;   // as well as present
   QScanner::format_t _format = QScanner::mono;
   int _dpi = 300;
   bool _adf = true;
   bool _duplex = true;
   int _exposure = 127;   // the middle, as the others are
   int _bright = 0;
   int _contrast = 0;
   bool _has_autosize = true;
   bool _autosize = false;
   bool _has_deskew = true;
   bool _deskew = false;
   int _size = 2;
   bool _options_shown = false;

   std::function<void()> changed;

   // as the preview has them, the first two not being paper sizes
   QStringList _sizes = {"Full size", "User size", "A4 (210x297 mm)",
                         "A5 (148x210 mm)", "Legal (8.5x14 inches)",
                         "Letter (8.5x11 inches)", "Long"};

   bool present() override { return _present; }
   bool settable() override { return _present && _settable; }
   QString title() override { return "Fake scanner"; }
   int adfType() override { return 1; }
   bool useAdf() override { return _adf; }
   void setAdf(bool adf) override { _adf = adf; tell(); }
   bool duplex() override { return _duplex; }
   void setDuplex(bool duplex) override { _duplex = duplex; tell(); }
   int xDpi() override { return _dpi; }
   int yDpi() override { return _dpi; }
   void setDpi(int dpi) override { _dpi = dpi; tell(); }
   QScanner::format_t format() override { return _format; }
   void setFormat(QScanner::format_t format, bool) override
   {
      _format = format;
      tell();
   }

   int exposure() override { return mono() ? _exposure : -1; }
   int brightness() override { return mono() ? -1 : _bright; }
   int contrast() override { return mono() ? -1 : _contrast; }
   bool exposureRange(int *minp, int *maxp) override
   {
      return mono() && range(minp, maxp, 0, 255);
   }
   bool brightnessRange(int *minp, int *maxp) override
   {
      return !mono() && range(minp, maxp, -127, 127);
   }
   bool contrastRange(int *minp, int *maxp) override
   {
      return !mono() && range(minp, maxp, -127, 127);
   }
   void setExposure(int value) override { _exposure = value; tell(); }
   void setBrightness(int value) override { _bright = value; tell(); }
   void setContrast(int value) override { _contrast = value; tell(); }

   bool hasAutoSize() override { return _has_autosize; }
   bool autoSize() override { return _has_autosize && _autosize; }
   void setAutoSize(bool on) override { _autosize = on; tell(); }
   bool autoSizeTrimsWidth() override { return false; }
   bool hasDeskewCrop() override { return _has_deskew; }
   bool deskewCrop() override { return _has_deskew && _deskew; }
   void setDeskewCrop(bool on) override { _deskew = on; tell(); }
   void showOptions() override { _options_shown = true; }

   QString sizeName(int id) override { return _sizes.value(id); }
   void setSize(int id) override
   {
      if (id >= 0 && id < _sizes.size())
         _size = id;
      tell();
   }
   int sizeA4() override { return _sizes.indexOf("A4 (210x297 mm)"); }
   int sizeLetter() override
   {
      return _sizes.indexOf("Letter (8.5x11 inches)");
   }
   int sizeLegal() override { return _sizes.indexOf("Legal (8.5x14 inches)"); }
   int sizeLong() override { return _sizes.indexOf("Long"); }

   //! the name of the size last chosen
   QString size() { return _sizes.value(_size); }

private:
   bool mono() { return _format == QScanner::mono; }

   bool range(int *minp, int *maxp, int min, int max)
   {
      *minp = min;
      *maxp = max;
      return true;
   }

   void tell()
   {
      if (changed)
         changed();
   }
};

#endif // FAKECONTROL_H
