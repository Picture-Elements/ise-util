
CONFIG += qt
FORMS += video_scope.ui
HEADERS += VideoScopeMain.h DeviceThread.h
SOURCES += main.cpp VideoScopeMain.cpp DeviceThread.cpp

unix:LIBS += -liseio
