cmake_minimum_required(VERSION 3.13)
project(examples)

message("Building examples with CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
add_compile_options(-Wall -Werror $<$<CONFIG:DEBUG>:-Og>)

link_libraries(zth)

add_executable(example example.cpp)
add_executable(socks socks.cpp)
add_executable(work work.cpp)

