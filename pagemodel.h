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
/*
   Project:    Maxview
   Author:     Simon Glass
   Copyright:  2001-2009 Bluewater Systems Ltd, www.bluewatersys.com
   File:       pagemodel.h
   Started:    5/6/09

   This file implements the model for pages within a stack. It supports
   obtaining pixmaps for display at various required scales

   The model has a fairly fixed layout with a fixed number of columns.
   Pages are simply spread from left to right, top to bottom.

   The view is in pageview.
*/


#include <QAbstractItemModel>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QVector>


class QPixmap;
class QTimer;

class Desktopmodel;
class Pagemodel;



class Pageinfo
   {
public:
   Pageinfo ();
   ~Pageinfo ();

   void setup (const Pagemodel *model, int itemnum);

   /** returns a pixmap for the page. Hopefully this will require very
       little action as the correctly scaled pixmap is already stored.
       However, if the scale has since changed, then this function will
       rescale the pixmap that it returns as a temporary measure */
   QPixmap pixmap (bool &dodgy);
   int pagenum (void) { return _pagenum; }
   int itemnum (void) { return _itemnum; }
   QString pagename (void) { return _pagename; }

   /** mark the page as invalid so that it will be recreated. This happens
       when we change the stack that is being used */
   void invalidate (void);

   /** complete a scan on a page */
   void scanDone (QString coverage, bool mark_blank);

   /** Stop any rescale that is pending. This happens when we change the
       scale of all the pages at once - any preview rescale is therefore
       now a waste of time */
   void haltRescale (void);

   /** checks if it is necessary to update the page's pixmap (due to a scale
       change). If so, does so.

      \returns true if an update was necessary, false if not */
   bool updatePixmap (void);

   /** returns the string with information on page coverage

      \returns    coverage string */
   QString getCoverage (void) const { return _coverage; }

   /* set the coverage for a page

      \param coverageStr   coverage string */
   void setCoverage (const QString &coverageStr) { _coverage = coverageStr; }

   /* mark a page as blank and to be removed */
   void markBlank (void);

   /** check if a page is blank

      \returns true if blank, else false */
   bool isBlank (void) const { return _blank; }

   /** check if a page is to be removed

      \returns true if marked for removal, else false */
   bool toRemove (void) const { return _remove; }

   /** set if a page is to be removed

      \returns true if marked for removal, else false */
   void setRemove (bool remove);

   /** mark the mark as still being scanned */
   void setScanning (bool scanning, int pagenum = -1);

   /** check if this page is currently being scanned

      \returns true if being scanned, else false */
   bool scanning (void) const { return _scanning; }

   /** update the scan image (called while scan is in progress

      \param image      scan image */
   void updateScanImage (const QImage &image);

private:
   bool _valid;            //!< true if this page has been set up
   QPixmap _pixmap;        //!< page image
   int _itemnum;           //!< item number (used to construct index)
   const Pagemodel *_model; //!< page model
   int _pagenum;           //!< the page number
   QString _pagename;      //!< the page name
//   int _scale_down;          //!< scale factor at which the pixmap was done (e.g. 24 means 1/24)
   QSize _size;            //!< image size
   bool _rescale;          //!< true if this page's pixmap needs to be rescaled
   QString _coverage;      //!< information about page coverage
   bool _blank;            //!< true if page is blank
   bool _remove;           //!< marked for removal
   bool _scanning;         //!< true if the page is still being scanned
   };

Q_DECLARE_METATYPE(Pageinfo *)



