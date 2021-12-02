if(FALSE)

if(NOT CMAKE_CROSSCOMPILING)

################################
## Toolchain

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
		set(toolchain_url https://developer.arm.com/-/media/Files/downloads/gnu-rm/8-2019q3/RC1.1/gcc-arm-none-eabi-8-2019-q3-update-linux.tar.bz2?revision=c34d758a-be0c-476e-a2de-af8c6e16a8a2?product=GNU%20Arm%20Embedded%20Toolchain,64-bit,,Linux,8-2019-q3-update)
		set(toolchain_base gcc-arm-none-eabi-8-2019-q3-update)
	else()
		set(toolchain_url https://developer.arm.com/-/media/Files/downloads/gnu-rm/5_4-2016q3/gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2?revision=111dee36-f88b-4672-8ac6-48cf41b4d375?product=GNU%20Arm%20Embedded%20Toolchain,32-bit,,Linux,5-2016-q3-update)
		set(toolchain_base gcc-arm-none-eabi-5_4-2016q3)
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(toolchain_url https://developer.arm.com/-/media/Files/downloads/gnu-rm/8-2019q3/RC1.1/gcc-arm-none-eabi-8-2019-q3-update-mac.tar.bz2?revision=6a06dd2b-bb98-4708-adac-f4c630c33f4f?product=GNU%20Arm%20Embedded%20Toolchain,64-bit,,Mac%20OS%20X,8-2019-q3-update)
	set(toolchain_base gcc-arm-none-eabi-8-2019-q3-update)
else()
	message(FATAL_ERROR "Unsupported host system")
endif()

set(toolchain_path ${CMAKE_CURRENT_BINARY_DIR}/${toolchain_base}/bin)
set(toolchain_prefix ${toolchain_path}/arm-none-eabi-)

add_custom_command(
	OUTPUT ${toolchain_base}.tar.bz2
	COMMAND wget --no-check-certificate -O ${toolchain_base}.tar.bz2 ${toolchain_url}
	COMMENT "Download arm-gcc")

add_custom_command(
	OUTPUT ${toolchain_base}
	COMMAND tar -xmjf ${toolchain_base}.tar.bz2
	DEPENDS ${toolchain_base}.tar.bz2
	COMMENT "Unpack arm-gcc")

add_custom_target(arm-none-eabi-gcc DEPENDS ${toolchain_base})



################################
## newlib

set(newlib newlib-3.1.0)

add_custom_command(
	OUTPUT ${newlib}.tar.gz
	COMMAND wget ftp://sourceware.org/pub/newlib/${newlib}.tar.gz
	COMMENT "Download newlib")

add_custom_command(
	OUTPUT ${newlib}
	COMMAND tar -xmzf ${newlib}.tar.gz
	DEPENDS ${newlib}.tar.gz
	COMMENT "Unpack newlib")

add_custom_target(arm-newlib-src DEPENDS ${newlib})

set(arm_cpuflags "-mcpu=cortex-a15 -mfpu=vfpv4 -mfloat-abi=softfp")
add_custom_command(
	OUTPUT ${newlib}.stamp
	COMMAND [ ! -e ${newlib}-build ] || rm -r ${newlib}-build
	COMMAND mkdir -p ${newlib}-build
	COMMAND export PATH=${toolchain_path}:$$PATH \; cd ${newlib}-build && ../${newlib}/configure --target=arm-none-eabi --disable-newlib-supplied-syscalls CFLAGS_FOR_TARGET=${arm_cpuflags}' -O2 -g' --with-cpu=cortex-a15 --with-fpu=vfpv4 --with-float=hard --enable-interwork --disable-multilib --with-gnu-as --with-gnu-ld --disable-nls --enable-newlib-io-long-long --enable-newlib-io-float --enable-newlib-io-c99-formats && make -j4
	COMMAND touch ${newlib}.stamp
	DEPENDS arm-none-eabi-gcc
	DEPENDS arm-newlib-src
	COMMENT "Build newlib")

add_custom_target(arm-newlib DEPENDS ${newlib}.stamp)





################################
## examples

add_custom_command(
	OUTPUT zth-arm.stamp
	COMMAND mkdir -p zth-arm
	COMMAND cd zth-arm && ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -Dtoolchain_base=${toolchain_base} -Dnewlib=${newlib} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/toolchain.cmake -DZTH_HAVE_LIBZMQ=OFF -DZTH_THREADS=OFF ${PROJECT_SOURCE_DIR}/../..
	COMMAND make -C zth-arm arm-bsp
	COMMAND touch ${newlib}.stamp
	DEPENDS arm-none-eabi-gcc
	DEPENDS arm-newlib
	COMMENT "Setup cross-compilation environment")

add_custom_target(zth-arm DEPENDS zth-arm.stamp)

add_custom_command(
	OUTPUT arm-examples
	COMMAND make -C zth-arm -j4
	DEPENDS zth-arm
	COMMENT "Build examples for ARM")

add_custom_target(arm DEPENDS arm-examples)

else() # CMAKE_CROSSCOMPILING

endif() # CMAKE_CROSSCOMPILING


endif() # FALSE

set_property(SOURCE src/startup.S PROPERTY LANGUAGE C)
add_library(arm-bsp src/crt0.c src/startup.S)
add_dependencies(libzth arm-bsp)

