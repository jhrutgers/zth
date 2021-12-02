set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)


#set(CMAKE_C_FLAGS "${arm_cpuflags} -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}-build/arm-none-eabi/newlib/targ-include -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}/newlib/libc/include -Wno-psabi" CACHE STRING "" FORCE)
#set(CMAKE_CXX_FLAGS "${arm_cpuflags} -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}-build/arm-none-eabi/newlib/targ-include -isystem ${CMAKE_CURRENT_BINARY_DIR}/../${newlib}/newlib/libc/include -Wno-psabi -fno-use-cxa-atexit -fno-threadsafe-statics -Wno-psabi" CACHE STRING "" FORCE)

#set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/examples/arm/arm.ld -nostartfiles -L${CMAKE_CURRENT_BINARY_DIR}/../${newlib}-build/arm-none-eabi/newlib -L${CMAKE_CURRENT_BINARY_DIR}/examples/arm -lc -lstdc++ -lsupc++ -fno-use-cxa-atexit -Wl,--whole-archive -larm-bsp -Wl,--no-whole-archive" CACHE INTERNAL "")



set(toolchain_prefix arm-none-eabi-)

set(arm_defines "")
set(arm_cpuflags "-mcpu=cortex-a15 -mfpu=vfpv4 -mfloat-abi=softfp")
set(arm_cflags "${arm_defines} ${arm_cpuflags} -fno-strict-aliasing -fno-builtin -fshort-enums -Wno-psabi -fshort-wchar")
set(arm_cxxflags "${arm_cflags} -fno-use-cxa-atexit -fno-threadsafe-statics")

set(CMAKE_AR "${toolchain_prefix}ar" CACHE FILEPATH "" FORCE)
set(CMAKE_SIZE "${toolchain_prefix}size" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${toolchain_prefix}ranlib")
set(CMAKE_GDB "${toolchain_prefix}gdb" CACHE FILEPATH "" FORCE)
set(CMAKE_READELF "${toolchain_prefix}readelf" CACHE FILEPATH "" FORCE)
set(CMAKE_C_COMPILER "${toolchain_prefix}gcc")
set(CMAKE_CXX_COMPILER "${toolchain_prefix}g++")
set(CMAKE_C_FLAGS_INIT "${arm_cflags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "${arm_cxxflags}" CACHE STRING "" FORCE)
set(CMAKE_ASM${ASM_DIALECT}_FLAGS_INIT "${arm_defines} ${arm_cpuflags}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${arm_cpuflags} -Wl,--gc-sections --specs=nano.specs -Wl,--no-wchar-size-warning -T ${ZTH_DIST_DIR}/src/arm.ld -nostartfiles -L${CMAKE_CURRENT_BINARY_DIR}/dist/qemu-arm -lc -lstdc++ -lsupc++ -fno-use-cxa-atexit -Wl,--whole-archive -larm-bsp -Wl,--no-whole-archive" CACHE INTERNAL "")
#set(CMAKE_STANDARD_LIBRARIES "-lc -lnosys -lm")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

