QT += core gui qml quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = windowtuner_live_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h
SOURCES += live_geometry.cpp ../src/backend.cpp ../src/safety.cpp ../src/placement.cpp \
           ../src/inspector.cpp ../src/workflows.cpp ../src/management.cpp ../src/appspace.cpp \
           ../src/health.cpp
RESOURCES += ../src/resources.qrc
SOURCES += ../src/migration.cpp
LIBS += -lcrypto
