QT += widgets testlib
CONFIG += testcase console c++17
CONFIG -= app_bundle
TARGET = stackedfractions-test
SOURCES += stackedfractions-test.cpp ../src/stackedfractions.cpp
HEADERS += ../src/stackedfractions.h
