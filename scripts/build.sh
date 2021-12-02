#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

function numproc {
	case `uname -s` in
		Linux*)
			nproc;;
		Darwin*)
			sysctl -n hw.logicalcpu;;
		*)
			echo 1;;
	esac
}

trap gotErr ERR

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")"/.. &> /dev/null; pwd -P)" > /dev/null

BUILD_TYPE="${1:-Release}"
shift || true

mkdir -p build
pushd build > /dev/null

# Simplify setting specific configuration flag.
cmake_opts=
support_test=1
do_test=0
while [[ ! -z ${1:-} ]]; do
	case "$1" in
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
		CC=*)
			CC="${1#CC=}";;
		CXX=*)
			CXX="${1#CXX=}";;
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
fi

case `uname -s` in
	Darwin*)
		# Use brew's gcc
		[[ ! -z ${CC:-} ]]  || CC=`ls -1 /usr/local/bin/gcc-[0-9]* | sort -n | tail -n 1`
		[[ ! -z ${CXX:-} ]] || CXX=`ls -1 /usr/local/bin/g++-[0-9]* | sort -n | tail -n 1`
		;;
	*)
		[[ ! -z ${CC:-} ]]  || CC=gcc
		[[ ! -z ${CXX:-} ]] || CXX=g++
		;;
esac

cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_COMPILER="${CC}" -DCMAKE_CXX_COMPILER="${CXX}" ${cmake_opts} "$@" ..
cmake --build . -j`numproc`
cmake --build . --target install -j`numproc`

if [[ ${do_test} == 1 ]]; then
	cmake --build .. --target test
fi

popd > /dev/null

