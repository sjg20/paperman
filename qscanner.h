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
#include <QMultiHash>


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

  /** Close the device handle and reopen it. Useful for recovering when
    * the scanner returns SANE_STATUS_IO_ERROR after going to sleep and
    * coming back. Note that all option values are reset to defaults; the
    * caller must reapply any settings (e.g. via the active preset).
    *
    * @returns true if the reopen succeeded
    */
  bool reconnect();
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
  /** set options from a name/value map, as saved settings and --set do

      \param omap            the options and their values
      \param allowTransport  also set transport options (see
                             isTransportOption()), as --set may; saved
                             settings may not */
  void setOptionsByName(QMap <QString,QString> omap,
                        bool allowTransport = false);

  /** Is this option a transport knob, which QScanner sets for itself
      (the read chunk size) rather than a scan setting? Such options are
      not saved with a device's settings nor restored from them: a saved
      buffer-size of 4 KB once made every USB read a 4 KB round trip and
      a scan three times slower than the scanner */
  static bool isTransportOption (const QString &name)
     { return name == "buffer-size"; }
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

  /** Ask the backend to stop the feeder but keep delivering the pages it
      has already scanned, so that a batch stopped early loses nothing.
      Only backends with a "stop-feed" button offer this (finet today).
      Must be called from the scanning thread, between pages.

     \returns true if the feeder is now stopped and the remaining pages
              will follow, false if the backend cannot do this */
  bool stopFeed (void);

  /** How many finished images the scanner is holding that have not been
      fetched yet, i.e. how far it is ahead of us. Only backends with a
      read-only "images-waiting" option know (finet today)

     \returns the count, or -1 if the backend cannot say */
  int imagesWaiting (void);

  /** Turn on the fast-transfer settings a backend may offer: buffering
      in the scanner ("buffermode"), so it scans ahead rather than
      stopping after every sheet, and JPEG delivery ("compression") in
      colour, which cuts a side from 25 MB to under 1 MB. The patched
      fujitsu backend has both; without them a USB fi-8950 runs at a
      third of its speed. Options a backend lacks are left alone */
  void useFastTransfer (const QStringList &except = QStringList ());

  /** Request the backend to use a smaller per-read chunk so the frontend
      sees the page progressively. No-op if the backend has no
      "buffer-size" option (only the patched fujitsu backend does today).
      Must be called before start(); silently clamped to the option's
      legal range. */
  void setBufferSize (int bytes);

  /** True if libsane provides sane_read_dup and the back end has not
      refused it, in which case readDup() will deliver front and back data
      progressively in a single call. */
  bool hasReadDup (void);

  /** Read both sides of a duplex scan in one call. Caller passes two
      output buffers of the same maxlen; *front_len / *back_len are filled
      with the bytes actually returned for each side. Returns SANE_STATUS_GOOD
      while either side may still produce data; SANE_STATUS_EOF when both
      sides are drained. Asserts hasReadDup(). A back end without the
      extension returns SANE_STATUS_UNSUPPORTED, after which hasReadDup()
      is false for this scanner. */
  SANE_Status readDup (SANE_Byte *front_buf, SANE_Byte *back_buf,
                       SANE_Int max_len,
                       SANE_Int *front_len, SANE_Int *back_len);

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
  int mOptionBufferSize; // backend chunk-size knob (patched fujitsu backend)

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

   /** where each option name sits in the list, so that looking one up
       by name does not mean asking the scanner about every option it
       has. Built when first needed and thrown away when the option
       list changes */
   QMultiHash<QString, int> mOptionByName;
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
  bool mNoReadDup;         //!< the back end refused sane_read_dup()
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
};

#endif
