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

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

function install_or_upgrade {
	if brew ls --versions "$1" >/dev/null; then
		HOMEBREW_NO_AUTO_UPDATE=1 brew upgrade "$1"
	else
		HOMEBREW_NO_AUTO_UPDATE=1 brew install "$1"
	fi
}

set -x

install_or_upgrade cmake
install_or_upgrade gnutls
install_or_upgrade gcc
install_or_upgrade doxygen
install_or_upgrade git
install_or_upgrade zeromq
install_or_upgrade jq

if [[ $do_rust -eq 1 ]]; then
	if ! command -v rustup >/dev/null 2>&1; then
		install_or_upgrade rustup-init
		rustup-init -y
		export PATH="$HOME/.cargo/bin:$PATH"
	fi
	rustup component add clippy rustfmt rust-src rust-docs
fi
