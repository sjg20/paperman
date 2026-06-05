TEMPLATE = app
TARGET = paperman-client
LANGUAGE = C++
QT = core network

CONFIG += qt warn_on console
CONFIG -= app_bundle

DEFINES += QT_NO_WIDGETS

HEADERS += backend.h \
    remotebackend.h

SOURCES += paperman-client.cpp \
    backend.cpp \
    remotebackend.cpp

unix {
    target.path = usr/bin
    target.files = paperman-client
    INSTALLS += target
}

message("Building Paperman client CLI")
