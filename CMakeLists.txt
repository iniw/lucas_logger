cmake_minimum_required(VERSION 3.5)

project(LucasLogger VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets SerialPort)

set(PROJECT_SOURCES
        main.cpp
        qlogger.cpp
        qlogger.h
        qlogger.ui
)

qt_add_executable(LucasLogger
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)
#Define target properties for Android with Qt 6 as:
set_property(TARGET LucasLogger APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android)
set_property(TARGET LucasLogger PROPERTY QT_ANDROID_TARGET_SDK_VERSION 33)
set_property(TARGET LucasLogger PROPERTY QT_ANDROID_MIN_SDK_VERSION 29)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

target_link_libraries(LucasLogger PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

set_target_properties(LucasLogger PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER lucas.logger.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS LucasLogger
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

qt_finalize_executable(LucasLogger)
