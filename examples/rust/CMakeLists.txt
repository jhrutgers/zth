# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set(ZTH_RUST_LINK_FLAGS "")
separate_arguments(_zth_rust_link_args NATIVE_COMMAND "${CMAKE_EXE_LINKER_FLAGS}")
foreach(arg IN LISTS _zth_rust_link_args)
	string(APPEND ZTH_RUST_LINK_FLAGS "\n  \"-C\", \"link-arg=${arg}\",")
endforeach()

configure_file(.cargo/config.toml.in ${CMAKE_CURRENT_SOURCE_DIR}/.cargo/config.toml @ONLY)

if(NOT ZTH_ENABLE_RUST)
	return()
endif()

function(add_rust_example name)
	set(cargo_args build -p "${name}" -q)
	if(ZTH_RUST_PROFILE)
		list(APPEND cargo_args ${ZTH_RUST_PROFILE})
	endif()
	if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
		list(APPEND cargo_args --no-default-features --features bare-metal)
	endif()

	if(WIN32)
		set(output_file "${name}.exe")
	else()
		set(output_file "${name}")
	endif()

	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/${ZTH_RUST_TARGET}/debug)
	else()
		set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/${ZTH_RUST_TARGET}/release)
	endif()

	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT CMAKE_CROSSCOMPILING)
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
		COMMAND "${CMAKE_COMMAND}" -E env --unset=ZTH_INSTALL "ZTH_REPO=${ZTH_SOURCE_DIR}"
			"ZTH_DIST=${ZTH_DIST}" "${CARGO_EXE}" ${cargo_args}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS libzth
		COMMENT "Build Rust ${name} example"
	)

	install(
		PROGRAMS ${output_dir}/${output_file}
		DESTINATION bin
		RENAME rust-${output_file}
	)
endfunction()

add_rust_example(hello)
add_rust_example(futures)
