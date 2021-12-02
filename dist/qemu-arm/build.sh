#!/bin/bash

set -e

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)"
pushd "${here}" > /dev/null

cmake_opts="-DZTH_DIST_DIR='${here}' -DCMAKE_TOOLCHAIN_FILE:PATH='${here}/toolchain.cmake' -DZTH_THREADS=OFF -DZTH_HAVE_LIBZMQ=OFF"
. ../common/build.sh

popd > /dev/null
