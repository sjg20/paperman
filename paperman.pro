TEMPLATE = app
LANGUAGE = C++
QT += widgets
QT += printsupport
QT += network
QT += testlib
QT += sql
QT += concurrent

unix:target.path = usr/bin
target.files = paperman
INSTALLS += target

message ("Type 'make' to build paperman")
# To build with testing: qmake CONFIG+=test
# To measure test coverage: scripts/coverage.sh (uses CONFIG+=coverage)

coverage {
    message("Building with gcov coverage instrumentation")
    QMAKE_CXXFLAGS += --coverage -O0
    QMAKE_CFLAGS += --coverage -O0
    QMAKE_LFLAGS += --coverage
}

OCRINCPATH = /usr/local/include/nuance-omnipage-csdk-15.5
OCRLIBPATH = /usr/local/lib/nuance-omnipage-csdk-15.5

CONFIG += qt warn_on
CONFIG -= release
!debug:!coverage: QMAKE_CXXFLAGS += -O2

#QMAKE_LFLAGS += -static

equals(QT_MAJOR_VERSION, 6) {
  QT += statemachine
  LIBS += -lpoppler-qt6
  INCLUDEPATH += /usr/include/poppler/qt6
}
equals(QT_MAJOR_VERSION, 5) {
  LIBS += -lpoppler-qt5
  INCLUDEPATH += /usr/include/poppler/qt5
}

# libraries for omnipage
#LIBS += -lkernelapi -Wl,-rpath-link,$$OCRLIBPATH,-rpath,$$OCRLIBPATH

LIBS += -lpodofo
LIBS += -ltiff -ljpeg -lz

# There is no libsane on Windows: build against the bundled headers and a
# stub library, leaving only the simulated scanner. dlsym() is only used to
# look up the optional sane_read_dup() extension, so it goes too
win32 {
    INCLUDEPATH += win32
    SOURCES += win32/sanestub.cpp
} else {
    LIBS += -lsane -ldl
}

INCLUDEPATH += qi /usr/local/lib
INCLUDEPATH += $$OCRINCPATH

#LIBPATH += $$OCRLIBPATH
QMAKE_LIBDIR += $$OCRLIBPATH

INCLUDEPATH += /usr/include/podofo

#QMAKE_EXTRA_UNIX_TARGETS += dox
QMAKE_EXTRA_TARGETS += dox

RESOURCES = maxview.qrc

TRANSLATIONS = maxview_en.ts

HEADERS += desktopwidget.h \
   folderlist.h \
   foldersel.h \
   mainwidget.h \
   desk.h \
   measure.h \
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
 imageadjust.h \
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
    qlistwidgetitemiterator.h \
    searchindex.h \
    backend.h \
    backendstats.h \
    cachedfile.h \
    localbackend.h \
    remotebackend.h

SOURCES += desktopwidget.cpp \
   folderlist.cpp \
   mainwidget.cpp \
   desk.cpp \
   maxview.cpp \
   measure.cpp \
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
 imageadjust.cpp \
 filepdf.cpp \
 fileother.cpp \
 pdfio.cpp \
 ocrtess.cpp \
 ocromni.cpp \
 zip.cpp \
 senddialog.cpp \
 transfer.cpp \
    filejpeg.cpp \
    qlistwidgetitemiterator.cpp \
    searchindex.cpp \
    backend.cpp \
    backendstats.cpp \
    localbackend.cpp \
    remotebackend.cpp

# add qtcreator debug macros if we are debugging
#SOURCES += /usr/share/qtcreator/gdbmacros/gdbmacros.cpp

FORMS = mainwindow.ui \
   move.ui \
   presetadd.ui \
   pscan.ui \
   about.ui \
   options.ui \
        printopt.ui \
        pagetools.ui \
        pageattr.ui \
        ocrbar.ui send.ui \
        search.ui \
   toolbar.ui

test {
   SOURCES += test/test_utils.cpp \
      test/test.cpp \
      test/suite.cpp \
      test/test_desktopui.cpp \
      test/test_dirmodel.cpp \
      test/test_dirview.cpp \
      test/test_file.cpp \
      test/test_ops.cpp \
      test/test_pageinfo.cpp \
      test/test_pagewidget.cpp \
      test/test_qscanner.cpp \
      test/test_searchserver.cpp \
      test/test_ocrsearch.cpp \
      test/test_localbackend.cpp \
      searchserver.cpp \
      serverlog.cpp \
      tokenstore.cpp \
      userstore.cpp

   HEADERS += test/suite.h \
      test/test_desktopui.h \
      test/test_dirmodel.h \
      test/test_dirview.h \
      test/test_file.h \
      test/test_ops.h \
      test/test_pageinfo.h \
      test/test_pagewidget.h \
      test/test_qscanner.h \
      test/test_utils.h \
      test/test_searchserver.h \
      test/test_ocrsearch.h \
      test/test_localbackend.h \
      searchserver.h \
      serverlog.h \
      tokenstore.h \
      userstore.h

    QMAKE_CXXFLAGS += -DENABLE_TEST
    QT += concurrent
    # a GUI-subsystem executable has no stdout on Windows, so the test
    # results would be lost
    win32: CONFIG += console
}

# tif_fax3sm.c   - causes tifflib to break

#CONFIG  += qt warn_on release static

#release


# Keep the generated files out of the source directory. This must apply on
# every platform: the server build shares the directory and would otherwise
# pick up these objects, compiled with widgets, instead of its own
UI_DIR = .ui
MOC_DIR = .moc
OBJECTS_DIR = .obj

win32 {
    # MSYS2 lays the headers out under the toolchain prefix; qmake does not
    # know about them, so ask pkg-config for poppler and podofo
    CONFIG += link_pkgconfig
    PKGCONFIG += libpodofo
    equals(QT_MAJOR_VERSION, 6): PKGCONFIG += poppler-qt6
    equals(QT_MAJOR_VERSION, 5): PKGCONFIG += poppler-qt5
    # the tests run with the source directory as the working directory, so
    # keep the executable next to the sources rather than in debug/release
    CONFIG -= debug_and_release
    DESTDIR = .
}

QT += xml

# custom target 'doc' in *.pro file
dox.target = doc
dox.commands = doxygen Doxyfile; \
    test -d doxydoc/html/images || mkdir doxydoc/html/images; \
    cp documentation/images/* doxydoc/html/images
dox.depends =
