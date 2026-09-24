/* The back door into the fake scanner

   fakescan.cpp is a SANE back end which answers as the fujitsu back end
   does for an fi-8170. A front end drives it through the ordinary SANE
   calls; a test also reaches in through these, to do what a person
   standing at the scanner would: put paper in the hopper, and so on.

   libsane's dll back end loads the library with dlopen(), inside the
   front end's own process. Opening the same file again hands back the
   same copy of it, so a test which does that and looks these up with
   dlsym() is talking to the scanner the front end has open */

#ifndef FAKESCAN_H
#define FAKESCAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* the file the dll back end looks for, and the name the front end sees */
#define FAKESCAN_LIB      "libsane-fakefujitsu.so.1"
#define FAKESCAN_BACKEND  "fakefujitsu"
#define FAKESCAN_DEVICE   FAKESCAN_BACKEND ":fi-8170:00001"

/** a sheet of paper, as it goes into the hopper */
struct fakescan_sheet
   {
   const char *front;   /**< image of the front, or NULL for plain paper */
   const char *back;    /**< image of the back, or NULL for plain paper */
   double width_mm;     /**< size of the sheet, or 0 to go by the images */
   double height_mm;
   int dpi;             /**< resolution of the images, or 0 for 300 */
   };

/** Take all the paper out of the hopper and put everything which is not
    set through SANE back as it was when the library was loaded */
void fakescan_reset (void);

/** Put a sheet in the hopper, behind any already there

    \param sheet   the sheet; the images are read before this returns
    \returns 0 if OK, -1 if an image cannot be read, -2 if the sheet has
             no size, since neither it nor an image gives one */
int fakescan_load_sheet (const struct fakescan_sheet *sheet);

/** \returns how many sheets are still in the hopper */
int fakescan_sheets_left (void);

/** Set the colour of the backing, which is what is scanned wherever the
    window reaches beyond the sheet

    \param rgb   colour as 0xRRGGBB */
void fakescan_set_backing (unsigned int rgb);

#ifdef __cplusplus
}
#endif

#endif /* FAKESCAN_H */
