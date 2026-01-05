#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

sudo apt install -y build-essential cmake doxygen git-core python3 python3-pip python3-venv

[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch
