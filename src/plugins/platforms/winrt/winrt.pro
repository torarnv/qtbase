TARGET = qwinrt
CONFIG -= precompile_header

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QWinRTIntegrationPlugin
load(qt_plugin)

QT += core-private gui-private platformsupport-private

LIBS += -ld3d11
DEFINES += __WRL_NO_DEFAULT_LIB__

winrt:QMAKE_CXXFLAGS += -ZW -EHsc

SOURCES = \
    main.cpp  \
    pointervalue.cpp \
    qwinrtbackingstore.cpp \
    qwinrteventdispatcher.cpp \
    qwinrtintegration.cpp \
    qwinrtkeymapper.cpp \
    qwinrtpageflipper.cpp \
    qwinrtscreen.cpp \
    qwinrtwindow.cpp

HEADERS = \
    pointervalue.h \
    qwinrtbackingstore.h \
    qwinrteventdispatcher.h \
    qwinrtintegration.h \
    qwinrtkeymapper.h \
    qwinrtpageflipper.h \
    qwinrtscreen.h \
    qwinrtwindow.h

OTHER_FILES += winrt.json

RESOURCES += \
    winrtfonts.qrc
