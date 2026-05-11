# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

configure_file(
	.cargo/config.toml.in ${CMAKE_CURRENT_SOURCE_DIR}/.cargo/config.toml @ONLY ESCAPE_QUOTES
)

find_program(
	CARGO_EXE
	NAMES "cargo"
	DOC "Path to cargo executable"
)

if(NOT CARGO_EXE)
	return()
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(RUST_PROFILE)
else()
	set(RUST_PROFILE "--release")
endif()

if(ZTH_TESTS)
	add_test(
		NAME rust
		COMMAND "${CARGO_EXE}" test ${RUST_PROFILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
endif()
