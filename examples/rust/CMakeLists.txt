# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set(ZTH_RUST_LINK_FLAGS "")
separate_arguments(_zth_rust_link_args NATIVE_COMMAND "${CMAKE_EXE_LINKER_FLAGS}")
foreach(arg IN LISTS _zth_rust_link_args)
	string(APPEND ZTH_RUST_FLAGS "\n  \"-Clink-arg=${arg}\",")
endforeach()

zth_repo_config(.cargo/config.toml)

if(NOT ZTH_ENABLE_RUST)
	return()
endif()

function(add_rust_example name)
	set(cargo_args rustc -p "${name}" -q)
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
		set(output_subdir debug)
	else()
		set(output_subdir release)
	endif()
	set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/${ZTH_RUST_TARGET}/${output_subdir})
	set(output_clone ${CMAKE_CURRENT_BINARY_DIR}/${output_subdir}/${output_file})

	add_launch_json(
		NAME "${name} (Rust)" BINARY "${output_dir}/${output_file}" WORKING_DIRECTORY
		"${output_dir}"
	)

	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
		set(post_build COMMAND "${CMAKE_SIZE}" "${output_clone}")
		set(map_file "${output_dir}/${name}.map")
		list(APPEND cargo_args -- -C "link-arg=-Wl,-Map=${map_file}")
	else()
		set(post_build)
		set(map_file "")
	endif()

	add_custom_target(
		rust-${name} ALL
		COMMAND "${CMAKE_COMMAND}" -E env --unset=ZTH_INSTALL "ZTH_REPO=${ZTH_SOURCE_DIR}"
			"ZTH_DIST=${ZTH_DIST}" "${CARGO_EXE}" ${cargo_args}
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${output_dir}/${output_file}"
			"${output_clone}" ${post_build}
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
