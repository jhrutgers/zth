# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

function(add_example example)
	add_executable(${example} ${ARGN})
	target_compile_options(
		${example}
		PRIVATE -Wall
			-Wextra
			-Werror
			-Wdouble-promotion
			-Wformat=2
			-Wundef
			-Wconversion
			-Wno-error=missing-braces
			-Wno-missing-braces
			-Wno-missing-field-initializers
	)
	target_link_libraries(${example} libzth)
	zth_clang_tidy(
		${example} -cppcoreguidelines-avoid-non-const-global-variables -cert-err58-cpp
		-hicpp-use-emplace
	)

	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		# Don't use the CRT's version which calls WinMain for a GUI, but use zth's weak
		# main(), if the use does not specify one.
		target_link_libraries(${example} -Wl,-umain)
		target_link_libraries(${example} -Wl,-uzth_logv)
	endif()

	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
		add_custom_command(
			TARGET ${example}
			POST_BUILD
			COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${example}>
		)

		target_link_options(
			${example} PRIVATE "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${example}.map"
		)
	endif()

	install(TARGETS ${example} RUNTIME DESTINATION bin)
	add_launch_json(${example})
endfunction()

add_example(1_helloworld 1_helloworld.cpp)
add_example(2_fibers 2_fibers.cpp)
add_example(3_coop 3_coop.cpp)
add_example(4_sync 4_sync.cpp)
add_example(5_perf 5_perf.cpp)
add_example(6_stack 6_stack.cpp)
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
	target_compile_options(6_stack PRIVATE -Wno-infinite-recursion)
endif()
if(ZTH_HAVE_CORO)
	add_example(7_coro 7_coro.cpp)
	set_property(TARGET 7_coro PROPERTY CXX_STANDARD 20)
endif()
add_example(9_c_api 9_c_api.c)

add_example(blinky blinky.cpp)
add_example(daemon_pattern daemon_pattern.cpp)

if("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES
   AND CMAKE_CXX_STANDARD GREATER_EQUAL 14
   AND CMAKE_CXX_STANDARD LESS 98
)
	add_example(measure measure.cpp)
	set_property(TARGET measure PROPERTY CXX_STANDARD 14)
endif()

if(ZTH_HAVE_CORO AND NOT ZTH_DISABLE_EXCEPTIONS)
	add_example(sock_factory sock_factory.cpp)
	set_property(TARGET sock_factory PROPERTY CXX_STANDARD 20)
endif()

add_example(socks socks.cpp)

if(UNIX)
	add_example(work work.cpp)

	if(CMAKE_CXX_STANDARD GREATER_EQUAL 98 OR CMAKE_CXX_STANDARD LESS 14)
		# Skip example.
	elseif("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES AND "${CMAKE_CXX_STANDARD}" STREQUAL
								   ""
	)
		add_example(fsm14 fsm14.cpp)
		set_property(TARGET fsm14 PROPERTY CXX_STANDARD 17)
	elseif("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES AND "${CMAKE_CXX_STANDARD}" STREQUAL
								   ""
	)
		add_example(fsm14 fsm14.cpp)
		set_property(TARGET fsm14 PROPERTY CXX_STANDARD 14)
	endif()
endif()

if(ZTH_HAVE_LIBZMQ)
	add_example(zmq zmq.cpp)
endif()
