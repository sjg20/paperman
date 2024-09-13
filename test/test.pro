QT += widgets testlib

SOURCES = testpaperman.cpp ../utils.cpp ../zip.cpp ../epeglite.cpp \
	../err.cpp ../mem.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/qtestlib/tutorial1
INSTALLS += target

LIBS += -ltiff -lsane -ljpeg -lz
