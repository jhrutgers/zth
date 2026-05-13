#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

# This is a simple test script to verify that the project can be built and run.

set -xeuo pipefail

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

# Cleanup for testing
[[ ! -e build ]] || rm -rf build
[[ ! -e install ]] || rm -rf install
[[ ! -e build-installed ]] || rm -rf build-installed

# This builds the project using the repository.
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="$(realpath ../install)" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
./hello-project

# Install the project to a temporary location.
cmake --build . --target install
cd ..

# This builds the project using the installed version.
mkdir build-installed
cd build-installed
cmake .. -DCMAKE_PREFIX_PATH="$(realpath ../install)" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
./hello-project
