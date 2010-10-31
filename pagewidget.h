/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/
#ifndef __pagewidget_h
#define __pagewidget_h

//Added by qt3to4:
// #include <QPixmap>



//#include <Q3ScrollView>
#include <QAbstractScrollArea>
#include <QModelIndex>
#include <QScrollArea>
#include <QStackedWidget>


class QAbstractModelIndex;
class QImage;
class QPixmap;
class QSplitter;
class QTextEdit;
class QTimer;
class QToolButton;
class QWidget;

class Ui_Ocrbar;
class Ui_Pageattr;

class Desktopmodelconv;
class Desk;
class Mainwidget;
class Pagedelegate;
class Pagetools;
class Pagemodel;
class Pageview;

struct file_info;

#include "qgraphicsview.h"
#include "desk.h"

struct err_info;


/* this class manages a scrolling page image. It supports scaling by holding ctrl and moving the wheel */

class MyScrollArea : public QAbstractScrollArea
   {
   Q_OBJECT

public:
   /** constructor

      \param parent  the parent widget into which this draws */
   MyScrollArea (QWidget *parent);

   //! destructor
   ~MyScrollArea ();

   /** set the scale of the image

      \param scale   the scale to set (1.0 is normal) */
   void setScale (double scale);

   /** set the pixmap or image to display

      \param pixmap  the pixmap to display
      \param image   the image to display (if the pixmap is too big)
      \param too_big true to display the image instead of the pixmap */
   void setPixmapImage (QPixmap &pixmap, QImage &image, bool too_big, int rotate);

   /** set the size of the pixmap, used to set up the scrollbars correctly

      \param size    the pixmap size */
   void setSize (QSize &size);

signals:
   /* signal emitted when the user changes the scale */
   void signalNewScale (double scale, bool delay_smoothing);

   /* signal emitted when we paint something */
   void signalPainting ();

protected:
   /** called to paint the window

      \param event      info about paint required, including clip area */
   void paintEvent (QPaintEvent * event);

   /** returns the size of the pixmap being displayed, used by QT to size the widget

      \returns    the pixmap size */
   QSize sizeHint () const;

   /** handle a mouse wheel event - we use this to scroll

      \param e    the mouse wheel event */
   void wheelEvent (QWheelEvent *e);

   void mousePressEvent ( QMouseEvent * e );

   void mouseMoveEvent ( QMouseEvent * e );

private:
   double _scale;    //!< scale of the image
   QPixmap _pixmap;  //!< current pixmap being displayed
   QImage _image;    //!< current image being displayed, for when the pixmap is too big
   QSize _size;      //!< size of current pixmap
   bool _too_big;      //!< true if the scaling is too big to use a pixmap
   QPoint _scroll_origin;   //!< scroll origin
   QPoint _mouse_origin;   //!< scroll origin
   int _rotate;         //!< rotation amount: 0, 90, 180, 270
   };


