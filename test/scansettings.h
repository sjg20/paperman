#ifndef SCANSETTINGS_H
#define SCANSETTINGS_H

#include <QString>

#include "qxmlconfig.h"

/** the name paperman gives its built-in simulated scanner */
#define SIMUL_NAME "simulscan"

static inline void ensureXmlConfig ()
{
   if (!xmlConfig)
      new QXmlConfig ();
}


/* A scan reads the settings the user keeps, so a test which drives one
   has to say what it wants rather than take whatever the machine it is
   running on happens to hold: on one of them a page with nothing on it
   is thrown away and on another it is kept, and a test which does not
   say which it wants passes or fails by where it is run */
class Scansettings
{
public:
   /** \param pages    sides to scan before stopping, or 0 to go on until
                       the scanner runs out of paper
       \param device   the scanner to use */
   Scansettings (int pages = 1, const char *device = SIMUL_NAME)
   {
      ensureXmlConfig ();
      _device = xmlConfig->stringValue ("LAST_DEVICE", QString ());
      _single = xmlConfig->intValue ("SCAN_SINGLE");
      _sideways = xmlConfig->intValue ("SCAN_SIDEWAYS");
      _blank = xmlConfig->intValue ("SCAN_BLANK");
      _colour = xmlConfig->boolValue ("SCAN_AUTO_COLOUR");

      xmlConfig->setStringValue ("LAST_DEVICE", device);
      xmlConfig->setIntValue ("SCAN_SINGLE", pages);
      xmlConfig->setIntValue ("SCAN_SIDEWAYS", 0);
      xmlConfig->setIntValue ("SCAN_BLANK", 0);   // keep every page
      xmlConfig->setBoolValue ("SCAN_AUTO_COLOUR", false);
   }

   ~Scansettings ()
   {
      xmlConfig->setStringValue ("LAST_DEVICE", _device);
      xmlConfig->setIntValue ("SCAN_SINGLE", _single);
      xmlConfig->setIntValue ("SCAN_SIDEWAYS", _sideways);
      xmlConfig->setIntValue ("SCAN_BLANK", _blank);
      xmlConfig->setBoolValue ("SCAN_AUTO_COLOUR", _colour);
   }

   //! say which way the sheets are fed, for a test which needs that
   void setSideways (int how)
   {
      xmlConfig->setIntValue ("SCAN_SIDEWAYS", how);
   }

private:
   QString _device;
   int _single, _sideways, _blank;
   bool _colour;
};

#endif // SCANSETTINGS_H
