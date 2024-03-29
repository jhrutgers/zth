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

function(zth_config_test TEST_LIST)
	set_tests_properties(${TEST_LIST} PROPERTIES
		ENVIRONMENT ZTH_CONFIG_ENABLE_DEBUG_PRINT=0
		TIMEOUT 300)
endfunction()

if(CMAKE_CROSSCOMPILING AND NOT MINGW)
	set(ZTH_USE_GTEST_DEFAULT OFF)
else()
	set(ZTH_USE_GTEST_DEFAULT ON)
endif()

option(ZTH_USE_GTEST "Use googletest" ${ZTH_USE_GTEST_DEFAULT})

if(ZTH_USE_GTEST)
	set(CMAKE_CXX_STANDARD 14)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_CLANG_TIDY "")

	include(FetchContent)
	FetchContent_Declare(
		googletest
		GIT_REPOSITORY https://github.com/google/googletest.git
		GIT_TAG        release-1.11.0
	)

	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	include(GoogleTest)
endif()

add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion)

if(ZTH_USE_GTEST)
	set(CMAKE_CXX_CLANG_TIDY "${DO_CLANG_TIDY}")

	function(zth_add_test_binary TESTNAME)
		add_executable(${TESTNAME} ${ARGN} test_main.cpp)
		target_link_libraries(${TESTNAME} gtest gmock libzth)

		set_target_properties(${TESTNAME} PROPERTIES
			FOLDER tests)

		if(TARGET test-prepare)
			add_dependencies(${TESTNAME} test-prepare)
		endif()
	endfunction()

	function(zth_add_test TESTNAME)
		zth_add_test_binary(${TESTNAME} ${ARGN})

		# Not using gtest_discover_tests(), because of:
		# https://github.com/google/googletest/issues/3475
		gtest_add_tests(TARGET ${TESTNAME} TEST_LIST tests)

		zth_config_test(${tests})
	endfunction()

	zth_add_test(test_poller test_poller.cpp)

	zth_add_test(test_fsm14 test_fsm14.cpp)
	if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
		zth_add_test(test_fsm17 test_fsm17.cpp)
		set_property(TARGET test_fsm17 PROPERTY CXX_STANDARD 17)
	endif()

	zth_add_test_binary(test_fiber11 test_fiber11.cpp)
	gtest_add_tests(TARGET test_fiber11
		SOURCES test_fiber.cpp
		TEST_PREFIX CXX11
		TEST_LIST tests)
	zth_config_test(${tests})

	zth_add_test_binary(test_fiber98 test_fiber98.cpp)
	gtest_add_tests(TARGET test_fiber98
		SOURCES test_fiber.cpp
		TEST_PREFIX CXX98
		TEST_LIST tests)
	zth_config_test(${tests})

	zth_add_test_binary(test_stack11 test_stack11.cpp)
	gtest_add_tests(TARGET test_stack11
		SOURCES test_stack.cpp
		TEST_PREFIX CXX11
		TEST_LIST tests)
	zth_config_test(${tests})

	zth_add_test_binary(test_stack98 test_stack98.cpp)
	gtest_add_tests(TARGET test_stack98
		SOURCES test_stack.cpp
		TEST_PREFIX CXX98
		TEST_LIST tests)
	zth_config_test(${tests})
endif()

if(ZTH_DIST_DIR)
	find_program(run_wrapper run.sh PATHS ${ZTH_DIST_DIR} NO_DEFAULT_PATH)
endif()

if(NOT run_wrapper OR run_wrapper STREQUAL "run_wrapper-NOTFOUND")
	if(CMAKE_CROSSCOMPILING)
		set(can_run OFF)
	else()
		set(can_run ON)
		set(run_wrapper "")
	endif()
else()
	set(can_run ON)
endif()

macro(zth_test_example EXAMPLE)
	if(can_run)
		if(NOT run_wrapper)
			if(VALGRIND_EXE AND NOT ZTH_ENABLE_ASAN AND ZTH_ENABLE_VALGRIND)
				# TODO: add --error-exitcode=1, but needs some investigation on current (false?) errors
				add_test(NAME ${EXAMPLE}-valgrind
					COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 ${VALGRIND_EXE} --leak-check=full $<TARGET_FILE:${EXAMPLE}> ${ARGN}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
			endif()
		endif()

		add_test(NAME ${EXAMPLE}-run COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 ${run_wrapper} $<TARGET_FILE:${EXAMPLE}> ${ARGN}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

		zth_config_test(${EXAMPLE}-run)
	endif()
endmacro()

zth_test_example(1_helloworld)
zth_test_example(2_fibers)
zth_test_example(3_coop)
zth_test_example(4_sync)
zth_test_example(5_perf)
zth_test_example(7_no_async)
zth_test_example(9_c_api)
zth_test_example(daemon_pattern)
zth_test_example(measure)
zth_test_example(socks)

if(ZTH_HAVE_LIBZMQ)
	zth_test_example(zmq 3)
endif()
