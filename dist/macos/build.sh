#!/bin/bash

# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -e

function nproc {
	sysctl -n hw.logicalcpu
}

if ! pkg-config libzmq > /dev/null; then
	# Brew does not seem to add libzmq.pc to the search path.
	libzmq_version="`brew info --json=v1 zmq | jq ".[].installed[].version" -r`"
	libzmq_path="/usr/local/Cellar/zeromq/${libzmq_version}/lib/pkgconfig"
	if [[ -e ${libzmq_path}/libzmq.pc ]]; then
		export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${libzmq_path}"
	fi
fi

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

# Use brew's gcc
[[ ! -z ${CC:-} ]]  || CC=`ls -1 /usr/local/bin/gcc-[0-9]* | sort -n | tail -n 1`
[[ ! -z ${CXX:-} ]] || CXX=`ls -1 /usr/local/bin/g++-[0-9]* | sort -n | tail -n 1`

cmake_opts=
. ../common/build.sh

popd > /dev/null
