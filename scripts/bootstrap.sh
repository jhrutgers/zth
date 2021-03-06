#!/bin/bash

function gotErr {
	echo -e "\nError occurred, stopping\n"
	exit 1
}

trap gotErr ERR

case `uname -s` in
	Linux*)
		sudo apt install -y build-essential git-core gcc-multilib cmake \
			gdb-multiarch doxygen \
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
		;;
	*)
		echo "Unknown OS"
		exit 1;;
esac

