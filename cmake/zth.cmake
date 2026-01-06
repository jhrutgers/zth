# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

if(TARGET libzth)
	return()
endif()

cmake_policy(VERSION 3.10)

option(ZTH_DEV "Enable development related build options" OFF)
option(ZTH_DRAFT_API "Enable draft API" OFF)
option(ZTH_HAVE_LIBZMQ "Use libzmq" OFF)
option(ZTH_ENABLE_ASAN "Build with Address Sanitizer" OFF)
option(ZTH_ENABLE_LSAN "Build with Leak Sanitizer" OFF)
option(ZTH_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" OFF)
option(ZTH_THREADS "Make libzth thread-aware" ON)
option(ZTH_ENABLE_VALGRIND "Enable valgrind support" OFF)
option(ZTH_CLANG_TIDY "Run clang-tidy" OFF)
option(ZTH_DISABLE_EXCEPTIONS "Disable exceptions to reduce code size" OFF)
option(ZTH_DISABLE_RTTI "Disable RTTI, to reduce code size" OFF)

if(NOT ZTH_SOURCE_DIR)
	get_filename_component(ZTH_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
	set(ZTH_SOURCE_DIR
	    "${ZTH_SOURCE_DIR}"
	    CACHE INTERNAL ""
	)
endif()

if(NOT PYTHON_EXECUTABLE)
	if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
		find_package(
			Python
			COMPONENTS Interpreter
			REQUIRED
		)
		set(PYTHON_EXECUTABLE ${Python_EXECUTABLE})
	elseif(CMAKE_HOST_WIN32)
		find_program(PYTHON_EXECUTABLE python REQUIRED)
	else()
		find_program(PYTHON_EXECUTABLE python3 REQUIRED)
	endif()
endif()

include(${ZTH_SOURCE_DIR}/version/CMakeLists.txt)

add_library(
	libzth
	${ZTH_SOURCE_DIR}/src/assert.cpp
	${ZTH_SOURCE_DIR}/src/config.cpp
	${ZTH_SOURCE_DIR}/src/context.cpp
	${ZTH_SOURCE_DIR}/src/fiber.cpp
	${ZTH_SOURCE_DIR}/src/init.cpp
	${ZTH_SOURCE_DIR}/src/inline.cpp
	${ZTH_SOURCE_DIR}/src/io.cpp
	${ZTH_SOURCE_DIR}/src/main.cpp
	${ZTH_SOURCE_DIR}/src/perf.cpp
	${ZTH_SOURCE_DIR}/src/poller.cpp
	${ZTH_SOURCE_DIR}/src/time.cpp
	${ZTH_SOURCE_DIR}/src/util.cpp
	${ZTH_SOURCE_DIR}/src/waiter.cpp
	${ZTH_SOURCE_DIR}/src/worker.cpp
	${ZTH_SOURCE_DIR}/src/zmq.cpp
	${ZTH_SOURCE_DIR}/src/zth_logv.cpp
)

set(ZTH_HEADERS
    ${ZTH_SOURCE_DIR}/include/libzth/allocator.h
    ${ZTH_SOURCE_DIR}/include/libzth/async.h
    ${ZTH_SOURCE_DIR}/include/libzth/config.h
    ${ZTH_SOURCE_DIR}/include/libzth/context.h
    ${ZTH_SOURCE_DIR}/include/libzth/fiber.h
    ${ZTH_SOURCE_DIR}/include/libzth/fsm.h
    ${ZTH_SOURCE_DIR}/include/libzth/fsm14.h
    ${ZTH_SOURCE_DIR}/include/libzth/init.h
    ${ZTH_SOURCE_DIR}/include/libzth/io.h
    ${ZTH_SOURCE_DIR}/include/libzth/list.h
    ${ZTH_SOURCE_DIR}/include/libzth/macros.h
    ${ZTH_SOURCE_DIR}/include/libzth/perf.h
    ${ZTH_SOURCE_DIR}/include/libzth/poller.h
    ${ZTH_SOURCE_DIR}/include/libzth/regs.h
    ${ZTH_SOURCE_DIR}/include/libzth/sync.h
    ${ZTH_SOURCE_DIR}/include/libzth/time.h
    ${ZTH_SOURCE_DIR}/include/libzth/util.h
    ${ZTH_SOURCE_DIR}/include/libzth/version.h
    ${ZTH_SOURCE_DIR}/include/libzth/waiter.h
    ${ZTH_SOURCE_DIR}/include/libzth/worker.h
    ${ZTH_SOURCE_DIR}/include/libzth/zmq.h
)

set_target_properties(libzth PROPERTIES OUTPUT_NAME "zth" PUBLIC_HEADER "${ZTH_HEADERS}")

if(ZTH_DRAFT_API)
	target_compile_definitions(libzth PUBLIC -DZTH_DRAFT_API)
endif()

# Workaround glibc's __longjmp_chk, which may trigger on our unusual setjmp/longjmp
set_source_files_properties(
	${ZTH_SOURCE_DIR}/src/context.cpp PROPERTIES COMPILE_FLAGS -U_FORTIFY_SOURCE
)

if(ZTH_CONTEXT)
	string(TOUPPER "${ZTH_CONTEXT}" ZTH_CONTEXT)
	message(STATUS "Prefer context switch approach ${ZTH_CONTEXT}")
	target_compile_definitions(libzth PUBLIC -DZTH_CONTEXT_${ZTH_CONTEXT})
endif()

target_include_directories(
	libzth PUBLIC $<BUILD_INTERFACE:${ZTH_PREPEND_INCLUDE_DIRECTORIES}>
		      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
)

target_compile_options(
	libzth
	PRIVATE -Wall
		-Wextra
		-Werror
		-Wdouble-promotion
		-Wformat=2
		-Wundef
		-Wconversion
		-Wshadow
		-Wswitch-default
		-Wswitch-enum
		-Wfloat-equal
		-ffunction-sections
		-fdata-sections
		-Wlogical-op
)
if(CMAKE_CROSSCOMPILING)
	target_compile_options(libzth PRIVATE -fstack-usage)
endif()

if(ZTH_DEV)
	if(NOT WIN32)
		target_compile_options(libzth PUBLIC -fstack-protector-strong)
	endif()
endif()

if(ZTH_DISABLE_EXCEPTIONS)
	target_compile_options(libzth PRIVATE -fno-exceptions)
endif()

if(ZTH_DISABLE_RTTI)
	target_compile_options(libzth PUBLIC -fno-rtti)
endif()

if(ZTH_CONFIG_ENABLE_DEBUG_PRINT)
	target_compile_options(libzth PRIVATE -DZTH_CONFIG_ENABLE_DEBUG_PRINT=1)
endif()

install(
	TARGETS libzth
	EXPORT libzth
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libzth
)

install(FILES include/zth include/zth.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
set(ZTH_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/include/zth_config.h)
foreach(d IN LISTS ZTH_PREPEND_INCLUDE_DIRECTORIES)
	if(EXISTS ${d}/zth_config.h)
		set(ZTH_CONFIG_FILE ${d}/zth_config.h)
		break()
	endif()
endforeach()
install(FILES ${ZTH_CONFIG_FILE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT libzth DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libzth/cmake)

configure_package_config_file(
	cmake/ZthConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/ZthConfig.cmake"
	INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/Zth
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ZthConfig.cmake
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/Zth
)

if(ZTH_HAVE_LIBZMQ)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_LIBZMQ)
	target_link_libraries(libzth PUBLIC libzmq)
endif()

if(NOT APPLE)
	check_include_file_cxx("libunwind.h" ZTH_HAVE_LIBUNWIND)
	if(ZTH_HAVE_LIBUNWIND)
		target_compile_definitions(libzth PRIVATE -DZTH_HAVE_LIBUNWIND)
		target_link_libraries(libzth INTERFACE unwind)
	endif()
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(libzth PUBLIC -DNDEBUG)
endif()

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

	if(CMAKE_THREAD_LIBS_INIT)
		target_link_libraries(libzth PUBLIC ${CMAKE_THREAD_LIBS_INIT})
	endif()
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

check_include_file_cxx("valgrind/memcheck.h" ZTH_HAVE_VALGRIND)
if(NOT ZTH_ENABLE_ASAN
   AND ZTH_HAVE_VALGRIND
   AND ZTH_ENABLE_VALGRIND
)
	target_compile_definitions(libzth PUBLIC -DZTH_HAVE_VALGRIND)
endif()

if(NOT CMAKE_CROSSCOMPILING
   AND CMAKE_BUILD_TYPE STREQUAL "Debug"
   AND NOT MINGW
)
	# Improve backtraces.
	target_link_options(libzth INTERFACE -rdynamic)
endif()

if(ZTH_CLANG_TIDY)
	find_program(
		CLANG_EXE
		NAMES "clang"
		DOC "Path to clang executable"
	)

	if(CLANG_EXE AND NOT CLANG_TIDY_EXE14)
		execute_process(
			COMMAND ${CLANG_EXE} -dumpversion
			OUTPUT_VARIABLE CLANG_VERSION
			ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		# We need clang-tidy 14 or later for --config-file.
		if("${CLANG_VERSION}" VERSION_GREATER_EQUAL 14)
			find_program(
				CLANG_TIDY_EXE14
				NAMES "clang-tidy"
				DOC "Path to clang-tidy executable"
			)

			if(CLANG_TIDY_EXE14)
				message(STATUS "Found clang-tidy ${CLANG_VERSION}")
			endif()
		endif()
	endif()

	if(CLANG_TIDY_EXE14)
		message(STATUS "Enabled clang-tidy for libzth")

		set(DO_CLANG_TIDY
		    "${CLANG_TIDY_EXE14}" "--config-file=${ZTH_SOURCE_DIR}/.clang-tidy"
		    "--extra-arg=-I${ZTH_SOURCE_DIR}/include"
		    "--extra-arg=-I${CMAKE_INSTALL_PREFIX}/include"
		)

		if(CMAKE_CXX_STANDARD)
			set(DO_CLANG_TIDY "${DO_CLANG_TIDY}"
					  "--extra-arg=-std=c++${CMAKE_CXX_STANDARD}"
			)
		endif()

		set_target_properties(libzth PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
	else()
		set_target_properties(libzth PROPERTIES CXX_CLANG_TIDY "")
	endif()
endif()
