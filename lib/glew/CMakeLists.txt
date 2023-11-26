cmake_minimum_required(VERSION 3.9.4)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
  message(STATUS "Setting release build type")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
project(glew CXX C)

file(GLOB_RECURSE SRC *.cpp *.c)
add_library(glew ${SRC})
target_include_directories(glew PRIVATE src)