class Pagewidget : public QWidget
   {
   Q_OBJECT

public:
   /** which mode the viewer is in */
   enum e_mode
      {
      Mode_select,         //!< allows selection of items
      Mode_move,           //!< allows selection and reordering
      Mode_scan,           //!< allows viewing of scanning as it happens
      Mode_info,           //!< display stack information

      Mode_none            //!< mode not set
      };

   /** create a new page widget

      \param modelconv     conversion class for changing from proxy to source models
      \param base          base string for settings, e.g. "desktopwidget/"
      \param parent        parent widget */
   Pagewidget (Desktopmodelconv *modelconv, QString base, QWidget *parent = 0);
   ~Pagewidget ();

   /** init the widget - must be called immediately after creation */
   void init (void);

   /** convert a mode to a tool button */
   QToolButton *modeToTool (e_mode mode);

   /** convert a tool button to a mode */
   e_mode toolToMode (QObject *tb);

   /** display a page from a maxdesk. This function checks the current page in f.

      \param model       the model containing the file (maybe a proxy)
      \param index       the file index (the current page number will be shown)
      \param delay_smoothing  delay smoothing to allow the user to perform another action first */
//    void showPage (const QAbstractItemModel *model, const QModelIndex &index,
//          bool delay_smoothing = false);

   /** display a set of pages from a maxdesk.

      \param model       the model containing the file (maybe a proxy)
      \param index       the file index
      \param start       start page (0 for first)
      \param count       number of pages (-1 for all)
      \param across      how many to show across (normally 1 or 2) */
   void showPages (const QAbstractItemModel *model, const QModelIndex &index,
         int start, int count, int across, bool reset = false);

   void pageLeft (void);
   void pageRight (void);

   /** returns the index of the page currently being displayed

      \param index   returns the index
      \param source  true to return a source index, else returns whatever
                     model is in use (maybe a proxy) */
   bool getCurrentIndex (QModelIndex &index, bool source = false);
   int getCurrentPage () { return _pagenum; }
   void checkSubsystem (int subsystem);

   //! returns true if the page area is visible (and therefore needs redrawing)
   bool isVisible (void);

   enum
      {
      SUBSYS_pixmap,    // use pixmaps am MyScrollArea
      SUBSYS_gview,     // use QGraphicsView
      SUBSYS_opengl,    // use OpenGL
      };

   struct err_info *operation (Desk::operation_t type, int ival);

   /** redisplay the current page - can be used if the smoothing setting
       has been changed, for example */
   void redisplay (void);

   /** set whether we are smoothing or not

      \param smooth     true to smooth the image */
   void setSmoothing (bool smooth);

   /** called when the widget is closing to save window settings */
   void closing (void);

signals:
   void newContents (QString);

   /** indicate that the mode is changing - this allows the parent window
       to save and adjust its window sizes if it wants to */
   void modeChanging (int new_mode, int old_mode);

public slots:
   /***************************** scanning ***************************/

   /** indicates that a scan is beginning into the given stack.

       This signal comes from Desktopmodel via a connect() in Desktopwidget */
   void beginningScan (const QModelIndex &ind);

   /** indicates that scanning into the current stack is ending

      \param cancel true if the scan was cancelled, and the stack will be
                    deleted */
   void endingScan (bool cancel);

   /** indicate that a scan is complete */
   void scanComplete (void);

   /** handle a new page being scanned. We update our page model and store
       the coverage information

       This signal comes from Desktopmodel via a connect() in Desktopwidget

       \param coverageStr  page coverage string
       \param blank        true if page is marked blank */
   void slotNewScannedPage (const QString &coverageStr, bool blank);

   /******************************************************************/
   /** handle an item being clicked

      \param index      item clicked
      \param which      which part of it was clicked (e_point) */
   void slotItemClicked (const QModelIndex &index, int which);

   /** show information about a page in the status bar */
   void slotShowInfo (const QModelIndex &index);

   /** reset the view and model so no pages are shown. Also existing page
       will be committed */
   void slotReset (void);

   /** handle a message that the current scan stack is being committed */
   void slotCommitScanStack (void);

protected:
   /** update the viewport to allow a change of page or scale

      \param update_scale  the scale has changed, or the pixmap needs regeneration for some other reason
      \param force_smoothing   force smoothing regardless of the setting
      \param delay_smoothing   rather than setting a timer to smooth immediately after this this call,
                                 delay it for 300ms */
   void updateViewport (bool update_scale = false, bool force_smoothing = false, bool delay_smoothing = false);

   /** clear the viewport so that it shows no image */
   void clearViewport (void);

   /** sets the page size to use for the page images. The grid size is set
       to slightly larger than this also

      \param size    max size of each page image */
   void setPagesize (QSize size);

   /** revert to the mode we were in before we started scanning */
   void revertMode (void);

   /** change the view mode of the widget */
   void setMode (e_mode mode);

   /** toggle the remove flag on a page */
   void toggleRemove (const QModelIndex &index);

   /** commit any changes to the current stack */
   void commit (void);

protected slots:
   void slotNewScale (double scale, bool delay_smoothing);
   void updateWindow (void);
   void slotDelayedUpdate (void);
   void slotZoomFit (void);
   void slotZoomOrig (void);
   void slotZoomIn (void);
   void slotZoomOut (void);
   void slotZoom (void);

   void slotNewScale (int new_scale);

   /** display a new page in the preview window */
   void slotPreviewPage (const QModelIndex &ind);

   /** called when there is a new scaled image for the page currently being
       scanned

      \param image           image fragment
      \param scaled_linenum  destination start line for this fragment */
   void slotNewScaledImage (const QImage &image, int scaled_linenum);

   /** called when we are beginning to scan a new page. We display it and
       monitor progress with calls we receive to slotNewScaledImage() */
   void slotBeginningPage (void);

   /** save changes to a paper stack */
   void slotSave (void);

   /** revert paper stack to original state */
   void slotRevert (void);

   /** handle a stack being changed - if it is the one we are previewing then
       we need to update our view

       \param from   start index of stack that changed
       \param to     end index of stack that changed */
   void slotStackChanged (const QModelIndex &from, const QModelIndex &to);

   /** slot to change mode according to the sender tool button */
   void slotChangeMode (bool selected);

   /** slot to handle annotation text being changed. We mark the stack as
       modified */
   void slotAnnotChanged (void);

   /** flip between horizontal and vertical view for OCR */
   void ocrFlipView (void);

   /** run ocr on a page. This produces some text */
   void ocrPage (void);

   /** clear the ocr text */
   void ocrClear (void);

   /** copy the ocr test to the clipboard */
   void ocrCopy (void);

   /** add an amount to the rotation of the current page

      \param add  amount to add (-90, 90, 180) */
   void addRotate (int add);

   /** rotate the current page image right 90 degrees */
   void slotRotateRight (void);

   /** rotate the current page image 180 degrees */
   void slotRotate180 (void);

   /** rotate the current page image left 90 degrees */
   void slotRotateLeft (void);

public:
   QAction *_returnToDesktop;

private:
   /** update the 'enabled' status of the toolbar icons */
   void updatePagetools (void);

   /** update the annotation text editor with text from the stack */
   void updateAttr (void);

   /** update the ocr text editor with text from the current page */
   void updateOcrText (void);

private:
   QScrollArea *_page;
   const QAbstractItemModel *_model;   // model containing the stacks
   QPersistentModelIndex _index;        // index of current stack displayed
/*   struct Desk *_desk;
   struct file_info *_file;*/
   QPixmap _pixmap;
   QImage _image;
   int _pagenum;             //!< current page number being displayed
   int _start;             //!< start page to show
   int _count;             //!< number of pages to shoe
   bool _smoothing;          //!< true to display with smoothing
   double _scale;           //!< display scale
//    QGraphicsScene *_scene;
//    QGraphicsPixmapItem *_pitem;
//    QGraphicsView *_view;
   MyScrollArea *_area;
   int _subsys;         // display subsystem to use
   bool _too_big;      //!< true if the scaling is too big to use a pixmap

   /** timer used to handle updates */
   QTimer *_timer;

   /** true if a delayed update is set - we will update when the page is visible on screen */
   bool _delayed_update;

   /** delayed value for update_scale */
   bool _delayed_update_scale;

   /** delayed value for force_smooth */
   bool _delayed_force_smooth;

   /** size to use for pixmap */
   //QSize _pixmap_size;

   Pagetools *_tools;            //!< the toolbar
   Desktopmodelconv *_modelconv; //!< the model converter

   Pageview *_pageview;        //!< page viewer
   Pagemodel *_pagemodel;      //!< page model
   Pagedelegate *_pagedelegate; //!< page delegate
   int _scale_down;           //!< scale down to use 1/n
   int _rotate;               //!< clockwise rotation amount in degrees (0, 90, 180, 270)
   QStackedWidget *_stack;    //!< the stack widget which holds the splitter and text details
   QSplitter *_splitter;      //!< the splitter between page selector and preview
   QFrame *_textframe;        //!< frame containing text information
   e_mode _mode;              //!< which mode we are in (selection or move)
   QString _settings_base;    //!< base string for saving window settings
   bool _modified;            //!< true if we have modified the current stack
   bool _scanning;            //!< are we scanning?
   e_mode _prescan_mode;      //!< the mode we were using before we started scanning
   Ui_Pageattr *_pageattr;    //!< page attribute dialogue fields
   QSplitter *_ocr_split;     //!< ocr / image splitter
   QTextEdit *_ocr_edit;      //!< OCR text editor
   Ui_Ocrbar *_ocr_bar;       //!< OCR toolbar
   bool _committing;          //!< true if currently committing a stack
   };

#endif

