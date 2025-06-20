QT       += core gui

LIBS += -luser32 -lpsapi -lgdi32 -ldwmapi -ld3d11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    backend/dxwindowcapture.cpp \
    backend/mediator.cpp \
    features/droparea.cpp \
    main.cpp \
    ui/mainwindow.cpp
HEADERS += \
    backend/dxwindowcapture.h \
    backend/mediator.h \
    features/droparea.h \
    ui/mainwindow.h

FORMS += \
    ui/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
