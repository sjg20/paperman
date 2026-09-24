# The fake scanner the tests drive, as a SANE back end: see fakescan.h
#
# paperman.pro builds this along with the tests, in test/fakescan under
# the build directory, where the tests look for it

TEMPLATE = lib
TARGET = sane-fakefujitsu
VERSION = 1.0.0
QT = core gui
CONFIG += c++11 warn_on
CONFIG -= debug_and_release

# only the SANE entry points and the back door are for other code
QMAKE_CXXFLAGS += -fvisibility=hidden

OBJECTS_DIR = .obj

SOURCES = fakescan.cpp
LIBS += -ljpeg
HEADERS = fakescan.h
