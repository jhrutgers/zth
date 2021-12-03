#!/bin/bash

# This file is sourced from a dist/*/ directory. Do not call directly.

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

function show_help {
	echo "Usage: $0 <build type> [<short flag>...] [--] [<cmake argument>...]"
	exit 2
}

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &> /dev/null; pwd -P)"

case ${1:-} in
	-?|-h|--help)
		show_help;;
esac

BUILD_TYPE="${1:-Release}"
shift || true

mkdir -p build
pushd build > /dev/null

# Simplify setting specific configuration flag.
support_test=1
do_test=0
use_ninja=0

if which ninja > /dev/null; then
	use_ninja=1
fi

while [[ ! -z ${1:-} ]]; do
	case "$1" in
		-?|-h|--help|help)
			show_help;;
		C++98|C++03)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=98 -DCMAKE_C_STANDARD=99"
			support_test=0
			;;
		C++11)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=11 -DCMAKE_C_STANDARD=11"
			support_test=0
			;;
		C++14)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=14 -DCMAKE_C_STANDARD=11";;
		C++17)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_STANDARD=17";;
		C++20)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=20 -DCMAKE_C_STANDARD=17";;
		dev)
			cmake_opts="${cmake_opts} -DZTH_DEV=ON"
			do_test=1
			;;
		draft)
			cmake_opts="${cmake_opts} -DZTH_DRAFT_API=ON";;
		zmq)
			cmake_opts="${cmake_opts} -DZTH_HAVE_LIBZMQ=ON";;
		nozmq)
			cmake_opts="${cmake_opts} -DZTH_HAVE_LIBZMQ=OFF";;
		san)
			cmake_opts="${cmake_opts} -DZTH_ENABLE_ASAN=ON -DZTH_ENABLE_LSAN=ON -DZTH_ENABLE_UBSAN=ON";;
		nosan)
			cmake_opts="${cmake_opts} -DZTH_ENABLE_ASAN=OFF -DZTH_ENABLE_LSAN=OFF -DZTH_ENABLE_UBSAN=OFF";;
		threads)
			cmake_opts="${cmake_opts} -DZTH_THREADS=ON";;
		nothreads)
			cmake_opts="${cmake_opts} -DZTH_THREADS=OFF";;
		test)
			do_test=1;;
		notest)
			do_test=0;;
		CC=*)
			CC="${1#CC=}";;
		CXX=*)
			CXX="${1#CXX=}";;
		make)
			use_ninja=0;;
		-*|*)
			# Only cmake flags follow.
			break;;
	esac
	shift
done

if [[ ${support_test} == 0 ]]; then
	do_test=0
fi

if [[ ${do_test} == 1 ]]; then
	cmake_opts="${cmake_opts} -DZTH_TESTS=ON"
else
	cmake_opts="${cmake_opts} -DZTH_TESTS=OFF"
fi

if [[ ${use_ninja} == 1 ]]; then
	cmake_opts="${cmake_opts} -G Ninja"
fi

if [[ ! -z ${CC:-} ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_C_COMPILER='${CC}'"
fi

if [[ ! -z ${CXX:-} ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_CXX_COMPILER='${CXX}'"
fi

cmake -DCMAKE_MODULE_PATH="${repo}/dist/common" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ${cmake_opts} "$@" ../../..
cmake --build . -j`nproc`
cmake --build . --target install -j`nproc`

if [[ ${do_test} == 1 ]]; then
	cmake --build . --target test
fi

popd > /dev/null
