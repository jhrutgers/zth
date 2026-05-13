# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGE_NAME "zth")
set(CPACK_PACKAGE_VENDOR "Jochem Rutgers")
set(CPACK_PACKAGE_CONTACT "Jochem Rutgers")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/jhrutgers/zth")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cooperative multitasking library")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RESOURCE_FILE_LICENSE "${ZTH_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${ZTH_DIST_DIR}/README.md")

set(_zth_debian_version "${ZTH_VERSION_BASE}")
if(NOT "${ZTH_VERSION_SUFFIX}" STREQUAL "")
	set(_zth_debian_suffix "${ZTH_VERSION_SUFFIX}")
	string(REGEX REPLACE "^-" "~" _zth_debian_suffix "${_zth_debian_suffix}")
	string(REGEX REPLACE "[^A-Za-z0-9.+:~]+" "" _zth_debian_suffix "${_zth_debian_suffix}")
	string(APPEND _zth_debian_version "${_zth_debian_suffix}")
endif()
set(CPACK_PACKAGE_VERSION "${_zth_debian_version}")

find_program(DPKG_CMD dpkg)
if(DPKG_CMD)
	execute_process(
		COMMAND ${DPKG_CMD} --print-architecture
		OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif()

include(CPack)
set(ZTH_CPACK_INCLUDED
    ON
    CACHE INTERNAL "CPack included in this build"
)
