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

pushd "$( cd "$(dirname "$0")"/..; pwd -P )" > /dev/null

if [ -z "$1" ]; then
	BUILD_TYPE=Release
else
	BUILD_TYPE="$1"
	shift
fi

git submodule init
git submodule update

mkdir -p build
pushd build > /dev/null
case `uname -s` in
	Darwin*)
		# Use brew's gcc
		export CC=`ls -1 /usr/local/bin/gcc-[0-9]* | sort -n | tail -n 1`
		export CXX=`ls -1 /usr/local/bin/g++-[0-9]* | sort -n | tail -n 1`
		;;
	*)
		export CC=gcc
		export CXX=g++
		;;
esac
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_COMPILER="${CC}" -DCMAKE_CXX_COMPILER="${CXX}" "$@" ..
cmake --build . -- -j`numproc` all
popd > /dev/null

