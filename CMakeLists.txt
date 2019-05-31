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
        src/commons/argument.h
        src/lexer.cpp
        src/lexer.h
        src/commons/scanner.h
        src/commons/scanner.cpp
        src/commons/common.h
        src/commons/token.h
        src/commons/token.cpp
        src/commons/error.cpp
        src/commons/error.h
        src/commons/type_util.cpp
        src/commons/type_util.h
        src/compiler.cpp
        src/compiler.h
        src/commons/argument.cpp
        src/parser.cpp
        src/parser.h
        src/symbols/symbol_table.cpp
        src/symbols/symbol_table.h
        src/symbols/function.cpp
        src/symbols/function.h
        src/symbols/variable.cpp
        src/symbols/variable.h
        src/ir/inter_code.cpp
        src/ir/inter_code.h
        src/ir/inter_code_collection.cpp
        src/ir/inter_code_collection.h
        src/ir_generator.cpp
        src/ir_generator.h
        src/dataflow/block.cpp
        src/dataflow/block.h
        src/dataflow/data_flow_graph.cpp
        src/dataflow/data_flow_graph.h
        src/optimization/const_opt.cpp
        src/optimization/const_opt.h)
