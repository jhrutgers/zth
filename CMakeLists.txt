cmake_minimum_required(VERSION 3.2.3)
project(Zth)
include(CheckIncludeFileCXX)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release FORCE)
endif()

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "CMake generation is not allowed within the source directory! \
    Remove the CMakeCache.txt file and try again from another folder, e.g.: \
    \
      rm CMakeCache.txt \
      mkdir build \
      cd build \
      cmake ..")
endif()

option(ZTH_HAVE_LIBZMQ "Use libzmq" ON)
if(ZTH_HAVE_LIBZMQ)
	if(MINGW)
		# See https://github.com/zeromq/libzmq/issues/3859
		set(ZMQ_CV_IMPL "win32api" CACHE STRING "Override auto-detect" FORCE)
	endif()

	if(APPLE)
		# Make sure home-brew headers are in there.
		include_directories(BEFORE SYSTEM /usr/local/include)
	endif()

	set(BUILD_TESTS OFF CACHE BOOL "Disabling building tests")
	set(BUILD_STATIC OFF CACHE BOOL "Don't build static library" FORCE)
	add_subdirectory(external/libzmq)
endif()

add_subdirectory(version)

set(ZTH_SOURCE_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "libzth source directory" FORCE)

add_library(libzth
	src/config.cpp
	src/context.cpp
	src/fiber.cpp
	src/init.cpp
	src/inline.cpp
	src/io.cpp
	src/main.cpp
	src/perf.cpp
	src/time.cpp
	src/util.cpp
	src/waiter.cpp
	src/worker.cpp
	src/zmq.cpp
	src/zth_logv.cpp
)
set_target_properties(libzth PROPERTIES OUTPUT_NAME "zth")

# Workaround glibc's __longjmp_chk, which may trigger on our unusual setjmp/longjmp
set_source_files_properties(src/context.cpp PROPERTIES COMPILE_FLAGS -U_FORTIFY_SOURCE)

target_include_directories(libzth PUBLIC include)

target_compile_options(libzth PRIVATE -Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -fstack-usage)
#target_compile_options(libzth PRIVATE $<$<CONFIG:DEBUG>:-Og>)

if(ZTH_HAVE_LIBZMQ)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_LIBZMQ)
	target_link_libraries(libzth PUBLIC libzmq)
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		add_custom_command(TARGET libzth POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/external/libzmq/bin/libzmq.dll ${CMAKE_CURRENT_BINARY_DIR})
	endif()
endif()

CHECK_INCLUDE_FILE_CXX("valgrind/memcheck.h" ZTH_HAVE_VALGRIND)
if(ZTH_HAVE_VALGRIND)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_VALGRIND)
endif()

if(NOT APPLE)
	CHECK_INCLUDE_FILE_CXX("libunwind.h" ZTH_HAVE_LIBUNWIND)
	if(ZTH_HAVE_LIBUNWIND)
		target_compile_definitions(libzth PUBLIC -DZTH_HAVE_LIBUNWIND)
		target_link_libraries(libzth INTERFACE unwind)
	endif()
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(libzth PUBLIC -DNDEBUG)
endif()

option(ZTH_THREADS "Make libzth thread-aware" ON)
if(ZTH_THREADS)
	target_compile_options(libzth PUBLIC -DZTH_THREADS=1)
else()
	target_compile_options(libzth PUBLIC -DZTH_THREADS=0)
endif()

if(UNIX OR MINGW)
	# Still compile/link with pthread, as it provides more than threads...
	set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	if(MINGW)
		set(THREADS_HAVE_PTHREAD_ARG 1)
	endif()
	find_package(Threads REQUIRED)
	target_link_libraries(libzth PUBLIC Threads::Threads)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(libzth INTERFACE rt)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if(NOT CMAKE_SYSTEM_NAME STREQUAL Generic)
		target_link_libraries(libzth INTERFACE dl)
	endif()
endif()

option(ZTH_BUILD_EXAMPLES "Build examples" ON)
if(ZTH_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

option(ZTH_TESTS "Run the tests" ON)
if(ZTH_TESTS)
	enable_testing()

	if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0")
		set(CTEST_OUTPUT_ON_FAILURE 1)
		list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
	endif()

	add_subdirectory(tests)
endif()

find_package(Doxygen)
option(ZTH_DOCUMENTATION "Create the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(ZTH_DOCUMENTATION)
	if(NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Doxygen is needed to build the documentation.")
	endif()

	add_custom_target(
		doc ALL
		COMMAND doxygen
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating API documentation with Doxygen (see doxygen/html/index.html)"
	)
endif()

