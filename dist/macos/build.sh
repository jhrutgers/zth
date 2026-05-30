#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

function nproc {
	sysctl -n hw.logicalcpu
}

if ! pkg-config libzmq > /dev/null; then
	# Brew does not seem to add libzmq.pc to the search path.
	libzmq_path="$(realpath "$(brew --prefix zeromq)/lib/pkgconfig")"
	if [[ -e ${libzmq_path}/libzmq.pc ]]; then
		export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}:${libzmq_path}"
	fi
fi

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

# Use brew's gcc
[[ ! -z ${CC:-} ]]  || CC="$(ls -1 "$(brew --prefix gcc)"/bin/gcc-[0-9]* | sort -n | tail -n 1)"
[[ ! -z ${CXX:-} ]] || CXX="$(ls -1 "$(brew --prefix gcc)"/bin/g++-[0-9]* | sort -n | tail -n 1)"

if ! command -v cargo >/dev/null 2>&1; then
	rustup_dir="$(brew --prefix rustup)/bin"
	if [[ -x ${rustup_dir}/cargo ]]; then
		export PATH="${rustup_dir}:${PATH}"
	fi
fi

cmake_opts=
. ../common/build.sh

popd > /dev/null
