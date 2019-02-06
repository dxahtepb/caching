cmake_minimum_required(VERSION 3.13)
project(cachingpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-O3")

add_executable(cachingpp main.cpp cache.h)