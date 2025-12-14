#include <QPixmap>
/***************************************************************************
                          qscanner.h  -  description
                             -------------------
    begin                : Thu Jul 6 2000
    copyright            : (C) 2000 by mh
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#ifndef QSCANNER_H
#define QSCANNER_H

extern "C"
{
#include <sane/sane.h>
}

#include <QByteArray>


//s #include <qarray.h>
#include <qdom.h>
#include <qmap.h>
#include <qobject.h>
#include <qstringlist.h>

class QImage;
class QPixmap;
class QProgressDialog;
class QString;
class PreviewWidget;
/** The QScanner class is mainly designed to access the SANE API
  * from within a Qt C++ program.
  * @author mh
  */



class QScanner : public QObject
{
Q_OBJECT
public:
   enum format_t
   {
   mono, dither, grey, colour,
   other
   };

   //! useful buttons on the scanner
   enum button_t
   {
      BUT_scan, BUT_email, BUT_copy, BUT_pdf,
      BUT_count
   };


	QScanner();
	~QScanner();
  static void qis_authorization(SANE_String_Const resource,
                       SANE_Char username[SANE_MAX_USERNAME_LEN],
                       SANE_Char password[SANE_MAX_PASSWORD_LEN]);  /**  */
  bool getDeviceList(bool local_only);
  /**  */
  bool isOptionSettable(int num);
  /**Returns the number of options for the device, including
    * option 0.
    */
  int optionCount();
  /** Returns true if the scanner was opened successfully,
    * otherwise false
    */
  bool isOpen();
  /**Set the device name. openDevice() must be called afterwards.
    */
  void setDeviceName(SANE_String_Const dev_name);
  /**Opens the device. Returns true, if the action
    * was successfull, otherwise false is returned.
    * Use saneStatus() to get the exact error.
    */
  bool openDevice();
  /** Return a QString if option num exists and is of
    * type SANE_TYPE_STRING, otherwise QString() is returned.
    */
  QString saneStringValue(int num);
  /**Return the value of option number num if the option exists and
    * is of type SANE_BOOL, SANE_INT or SANE_FIXED. Otherwise
    * INT_MIN is returned.
    */
  int saneWordValue(int num);
  /**Return the value of option number num if the option exists and
    * is of type SANE_INT or SANE_FIXED with an option size >
    * sizeof(SANE_Word>.
    */
  QVector<SANE_Word> saneWordArray(int num);
  /**Return an array wich hold the possible values for this option if the
    * option exists and is has a constraint_type SANE_CONSTRYINT_WORD_LIST.
    * Otherwise an empty array is returned
    */
  QVector<SANE_Word> saneWordList(int num);
  /**  */
  SANE_Value_Type getOptionType(int num);
  /**  */
  void exitScanner();
  /**  */
  SANE_String_Const name(int num);
  /**  */
  SANE_String_Const vendor(int num);
  /**  */
  SANE_String_Const model(int num);
  /**  */
  SANE_String_Const type(int num);
  /**Returns the number of available devices.
    */
  int deviceCount();
  /**Initialises the scanner with a call to sane_init(). If this action
    * was successfull, true is returned, otherwise false.
    * Call saneStatus() to get the exact error.
    * Use isInit() to see, whether Sane is initialized already.
    */
  bool initScanner();
  /**Returns true, if the device eas successfully initialised,
    * otherwise false.
    */
  bool isInit();
  /**Returns the SANE_Constraint_Type for option number num.
    */
  SANE_Constraint_Type getConstraintType(int num);
  /**Returns the name of option number num.
    */
  SANE_String_Const getOptionName(int num);
  /**Returns true if option number num is active, otherwise false.
    */
  bool isOptionActive(int num);
  /**Returns the title of option number num.
    */
  QString getOptionTitle(int num);
  /** Returns the number of the last option in the group
    * specified by num.
    */
  int lastGroupItem(int num);
  /** Returns the number of the first option in the group
    * specified by num.
    */
  int firstGroupItem(int num);
  /** Returns the number of groups. A backend may decide to place
    * related options in groups.
    */
  int getGroupCount();
  /**Checks whether there are active
    * items in the specified group
    */
  bool groupHasActiveItems(int num);
  /**Returns the number of items in the specified
    * group
    */
  int itemsInGroup(int num);
  /**Returns the range quant if the option descriptor of option number
  * num has a SANE_Constraint_Type of SANE_Range attached.
  * Legal values for this option can be calculated like:
  * <p><code>optvalue = k*q + minvalue</code>,</p>
  * where k is a non-negative integer value such that
  * <p><code>optvalue<=maxvalue</code>.</p>
  * A return value of 0 means, that all values between minvalue and
  * maxvalue are valid.
  * INT_MIN is returned if this option doesn't have a SANE_Range
  * attached.
  */
  SANE_Word getRangeQuant(int num);
  /**Returns the minimum range if the option descriptor of option number
  * num has a SANE_Constraint_Type of SANE_Range attached.
  * Otherwise INT_MIN is returned.
  */
  SANE_Word getRangeMin(int num);
  /**Returns the maximum range if the option descriptor of option number
  * num has a SANE_Constraint_Type of SANE_Range attached.
  * Otherwise INT_MIN is returned.
  */
  SANE_Word getRangeMax(int num);
  /** Returns the unit of the option specified by num. */
  SANE_Unit getUnit(int num);
  /**Returns string list item number item of option number num if
    * the option has a SANE_CONSTRAINT_STRING_LIST attached,
    * otherwise NULL.  */
  SANE_String_Const getStringListItem(int num, int item);
  /**Returns a string list option number num if
    * the option has a SANE_CONSTRAINT_STRING_LIST attached,
    * otherwise an empty list is returned.  */
  QStringList getStringList(int option);
  /**Returns the title of the group specified by num
    */
  QString getGroupTitle(int num);
  /**Returns true if there is an settable bry-option.*/
  bool isBrySettable();
  /**Returns true if there is an settable brx-option.*/
  bool isBrxSettable();
  /**Returns true if there is an settable tly-option.*/
  bool isTlySettable();
  /**Returns true if there is an settable tlx-option.*/
  bool isTlxSettable();
  /**Returns the number of the tly-option.*/
  int getTlyOption();
  /**Returns the number of the tlx-option.  */
  int getTlxOption();
  /**Returns the number of the bry-option.  */
  int getBryOption();
  /**Returns the number of the brx-option.  */
  int getBrxOption();
  /**Set the option specified by num. v is a pointer to an
    * approbiate datatype for this option. */
  SANE_Status setOption(int num,void* v,bool automatic = false);
  /**Call this function to cancel the currently pending operation of
    * the device immediately or as quickly as possible.
    */
  void cancel();
  /**Read image data from the device. Argument buf is a pointer to
    * a memory area that can hold maxlen bytes at least. Argument
    * len holds the number of bytes actually read if the call to read()
    * succeeded, otherwise 0.
    */
  SANE_Status read(SANE_Byte* buf,SANE_Int maxlen,SANE_Int* len);
  /**This method initiates aquisition of an image from the device.
    */
  SANE_Status start();
  /**Returns the number of the preview option or 0 if
	 there is no preview option. */
  int previewOption();
  /**  */
  int resolutionOption();
  /**  */
  void enablePreviewOption(bool b);
  /**  */
  void setPreviewResolution(int res);
  /**Sets the preview scan area.*/
  void setPreviewScanArea(double tlx,double tly,double brx,double bry);
  /** Return the description for option number num.*/
  QString getOptionDescription(int num);
  /**Scans the image with the currently specified options,
    * and save it in PNM format under the filename given by file  */
  SANE_Status scanImage(QString file,QWidget* parent=0,PreviewWidget* preview_widget=0);
  /**Set the io mode to non blocking io if b is true and if non
    * blocking io is supported by the device. Otherwise blocking
    * io is used. */
  void setIOMode(bool b);
  /**Returns the number of options that are not placed
    * inside groups.
    */
  int nonGroupOptionCount();
  /**Scans an image preview.
    * The scan area is set to the maximum values and a
    * resolution around 50 dpi is chosen. The preview image is saved
    * in PNM format under the filename given by argument path.
    */
//   SANE_Status scanPreview(QString path,QWidget* parent=0,double tlx=0.0,
//                           double tly=0.0,double brx=1.0,double bry=1.0,int res=50);
  /**  */
  SANE_Status getParameters(SANE_Parameters* par);
  /** Creates an QImage from the file specified by
    * path. If path is empty, the QImage is created
    * from the last temporary file created during the
    * last scan. Returns NULL if the image creation fails.
    * The caller is responsible for the deletion of the image.*/
  QImage* createImage(QString path = "");
  /** Creates an QPixmap from the file specified by
    * path. If path is empty, the QPixmap is created
    * from the last temporary file created during the
    * last scan. Returns NULL if the pixmap creation fails.
    * The caller is responsible for the deletion of the pixmap.*/
  QPixmap* createPixmap(QString path = "");
  /**  */
  void enableReloadSignal(bool status);
  /** Returns an info string which contains information
about pixel size, colormode and byte size.
This string can be used to inform the user about the
current settings. */
  QString imageInfo();
  /**  */
  int optionValueSize(int num);
  /**  */
  int yResolution();
  /**  */
  int xResolution();
  /**  */
  int yResolutionDpi();
  /**  */
  int xResolutionDpi();
  /**  */
  int pixelHeight();
  /**  */
  int pixelWidth();
  /**  */
  void close();
  /**  */
  bool appCancel();
  /**  */
  void setAppCancel(bool app);
  /**  */
  bool cancelled();
  /** Returns true, if SANE_CAP_AUTOMATIC is
set for option num. This means, that the backend
is able to choose an option value automatically. */
  bool automaticOption(int num);
  /**  */
  QString saneReadOnly(int num);
  /**  */
  bool isReadOnly(int num);
  /**  */
  void setOptionsByName(QMap <QString,QString> omap);
  /**Returns the name of the selected device.
    */
  QString name();
  /**Returns the vendor of the selected device.
    */
  QString vendor();
  /**Returns the type of the selected device.
    */
  QString type();
  /**Returns the model of the selected device.
    */
  QString model();
  /**  */
  SANE_Status saneStatus();
  /**Appends the currently active and settable options to a QDomElement
  */
  void settingsDomElement(QDomDocument doc,QDomElement domel);
  /** No descriptions */
  int optionSize(int num);
  /** No descriptions */
  double imageInfoMB();
  /** No descriptions */
  QString deviceSettingsName();
  /** No descriptions */
  void setType(QString type);
  /** No descriptions */
  void setModel(QString model);
  /** No descriptions */
  void setVendor(QString vendor);
  /** */
  static bool msAuthorizationCancelled;

