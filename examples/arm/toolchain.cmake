set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(toolchain_path ${CMAKE_CURRENT_BINARY_DIR}/../${toolchain_base}/bin)
set(toolchain_prefix ${toolchain_path}/arm-none-eabi-)
set(arm_cpuflags "-mcpu=cortex-a15 -mfpu=vfpv4 -mfloat-abi=softfp")

set(CMAKE_AR "${toolchain_prefix}ar")
set(CMAKE_RANLIB "${toolchain_prefix}ranlib")
set(CMAKE_C_COMPILER "${toolchain_prefix}gcc")
set(CMAKE_CXX_COMPILER "${toolchain_prefix}g++")
set(CMAKE_C_FLAGS "${arm_cpuflags} -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}-build/arm-none-eabi/newlib/targ-include -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}/newlib/libc/include" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${arm_cpuflags} -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}-build/arm-none-eabi/newlib/targ-include -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}/newlib/libc/include -Wno-psabi -fno-use-cxa-atexit -fno-threadsafe-statics" CACHE STRING "" FORCE)

set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/examples/arm/arm.ld -nostartfiles -L${CMAKE_CURRENT_BINARY_DIR}/../${newlib}-build/arm-none-eabi/newlib -L${CMAKE_CURRENT_BINARY_DIR}/examples/arm -lc -lstdc++ -lsupc++ -fno-use-cxa-atexit -Wl,--whole-archive -larm-bsp -Wl,--no-whole-archive" CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
