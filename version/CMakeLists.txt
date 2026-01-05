# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

file(STRINGS "version.txt" ZTH_VERSION LIMIT_COUNT 1)

message(STATUS "Zth version ${ZTH_VERSION}")

string(REGEX REPLACE "^([0-9.]+).*$" "\\1" ZTH_VERSION_BASE ${ZTH_VERSION})
string(REPLACE "." ";" ZTH_VERSION_LIST ${ZTH_VERSION_BASE})
list(GET ZTH_VERSION_LIST 0 ZTH_VERSION_MAJOR)
list(GET ZTH_VERSION_LIST 1 ZTH_VERSION_MINOR)
list(GET ZTH_VERSION_LIST 2 ZTH_VERSION_PATCH)

if(ZTH_VERSION_MAJOR GREATER "99"
   OR ZTH_VERSION_MINOR GREATER "99"
   OR ZTH_VERSION_PATCH GREATER "99"
)
	message(FATAL_ERROR "Please update version.h.in's ZTH_VERSION_NUM")
endif()

string(REGEX REPLACE "^[0-9.]+(.*)$" "\\1" ZTH_VERSION_SUFFIX ${ZTH_VERSION})

# Force reconfigure when version.txt is changed.
configure_file(version.txt version.txt COPYONLY)

configure_file(
	${PROJECT_SOURCE_DIR}/include/libzth/version.h.in
	${PROJECT_SOURCE_DIR}/include/libzth/version.h @ONLY ESCAPE_QUOTES
)
