QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

QMAKE_LFLAGS_WINDOWS +=  /NODEFAULTLIB:LIBCMT
CONFIG += c++17 console

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    qcustomplot.cpp

HEADERS += \
    CyAPI.h \
    CyUSB30_def.h \
    UsbdStatus.h \
    VersionNo.h \
    cyioctl.h \
    mainwindow.h \
    qcustomplot.h \
    usb100.h \
    usb200.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: LIBS += -L$$PWD/libs/ -lCyAPI

INCLUDEPATH += $$PWD/libs
DEPENDPATH += $$PWD/libs

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/libs/CyAPI.lib
else:win32-g++: PRE_TARGETDEPS += $$PWD/libs/libCyAPI.a
