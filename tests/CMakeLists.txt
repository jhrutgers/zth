find_program(VALGRIND_EXE valgrind)

if(WIN32 AND ZTH_HAVE_LIBZMQ)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libzmq.timestamp
		DEPENDS libzmq
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq> ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:libzmq>
		COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/libzmq.timestamp
		VERBATIM
	)

	add_custom_target(test-prepare
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libzmq.timestamp
		COMMENT "Prepare tests"
	)

endif()

macro(zth_test_example EXAMPLE)
	if(VALGRIND_EXE)
		# TODO: add --error-exitcode=1, but needs some investigation on current (false?) errors
		add_test(NAME ${EXAMPLE}-valgrind
			COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 ${VALGRIND_EXE} --leak-check=full $<TARGET_FILE:${EXAMPLE}>
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	endif()

	add_test(NAME ${EXAMPLE}-run COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 $<TARGET_FILE:${EXAMPLE}>
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endmacro()

zth_test_example(1_helloworld)
zth_test_example(2_fibers)
zth_test_example(3_coop)
zth_test_example(4_sync)
zth_test_example(5_perf)
zth_test_example(9_c_api)
zth_test_example(daemon_pattern)
zth_test_example(measure)
zth_test_example(socks)
