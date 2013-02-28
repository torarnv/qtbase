TARGET = qwinrt
CONFIG -= precompile_header

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QWinRTIntegrationPlugin
load(qt_plugin)

QT += core-private gui-private platformsupport-private

LIBS += -ld3d11 -ldxgi
DEFINES += __WRL_NO_DEFAULT_LIB__

winrt:QMAKE_CXXFLAGS += -ZW -EHsc

SOURCES = \
    main.cpp \
    qwinrtbackingstore.cpp \
    qwinrteventdispatcher.cpp \
    #qwinrtglcontext.cpp \
    qwinrtintegration.cpp \
    qwinrtscreen.cpp \
    qwinrtwindow.cpp \
    pointervalue.cpp \
    qwinrtpageflipper.cpp

HEADERS = \
    qwinrtbackingstore.h \
    qwinrteventdispatcher.h \
    #qwinrtglcontext.h \
    qwinrtintegration.h \
    qwinrtscreen.h \
    qwinrtwindow.h \
    pointervalue.h \
    qwinrtpageflipper.h

OTHER_FILES += winrt.json

RESOURCES += \
    winrtfonts.qrc
