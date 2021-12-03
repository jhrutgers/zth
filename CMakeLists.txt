# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2021  Jochem Rutgers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.2.3)
project(Zth)
include(CheckIncludeFileCXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
	message(FATAL_ERROR "CMake generation is not allowed within the source directory! \
		Remove the CMakeCache.txt file and try again from another folder, e.g.: \
		\
		  rm CMakeCache.txt \
		  mkdir build \
		  cd build \
		  cmake ..")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/deploy" CACHE PATH "Override default install path" FORCE)
endif()

option(ZTH_DEV "Enable by default development related build options" OFF)
if(ZTH_DEV)
	set(ZTH_DEV_OPTION ON)
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
else()
	set(ZTH_DEV_OPTION OFF)
endif()

option(ZTH_DRAFT_API "Enable draft API" OFF)
if(ZTH_DRAFT_API)
	message(STATUS "Enable Zth draft API")
endif()

option(ZTH_HAVE_LIBZMQ "Use libzmq" ON)
if(ZTH_HAVE_LIBZMQ)
	find_package(ZeroMQ REQUIRED)
endif()

if(ZTH_DEV_OPTION AND NOT CMAKE_CROSSCOMPILING AND NOT MINGW)
	set(ZTH_SAN_DEFAULT ON)
else()
	set(ZTH_SAN_DEFAULT OFF)
endif()

option(ZTH_ENABLE_ASAN "Build with Address Sanitizer" ${ZTH_SAN_DEFAULT})
option(ZTH_ENABLE_LSAN "Build with Leak Sanitizer" ${ZTH_SAN_DEFAULT})
option(ZTH_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" ${ZTH_SAN_DEFAULT})

add_subdirectory(version)

set(ZTH_SOURCE_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "libzth source directory" FORCE)

if(${CMAKE_VERSION} VERSION_GREATER "3.6.0" AND NOT CMAKE_CROSSCOMPILING)
	find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
	if(CLANG_TIDY_EXE AND (NOT CMAKE_CXX_STANDARD OR NOT CMAKE_CXX_STANDARD EQUAL 98))
		# It seems that if clang is not installed, clang-tidy doesn't work properly.
		find_program(CLANG_EXE NAMES "clang" DOC "Path to clang executable")
		if(CLANG_EXE AND ZTH_DEV AND NOT WIN32)
			option(ZTH_CLANG_TIDY "Run clang-tidy" ${ZTH_DEV_OPTION})
		else()
			option(ZTH_CLANG_TIDY "Run clang-tidy" OFF)
		endif()

		if(ZTH_CLANG_TIDY)
			message(STATUS "Enabled clang-tidy")

			string(CONCAT CLANG_TIDY_CHECKS "-checks="
				"bugprone-*,"
				"clang-analyzer-*,"
				"concurrency-*,"

				"cppcoreguidelines-*,"
				"-cppcoreguidelines-avoid-c-arrays,"
				"-cppcoreguidelines-avoid-goto,"
				"-cppcoreguidelines-avoid-magic-numbers,"
				"-cppcoreguidelines-avoid-non-const-global-variables,"
				"-cppcoreguidelines-explicit-virtual-functions,"
				"-cppcoreguidelines-macro-usage,"
				"-cppcoreguidelines-no-malloc,"
				"-cppcoreguidelines-owning-memory,"
				"-cppcoreguidelines-pro-bounds-array-to-pointer-decay,"
				"-cppcoreguidelines-pro-bounds-constant-array-index,"
				"-cppcoreguidelines-pro-bounds-pointer-arithmetic,"
				"-cppcoreguidelines-pro-type-reinterpret-cast,"
				"-cppcoreguidelines-pro-type-union-access,"
				"-cppcoreguidelines-pro-type-vararg,"

				"hicpp-*,"
				"-hicpp-avoid-c-arrays,"
				"-hicpp-avoid-goto,"
				"-hicpp-braces-around-statements,"
				"-hicpp-member-init,"
				"-hicpp-no-array-decay,"
				"-hicpp-no-malloc,"
				"-hicpp-signed-bitwise,"
				"-hicpp-uppercase-literal-suffix,"
				"-hicpp-use-auto,"
				"-hicpp-use-override,"
				"-hicpp-vararg,"

				"misc-*,"
				"-misc-no-recursion,"
				"-misc-non-private-member-variables-in-classes,"

				"readability-*,"
				"-readability-braces-around-statements,"
				"-readability-convert-member-functions-to-static,"
				"-readability-else-after-return,"
				"-readability-function-cognitive-complexity,"
				"-readability-implicit-bool-conversion,"
				"-readability-magic-numbers,"
				"-readability-make-member-function-const,"
				"-readability-redundant-access-specifiers,"
				"-readability-uppercase-literal-suffix,"

				"performance-*,"
				"-performance-no-int-to-ptr," # Especially on WIN32 HANDLEs.

				"portability-*,"
			)
			set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "${CLANG_TIDY_CHECKS}"
				"--extra-arg=-I${PROJECT_SOURCE_DIR}/include"
				"--extra-arg=-I${CMAKE_INSTALL_PREFIX}/include"
				"--warnings-as-errors=*"
				"--extra-arg=-Wno-unknown-warning-option"
			)

			set(CMAKE_CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
		else()
			set(CMAKE_CXX_CLANG_TIDY "")
		endif()
	endif()
endif()

add_library(libzth
	src/config.cpp
	src/context.cpp
	src/fiber.cpp
	src/init.cpp
	src/inline.cpp
	src/io.cpp
	src/main.cpp
	src/perf.cpp
	src/poller.cpp
	src/time.cpp
	src/util.cpp
	src/waiter.cpp
	src/worker.cpp
	src/zmq.cpp
	src/zth_logv.cpp
)
set_target_properties(libzth PROPERTIES OUTPUT_NAME "zth")

if(ZTH_DRAFT_API)
	target_compile_definitions(libzth PUBLIC -DZTH_DRAFT_API)
endif()

# Workaround glibc's __longjmp_chk, which may trigger on our unusual setjmp/longjmp
set_source_files_properties(src/context.cpp PROPERTIES COMPILE_FLAGS -U_FORTIFY_SOURCE)

target_include_directories(libzth PUBLIC
	$<BUILD_INTERFACE:${ZTH_PREPEND_INCLUDE_DIRECTORIES}>
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

target_compile_options(libzth PRIVATE -Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -ffunction-sections -fdata-sections)
if(CMAKE_CROSSCOMPILING)
	target_compile_options(libzth PRIVATE -fstack-usage)
endif()

if(ZTH_CONFIG_ENABLE_DEBUG_PRINT)
	target_compile_options(libzth PRIVATE -DZTH_CONFIG_ENABLE_DEBUG_PRINT=1)
endif()

install(TARGETS libzth EXPORT libzth
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include
)

configure_file(cmake/libzth.cmake.in ${CMAKE_BINARY_DIR}/libzth.cmake)
install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN "*.h")
install(FILES include/zth DESTINATION include)
install(EXPORT libzth DESTINATION share/libzth/cmake)
install(FILES ${CMAKE_BINARY_DIR}/libzth.cmake DESTINATION share/cmake/libzth)

if(ZTH_HAVE_LIBZMQ)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_LIBZMQ)
	target_link_libraries(libzth PUBLIC libzmq)
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

if(NOT COMMAND target_link_options)
	macro(target_link_options target scope)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ARGN}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ARGN}")
	endmacro()
