cmake_minimum_required(VERSION 3.2.3)
project(examples)

message("Building examples with CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

add_compile_options(-Wall -Werror -Wno-error=missing-braces -Wno-missing-braces)
#add_compile_options($<$<CONFIG:DEBUG>:-Og>)

link_libraries(libzth)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# Don't use the CRT's version which calls WinMain for a GUI,
	# but use zth's weak main(), if the use does not specify one.
	add_link_options("-Wl,-umain")
	add_link_options("-Wl,-uzth_logv")
endif()

add_executable(1_helloworld 1_helloworld.cpp)
add_executable(2_fibers 2_fibers.cpp)
add_executable(3_coop 3_coop.cpp)
add_executable(4_sync 4_sync.cpp)
add_executable(5_perf 5_perf.cpp)
add_executable(6_stack 6_stack.cpp)
add_executable(9_c_api 9_c_api.c)
add_executable(socks socks.cpp)
add_executable(daemon_pattern daemon_pattern.cpp)
add_executable(measure measure.cpp)
add_executable(blinky blinky.cpp)

if(NOT WIN32)
	add_executable(work work.cpp)
	add_executable(fsm fsm.cpp)
endif()

if(ZTH_HAVE_LIBZMQ)
	add_executable(zmq zmq.cpp)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_subdirectory(arm EXCLUDE_FROM_ALL)
endif()

