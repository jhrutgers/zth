cmake_minimum_required(VERSION 3.2.3)
project(Zth)
include(CheckIncludeFileCXX)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release FORCE)
endif()

option(ZTH_HAVE_LIBZMQ "Use libzmq" ON)
if(ZTH_HAVE_LIBZMQ)
	set(BUILD_TESTS OFF CACHE BOOL "Disabling building tests")
	set(BUILD_STATIC OFF CACHE BOOL "Don't build static library" FORCE)
	add_subdirectory(external/libzmq)
endif()

add_subdirectory(examples)

file(GLOB_RECURSE sources src/*.cpp)
add_library(libzth ${sources})
set_target_properties(libzth PROPERTIES OUTPUT_NAME "zth")

target_include_directories(libzth PUBLIC include)

target_compile_options(libzth PRIVATE -Wall -Werror)
#target_compile_options(libzth PRIVATE $<$<CONFIG:DEBUG>:-Og>)

if(ZTH_HAVE_LIBZMQ)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_LIBZMQ)
	target_link_libraries(libzth PUBLIC libzmq)
endif()

CHECK_INCLUDE_FILE_CXX("valgrind/memcheck.h" ZTH_HAVE_VALGRIND)
if(ZTH_HAVE_VALGRIND)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_VALGRIND)
endif()

CHECK_INCLUDE_FILE_CXX("libunwind.h" ZTH_HAVE_LIBUNWIND)
if(ZTH_HAVE_LIBUNWIND)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_LIBUNWIND)
	target_link_libraries(libzth INTERFACE unwind)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(libzth PUBLIC -DNDEBUG)
endif()

option(ZTH_THREADS "Make libzth thread-aware" ON)
if(ZTH_THREADS)
	target_compile_options(libzth PUBLIC -DZTH_THREADS=1 -pthread)
#	target_link_options(libzth PUBLIC -pthread)
else()
	target_compile_options(libzth PUBLIC -DZTH_THREADS=0)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(libzth INTERFACE rt)
endif()

target_link_libraries(libzth INTERFACE dl)

