cmake_minimum_required(VERSION 3.2.3)
project(examples)

message("Building examples with CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

add_compile_options(-Wall -Werror)
#add_compile_options($<$<CONFIG:DEBUG>:-Og>)

link_libraries(libzth)

add_executable(example example.cpp)
add_executable(socks socks.cpp)
add_executable(work work.cpp)