  /** true if using the auto document feeder */
  bool useAdf (void);

  /** true if using duplex mode */
  bool duplex (void);

  /** format of scan */
  format_t format (void);

  /** set format of scan */
  void setFormat (format_t f, bool select_compression = false);

  /** true if using JPEG compression */
  bool compression (void);

  /** returns 0 if no ADF, 1 if only ADF, -1 if both or unknown */
  int adfType(void);

  /** set adf */
  bool setAdf (bool adf);

  /** set duplex */
  bool setDuplex (bool duplex);

  /** set DPI */
  void setDpi (int dpi);

  /** set the 'exposure'. This is the threshold for monochrome scans */
  void setExposure (int exposure);

  /** set the brightness */
  void setBrightness (int value);

  /** set the contrast */
  void setContrast (int value);

  /** get the 'exposure' */
  int getExposure (void);

  /** get the brightness */
  int getBrightness (void);

  /** get the contrast */
  int getContrast (void);

  /** get the range of contrast. Returns true on success

     \param minp    returns minimum value
     \param maxp    returns maximum value

     \returns true if option found and ranges returned are valid, else false */
  bool getRangeContrast (int *minp, int *maxp);

  /** get the range of brightness. Returns true on success

     \param minp    returns minimum value
     \param maxp    returns maximum value

     \returns true if option found and ranges returned are valid, else false */
  bool getRangeBrightness (int *minp, int *maxp);

