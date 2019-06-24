cmake_minimum_required(VERSION 3.13)
project(zth)

option(ZTH_THREADS "Make libzth thread-aware" ON)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release FORCE)
endif()

add_subdirectory(examples)

file(GLOB_RECURSE sources src/*.cpp)

add_library(zth ${sources})

target_include_directories(zth PUBLIC include)

target_compile_options(zth PRIVATE -Wall -Werror)
#target_compile_options($<$<CONFIG:DEBUG>:-Og>)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(zth PUBLIC -DNDEBUG)
endif()

if(ZTH_THREADS)
	target_compile_options(zth PUBLIC -DZTH_THREADS=1 -pthread)
	target_link_options(zth PUBLIC -pthread)
else()
	target_compile_options(zth PUBLIC -DZTH_THREADS=0)
endif()

target_link_libraries(zth INTERFACE unwind dl)

