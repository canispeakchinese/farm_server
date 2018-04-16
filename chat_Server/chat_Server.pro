QT       += sql

LIBS += /usr/lib/muduo/*

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle

SOURCES += main.cpp \
    farm_server.cpp

HEADERS += \
    connect_mysql.h \
    farm_server.h

