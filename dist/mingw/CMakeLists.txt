# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

if(MINGW AND CMAKE_CROSSCOMPILING)
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
		OUTPUT_VARIABLE mingw_version
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	install(FILES
		/usr/lib/gcc/x86_64-w64-mingw32/${mingw_version}/libgcc_s_seh-1.dll
		/usr/lib/gcc/x86_64-w64-mingw32/${mingw_version}/libstdc++-6.dll
		/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
		DESTINATION bin
	)
endif()
