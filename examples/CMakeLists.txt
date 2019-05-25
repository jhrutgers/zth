cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_COMPILER gcc-mp-8)
set(CMAKE_CXX_COMPILER g++-mp-8)

project (example)
	include_directories("../include")
	add_executable(example example.cpp ../src/util.cpp)
