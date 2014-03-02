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


#include <QAbstractItemDelegate>
#include <QLineEdit>
#include <QPersistentModelIndex>


class Desktopmodelconv;
struct measure_info;


class Desktopeditor : public QLineEdit
   {
   Q_OBJECT
public:
   Desktopeditor (QWidget * parent = 0);
   ~Desktopeditor ();

signals:
   /** indicates that editing should finish

      \param editor  the editor
      \param save    true to save the data, false to forget it */
   void myEditingFinished (Desktopeditor *editor, bool save);

public slots:
   void slotEditingFinished ();

protected:
   void keyPressEvent (QKeyEvent *event);
   };


class Desktopdelegate : public QAbstractItemDelegate
   {
   Q_OBJECT

public:
   enum e_point  // which point of an item was clicked on
      {
      Point_other,  // somewhere else
      Point_left,   // left arrow
      Point_right,  // right arrow
      Point_page    // page selector (in between arrows)
      };

   Desktopdelegate (Desktopmodelconv *modelconv, QObject * parent = 0);
   ~Desktopdelegate ();

   //! paint the delegate
   void paint (QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

   //! get a size hint for the delegate
   QSize sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const;

   //! handle an editor event
   bool editorEvent (QEvent * event, QAbstractItemModel *model,
      const QStyleOptionViewItem &option, const QModelIndex &index);

   QWidget *createEditor(QWidget *parent,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

   void setEditorData(QWidget *editor,
                                       const QModelIndex &index) const;

   void setModelData(QWidget *editor, QAbstractItemModel *model,
                                       const QModelIndex &index) const;

   void updateEditorGeometry(QWidget *editor,
      const QStyleOptionViewItem &option, const QModelIndex &/* index */) const;

   /** works out which part of an item is being clicked on

      \param index      item model index
      \param option     style options
      \param pos        mouse position relative to top left of item

      \returns the part of the item clicked (enum point) */
   enum e_point find_which (const QModelIndex &index,
         const QStyleOptionViewItem &option, const QPoint &pos);

   bool containsPoint (const QStyleOptionViewItem &option,
            const QModelIndex &index, const QPoint &point) const;

   /** hint as to whether the user wants to edit the stack name or page name

      \param edit_page     true to edit page name, false for stack name */
   void setEditPageHint (bool edit_page);

   bool userBusy (void) { return _userbusy; }

signals:
   /** indicate that an item has been clicked

      \param index      item clicked
      \param which      which part of it was clicked (e_point) */
   void itemClicked (const QModelIndex &index, int which);

   /** indicate that an item has been double clicked

      \param index      item double clicked */
   void itemDoubleClicked (const QModelIndex &index);

//    void closeEditor (QWidget * editor, QAbstractItemDelegate::EndEditHint hint = QAbstractItemDelegate::NoHint);

   /** indicate that an item should be previewed

      \param index      item clicked
      \param which      which part of it was clicked
      \param now        true to preview now (else waits for user to finish clicking) */
   void itemPreview (const QModelIndex &index, int which, bool now);

public slots:
   void autoRepeatTimeout();

   /** indicates that editing should finish

      \param editor  the editor
      \param save    true to save the data, false to forget it */
   void slotEditingFinished (Desktopeditor *edit, bool save);

private:
   void recordEditor (Desktopeditor *editor);

   void measureItem (const QStyleOptionViewItem &option, const QModelIndex &index,
      measure_info &measure, QPoint *offset, bool size_only = false) const;

private:
   e_point _hitwhich;       //!< which item was hit (enum e_point)
   bool _userbusy;      //!< true if user is busy scrolling through pages
   QTimer *_timer;      //!< used for autorepeat on left/right button
   QPersistentModelIndex _hit;   //!< item clicked on for popup menu
   bool _edit_page;     //!< true to edit page name (instead of stack)
   int _edit_role;      //!< role to edit
   Desktopmodelconv *_modelconv; //!< convert between source and proxy models
   };

