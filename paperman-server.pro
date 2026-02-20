TEMPLATE = app
TARGET = paperman-server
LANGUAGE = C++
QT += network
QT += gui
QT += concurrent

CONFIG += qt warn_on console
CONFIG -= app_bundle

# No widgets module — Qt GUI only (for QImage, etc.)
DEFINES += QT_NO_WIDGETS

# Build date is written to builddate.h by GNUmakefile

equals(QT_MAJOR_VERSION, 5) {
  QT += core
  LIBS += -lpoppler-qt5
  INCLUDEPATH += /usr/include/poppler/qt5
}

LIBS += -lpodofo -ltiff -ljpeg -lz
INCLUDEPATH += /usr/include/podofo

HEADERS += builddate.h \
    searchserver.h \
    serverlog.h \
    file.h \
    filemax.h \
    filejpeg.h \
    filepdf.h \
    fileother.h \
    utils.h \
    pdfio.h \
    err.h \
    mem.h \
    epeglite.h \
    zip.h \
    zip_p.h \
    zipentry_p.h \
    config.h

SOURCES += searchserver.cpp \
    serverlog.cpp \
    paperman-server.cpp \
    file.cpp \
    filemax.cpp \
    filejpeg.cpp \
    filepdf.cpp \
    fileother.cpp \
    utils.cpp \
    pdfio.cpp \
    err.cpp \
    mem.cpp \
    epeglite.cpp \
    zip.cpp

unix {
    target.path = usr/bin
    target.files = paperman-server
    INSTALLS += target
}

message("Building Paperman Search Server")
message("Using File class for direct page access")
message("Run 'qmake && make' to build")
