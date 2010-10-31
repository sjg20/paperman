TEMPLATE = app
LANGUAGE = C++
CONFIG += qt warn_on debug console
QT += network

QMAKE_CXXFLAGS += -DDELIVER
#LIBS += -ljpeg

SOURCES += delivermv.cpp transfer.cpp delivery.cpp err.cpp utils.cpp mem.cpp zip.cpp \
/usr/share/qtcreator/gdbmacros/gdbmacros.cpp

HEADERS += transfer.h delivery.h err.h utils.h mem.h zip.h


