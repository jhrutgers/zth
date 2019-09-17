cmake_minimum_required(VERSION 3.2.3)
project(examples)

message("Building examples with CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

add_compile_options(-Wall -Werror)
#add_compile_options($<$<CONFIG:DEBUG>:-Og>)

link_libraries(libzth)

add_executable(1_helloworld 1_helloworld.cpp)
add_executable(2_fibers 2_fibers.cpp)
add_executable(3_coop 3_coop.cpp)
add_executable(4_sync 4_sync.cpp)
add_executable(5_perf 5_perf.cpp)
add_executable(9_c_api 9_c_api.c)
add_executable(example example.cpp)
add_executable(socks socks.cpp)
add_executable(work work.cpp)
add_executable(daemon_pattern daemon_pattern.cpp)
add_executable(measure measure.cpp)
add_executable(fsm fsm.cpp)

if(ZTH_HAVE_LIBZMQ)
	add_executable(zmq zmq.cpp)
endif()

