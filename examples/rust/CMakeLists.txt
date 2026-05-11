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

function(add_rust_example name)
	if(WIN32)
		set(output_file "${name}.exe")
	else()
		set(output_file "${name}")
	endif()

	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(RUST_PROFILE)
		set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/debug)
		add_launch_json_entry(
			"
			\"type\": \"cppdbg\",
			\"request\": \"launch\",
			\"name\": \"${name} (Rust)\",
			\"program\": \"${output_dir}/${output_file}\",
			\"cwd\": \"${output_dir}\",
			\"environment\": [
				{
					\"name\": \"ASAN_OPTIONS\",
					\"value\": \"detect_leaks=0\"
				}
			]"
		)
	else()
		set(RUST_PROFILE "--release")
		set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/release)
		add_launch_json_entry(
			"
			\"type\": \"f5anything\",
			\"request\": \"launch\",
			\"name\": \"${name} (Rust)\",
			\"command\": \"${output_dir}/${output_file}\",
		"
		)
	endif()

	add_custom_target(
		rust-${name} ALL
		COMMAND "${CARGO_EXE}" build -p "${name}" ${RUST_PROFILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Build Rust ${name} example"
	)

	add_dependencies(rust-${name} libzth)

	install(
		PROGRAMS ${output_dir}/${output_file}
		DESTINATION bin
		RENAME rust-${output_file}
	)
endfunction()

add_rust_example(hello)
add_rust_example(futures)
