TEMPLATE = app
TARGET = paperman-server
LANGUAGE = C++
QT += network
QT -= gui

CONFIG += qt warn_on console
CONFIG -= app_bundle

# Use QCoreApplication instead of QApplication
DEFINES += QT_NO_GUI

equals(QT_MAJOR_VERSION, 5) {
  QT += core
}

HEADERS += searchserver.h

SOURCES += searchserver.cpp \
    paperman-server.cpp

unix {
    target.path = usr/bin
    target.files = paperman-server
    INSTALLS += target
}

message("Building Paperman Search Server")
message("PDF conversion uses 'paperman -p' command")
message("Run 'qmake && make' to build")