  /** get the range of exposure. Returns true on success

     \param minp    returns minimum value
     \param maxp    returns maximum value

     \returns true if option found and ranges returned are valid, else false */
  bool getRangeExposure (int *minp, int *maxp);


  /** force reload of options when we know something has changed */
  void reloadOptions (void);

  /**
   * check the scanner buttons and return the value
   *
   * Return: buttoms mask on success, INT_MIN on failure
   */
  int checkButtons (void);

  /** returns true if we are currently scanning

     \returns true if scanning */
  bool isScanning (void);

  /** locate a particular option given its name

     \param settable   if true, then only settable parameters are returned */
  int findOption (const char *opt, bool settable = true);

  /** check if scanner has detected a double-feed condition

     \returns true if double-feed is detected, false otherwise */
  bool checkDoubleFeed (void);

private: // Private attributes

  /** locate option numbers for common options */
  void findOptions (void);

  /** option numbers for common options */
  int mOptionSource;   // option number of 'source'
  int mOptionDuplex;   // option number of 'duplex'
  int mOptionXRes;     // option number of DPI x
  int mOptionYRes;     // option number of DPI y
  int mOptionFormat;   // option number of format
  int mOptionCompression; // option number of compression (for JPEG)
  int mOptionThreshold;
  int mOptionBrightness;
  int mOptionContrast;

