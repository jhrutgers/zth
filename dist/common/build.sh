#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

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

distdir="$(pwd -P)"
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &> /dev/null; pwd -P)"

case ${1:-} in
	-?|-h|--help)
		show_help;;
esac

BUILD_TYPE="${1:-}"
shift || true

# Simplify setting specific configuration flag.
do_clean=0
do_build=1
do_test=0
use_ninja=0

if which ninja > /dev/null; then
	use_ninja=1
fi

cmake_opts="${cmake_opts} -DZTH_DIST='$(basename "${distdir}")' -DZTH_DIST_DIR='${distdir}'"

while [[ ! -z ${1:-} ]]; do
	case "$1" in
		"-?"|-h|--help|help)
			show_help;;
		C++98|C++03)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=98 -DCMAKE_C_STANDARD=99"
			;;
		C++11)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=11 -DCMAKE_C_STANDARD=11"
			;;
		C++14)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=14 -DCMAKE_C_STANDARD=11";;
		C++17)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_STANDARD=17";;
		C++20)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=20 -DCMAKE_C_STANDARD=17";;
		C++23)
			cmake_opts="${cmake_opts} -DCMAKE_CXX_STANDARD=23 -DCMAKE_C_STANDARD=23";;
		clean)
			do_clean=1
			cmake_opts="${cmake_opts} -DZTH_REGEN_LAUNCH_JSON=ON"
			;;
		conf)
			do_build=0;;
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


builddir="${distdir}/build"
if [[ ${do_clean} -eq 1 ]] && [[ -d "${builddir}" ]]; then
	rm -rf "${builddir}"
fi

mkdir -p "${builddir}"
pushd "${builddir}" > /dev/null

if [[ ${repo}/dist/common/requirements.txt -nt ${repo}/dist/venv/.timestamp ]]; then
	[[ ! -e ${repo}/dist/venv ]] || rm -rf "${repo}/dist/venv"
	python3 -m venv "${repo}/dist/venv"
	"${repo}/dist/venv/bin/python3" -m pip install --upgrade pip setuptools wheel
	"${repo}/dist/venv/bin/python3" -m pip install --upgrade -r "${repo}/dist/common/requirements.txt"
	touch "${repo}/dist/venv/.timestamp"
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

if [[ ! -z ${BUILD_TYPE:-} ]]; then
	cmake_opts="${cmake_opts} -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
fi

cmake -DCMAKE_MODULE_PATH="${repo}/dist/common" ${cmake_opts} "$@" ../../..
if [[ ${do_build} == 1 ]]; then
	cmake --build . -j`nproc`
	cmake --build . --target install -j`nproc`

	if [[ ${do_test} == 1 ]]; then
		cmake --build . --target test
	fi
fi

popd > /dev/null
