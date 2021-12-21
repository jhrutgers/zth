cmake_minimum_required(VERSION 3.3)
project(examples)

cmake_policy(SET CMP0057 NEW)

function(add_example example)
	add_executable(${example} ${ARGN})
	target_compile_options(${example} PRIVATE -Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -Wno-error=missing-braces -Wno-missing-braces -Wno-missing-field-initializers)
	target_link_libraries(${example} libzth)

	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		# Don't use the CRT's version which calls WinMain for a GUI,
		# but use zth's weak main(), if the use does not specify one.
		target_link_libraries(${example} -Wl,-umain)
		target_link_libraries(${example} -Wl,-uzth_logv)
	endif()

	install(TARGETS ${example}
		RUNTIME DESTINATION bin
	)
endfunction()

add_example(1_helloworld 1_helloworld.cpp)
add_example(2_fibers 2_fibers.cpp)
add_example(3_coop 3_coop.cpp)
add_example(4_sync 4_sync.cpp)
add_example(5_perf 5_perf.cpp)
add_example(6_stack 6_stack.cpp)
add_example(9_c_api 9_c_api.c)
add_example(socks socks.cpp)
add_example(daemon_pattern daemon_pattern.cpp)
add_example(blinky blinky.cpp)

if("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
	add_example(measure measure.cpp)
	set_property(TARGET measure PROPERTY CXX_STANDARD 14)
endif()

if(UNIX)
	add_example(work work.cpp)

	# deprecated
#	add_example(fsm fsm.cpp)

	if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
		add_example(fsm14 fsm14.cpp)
		set_property(TARGET fsm14 PROPERTY CXX_STANDARD 17)
	elseif("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
		add_example(fsm14 fsm14.cpp)
		set_property(TARGET fsm14 PROPERTY CXX_STANDARD 14)
	endif()
endif()

if(ZTH_HAVE_LIBZMQ)
	add_example(zmq zmq.cpp)
endif()
