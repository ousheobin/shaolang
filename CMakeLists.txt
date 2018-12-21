cmake_minimum_required(VERSION 3.12)
project(shaolang)

set(CMAKE_CXX_STANDARD 14)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DLINUX -DLF)
    message(STATUS "current platform: Linux ")
elseif (WIN32)
    add_definitions(-DWINDOWS -DCRLF)
    message(STATUS "current platform: Windows")
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_definitions(-DMAC_OSX -DLF)
    message(STATUS "current platform: OSX")
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

include_directories(src)

add_executable(shaolang
        src/shaolang.cpp
        src/shaolang.h src/argument.h src/lexer.cpp src/lexer.h src/scanner.h src/scanner.cpp)
