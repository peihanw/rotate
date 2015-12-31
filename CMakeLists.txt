## memo on howto build release version
## $ mkdir build
## $ cd build
## $ cmake -DCMAKE_BUILD_TYPE=Release ..
## $ make
cmake_minimum_required(VERSION 2.4)
project(rotate)
set(SOURCE_FILES rotate.cc)
add_executable(rotate ${SOURCE_FILES})
