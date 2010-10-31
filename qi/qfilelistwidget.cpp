/***************************************************************************
                          qfilelistwidget.cpp  -  description
                             -------------------
    begin                : Sat Aug 26 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#include "resource.h"

#include "./pics/fileopen.xpm"
#include "./pics/setup.xpm"
#include "fileiosupporter.h"
#include "imageiosupporter.h"
#include "qextensionwidget.h"
#include "qfiledialogext.h"
#include "qfilelistwidget.h"
#include "qmultiscan.h"
#include "quiteinsane.h"
#include "qdraglistbox.h"
#include "qxmlconfig.h"

#include <qapplication.h>
#include <qbuttongroup.h>
#include <qclipboard.h>
#include <qcombobox.h>
#include <qcursor.h>
#include <qdir.h>
#include <qdragobject.h>
#include <qevent.h>
#include <qfile.h>
#include <qframe.h>
#include <qhbox.h>
#include <qheader.h>
#include <qimage.h>
#include <qinputdialog.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qpopupmenu.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qregexp.h>
#include <qslider.h>
#include <qstringlist.h>
#include <qstrlist.h>
#include <qtextstream.h>
#include <qtoolbutton.h>
#include <qvbox.h>
#include <qwhatsthis.h>


QFileListWidget::QFileListWidget(bool tm,QWidget *parent, const char *name )
                :QGroupBox(parent,name)
{
  mTextMode = tm;
  mFileCnt = -1;
  mpButtonClose = 0L;
  initWidget();
  loadSettings();
}

QFileListWidget::~QFileListWidget()
{
}
/**  */
void QFileListWidget::initWidget()
{
  QString qs;
	QPixmap pixmap((const char **)fileopen);
  QPushButton* tb1 = 0;
///////////////////////
//save mode group box
///////////////////////
  if(mTextMode)
	  setTitle(tr("Text list settings"));
  else
	  setTitle(tr("Image list settings"));
  setColumnLayout(0, Qt::Vertical );
  layout()->setSpacing( 4 );
  layout()->setMargin( 4 );
  QGridLayout* sublayout1;
  if(!mTextMode)
  {
  	sublayout1 = new QGridLayout(layout(),8,5);
    sublayout1->setSpacing( 6 );
    sublayout1->setMargin( 11 );
    sublayout1->setAlignment( Qt::AlignTop );
  }
  else
  {
  	sublayout1 = new QGridLayout(layout(),8,2);
    sublayout1->setSpacing( 6 );
    sublayout1->setMargin( 11 );
    sublayout1->setAlignment( Qt::AlignTop );
  }

  QLabel* label2 = new QLabel(tr("Folder:"),this);
  QHBox* qhb2 = new QHBox(this);
  qhb2->setSpacing(4);
  mpListDirLineEdit = new QLineEdit(qhb2);
  mpListDirLineEdit->setReadOnly(true);
	mpButtonListDir = new QToolButton(qhb2);
  mpButtonListDir->setPixmap(pixmap);
  qhb2->setStretchFactor(mpListDirLineEdit,1);

  QHBox* templatehb = new QHBox(this);
  QLabel* label2a = new QLabel(tr("Filename template:"),templatehb);
  mpFileGenerationButton = new QToolButton(templatehb);
  mpFileGenerationButton->setPixmap(QPixmap((const char **)setup));
  templatehb->setStretchFactor(label2a,1);
  QHBox* qhbox = new QHBox(this);
  qhbox->setSpacing(4);
  mpTemplateLineEdit = new QLineEdit(qhbox);//QLabel(qhbox);
  mpTemplateLineEdit->setReadOnly(true);
  qhbox->setStretchFactor(mpTemplateLineEdit,1);
  if(mTextMode)
    mpButtonTemplate = new QPushButton(tr("Ch&ange ..."),qhbox);
  else
    mpButtonTemplate = new QPushButton(tr("C&hange ..."),qhbox);
  QLabel* label2b;

  mpComboFileType = new QComboBox(this);
  if(mTextMode)
  {
    label2b = new QLabel(tr("File type:"),this);
    mpComboFileType->insertItem("TXT");
  }
  else
  {
    label2b = new QLabel(tr("Image type:"),this);
    QStrList sl = QImage::outputFormats();
    mpComboFileType->insertItem("BMP");
    //test whether JPEG is supported
    if(sl.find("JPEG") != -1)
    {
      mpComboFileType->insertItem("JPEG");
    }
    //test whether TIF is supported
    if(sl.find("TIF") != -1)
    {
      mpComboFileType->insertItem("TIF");
    }
    mpComboFileType->insertItem("PNG");
    mpComboFileType->insertItem("PNM");
    mpComboFileType->insertItem("PBM");
    mpComboFileType->insertItem("PGM");
    mpComboFileType->insertItem("PPM");
    mpComboFileType->insertItem("XBM");
    mpComboFileType->insertItem("XPM");
    tb1 = new QPushButton(this);
    tb1->setPixmap(QPixmap((const char **)setup));
    tb1->resize(tb1->sizeHint());
    connect(tb1,SIGNAL(clicked()),this,SLOT(slotImageSettings()));
  }
//the listbox
	mpListBox = new QDragListBox(this);
  mpListBox->setMinimumHeight(100);
  mpListBox->setSelectionMode(QListView::Extended);
  mpListBox->addColumn("");
  mpListBox->header()->hide();
//delete button
  if(mTextMode)
  	mpButtonDelete = new QPushButton(tr("D&elete"),this);
  else
  	mpButtonDelete = new QPushButton(tr("&Delete"),this);
  mpButtonDelete->setEnabled(FALSE);
///add to sublayout
  if(!mTextMode)
  {
  	sublayout1->addWidget(label2,0,0);
  	sublayout1->addMultiCellWidget(qhb2,1,1,0,4);
  	sublayout1->addWidget(templatehb,2,0);
  	sublayout1->addMultiCellWidget(label2b,2,2,2,3);
  	sublayout1->addMultiCellWidget(qhbox,3,3,0,1);
  	sublayout1->addMultiCellWidget(mpComboFileType,3,3,2,3);
 	  sublayout1->addWidget(tb1,3,4);
  	sublayout1->addMultiCellWidget(mpListBox,6,6,0,4);
  	sublayout1->addMultiCellWidget(mpButtonDelete,7,7,3,4);
    sublayout1->setColStretch(0,1);
    sublayout1->setRowStretch(6,1);
  }
  else
  {
  	sublayout1->addWidget(label2,0,0);
  	sublayout1->addMultiCellWidget(qhb2,1,1,0,1);
  	sublayout1->addWidget(templatehb,2,0);
  	sublayout1->addWidget(label2b,2,1);
  	sublayout1->addWidget(qhbox,3,0);
  	sublayout1->addWidget(mpComboFileType,3,1);
  	sublayout1->addMultiCellWidget(mpListBox,6,6,0,1);
  	sublayout1->addWidget(mpButtonDelete,7,1);
    sublayout1->setColStretch(0,1);
    sublayout1->setRowStretch(6,1);
  }
	connect(mpListBox,SIGNAL(signalDragStarted(QListViewItem*)),
          this,SLOT(slotStartDrag(QListViewItem*)));	

  if(mTextMode)
  {
  	connect(mpListBox,SIGNAL(doubleClicked(QListViewItem*)),
            this,SLOT(slotDisplayText(QListViewItem*)));	
  	connect(mpComboFileType,SIGNAL(activated(int)),
            this,SLOT(slotTypeChanged(int)));	
  }
  else
  {
  	connect(mpListBox,SIGNAL(doubleClicked(QListViewItem*)),
            this,SLOT(slotDisplayImage(QListViewItem*)));	
  	connect(mpComboFileType,SIGNAL(activated(int)),
            this,SLOT(slotTypeChanged(int)));	
  	mFileType = ".bmp";
  }
  connect(mpFileGenerationButton,SIGNAL(clicked()),
          this,SLOT(slotFilenameGenerationSettings()));
	connect(mpButtonDelete,SIGNAL(clicked()),this,SLOT(slotDeleteFile()));	
  connect(mpButtonTemplate,SIGNAL(clicked()),this,SLOT(slotNewFileTemplate()));	
	connect(mpButtonListDir,SIGNAL(clicked()),this,SLOT(slotChangeFolder()));	
	connect(mpListBox,SIGNAL(selectionChanged()),
            this,SLOT(slotSelectionChanged()));	
  //help
  createWhatsThisHelp();
}
/**  */
void QFileListWidget::slotClearList()
{
	mpListBox->clear();
}
/**  */
void QFileListWidget::createContents()
{
  ImageIOSupporter iosup;
  FileIOSupporter fiosup;
  int counter_width;
  unsigned int c;
  QString qs;
  QString ext;
  QStringList filelist;
  mFolderName = mpListDirLineEdit->text();
  mFileNameTemplate = mpTemplateLineEdit->text();

  if(xmlConfig->boolValue("FILE_GENERATION_PREPEND_ZEROS",false) == true)
    counter_width = xmlConfig->intValue("FILE_GENERATION_COUNTER_WIDTH",3);
  else
    counter_width = 0;
qDebug("filelistwidget counter_width: %i",counter_width);
  if(!mTextMode)
  {
    ext = iosup.getExtensionByFormat(mpComboFileType->currentText());
  }
  else
  {
    if(mpComboFileType->currentText() == "TXT")
      ext = ".txt";
    else
      ext = QString::null;
  }
  filelist = fiosup.getAbsPathList(mFolderName,mFileNameTemplate,counter_width,ext);
qDebug("filelistwidget filelist.count(): %i",int(filelist.count()));

  mpListBox->clear();
  mpButtonDelete->setEnabled(false);
  for(c=0;c<filelist.count();c++)
  {
    mpListBox->insertItem(*filelist.at(c));
  }
  slotSelectionChanged();
}
/**Return a filename with absolute path based on the
filetemplate name and the given file type.  */
QString QFileListWidget::getFilename()
{
	QString qs;//null string
	QString qs2;
	qs = mFolderName;
	if(qs.right(1) != "/")
    qs += "/";
	qs += mFileNameTemplate;
	return qs;
}
/**Return a filename with absolute path based on the
filetemplate name and the given file type.  */
void QFileListWidget::addFilename(QString fn)
{
  mpListBox->insertItem(fn);
}
/**  */
void QFileListWidget::slotDisplayImage(QListViewItem* lvi)
{
 	QString qs;
  qs = mFolderName;
	if(qs.right(1) != "/") qs += "/";
	qs += lvi->text(0);
  QuiteInsane* qi = new QuiteInsane();
  if(!qi->statusOk())
  {
    QMessageBox::warning(0,tr("Error"),
                         tr("Could not create image viewer."),tr("Cancel"));	

    delete qi;
    return;
  }
  qi->show();
  qi->loadImage(qs);
}
/**  */
void QFileListWidget::slotChangeFolder()
{
  QFileDialogExt fd(mFolderName,QString::null,this,"",TRUE);
  fd.setMode(QFileDialog::DirectoryOnly);
  fd.setViewMode((QFileDialog::ViewMode)xmlConfig->intValue("SINGLEFILE_VIEW_MODE"));
  fd.setCaption(tr("Select a folder"));
  if(fd.exec())
	{
    mpListDirLineEdit->setText(fd.dir()->absPath());
		mFolderName=fd.dir()->absPath();
    createContents();
    if(mTextMode)
      xmlConfig->setStringValue("TEXTLIST_SAVE_PATH",mFolderName);
    else
      xmlConfig->setStringValue("FILELIST_SAVE_PATH",mFolderName);
  }
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",fd.intViewMode());
}
/**  */
void QFileListWidget::slotDeleteFile()
{
  int i;
	QString qs;
  i = QMessageBox::warning(this,tr("Delete file ..."),
         tr("Do you really want to delete the selected files?\n"
    	  	"They can not be restored.\n"),
          tr("&Delete"),tr("&Cancel"));
  if(i == 1) return;
  QListViewItemIterator it(mpListBox);
  for ( ; it.current(); ++it )
  {
    if(it.current()->isSelected())
    {
    	qs = mFolderName;
    	if(mFolderName.right(1) != "/") qs += "/";
    	qs += it.current()->text(0);
      if(!QFile::remove(qs))
      {
        i = QMessageBox::warning(this,tr("Error"),
            tr("The file\n\n %1 \n\n could not be deleted.\n"
               "Do you want to continue with the next file?").arg(qs),
            tr("Continue"),tr("Cancel"));
        if(i == 1) break;
      }
    }
  }
 	createContents();//just to be sure the contents is up to date
}
/**  */
void QFileListWidget::slotImageSettings()
{
  QExtensionWidget ew(this);
  ew.setPage(4);
  ew.exec();
}
/**  */
void QFileListWidget::slotFilenameGenerationSettings()
{
  QExtensionWidget ew(this);
  ew.setPage(10);
  ew.exec();
  //crappy
  QObject* o;
  o = parent();
  if(o)
  {
    if(o->isA("QMultiScan"))
    {
      QMultiScan* ms = (QMultiScan*)o;
      ms->createContents();
    }
  }
}
/**  */
void QFileListWidget::slotTypeChanged(int)
{
	QString type;
	type = mpComboFileType->currentText();
  QString qs;
  qs.setNum((int)mpComboFileType->currentItem());
  if(mTextMode)
    xmlConfig->setStringValue("TEXTLIST_FILE_TYPE",qs);
  else
    xmlConfig->setStringValue("FILELIST_IMAGE_TYPE",qs);
  createContents();
}
/**  */
QString QFileListWidget::format()
{
  return mpComboFileType->currentText();
}
/**  */
void QFileListWidget::slotNewFileTemplate()
{
  bool ok;
#ifdef USE_QT3
  QString text = QInputDialog::getText(tr("New filename template"),
                                  tr("Please enter a new name"),
                                  QLineEdit::Normal,
                                  mFileNameTemplate, &ok, this );
#else
  QString text = QInputDialog::getText(tr("New filename template"),
                                  tr("Please enter a new name"),
                                  mFileNameTemplate, &ok, this );
#endif
  if(!ok || text.isEmpty())return;//user entered nothing
  mFileNameTemplate = text;
  mpTemplateLineEdit->setText(text);
  if(mTextMode)
    xmlConfig->setStringValue("TEXTLIST_TEMPLATE",mFileNameTemplate);
  else
    xmlConfig->setStringValue("FILELIST_TEMPLATE",mFileNameTemplate);
 createContents();
}
/**  */
void QFileListWidget::createWhatsThisHelp()
{
  if(!mTextMode)
  {
    //file type
    QWhatsThis::add(mpComboFileType,
                    tr("Use this combobox to select the "
                       "image format."));
    //list box
    QWhatsThis::add(mpListBox,
                    tr("Shows the already scanned images that fit the "
                       "file name template. A double click "
                       "on a list box item opens the image in QuiteInsanes "
                       "internal viewer.\n"
                       "It is also possible to drag files to other "
                       "applications.\n Press down CTRL or SHIFT to select "
                       "multiple files."));
  }
  else
  {
    //file type
    QWhatsThis::add(mpComboFileType,
                    tr("Use this combobox to select the "
                       "filename extension."));
    //list box
    QWhatsThis::add(mpListBox,
                    tr("Shows the already recognized text files that fit the "
                       "file name template. A double click "
                       "on a list box item opens the file in QuiteInsanes "
                       "internal text viewer.\n"
                       "It is also possible to drag files to other "
                       "applications.\n Press down CTRL or SHIFT to select "
                       "multiple files."));
  }

//delete
  QWhatsThis::add(mpButtonDelete,tr("Deletes the selected file."));
//template name
  QWhatsThis::add(mpTemplateLineEdit,
                  tr("Shows the current file name template. An "
    							   "increasing number and an extension "
                     "will be appended."));
//list dir
  QWhatsThis::add(mpListDirLineEdit,
                  tr("Shows the current working directory."));
//change list dir
  QWhatsThis::add(mpButtonListDir,
                  tr("Click this button to show a file dialog "
                     "which lets you change the working directory."));
//close
  if(mpButtonClose)
    QWhatsThis::add(mpButtonClose,tr("Close this window."));
//template button
  QWhatsThis::add(mpButtonTemplate,
                  tr("Shows a dialog which allows you to change "
                     "the file name template."));
}
/**  */
void QFileListWidget::loadSettings()
{
  QString qs;
  if(mTextMode)
  {
    mFolderName = xmlConfig->stringValue("TEXTLIST_SAVE_PATH");
	  mpListDirLineEdit->setText(mFolderName);
    mFileNameTemplate = xmlConfig->stringValue("TEXTLIST_TEMPLATE");
	  mpTemplateLineEdit->setText(mFileNameTemplate);
    mpComboFileType->setCurrentItem(xmlConfig->intValue("TEXTLIST_FILE_TYPE"));
    slotTypeChanged(0);//Index doesn't matter
  }
  else
  {
    mFolderName = xmlConfig->stringValue("FILELIST_SAVE_PATH");
	  mpListDirLineEdit->setText(mFolderName);

    mFileNameTemplate = xmlConfig->stringValue("FILELIST_TEMPLATE");
	  mpTemplateLineEdit->setText(mFileNameTemplate);

    mpComboFileType->setCurrentItem(xmlConfig->intValue("FILELIST_IMAGE_TYPE"));
    slotTypeChanged(0);//Index doesn't matter
  }
}
/**  */
void QFileListWidget::slotDisplayText(QListViewItem* lvi)
{
 	QString qs;//null string
  qs = mFolderName;
	if(qs.right(1) != "/") qs += "/";
	qs += lvi->text(0);
  QuiteInsane* qi = new QuiteInsane(QuiteInsane::Mode_TextOnly,0);
  qi->show();
  qi->loadText(qs);
  qi->setTextModified(false);
}
/**  */
void QFileListWidget::slotSelectionChanged()
{
  QListViewItemIterator it(mpListBox);
  for ( ; it.current(); ++it )
  {
    if(it.current()->isSelected())
    {
      mpButtonDelete->setEnabled(TRUE);
      return;
    }
  }
  mpButtonDelete->setEnabled(FALSE);
}
/**  */
void QFileListWidget::slotStartDrag(QListViewItem*)
{
  QDragObject *d;
 	QString qs;
  QString text;
  QStrList slist;
  QCString uri;

  //we (try to) drag all selected items
  QListViewItemIterator it(mpListBox);
  for ( ; it.current(); ++it )
  {
    if(it.current()->isSelected())
    {
      qs = mFolderName;
	    if(qs.right(1) != "/") qs += "/";
    	qs += it.current()->text(0);
      uri = QUriDrag::localFileToUri(qs);
      slist.append(uri);
    }
  }
  if(slist.isEmpty()) return;
  d = new QUriDrag(slist,this);
  d->dragCopy();
}