  // option number of topleft and bottom right of image
  int mOptionX0, mOptionY0, mOptionX1, mOptionY1;

  /** sane option numbers for the buttons */
  int mOptionButton [BUT_count];
  int mOptionFunction;  // function number selector
  int mOptionDoubleFeed; // double-feed hardware status (for Fujitsu)

  /** set to true if the call to sane_init was
successfull */
  bool mInitOk;
  const SANE_Device** mpDeviceList;
  bool mEmitSignals;
  /**  */
  bool mOpenOk;
  /**  */
  QString mDeviceName;
  QString mDeviceVendor;
  QString mDeviceType;
  QString mDeviceModel;
  /**  */
  SANE_Handle mDeviceHandle;
  /** */
	int mDeviceCnt;
  /**The number of options is guaranteed to be valid between calls to sane_open()
     and sane_close(). We only have to query the number of options once after a call
     to sane_open. */
	int mOptionNumber;
  /**  */
  QString mTempFilePath;
  /**  */
  SANE_Bool mNonBlockingIo;
  /**  */
  bool mAppCancel;
  /**  */
  SANE_Status mSaneStatus;
  /**  */
  bool mCancelled;
  /**  */
  QProgressDialog* mpProgress;

  /** true if we are currently scanning. This inhibits updating the options */
  bool mScanning;

private://methods
  /**  */
  QString createPNMHeader(SANE_Frame format,int lines,int ppl,int depth,
                          int resx,int resy);
  /**  */
  int optionNumberByName(QString name);
 /**  */
  int yResolutionOption();
  /**  */
  int xResolutionOption();

  // set a single option
  void set256 (int opt, int value);

  //! get a single option
  int get256 (int opt);

  /** get the range of an option. Returns true on success

     \param opt     option to check (-1 if none)
     \param minp    returns minimum value
     \param maxp    returns maximum value

     \returns true if option found and ranges returned are valid, else false */
  bool get_range (int opt, int *minp, int *maxp);

