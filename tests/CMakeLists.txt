find_program(VALGRIND_EXE valgrind)

macro(zth_test_example EXAMPLE)
	if(VALGRIND_EXE)
		# TODO: add --error-exitcode=1, but needs some investigation on current (false?) errors
		add_test(NAME ${EXAMPLE}-valgrind
			COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 ${VALGRIND_EXE} --leak-check=full $<TARGET_FILE:${EXAMPLE}>)
	endif()

	add_test(NAME ${EXAMPLE}-run COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 $<TARGET_FILE:${EXAMPLE}>)
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
