#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

function show_help() {
	echo "Usage: $0 [OPTIONS]"
	echo
	echo "OPTIONS:"
	echo "  -h    Show this help message and exit"
	echo "  -r    Install Rust toolchain and dependencies too"
}

do_rust=0

while getopts "hr" opt; do
	case "${opt}" in
		h)
			show_help
			exit 0
			;;
		r)
			do_rust=1
			;;
		*)
			echo "" >&2
			show_help >&2
			exit 1
			;;
	esac
done

shift $((OPTIND - 1))

set -x

sudo apt install -y build-essential cmake doxygen git-core python3 python3-pip python3-venv \
	clang-format clang clang-tidy cppcheck dpkg-dev fakeroot file libzmq3-dev libunwind-dev chrpath

[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch

if [[ $do_rust -eq 1 ]]; then
	which rustup > /dev/null || sudo apt install -y rustup
	rustup default stable
	rustup component add clippy rustfmt rust-src rust-docs
fi
