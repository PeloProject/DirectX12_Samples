cmake_minimum_required(VERSION 3.21)

project(Editor LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(Editor
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/RuntimeBridge.cpp
    src/RuntimeBridge.h
)

target_link_libraries(Editor PRIVATE Qt6::Widgets)

if (WIN32)
    target_compile_definitions(Editor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
