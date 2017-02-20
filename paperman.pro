TEMPLATE = app
LANGUAGE = C++
QT += qt3support
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

unix:target.path = /usr/bin
INSTALLS += target

message ("Type 'make' to build paperman")

OCRINCPATH = /usr/local/include/nuance-omnipage-csdk-15.5
OCRLIBPATH = /usr/local/lib/nuance-omnipage-csdk-15.5

CONFIG += qt warn_on debug

#QMAKE_LFLAGS += -static

LIBS += -lpoppler-qt4

# libraries for omnipage
#LIBS += -lkernelapi -Wl,-rpath-link,$$OCRLIBPATH,-rpath,$$OCRLIBPATH

LIBS += -lpodofo
LIBS += -ltiff -lsane -ljpeg -lz

INCLUDEPATH += qi /usr/local/lib /usr/include/poppler/qt4
INCLUDEPATH += $$OCRINCPATH

#LIBPATH += $$OCRLIBPATH
QMAKE_LIBDIR += $$OCRLIBPATH

INCLUDEPATH += /usr/include/podofo

#QMAKE_EXTRA_UNIX_TARGETS += dox
QMAKE_EXTRA_TARGETS += dox

RESOURCES = maxview.qrc

TRANSLATIONS = maxview_en.ts

HEADERS += desktopwidget.h \
   mainwidget.h \
   desk.h \
   pagewidget.h \
   qscannersetupdlg.h \
   qscanner.h \
   qxmlconfig.h \
   saneconfig.h \
   qsanestatusmessage.h \
   qscandialog.h \
   qi/qcombooption.h \
   qi/previewwidget.h \
   qi/qbooloption.h \
   qi/qbuttonoption.h \
   qi/qdevicesettings.h \
   qi/qextensionwidget.h \
   qi/qlistviewitemext.h \
   qi/qoptionscrollview.h \
   qi/qreadonlyoption.h \
   qi/qsaneoption.h \
   qi/qscrollbaroption.h \
   qi/qstringoption.h \
   qi/qwordarrayoption.h \
   qi/qwordcombooption.h \
   qi/sanefixedoption.h \
   qi/saneintoption.h \
   qi/sanewidgetholder.h \
   qi/scanarea.h \
   qi/checklistitemext.h \
   qi/previewupdatewidget.h \
   qi/ruler.h \
   qi/scanareacanvas.h \
   qi/imagebuffer.h \
   qi/imageiosupporter.h \
   qi/imagedetection.h \
   qi/scanareatemplate.h \
   qi/qdoublespinbox.h \
   qi/qcurvewidget.h \
   qi/canvasrubberrectangle.h \
   qi/sanefixedspinbox.h \
   qi/qqualitydialog.h \
   qi/qimageioext.h \
   qi/qsplinearray.h \
   paperstack.h \
   err.h \
   mem.h \
   op.h \
   qi/sliderspin.h \
   desktopmodel.h \
   epeglite.h \
        options.h \
        pscan.h \
        mainwindow.h \
 pagetools.h \
 dirmodel.h \
 dirview.h \
 desktopview.h \
 desktopdelegate.h \
 desktopundo.h \
 printopt.h \
 pagemodel.h \
 pageview.h \
 pagedelegate.h \
 utils.h \
 ocr.h \
 file.h \
 filemax.h \
 filepdf.h \
 fileother.h \
 pdfio.h \
 ocrtess.h \
 ocromni.h \
 zip.h \
 zip_p.h \
 zipentry_p.h \
 senddialog.h \
 transfer.h \
    filejpeg.h \
    qlistwidgetitemiterator.h

SOURCES += desktopwidget.cpp \
   mainwidget.cpp \
   desk.cpp \
   maxview.cpp \
   pagewidget.cpp \
   md5.c \
   qscannersetupdlg.cpp \
   qscanner.cpp \
   qxmlconfig.cpp \
   saneconfig.cpp \
   qsanestatusmessage.cpp \
   qscandialog.cpp \
   qi/qcombooption.cpp \
   qi/previewwidget.cpp \
   qi/qbooloption.cpp \
   qi/qbuttonoption.cpp \
   qi/qdevicesettings.cpp \
   qi/qextensionwidget.cpp \
   qi/qlistviewitemext.cpp \
   qi/qoptionscrollview.cpp \
   qi/qreadonlyoption.cpp \
   qi/qsaneoption.cpp \
   qi/qscrollbaroption.cpp \
   qi/qstringoption.cpp \
   qi/qwordarrayoption.cpp \
   qi/qwordcombooption.cpp \
   qi/sanefixedoption.cpp \
   qi/saneintoption.cpp \
   qi/sanewidgetholder.cpp \
   qi/scanarea.cpp \
   qi/checklistitemext.cpp \
   qi/previewupdatewidget.cpp \
   qi/ruler.cpp \
   qi/scanareacanvas.cpp \
   qi/imagebuffer.cpp \
   qi/imageiosupporter.cpp \
   qi/imagedetection.cpp \
   qi/scanareatemplate.cpp \
   qi/qdoublespinbox.cpp \
   qi/qcurvewidget.cpp \
   qi/canvasrubberrectangle.cpp \
   qi/sanefixedspinbox.cpp \
   qi/qqualitydialog.cpp \
   qi/qimageioext.cpp \
   qi/qsplinearray.cpp \
   paperstack.cpp \
   err.cpp \
   mem.cpp \
   op.cpp \
   qi/sliderspin.cpp \
   desktopmodel.cpp \
   epeglite.cpp \
        options.cpp \
        pscan.cpp \
        mainwindow.cpp \
 pagetools.cpp \
 dirmodel.cpp \
 dirview.cpp \
 desktopview.cpp \
 desktopdelegate.cpp \
 desktopundo.cpp \
 printopt.cpp \
 pagemodel.cpp \
 pageview.cpp \
 pagedelegate.cpp \
 utils.cpp \
 ocr.cpp \
 dmop.cpp \
 dmuserop.cpp \
 file.cpp \
 filemax.cpp \
 filepdf.cpp \
 fileother.cpp \
 pdfio.cpp \
 ocrtess.cpp \
 ocromni.cpp \
 zip.cpp \
 senddialog.cpp \
 transfer.cpp \
    filejpeg.cpp \
    qlistwidgetitemiterator.cpp

# add qtcreator debug macros if we are debugging
#SOURCES += /usr/share/qtcreator/gdbmacros/gdbmacros.cpp



#The following line was changed from FORMS to FORMS3 by qt3to4
FORMS = mainwindow.ui \
   pscan.ui \
   about.ui \
   options.ui \
        printopt.ui \
        pagetools.ui \
        pageattr.ui \
        ocrbar.ui send.ui

IMAGES = images/filenew \
   images/fileopen \
   images/filesave \
   images/print \
   images/undo \
   images/redo \
   images/searchfind

# tif_fax3sm.c   - causes tifflib to break




#CONFIG  += qt warn_on release static

#release


unix {
    UI_DIR = .ui
    MOC_DIR = .moc
    OBJECTS_DIR = .obj
}


#The following line was inserted by qt3to4
QT += xml

# custom target 'doc' in *.pro file
dox.target = doc
dox.commands = doxygen Doxyfile; \
    test -d doxydoc/html/images || mkdir doxydoc/html/images; \
    cp documentation/images/* doxydoc/html/images
dox.depends =
