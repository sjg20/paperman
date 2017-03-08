/***************************************************************************
                          qmultilineeditpe.h  -  description
                             -------------------
    begin                : Sat Feb 10 2001
    copyright            : (C) 2001 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#ifndef QMULTILINEEDITPE_H
#define QMULTILINEEDITPE_H

#include <qdialog.h>
#include <qfont.h>

#include <qmultilineedit.h>
/**The dialog for the text editor setup.
  *@author Michael Herder
  */
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QMultiLineEditPE;
class QRadioButton;
class QToolButton;
class QPushButton;

class QTextEditorSetup : public QDialog
{
   Q_OBJECT
public:
	QTextEditorSetup(QMultiLineEditPE* parent,const char* name=0,
                   bool modal=false,WFlags f=0);
	~QTextEditorSetup();
private slots: // Private slots
  /**  */
  void slotUse();
  /**  */
  void slotSave();
  /**  */
  void slotChangeFont();
private:
  QPushButton* mpFontButton;
  QPushButton* mpSaveButton;
  QPushButton* mpCancelButton;
  QPushButton* mpUseButton;
  QToolButton* mpWhatsThisButton;
  QDoubleSpinBox* mpDoubleSpinLeft;
  QDoubleSpinBox* mpDoubleSpinTop;
  QDoubleSpinBox* mpDoubleSpinRight;
  QDoubleSpinBox* mpDoubleSpinBottom;
  QRadioButton* mpRadioKeepFormat;
  QRadioButton* mpRadioWordWrap;
  QCheckBox* mpCheckSelected;
  /**  */
  double mMetricFactor;
  /**  */
  QFont mFont;
  /**  */
  QLineEdit* mpLineEditFont;
private: //methods
  /**Initialize the dialog. */
  void initDlg();
  /**  */
  void createWhatsThisHelp();
};
/**A class derived from QMultiLineEdit with a printing extension
(PE), which makes it easier to print the contents of a
line edit widget.
  *@author Michael Herder
  */

class QMultiLineEditPE : public QMultiLineEdit
{
Q_OBJECT
public:
	QMultiLineEditPE(QWidget * parent=0, const char * name=0);
	~QMultiLineEditPE();
  /** Set the margins used for printing. The unit is millimetre.
  */
  void setMarginsMM(double left,double top,double right,double bottom);
  /** Set the left margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
  void setLeftMarginMM(double left);
 /** Set the right margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
  void setRightMarginMM(double right);
  /** Set the top margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
  void setTopMarginMM(double top);
  /** Set the bottom margin used for printing. The unit is millimetre.
    The default value is 10mm.
 */
  void setBottomMarginMM(double bottom);
  /**  */
  int lineWidthMax(QMultiLineEdit* le);
public slots: // Public slots
  /**  */
  void slotSetup();
  /**  Print the text. Note that there are two printmodes:
     1. Keep format
        This means, that the output is scaled down if neccessary.
     2. Auto word wrap
        This means, that linebreaks are inserted where neccessary.
     The default mode is mode 1. The mode can be changed through the setup
     dialog.
  */
  bool slotPrint();
private:
  double mLeftMargin;
  double mTopMargin;
  double mRightMargin;
  double mBottomMargin;
  /**  */
  bool mPrintSelected;
  /**  */
  int mPrintMode;

  friend class QTextEditorSetup;
protected: // Protected methods
  /** No descriptions */
  void dragMoveEvent(QDragMoveEvent* e);
protected: // Protected methods
  /** No descriptions */
  void dropEvent(QDropEvent* e);
};

#endif
