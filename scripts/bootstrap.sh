#!/bin/bash

set -euo pipefail

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

case `uname -s` in
	Linux*)
		sudo apt install -y build-essential
		[[ ! -z ${CXX:-} ]] || which g++ > /dev/null || sudo apt install -y g++-multilib gdb-multiarch
		[[ ! -z ${CC:-} ]] || which gcc > /dev/null || sudo apt install -y gcc-multilib gdb-multiarch
		which cmake > /dev/null || sudo apt install -y cmake
		which doxygen > /dev/null || sudo apt install -y doxygen
		which git > /dev/null || sudo apt install -y git-core
		;;
	Darwin*)
		function install_or_upgrade {
			if brew ls --versions "$1" >/dev/null; then
				HOMEBREW_NO_AUTO_UPDATE=1 brew upgrade "$1"
			else
				HOMEBREW_NO_AUTO_UPDATE=1 brew install "$1"
			fi
		}
		install_or_upgrade cmake
		install_or_upgrade gnutls
		install_or_upgrade gcc
		install_or_upgrade doxygen
		install_or_upgrade git
		install_or_upgrade zeromq
		;;
	*)
		echo "Unknown OS"
		exit 1;;
esac

