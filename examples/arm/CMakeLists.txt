cmake_minimum_required(VERSION 3.2.3)
project(arm-example)

if(NOT CMAKE_CROSSCOMPILING)

################################
## Toolchain

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(toolchain_url https://developer.arm.com/-/media/Files/downloads/gnu-rm/8-2019q3/RC1.1/gcc-arm-none-eabi-8-2019-q3-update-linux.tar.bz2?revision=c34d758a-be0c-476e-a2de-af8c6e16a8a2?product=GNU%20Arm%20Embedded%20Toolchain,64-bit,,Linux,8-2019-q3-update)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(toolchain_url https://developer.arm.com/-/media/Files/downloads/gnu-rm/8-2019q3/RC1.1/gcc-arm-none-eabi-8-2019-q3-update-mac.tar.bz2?revision=6a06dd2b-bb98-4708-adac-f4c630c33f4f?product=GNU%20Arm%20Embedded%20Toolchain,64-bit,,Mac%20OS%20X,8-2019-q3-update)
else()
	message(FATAL_ERROR "Unsupported host system")
endif()

set(toolchain_base gcc-arm-none-eabi-8-2019-q3-update)
set(toolchain_path ${toolchain_base}/bin)
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
	COMMAND cd ${newlib}-build && PATH=${toolchain_path}:$$PATH ../${newlib}/configure --target=arm-none-eabi --disable-newlib-supplied-syscalls CFLAGS_FOR_TARGET=${arm_cpuflags}' -O2 -g' --with-cpu=cortex-a15 --with-fpu=vfpv4 --with-float=hard --enable-interwork --disable-multilib --with-gnu-as --with-gnu-ld --disable-nls --enable-newlib-io-long-long --enable-newlib-io-float --enable-newlib-io-c99-formats && make -j4
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

set_property(SOURCE startup.S PROPERTY LANGUAGE C)
add_library(arm-bsp crt0.c startup.S)

endif() # CMAKE_CROSSCOMPILING

