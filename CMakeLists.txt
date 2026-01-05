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

if(${CMAKE_VERSION} VERSION_GREATER "3.6.0" AND NOT CMAKE_CROSSCOMPILING)
	find_program(
		CLANG_TIDY_EXE
		NAMES "clang-tidy"
		DOC "Path to clang-tidy executable"
	)
	if(CLANG_TIDY_EXE AND (NOT CMAKE_CXX_STANDARD OR NOT CMAKE_CXX_STANDARD EQUAL 98))
		# It seems that if clang is not installed, clang-tidy doesn't work properly.
		find_program(
			CLANG_EXE
			NAMES "clang"
			DOC "Path to clang executable"
		)
		if(CLANG_EXE
		   AND ZTH_DEV
		   AND NOT WIN32
		)
			option(ZTH_CLANG_TIDY "Run clang-tidy" ${ZTH_DEV_OPTION})
		else()
			option(ZTH_CLANG_TIDY "Run clang-tidy" OFF)
		endif()

		if(ZTH_CLANG_TIDY)
			message(STATUS "Enabled clang-tidy")

			string(
				CONCAT CLANG_TIDY_CHECKS
				       "-checks="
				       "bugprone-*,"
				       "-bugprone-easily-swappable-parameters,"
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
				       "-readability-identifier-length,"
				       "-readability-implicit-bool-conversion,"
				       "-readability-magic-numbers,"
				       "-readability-make-member-function-const,"
				       "-readability-redundant-access-specifiers,"
				       "-readability-uppercase-literal-suffix,"
				       "performance-*,"
				       "-performance-no-int-to-ptr," # Especially on WIN32 HANDLEs.
				       "portability-*,"
			)
			set(DO_CLANG_TIDY
			    "${CLANG_TIDY_EXE}"
			    "${CLANG_TIDY_CHECKS}"
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
			${CPPCHECK_CMD} --std=c++11 --language=c++ -D__cplusplus=201103L
			--enable=warning,style,information --force --inline-suppr --quiet
			--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr
			--error-exitcode=1 -j 4 --library=gnu
			"--template=[{file}:{line}]: ({severity},{id}) {message}" -D__GNUC__=10
			-DCPPCHECK -U__ARM_ARCH
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
