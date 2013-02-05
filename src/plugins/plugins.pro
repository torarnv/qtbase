TEMPLATE = subdirs

SUBDIRS *= sqldrivers
qtHaveModule(network): SUBDIRS += bearer
qtHaveModule(gui): SUBDIRS *= imageformats platforms platforminputcontexts platformthemes generic
qtHaveModule(widgets): SUBDIRS += accessible

!winrt:!wince*:!contains(QT_CONFIG, no-widgets):SUBDIRS += printsupport
