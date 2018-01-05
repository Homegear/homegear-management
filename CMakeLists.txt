cmake_minimum_required(VERSION 3.8)
project(homegear_management)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/GD.cpp
        src/GD.h
        src/IpcClient.cpp
        src/IpcClient.h
        src/main.cpp
        src/Settings.cpp
        src/Settings.h
        config.h)

add_custom_target(homegear-management COMMAND ../makeDebug.sh SOURCES ${SOURCE_FILES})

add_library(homegear_management ${SOURCE_FILES})
