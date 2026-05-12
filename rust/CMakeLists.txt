# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

zth_repo_config(.cargo/config.toml)
zth_repo_config(Cargo.toml)
zth_repo_config(macros/Cargo.toml)

if(NOT ZTH_ENABLE_RUST)
	return()
endif()

if(ZTH_TESTS AND NOT CMAKE_CROSSCOMPILING)
	add_test(
		NAME rust
		COMMAND "${CARGO_EXE}" test ${ZTH_RUST_PROFILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
endif()

add_custom_target(
	rust-package ALL
	COMMAND "${CARGO_EXE}" package -q --manifest-path
		"${CMAKE_CURRENT_SOURCE_DIR}/macros/Cargo.toml" --no-verify
	COMMAND "${CARGO_EXE}" package -q --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS libzth
	COMMENT "Generate Rust crate package output files"
)
