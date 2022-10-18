# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

include(ExternalProject)

if(TARGET libzmq)
	# Target already exists.
	set(ZeroMQ_FOUND 1)
	message(STATUS "Skipped looking for ZeroMQ; target already exists")
endif()

if(NOT TARGET libzmq AND NOT CMAKE_CROSSCOMPILING)
	# Try pkg-config
	find_package(PkgConfig)

	if(PkgConfig_FOUND)
		pkg_check_modules(ZeroMQ libzmq>=4 IMPORTED_TARGET)

		if(ZeroMQ_FOUND)
			if(NOT ZeroMQ_LINK_LIBRARIES)
				set(ZeroMQ_LINK_LIBRARIES ${pkgcfg_lib_ZeroMQ_zmq})
			endif()
			if(ZeroMQ_LINK_LIBRARIES)
				message(STATUS "Found ZeroMQ via pkg-config at ${ZeroMQ_LINK_LIBRARIES}")
				add_library(libzmq SHARED IMPORTED GLOBAL)
				set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${ZeroMQ_LINK_LIBRARIES})
				set_property(TARGET libzmq PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ZeroMQ_INCLUDE_DIRS})
				set_property(TARGET libzmq PROPERTY INTERFACE_COMPILE_OPTIONS ${ZeroMQ_CFLAGS})
				target_link_libraries(libzmq INTERFACE ${ZeroMQ_LDFLAGS})
			endif()
		endif()
	endif()
endif()

if(NOT TARGET libzmq)
	# Try previously built and installed
	unset(ZeroMQ_FOUND CACHE)
	find_package(ZeroMQ CONFIG)
	if(ZeroMQ_FOUND)
		message(STATUS "Found ZeroMQ using cmake")
	endif()
endif()

if(NOT TARGET libzmq AND ZeroMQ_FIND_REQUIRED)
	# Build from source
	message(STATUS "Building ZeroMQ from source")
	set(ZeroMQ_FOUND 1)

	set(libzmq_flags -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_GENERATOR=${CMAKE_GENERATOR}
		-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
	)

	if(MINGW)
		set(libzmq_flags ${libzmq_flags} -DZMQ_WIN32_WINNT_DEFAULT=0x0A00)

		# See https://github.com/zeromq/libzmq/issues/3859
		set(libzmq_flags ${libzmq_flags} -DZMQ_CV_IMPL=win32api)
	endif()

	if(CMAKE_CROSSCOMPILING)
		# It seems that in case of crosscompiling, the host headers are
		# found anyway. Force using builtins instead.
		if(NOT WITH_LIBBSD)
			set(libzmq_flags ${libzmq_flags} -DWITH_LIBBSD=OFF)
		endif()
		if(NOT WITH_LIBSODIUM)
			set(libzmq_flags ${libzmq_flags} -DWITH_LIBSODIUM=OFF)
		endif()
	endif()

	set(libzmq_flags ${libzmq_flags} -DBUILD_TESTS=OFF -DBUILD_STATIC=OFF)

	if(ZTH_ENABLE_ASAN)
		set(libzmq_flags ${libzmq_flags} -DENABLE_ASAN=ON)
	endif()

	if(WIN32)
		set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/bin/libzmq.dll)
		set(_libzmq_implib ${CMAKE_INSTALL_PREFIX}/lib/libzmq.dll.a)
	elseif(APPLE)
		set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/lib/libzmq.dylib)
	else()
		set(_libzmq_loc ${CMAKE_INSTALL_PREFIX}/lib/libzmq.so)
	endif()

	ExternalProject_Add(
		libzmq-extern
		GIT_REPOSITORY https://github.com/zeromq/libzmq.git
		GIT_TAG v4.3.4
		CMAKE_ARGS ${libzmq_flags}
		INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
		BUILD_BYPRODUCTS ${_libzmq_loc} ${_libzmq_implib}
		UPDATE_DISCONNECTED 1
	)

	file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/include)
	file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

	add_library(libzmq SHARED IMPORTED GLOBAL)

	if(_libzmq_loc)
		set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${_libzmq_loc})
	endif()
	if(_libzmq_implib)
		set_property(TARGET libzmq PROPERTY IMPORTED_IMPLIB ${_libzmq_implib})
	endif()

	if(WIN32)
		target_link_libraries(libzmq INTERFACE ws2_32 rpcrt4 iphlpapi)
	else()
		target_link_libraries(libzmq INTERFACE pthread rt)
	endif()

	set_property(TARGET libzmq PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_INSTALL_PREFIX}/include)
	add_dependencies(libzmq libzmq-extern)
endif()
