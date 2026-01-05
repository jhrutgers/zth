# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

function(copy_lib lib)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
	find_library(${lib}_path "${lib}")

	if(NOT "${${lib}_path}" STREQUAL "${lib}_path-NOTFOUND")
		install(FILES "${${lib}_path}" DESTINATION bin)

		if(ZTH_TESTS)
			file(COPY "${${lib}_path}" DESTINATION "${CMAKE_BINARY_DIR}/tests")
		endif()
	endif()
endfunction()

copy_lib(libgcc_s_seh-1)
copy_lib(libstdc++-6)
copy_lib(libwinpthread-1)
