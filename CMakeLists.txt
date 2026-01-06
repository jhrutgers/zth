# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.10)
project(Zth)

include(CheckIncludeFileCXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE
	    Release
	    CACHE STRING "Build type" FORCE
	)
endif()

message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
	message(
		FATAL_ERROR
			"CMake generation is not allowed within the source directory! \
		Remove the CMakeCache.txt file and try again from another folder, e.g.: \
		\
		  rm CMakeCache.txt \
		  mkdir build \
		  cd build \
		  cmake .."
	)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX
	    "${CMAKE_CURRENT_BINARY_DIR}/deploy"
	    CACHE PATH "Override default install path" FORCE
	)
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

if(ZTH_DEV_OPTION
   AND NOT CMAKE_CROSSCOMPILING
   AND NOT MINGW
)
	set(ZTH_SAN_DEFAULT ON)
else()
	set(ZTH_SAN_DEFAULT OFF)
endif()

option(ZTH_ENABLE_ASAN "Build with Address Sanitizer" ${ZTH_SAN_DEFAULT})
option(ZTH_ENABLE_LSAN "Build with Leak Sanitizer" ${ZTH_SAN_DEFAULT})
option(ZTH_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" ${ZTH_SAN_DEFAULT})

if(APPLE)
	list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../lib")
	if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.8")
		list(APPEND CMAKE_BUILD_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
	else()
		set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
	endif()
endif()

set(ZTH_CLANG_TIDY_DEFAULT OFF)

find_program(
	CLANG_TIDY_EXE
	NAMES "clang-tidy"
	DOC "Path to clang-tidy executable"
)

if(# We must have clang-tidy.
   CLANG_TIDY_EXE
   AND # We must compile for C++11 or later...
       (NOT CMAKE_CXX_STANDARD OR NOT CMAKE_CXX_STANDARD EQUAL 98)
   AND # ...but Windows does not work properly, as there is an MSVC header conflict...
       (NOT WIN32)
   AND # ...and somehow mingw builds don't work properly on newer versions of clang-tidy.
       (NOT MINGW)
)
	# It seems that if clang is not installed, clang-tidy doesn't work properly.
	find_program(
		CLANG_EXE
		NAMES "clang"
		DOC "Path to clang executable"
	)
	if(CLANG_EXE AND ZTH_DEV)
		set(ZTH_CLANG_TIDY_DEFAULT ${ZTH_DEV_OPTION})
	endif()
endif()

option(ZTH_CLANG_TIDY "Run clang-tidy" ${ZTH_CLANG_TIDY_DEFAULT})

set(ZTH_SOURCE_DIR
    ${PROJECT_SOURCE_DIR}
    CACHE STRING "libzth source directory" FORCE
)

if(NOT PYTHON_EXECUTABLE AND EXISTS ${ZTH_SOURCE_DIR}/dist/venv/bin/python3)
	set(PYTHON_EXECUTABLE ${ZTH_SOURCE_DIR}/dist/venv/bin/python3)
endif()

option(ZTH_ENABLE_VALGRIND "Enable valgrind support" ${ZTH_DEV_OPTION})

list(APPEND CMAKE_MODULE_PATH ${ZTH_SOURCE_DIR}/cmake)
include(zth)

option(ZTH_REGEN_LAUNCH_JSON "Regenerate launch.json" OFF)
if(NOT EXISTS ${ZTH_SOURCE_DIR}/.vscode/launch.json OR ZTH_REGEN_LAUNCH_JSON)
	set(ZTH_GEN_LAUNCH_JSON ON)
else()
	set(ZTH_GEN_LAUNCH_JSON OFF)
endif()

set_property(GLOBAL PROPERTY ZTH_LAUNCH_JSON "")

function(add_launch_json TARGET)
	get_property(_launch GLOBAL PROPERTY ZTH_LAUNCH_JSON)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set_property(
			GLOBAL
			PROPERTY
				ZTH_LAUNCH_JSON
				"${_launch}
		{
			\"type\": \"cppdbg\",
			\"request\": \"launch\",
			\"name\": \"${TARGET}\",
			\"program\": \"$<TARGET_FILE:${TARGET}>\",
			\"cwd\": \"$<TARGET_FILE_DIR:${TARGET}>\",
			\"environment\": [
				{
					\"name\": \"ASAN_OPTIONS\",
					\"value\": \"detect_leaks=0\"
				}
			]
		},"
		)
	else()
		set_property(
			GLOBAL
			PROPERTY
				ZTH_LAUNCH_JSON
				"${_launch}
		{
			\"type\": \"f5anything\",
			\"request\": \"launch\",
			\"name\": \"${TARGET}\",
			\"command\": \"$<TARGET_FILE:${TARGET}>\",
		},"
		)
	endif()
endfunction()

option(ZTH_BUILD_EXAMPLES "Build examples" ON)
if(ZTH_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.11.0" AND "cxx_std_14" IN_LIST
						       CMAKE_CXX_COMPILE_FEATURES
)
	if(CMAKE_CROSSCOMPILING)
		if(UNIX
		   AND ZTH_DIST_DIR
		   AND EXISTS ${ZTH_DIST_DIR}/run.sh
		)
			set(ZTH_TESTS_DEFAULT ${ZTH_DEV_OPTION})
		else()
			set(ZTH_TESTS_DEFAULT OFF)
		endif()
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
option(ZTH_DOCUMENTATION "Create the HTML based API documentation (requires Doxygen)"
       ${DOXYGEN_FOUND}
)

if(ZTH_DOCUMENTATION)
	if(NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Doxygen is needed to build the documentation.")
	endif()

	add_custom_target(
		doc ALL
		COMMAND Doxygen::doxygen ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT
			"Generating API documentation with Doxygen (see ${CMAKE_CURRENT_BINARY_DIR}/doxygen/html/index.html)"
	)
endif()

find_program(CPPCHECK_CMD NAMES cppcheck)
if(CPPCHECK_CMD)
	add_custom_target(
		libzth-cppcheck
		COMMAND
			${CPPCHECK_CMD} --std=c++17 --language=c++ -D__cplusplus=201703L
			--enable=warning,style,information --force --inline-suppr --quiet
			--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr
			--error-exitcode=1 -j 4 --library=gnu
			"--template=[{file}:{line}]: ({severity},{id}) {message}" -D__GNUC__=10
			--enable=missingInclude -DCPPCHECK -U__ARM_ARCH
			# --xml
			-I include src examples tests -i tests/test_fsm14.cpp -i
			tests/test_fsm17.cpp
		# --check-config
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

add_custom_target(
	zth-reuse
	COMMAND ${PYTHON_EXECUTABLE} -m reuse lint
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	COMMENT "Checking REUSE compliance"
	VERBATIM
)

if(ZTH_DEV)
	add_custom_target(zth-reuse-all ALL DEPENDS zth-reuse)
endif()

if(ZTH_GEN_LAUNCH_JSON)
	get_property(_launch GLOBAL PROPERTY ZTH_LAUNCH_JSON)
	set(ZTH_LAUNCH_JSON
	    "\
{
	\"version\": \"0.2.0\",
	\"configurations\": [${_launch}
	]
}
"
	)

	file(
		GENERATE
		OUTPUT ${ZTH_SOURCE_DIR}/.vscode/launch.json
		CONTENT "${ZTH_LAUNCH_JSON}"
	)

	set(ZTH_REGEN_LAUNCH_JSON
	    OFF
	    CACHE INTERNAL "" FORCE
	)
endif()
