#!/bin/bash

set -e

function nproc {
	sysctl -n hw.logicalcpu
}

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

# Use brew's gcc
[[ ! -z ${CC:-} ]]  || CC=`ls -1 /usr/local/bin/gcc-[0-9]* | sort -n | tail -n 1`
[[ ! -z ${CXX:-} ]] || CXX=`ls -1 /usr/local/bin/g++-[0-9]* | sort -n | tail -n 1`

cmake_opts=
. ../common/build.sh

popd > /dev/null
