#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

# This is a simple test script to verify that the project can be built and run.

set -xeuo pipefail

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

# Cleanup for testing
[[ ! -e build ]] || rm -rf build
mkdir build
cd build

# This builds the project using the repository.
mkdir build-static
cd build-static
cmake ../.. -DCMAKE_INSTALL_PREFIX="$(realpath ../install-static)" -DCMAKE_BUILD_TYPE=Debug -DZTH_SHARED_LIB=OFF
cmake --build .
./hello-project

# Install the project to a temporary location.
cmake --build . --target install
cd ..

# This builds the project using the installed version.
mkdir build-static-installed
cd build-static-installed
cmake ../.. -DCMAKE_PREFIX_PATH="$(realpath ../install-static)" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
./hello-project

# Again, but now with a shared library.
cd ..
mkdir build-shared
cd build-shared
cmake ../.. -DCMAKE_INSTALL_PREFIX="$(realpath ../install-shared)" -DCMAKE_BUILD_TYPE=Debug -DZTH_SHARED_LIB=ON
cmake --build .
./hello-project

# Install the project to a temporary location.
cmake --build . --target install
cd ..

# This builds the project using the installed version.
mkdir build-shared-installed
cd build-shared-installed
cmake ../.. -DCMAKE_PREFIX_PATH="$(realpath ../install-shared)" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
./hello-project

popd > /dev/null
