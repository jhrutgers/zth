#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

sudo apt install -y build-essential
[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch
which cmake > /dev/null || sudo apt install -y cmake
which doxygen > /dev/null || sudo apt install -y doxygen
which git > /dev/null || sudo apt install -y git-core
