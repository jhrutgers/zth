#!/bin/bash

# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -eu

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)"
pushd "${here}" > /dev/null

cmake_opts="-DZTH_DIST_DIR='${here}' -DCMAKE_TOOLCHAIN_FILE:PATH='${here}/toolchain.cmake' \
	-DZTH_THREADS=OFF -DZTH_HAVE_LIBZMQ=OFF -DZTH_CONFIG_ENABLE_DEBUG_PRINT=1 \
	-DZTH_PREPEND_INCLUDE_DIRECTORIES='${here}/include'"

. ../common/build.sh

popd > /dev/null
