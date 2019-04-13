cmake_minimum_required(VERSION 2.8)
project(shaolang)

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DLINUX -DLF)
    message(STATUS "current platform: Linux ")
    add_compile_options(-std=c++11)
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
        src/shaolang.h
        src/argument.h
        src/lexer.cpp
        src/lexer.h
        src/scanner.h
        src/scanner.cpp
        src/common.h
        src/token.h
        src/token.cpp
        src/keyword.h
        src/keyword.cpp
        src/error.cpp
        src/error.h
        src/compiler.cpp
        src/compiler.h
        src/argument.cpp
        src/parser.cpp
        src/parser.h src/symbol_table.cpp src/symbol_table.h src/symbols.cpp src/symbols.h src/inter_code.cpp src/inter_code.h src/ir_generator.cpp src/ir_generator.h)