  /* the simulator, much of this taken from Sane's fujitsu.c driver :

     Copyright (C) 2000 Randolph Bentson
   Copyright (C) 2001 Frederik Ramm
   Copyright (C) 2001-2004 Oliver Schirrmeister
   Copyright (C) 2003-2008 m. allan noah

   JPEG output support funded by Archivista GmbH, www.archivista.ch
   Endorser support funded by O A S Oilfield Accounting Service Ltd, www.oas.ca
   */

#define MM_PER_INCH        25.4
#define MM_PER_UNIT_UNFIX            SANE_UNFIX (SANE_FIX (MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX    SANE_FIX (SANE_UNFIX (SANE_FIX (MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define SOURCE_FLATBED 0
#define SOURCE_ADF_FRONT 1
#define SOURCE_ADF_BACK 2
#define SOURCE_ADF_DUPLEX 3

#define COMP_NONE 0
#define COMP_JPEG 1

#define JPEG_STAGE_HEAD 0
#define JPEG_STAGE_SOF 1
#define JPEG_STAGE_SOS 2
#define JPEG_STAGE_FRONT 3
#define JPEG_STAGE_BACK 4
#define JPEG_STAGE_EOI 5

/* these are same as scsi data to make code easier */
#define MODE_LINEART 1
#define MODE_HALFTONE 2
#define MODE_GRAYSCALE 3
#define MODE_COLOR_LINEART 4
#define MODE_COLOR_HALFTONE 5
#define MODE_COLOR 6

#define COLOR_INTERLACE_UNK 0
#define COLOR_INTERLACE_RGB 1
#define COLOR_INTERLACE_BGR 2
#define COLOR_INTERLACE_RRGGBB 3
#define COLOR_INTERLACE_3091 4

#define DUPLEX_INTERLACE_ALT 0
#define DUPLEX_INTERLACE_NONE 1
#define DUPLEX_INTERLACE_3091 2

#define JPEG_INTERLACE_ALT 0
#define JPEG_INTERLACE_NONE 1

enum
   {
   OPT_NUM_OPTS,

   OPT_STANDARD_GROUP,
   OPT_SOURCE, /*fb/adf/front/back/duplex*/
   OPT_MODE,   /*mono/gray/color*/
   OPT_X_RES,  /*a range or a list*/
   OPT_Y_RES,  /*a range or a list*/

   OPT_GEOMETRY_GROUP,
   OPT_TL_X,
   OPT_TL_Y,
   OPT_BR_X,
   OPT_BR_Y,
   OPT_PAGE_WIDTH,
   OPT_PAGE_HEIGHT,

   OPT_ENHANCEMENT_GROUP,
   OPT_BRIGHTNESS,
   OPT_CONTRAST,
   OPT_GAMMA,
   OPT_THRESHOLD,

   OPT_ADVANCED_GROUP,
   OPT_COMPRESS,
   OPT_COMPRESS_ARG,

   NUM_OPTIONS   // number of options the SIMUL device supports
   };

  SANE_Option_Descriptor _opt[NUM_OPTIONS];

  /*mode group*/
  SANE_String_Const _mode_list[7];
  SANE_String_Const _source_list[5];

  SANE_Int _x_res_list[17];
  SANE_Int _y_res_list[17];
  SANE_Range _x_res_range;
  SANE_Range _y_res_range;

  /*geometry group*/
  SANE_Range _tl_x_range;
  SANE_Range _tl_y_range;
  SANE_Range _br_x_range;
  SANE_Range _br_y_range;
  SANE_Range _paper_x_range;
  SANE_Range _paper_y_range;

  /*enhancement group*/
  SANE_Range _brightness_range;
  SANE_Range _contrast_range;
  SANE_Range _gamma_range;
  SANE_Range _threshold_range;

  /*advanced group*/
  SANE_String_Const _compress_list[3];
  SANE_Range _compress_arg_range;

  /*mode group*/
  int _mode;           /*color,lineart,etc*/
  int _source;         /*fb,adf front,adf duplex,etc*/
  int _resolution_x;   /* X resolution in dpi                       */
  int _resolution_y;   /* Y resolution in dpi                       */

  /*geometry group*/
  /* The desired size of the scan, all in 1/1200 inch */
  int _tl_x;
  int _tl_y;
  int _br_x;
  int _br_y;
  int _page_width;
  int _page_height;

