#!/bin/bash

# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
