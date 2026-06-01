# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

zth_repo_config(.cargo/config.toml)
zth_repo_config(Cargo.toml)
zth_repo_config(macros/Cargo.toml)

if(NOT ZTH_ENABLE_RUST)
	return()
endif()

if(ZTH_TESTS AND NOT CMAKE_CROSSCOMPILING AND NOT (APPLE AND NOT ZTH_SHARED_LIB))
	add_test(
		NAME rust
		COMMAND ${CMAKE_COMMAND} -E env RUST_LOG=info "PATH=${ENV_PATH_ESC_BSSC}"
			"${CARGO_EXE}" test ${ZTH_RUST_PROFILE} -- --test-threads=1 --nocapture
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)

	zth_config_test(rust)
endif()

set(cargo_features)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
	list(APPEND cargo_features --no-default-features --features bare-metal)
endif()

add_custom_target(
	rust-package ALL
	COMMAND "${CARGO_EXE}" package -q --manifest-path
		"${CMAKE_CURRENT_SOURCE_DIR}/macros/Cargo.toml" --no-verify
	COMMAND "${CARGO_EXE}" package -q --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
		${cargo_features}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS libzth
	COMMENT "Generate Rust crate package output files"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/package/zth-${ZTH_VERSION}.crate"
	      "${CMAKE_CURRENT_BINARY_DIR}/package/zth_macros-${ZTH_VERSION}.crate"
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libzth/rust
)

add_custom_target(
	rust-doc
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${ZTH_SOURCE_DIR}/doxygen/html/rust"
	COMMAND
		"${CARGO_EXE}" doc --no-deps --manifest-path
		"${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir
		"${CMAKE_CURRENT_BINARY_DIR}/rustdoc" ${cargo_features}
	COMMAND
		"${CMAKE_COMMAND}" -E copy_directory
		"${CMAKE_CURRENT_BINARY_DIR}/rustdoc/${ZTH_RUST_TARGET}/doc"
		"${ZTH_SOURCE_DIR}/doxygen/html/rust"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS libzth
	COMMENT "Generate Rust HTML API documentation in doxygen/html/rust"
)
