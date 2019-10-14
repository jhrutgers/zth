# Using Zth on Bare-Metal ARM

There is preliminary support for bare-metal ARM with Zth.
Most of Zth's source code is just C++, so usable on any target.
Only the context switching and hardware-related stuff like `clock_gettime()` is to be ported.

The directory `examples/arm` contains a set of files that shows how to do this.
This examples targets a Qemu virt machine to run the examples.
This virt machine uses an ARM Cortex-A15 with VFP (without NEON).
The implementation is not very nice, but shows you where to start, if you have your own ARM system.
The compiler, linker script, and startup files are probably supplied to you by the manufacturer of your board.

To get the stuff running, do this:

1. Create an out-of-tree build directory, just like normal:

		mkdir build-arm
		cd build-arm
		cmake ..

2. Now, cross-compile all examples in one go.
	The files will be generated in the directory `examples/arm/zth-arm/examples`.

		cd build-arm
		make arm

	That was easy!
	Ok, I was cheating a bit. This actually happened (see also `examples/arm/CMakeLists.txt`):

	1. Download and extract ARM toolchain (`arm-none-eabi-gcc` and friends).
		The toolchain is installed in `examples/arm/gcc-arm-none...`

			make arm-none-eabi-gcc
	
	2. Download, extract and build `newlib`.
		This uses the toolchain, as installed in the previous step.
		`newlib` is compiled specifically for our target processor.

			make arm-newlib
	
	3. Build BSP (`libarm-bsp.a`).
		This library contains `crt0.c` and `startup.S`, which both reside in `examples/arm`.
		These files include the interrupt vector for the ARM, intialization of the VFP, C/C++-runtime boot code, and the system calls that `newlib` needs, like `_read()` and `_write()`.
		All binaries compiled for our target need this BSP library further on.

			make arm-bsp

	4. A new CMake out-of-tree build is initiated with the propert toolchain configuration.
		Basically, this calls CMake with `-DCMAKE_TOOLCHAIN_FILE=examples/arm/toolchain.cmake` in the directory `examples/arm/zth-arm`.

			make zth-arm

	5. Build Zth for ARM. This target includes everything above, but also runs `make` in `examples/arm/zth-arm`.
		As a result, all examples are cross-compiled for ARM in `examples/arm/zth-arm/examples`.
		The examples are built using the `toolchain.cmake` configuration, as provided above, and are linked against our `newlib` and `libarm-bsp.a`.
		
			make arm

3. Run Qemu with one of the examples.
	I have tested it with Qemu 3.0.0.
	(Note: Press 'Ctrl-A X' to terminate qemu)

		qemu-system-arm -machine virt -cpu cortex-a15 -s -m 8M -nographic -kernel examples/arm/zth-arm/examples/1_helloworld

	If you have a debug build, the output will include the banner, saying something like:

		> zth::banner: Zth 0.1.0 g++-8.3.1 C++14 baremetal newlib3.1.0 arm debug sjlj

	Note that there is no OS that supplies `poll()`. So reading stdin is a bit tricky and not all examples behave the same way as at your PC.
	Moreover, 0MQ isn't available too, of course.

4. Connect `gdb` to Qemu. If you supply the `-S` argument to Qemu, it waits for `gdb` to connect before it starts.
	That might make debugging a bit easier.

		examples/arm/gcc-arm-none-eabi-8-2019-q3-update/bin/arm-none-eabi-gdb -ex 'target remote :1234' -ex 'break main' -ex 'layout split' examples/arm/zth-arm/examples/1_helloworld

