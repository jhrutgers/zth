find_program(VALGRIND_EXE valgrind)

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

set(CMAKE_CXX_CLANG_TIDY "${DO_CLANG_TIDY}")

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

function(zth_add_test_binary TESTNAME)
	add_executable(${TESTNAME} ${ARGN} test_main.cpp)
	target_link_libraries(${TESTNAME} gtest gmock libzth)

	set_target_properties(${TESTNAME} PROPERTIES
		FOLDER tests)

	if(TARGET test-prepare)
		add_dependencies(${TESTNAME} test-prepare)
	endif()
endfunction()

function(zth_config_test TEST_LIST)
	set_tests_properties(${TEST_LIST} PROPERTIES
		ENVIRONMENT ZTH_CONFIG_ENABLE_DEBUG_PRINT=0
		TIMEOUT 300)
endfunction()

function(zth_add_test TESTNAME)
	zth_add_test_binary(${TESTNAME} ${ARGN})

	# Not using gtest_discover_tests(), because of:
	# https://github.com/google/googletest/issues/3475
	gtest_add_tests(TARGET ${TESTNAME} TEST_LIST tests)

	zth_config_test(${tests})
endfunction()

zth_add_test(test_poller test_poller.cpp)

zth_add_test_binary(test_fiber11 test_fiber11.cpp)
gtest_add_tests(TARGET test_fiber11
	SOURCES test_fiber.cpp
	TEST_PREFIX CXX11
	TEST_LIST tests)
zth_config_test(${tests})

zth_add_test_binary(test_fiber98 test_fiber98.cpp)
gtest_add_tests(TARGET test_fiber11
	SOURCES test_fiber.cpp
	TEST_PREFIX CXX98
	TEST_LIST tests)
zth_config_test(${tests})

macro(zth_test_example EXAMPLE)
	if(VALGRIND_EXE AND NOT ZTH_ENABLE_ASAN AND ZTH_ENABLE_VALGRIND)
		# TODO: add --error-exitcode=1, but needs some investigation on current (false?) errors
		add_test(NAME ${EXAMPLE}-valgrind
			COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 ${VALGRIND_EXE} --leak-check=full $<TARGET_FILE:${EXAMPLE}> ${ARGN}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	endif()

	add_test(NAME ${EXAMPLE}-run COMMAND ${CMAKE_COMMAND} -E env ZTH_CONFIG_ENABLE_DEBUG_PRINT=0 $<TARGET_FILE:${EXAMPLE}> ${ARGN}
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

if(ZTH_HAVE_LIBZMQ)
	zth_test_example(zmq 3)
endif()
