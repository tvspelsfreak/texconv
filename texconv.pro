CONFIG += c++11

QMAKE_CXXFLAGS += -std=c++11 \

#Required to get C++11x to work on OSX
macx {
  QMAKE_CXXFLAGS += -mmacosx-version-min=10.7 -stdlib=libc++
  LIBS += -mmacosx-version-min=10.7 -stdlib=libc++
}

SOURCES += \
    preview.cpp \
    palette.cpp \
    textool.cpp \
    twiddler.cpp \
    common.cpp \
    imagecontainer.cpp \
    conv16bpp.cpp \
    convpal.cpp

HEADERS += \
	vqtools.h \
    palette.h \
    twiddler.h \
    common.h \
    imagecontainer.h
