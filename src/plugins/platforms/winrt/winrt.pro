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
    qwinrtbackingstore.cpp \
    qwinrtcursor.cpp \
    qwinrteventdispatcher.cpp \
    qwinrtinputcontext.cpp \
    qwinrtintegration.cpp \
    qwinrtkeymapper.cpp \
    qwinrtpageflipper.cpp \
    qwinrtscreen.cpp \
    qwinrtservices.cpp \
    qwinrtwindow.cpp

HEADERS = \
    qwinrtbackingstore.h \
    qwinrtcursor.h \
    qwinrteventdispatcher.h \
    qwinrtinputcontext.h \
    qwinrtintegration.h \
    qwinrtkeymapper.h \
    qwinrtpageflipper.h \
    qwinrtscreen.h \
    qwinrtservices.h \
    qwinrtwindow.h

OTHER_FILES += winrt.json

RESOURCES += \
    winrtfonts.qrc
