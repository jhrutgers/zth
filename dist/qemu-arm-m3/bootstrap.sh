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
"${here}/../ubuntu/bootstrap.sh" "$@"

set -x

sudo apt install -y gdb-multiarch qemu-system-arm gcc-arm-none-eabi

if [[ $do_rust -eq 1 ]]; then
	rustup target add thumbv7em-none-eabi
fi
