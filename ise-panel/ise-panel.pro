
CONFIG += qt
win32:INCLUDEPATH += "win32"
win32:DEFINES += ISE_PANEL_WIN32 WINNT

FORMS   += ise_panel.ui
HEADERS += IsePanelMain.h
SOURCES += main.cpp IsePanelMain.cpp

INCLUDEPATH += ../sys-common/
