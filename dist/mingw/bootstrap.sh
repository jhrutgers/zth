#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

sudo apt install -y build-essential gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
which cmake > /dev/null || sudo apt install -y cmake
which doxygen > /dev/null || sudo apt install -y doxygen
which git > /dev/null || sudo apt install -y git-core
