# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

if(MINGW AND CMAKE_CROSSCOMPILING)
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
		OUTPUT_VARIABLE mingw_version
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	set(mingw_libs
	    /usr/lib/gcc/x86_64-w64-mingw32/${mingw_version}/libgcc_s_seh-1.dll
	    /usr/lib/gcc/x86_64-w64-mingw32/${mingw_version}/libstdc++-6.dll
	    /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
	)

	install(FILES ${mingw_libs} DESTINATION bin)

	if(ZTH_TESTS)
		foreach(lib ${mingw_libs})
			file(COPY "${lib}" DESTINATION "${CMAKE_BINARY_DIR}/tests")
		endforeach()
	endif()
endif()
