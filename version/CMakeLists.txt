file(STRINGS "version.txt" ZTH_VERSION LIMIT_COUNT 1)

message(STATUS "Zth version ${ZTH_VERSION}")

string(REGEX REPLACE "^([0-9.]+).*$" "\\1" ZTH_VERSION_BASE ${ZTH_VERSION})
string(REPLACE "." ";" ZTH_VERSION_LIST ${ZTH_VERSION_BASE})
list(GET ZTH_VERSION_LIST 0 ZTH_VERSION_MAJOR)
list(GET ZTH_VERSION_LIST 1 ZTH_VERSION_MINOR)
list(GET ZTH_VERSION_LIST 2 ZTH_VERSION_PATCH)

string(REGEX REPLACE "^[0-9.]+(.*)$" "\\1" ZTH_VERSION_SUFFIX ${ZTH_VERSION})

# Force reconfigure when version.txt is changed.
configure_file(version.txt version.txt COPYONLY)

configure_file(
	${PROJECT_SOURCE_DIR}/include/libzth/version.h.in
	${PROJECT_SOURCE_DIR}/include/libzth/version.h
	@ONLY ESCAPE_QUOTES)

