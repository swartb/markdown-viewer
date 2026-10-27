QT += widgets svg
CONFIG += c++17
TEMPLATE = app
TARGET = markdown-viewer

SOURCES += src/main.cpp src/mainwindow.cpp src/theme.cpp
HEADERS += src/mainwindow.h src/theme.h
RESOURCES += src/resources.qrc

OBJECTS_DIR = .obj
MOC_DIR = .moc
RCC_DIR = .rcc
