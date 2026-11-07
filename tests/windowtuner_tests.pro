QT += core testlib
CONFIG += c++17 testcase
TARGET = windowtuner_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h
SOURCES += windowtuner_tests.cpp ../src/backend.cpp