class Pagemodel : public QAbstractItemModel
   {
   Q_OBJECT
public:
   Pagemodel (QObject *parent);
   ~Pagemodel ();

   /** reset the model to point to no stack */
   void clear (void);

   /** reset the model to point to the given stack */
   void reset (const Desktopmodel *model, const QModelIndex &index,
      int start, int count);

   enum e_role
      {
/* roles available in this model:

   Qt::DisplayRole         QString  stack name
   Qt::EditRole            QString  stack name (same as display)
   Qt::DecorationRole      QIcon    a QIcon of the pixmap

*/
      Role_pageinfo   = Qt::UserRole,  // Pageinfo *    the page info pointer
      Role_pagenum,                    // int           the page number
      Role_coverage,                   // QString       page coverage
      Role_blank,                      // bool          true if page is blank
      Role_remove,                     // bool          true if page is marked to remove
      Role_scanning,                   // bool          true if the page is being scanned

      Role_count
      };

   /************************ MODEL ACCESS FUNCTIONS *******************/
   /** retrieve data from the model

      \param index   index to retrieve
      \param role    role required

      \returns data from the model */
   QVariant data (const QModelIndex &index, int role = Qt::DisplayRole) const;

   /** set data in the model and update the view

      \param index   index to update
      \param value   data value
      \param role    role required */
   bool setData (const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

   /** returns flags for a given model index

      \param index   index to check

      \returns flags for the given index */
   Qt::ItemFlags flags (const QModelIndex &index) const;

   /** returns the number of rows under a given parent

      \param parent     the parent index to check

      \returns number of rows under the parent */
   int rowCount (const QModelIndex &parent) const;

   /** returns the number of columns under a given parent

      \param parent     the parent index to check

      \returns number of columns under the parent */
   int columnCount (const QModelIndex &parent) const;

   /** returns the drop actions supported by this model

      \returns drop actions */
   Qt::DropActions supportedDropActions() const;

   /** returns an index given the parent, row and column

      \param row     row to retrieve
      \param column  column to retrieve
      \param parent  parent index

      \returns the index of the item, or QModelIndex() if none was found */
   QModelIndex index (int row, int col, const QModelIndex& parent) const;

   /** looks up a filename and returns its index

      \param fname   filename to look up
      \returns the model index for that filename, which is null if not found */
   QModelIndex index (QString &fname) const;

   /** returns the parent of a given index

      \param index   index to check

      \returns the parent of the item */
   QModelIndex parent (const QModelIndex& index) const;

   //* returns a list of mime types that we can generate */
   QStringList mimeTypes() const;

   QMimeData *mimeData (const QModelIndexList &list) const;

   /************************* scanning *******************************/
public:
   /** indicate that we are starting a scan, which we will own */
   void beginningScan (void);

   /** indicate that a new page is being scanned. We add it to the model
       and await updates through newScaledImage() */
   void beginningPage (void);

   /** indicate that a new scaled image is available for the page currently
       being scanned. We record it and provide it to the view

      \param image            scaled image to display
      \param scaled_linenum  destination start line for this fragment */
   void newScaledImage (const QImage &image, int scaled_linenum);

   /** tell the model to disassociate itself with the scanning, since the
       user has clicked on another stack. This will halt previews */
   void disownScanning (void);

   /** indicate that we are ending a scan */
   void endingScan (void);

public slots:
   /** handle new pages being scanned into our 'scanning' stack

      \param coverageStr   coverage string
      \param mark_blank    true if page should be marked blank */
   void slotNewScannedPage (const QString &coverageStr, bool mark_blank);

signals:
   /* indicate that part of a page has changed

      \param index      page which has changed
      \param image      the new partial image
      \param scaled_linenum the start line number for the new image */
   void pagePartChanged (const QModelIndex &index, const QImage &image, int scaled_linenum);

   /************************* other functions **************************/

public:
   /** sets the maximum size for each page image. The preview images shown
       will match this in at least one dimension, and exceed it in neither

      \param size    maximum pixel size */
   void setPagesize (QSize size);

   /** sets the scale factor to use for the pages list - the actual factor
       used is 1/n. So 24 means 1/24 scale.

      \param scale_down    scale down factor */
   void setScale (int scale_down);

   int scale (void) const { return _scale_down; }

   QSize pagesize (void) const { return _pagesize; }

   /** gets a pixmap for the given page

      \param ind     index of page
      \param size    required size in pixels
      \param pixmap  returns pixmap
      \param blank   true if the pixmap should be modified to show the page as blank */
   void getPixmap (const QModelIndex &ind, QSize &size, QPixmap &pixmap, bool blank = false) const;

   /** indicates that one or more pages need rescaling. This will start doing
       this in the background */
   void scheduleRescale (void);

   /** ensure that we are rescaling, and start doing so if not */
   void ensureRescale (void);

   /** commit any changes to the stack

      \return error, or NULL if none */
   struct err_info *commit (void);

   /** mark all pages as kept */
   void keepAllPages (void);

   /** updates the annotation string for the current stack

      \param type    type to update (MAXA_...)
      \param str     string */
   void updateAnnot (int type, QString str);

protected:
   /** ensures that the given item number is set up ready for use, and
       returns a pointer to it */
   Pageinfo *ensurePage (int item);

   /** remove rows from the model */
   bool removeRows (int row, int count, const QModelIndex &parent);

protected slots:
   void nextUpdate (void);    //!< do the next rescale update

private:
   const Desktopmodel *_contents;    //!< stack model
   QPersistentModelIndex _stackindex;  //!< index of stack in _model that we are displaying
   int _column_count;      //!< number of columns
   int _start;             //!< start page to show
   int _count;             //!< number of pages to show
   int _row_count;         //!< number of rows
   QSize _pagesize;        //!< max pixel size of the page images
   QVector<Pageinfo> _pages;   //!< the pages in the file
   int _scale_down;        //!< scale factor (e.g. 24 means 1/24)

   /** these fields are for rescaling operations */
   QTimer *_updateTimer;   //!< timer for background scaling operations
   int _update_upto;       //!< where we are up to with updating
   bool _rescaling;        //!< true if we are rescaling in the background
   QImage _scan_image;     //!< the scan image scaled for us by Desktopmodel
   bool _own_scan;         //!< true if we own the scanning operation
   bool _lost_scan;        //!< true if we did own the scanning operation, but lost it
   const Desktopmodel *_lost_contents;    //!< lost stack model
   QVector<Pageinfo> _scan_pages;   //!< scanned pages
   QHash<int, QString> _annot_updates;
   };
