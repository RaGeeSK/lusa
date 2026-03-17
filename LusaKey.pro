QT += core gui qml quick quickcontrols2

CONFIG += c++20
CONFIG += windows
TEMPLATE = app
TARGET = LusaKey

SOURCES += \
    src/main.cpp \
    src/appcontroller.cpp \
    src/vaultentry.cpp \
    src/vaultlistmodel.cpp \
    src/vaultservice.cpp \
    src/crypto/cryptoservice.cpp

HEADERS += \
    src/appcontroller.h \
    src/vaultentry.h \
    src/vaultlistmodel.h \
    src/vaultservice.h \
    src/crypto/cryptoservice.h

RESOURCES += lusakey.qrc

win32: LIBS += -lbcrypt
unix:!win32: LIBS += -lcrypto
