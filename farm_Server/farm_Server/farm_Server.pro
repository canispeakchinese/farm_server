#-------------------------------------------------
#
# Project created by QtCreator 2017-01-19T20:09:57
#
#-------------------------------------------------

QT       += sql

LIBS += /usr/lib/muduo/*

TARGET = farm_Server
TEMPLATE = app

CONFIG += console c++11

SOURCES += main.cpp \
    farm_server.cpp

HEADERS  += \
    connect_mysql.h \
    farm_server.h
