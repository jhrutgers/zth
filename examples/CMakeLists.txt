cmake_minimum_required(VERSION 3.13)
project (examples)

set(CMAKE_C_COMPILER gcc-mp-8)
set(CMAKE_CXX_COMPILER g++-mp-8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pthread -L/opt/local/lib")

include_directories("../include")
file(GLOB_RECURSE _HDRS "../include/*")
file(GLOB_RECURSE _SRCS "../src/*.cpp")
#add_library(zth ${_HDRS} ${_SRCS})
#add_library(zth ../src/util.cpp ../src/context.cpp ../src/worker.cpp ../src/waiter.cpp ../src/perf.cpp ../include/libzth/util.h)

link_libraries(unwind dl)
#add_executable(example example.cpp ${_SRCS})
add_executable(socks socks.cpp ${_SRCS})

#include_directories("../include")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pthread -L/opt/local/lib -lunwind -ldl")
#add_executable(example example.cpp ../src/util.cpp ../src/context.cpp ../src/worker.cpp ../src/waiter.cpp ../src/perf.cpp)
#
#project (socks)
#	include_directories("../include")
#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pthread -L/opt/local/lib -lunwind -ldl")
#	add_executable(socks socks.cpp ../src/util.cpp ../src/context.cpp ../src/worker.cpp ../src/waiter.cpp ../src/perf.cpp)
