QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = windowtuner
TEMPLATE = app

HEADERS += src/backend.h src/theme.h
SOURCES += src/main.cpp src/backend.cpp src/safety.cpp src/placement.cpp src/inspector.cpp src/workflows.cpp src/management.cpp src/appspace.cpp src/health.cpp src/theme.cpp
RESOURCES += src/resources.qrc
