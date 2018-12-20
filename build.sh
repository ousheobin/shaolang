#! /bin/bash
if [ -d ./build/ ]; then
    rm -rf ./build/
fi
mkdir ./build
mkdir ./build/bin
cd ./build/
cmake ../ >>./console_cmake.log
make