  /*enhancement group*/
  int _brightness;
  int _contrast;
  int _gamma;
  int _threshold;

  /*advanced group*/
  int _compress;
  int _compress_arg;

  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int _started;
  int _reading;
  int _cancelled;
  int _side;

  /* total to read/write */
  int _bytes_tot[2];

  /* how far we have read */
  int _bytes_rx[2];
  int _lines_rx[2]; /*only used by 3091*/

  /* how far we have written */
  int _bytes_tx[2];

  unsigned char *_buffers[2];
  int _fds[2];

  /* --------------------------------------------------------------------- */
  /* values used by the compression functions, esp. jpeg with duplex       */
  int _jpeg_stage;
  int _jpeg_ff_offset;
  int _jpeg_front_rst;
  int _jpeg_back_rst;
  int _jpeg_x_byte;

  int _color_interlace;  /* different models interlace colors differently     */
  int _duplex_interlace; /* different models interlace sides differently      */
  int _jpeg_interlace;   /* different models interlace jpeg sides differently */

  int get_page_width();
  int get_page_height(void);

  SANE_Status do_sane_open (SANE_String_Const name, SANE_Handle *handle);

  SANE_Status do_sane_control_option (SANE_Handle handle, SANE_Int option,
                                        SANE_Action action, void *value,
                                        SANE_Int * info);

  const SANE_Option_Descriptor *do_sane_get_option_descriptor (SANE_Handle handle, SANE_Int option);

  SANE_Status do_sane_start (SANE_Handle handle);

  void do_sane_cancel (SANE_Handle handle);

  SANE_Status do_sane_set_io_mode (SANE_Handle handle, SANE_Bool nbio);

   SANE_Status do_sane_read (SANE_Handle handle, SANE_Byte* buf,SANE_Int maxlen,SANE_Int* len);

   SANE_Status do_sane_get_parameters (SANE_Handle handle, SANE_Parameters * params);//////

   void do_sane_close (SANE_Handle handle);

  SANE_Status get_pixelsize(void);

  SANE_Status setup_buffers (void);

  SANE_Status check_for_cancel();

signals: // Signals
  /**  */
  void signalReloadOptions();
  /**  */
  void signalInfoInexact(int);
  /**  */
  void signalReloadParams();

  /** signal that an option value has changed */
  void signalSetOption(int);

  /** signal that a scan is complete */
  void signalScanDone();
private:

  /** generate the front and back page images

      \return true if ok, false if we have no more pages left */
  bool generate_pages (void);

  void generate_page (int pagenum, bool front, QImage &image, QByteArray &data);

   SANE_Status read_from_JPEGduplex (void);

   SANE_Status read_from_scanner(int side);

   /** read the next lot of bytes from the image

      \param ba         byte array to read
      \param upto       where we got up to last time (this function updates this value)
      \param buff       where to copy the image data
      \param nread      number of bytes to read (updated with number of bytes actually read)
      \returns sane status, normally GOOD, but will be EOF if there are no bytes left to read */
   SANE_Status read_bytes (QByteArray &ba, int &upto, unsigned char *buff, size_t &nread);

   SANE_Status read_from_buffer (SANE_Byte * buf,
      SANE_Int max_len, SANE_Int * len, int side);

   SANE_Status copy_buffer (unsigned char * buf, int len, int side);

   void build_jpeg (QImage &image, QByteArray &ba);

private:
   // simulator
   SANE_Parameters _simul_params;   // what the 'scanner' things
//    SANE_Parameters _params;         // our private copy
   int _max_x;
   int _max_y;
   int _min_x;
   int _min_y;
   int _max_x_fb;
   int _max_y_fb;

   // the images we return
   QImage _front;
   QImage _back;
   QByteArray _front_data;
   QByteArray _back_data;

   // how many bytes we have read from each
   int _front_upto;
   int _back_upto;

   int _page_upto;      //! simulated page number that we are up to
};

#endif