endif()

if(ZTH_ENABLE_ASAN)
	target_compile_options(libzth PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_compile_definitions(libzth PRIVATE -DZTH_ENABLE_ASAN)
	target_link_options(libzth INTERFACE -fsanitize=address)
endif()

if(ZTH_ENABLE_LSAN)
	target_compile_options(libzth PRIVATE -fsanitize=leak -fno-omit-frame-pointer)
	target_compile_definitions(libzth PRIVATE -DZTH_ENABLE_LSAN)
	target_link_options(libzth INTERFACE -fsanitize=leak)
endif()

if(ZTH_ENABLE_UBSAN)
	target_compile_options(libzth PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
	target_compile_definitions(libzth PRIVATE -DZTH_ENABLE_UBSAN)
	target_link_options(libzth INTERFACE -fsanitize=undefined)
endif()

CHECK_INCLUDE_FILE_CXX("valgrind/memcheck.h" ZTH_HAVE_VALGRIND)
if(NOT ZTH_ENABLE_ASAN AND ZTH_HAVE_VALGRIND)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_VALGRIND)
endif()

if(NOT CMAKE_CROSSCOMPILING AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MINGW)
	# Improve backtraces.
	target_link_options(libzth INTERFACE -rdynamic)
endif()

option(ZTH_BUILD_EXAMPLES "Build examples" ON)
if(ZTH_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.11.0")
	if(CMAKE_CROSSCOMPILING)
		set(ZTH_TESTS_DEFAULT OFF)
	else()
		set(ZTH_TESTS_DEFAULT ${ZTH_DEV_OPTION})
	endif()

	option(ZTH_TESTS "Build the tests" ${ZTH_TESTS_DEFAULT})

	if(ZTH_TESTS)
		enable_testing()

		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0")
			set(CTEST_OUTPUT_ON_FAILURE 1)
			list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
		endif()

		add_subdirectory(tests)
	endif()
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

find_program(CPPCHECK_CMD NAMES cppcheck)
if(CPPCHECK_CMD)
	add_custom_target(libzth-cppcheck
		COMMAND
			${CPPCHECK_CMD}
			--std=c++11 --language=c++ -D__cplusplus=201103L
			--enable=warning,style,information
			--force
			--inline-suppr
			--quiet
			--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr
			--error-exitcode=1
			-j 4
			--library=gnu
			"--template=[{file}:{line}]: ({severity},{id}) {message}"
			-D__GNUC__=10 -DCPPCHECK
#			--xml
			-I include
			src
			examples
			tests
#			--check-config
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
		COMMENT "Running cppcheck"
	)

	option(ZTH_CPPCHECK "Run cppcheck by default" ${ZTH_DEV_OPTION})
	if(ZTH_CPPCHECK)
		add_custom_target(libzth-cppcheck-all ALL DEPENDS libzth-cppcheck)
	endif()
endif()

if(ZTH_DIST_DIR)
	add_subdirectory(${ZTH_DIST_DIR})
endif()
