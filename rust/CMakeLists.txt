# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

configure_file(
	.cargo/config.toml.in ${CMAKE_CURRENT_SOURCE_DIR}/.cargo/config.toml @ONLY ESCAPE_QUOTES
)

if(NOT CARGO_EXE)
	return()
endif()

if(ZTH_TESTS)
	add_test(
		NAME rust
		COMMAND "${CARGO_EXE}" test ${ZTH_RUST_PROFILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
endif()
