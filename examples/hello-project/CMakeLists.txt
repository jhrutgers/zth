# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

# Let's assume, this is the top-level CMakeLists file for your project. This is what you have to do
# to get this project linked against Zth.

cmake_minimum_required(VERSION 3.10)
project(hello-project)

# Assuming you have Zth checked out as a git submodule, include the zth.cmake file by specifying the
# path directly.
include(../../cmake/zth.cmake)

# Just create your binary.
add_executable(hello-project main.cpp)

# Link Zth to it.
target_link_libraries(hello-project libzth)

# Done.
