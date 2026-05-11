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

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)"
echo "${here}/../ubuntu/bootstrap.sh" "$@"
shift $((OPTIND - 1))

set -x

sudo apt install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

if [[ $do_rust -eq 1 ]]; then
	rustup target add x86_64-pc-windows-gnu
fi
