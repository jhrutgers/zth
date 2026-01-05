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

sudo apt install -y build-essential gdb-multiarch qemu-system-arm
which arm-none-eabi-g++ > /dev/null || sudo apt install -y gcc-arm-none-eabi
which cmake > /dev/null || sudo apt install -y cmake
which doxygen > /dev/null || sudo apt install -y doxygen
which git > /dev/null || sudo apt install -y git-core
