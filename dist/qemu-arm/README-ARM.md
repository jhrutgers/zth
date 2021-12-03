# Using Zth on Bare-Metal ARM

There is support for bare-metal ARM with Zth.  Most of Zth's source code is
just C++, so usable on any target.  Only the context switching and
hardware-related stuff like `clock_gettime()` is to be ported.

The directory `dist/qemu-arm` contains a set of files that shows how to do
this.  It targets a Qemu virt machine to run the examples, and can be built on
Ubuntu.  This virt machine uses an ARM Cortex-A15 with VFP (without NEON).  The
implementation is not very sophisticated, but shows you where to start, if you
have your own ARM system.  The compiler, linker script, and startup files are
probably supplied to you by the manufacturer of your board.

To get the stuff running, do this:

1. Run `bootstrap.sh`.
2. Run `build.sh`. Optionally. provide the `CMAKE_BUILD_TYPE` as first command
   line argument.  It will use `arm-none-eabi-g++` to cross-compile the Zth
   library and examples.  Additionally, a BSP library is built, using `crt0.c`
   and `startup.S`.  These files include the interrupt vector for the ARM,
   intialization of the VFP, C/C++-runtime boot code, and the system calls that
   `newlib` needs, like `_read()` and `_write()`.
3. Run Qemu with one of the examples.  (Note: Press 'Ctrl-A X' to terminate
   Qemu)

		qemu-system-arm -machine virt -cpu cortex-a15 -s -m 8M -nographic -kernel build/deploy/bin/1_helloworld

   If you have a debug build, the output will include the banner, saying something like:

		> zth::banner: Zth 1.0.0-rc g++-6.3.1 C++14 baremetal newlib2.4.0 armAv7 debug sjlj

   Note that there is no OS that supplies `poll()`. So reading stdin is a bit
   tricky and not all examples behave the same way as at your PC.  Moreover,
   0MQ isn't available either, of course.
4. Connect `gdb` to Qemu. If you supply the `-S` argument to Qemu, it waits for
   `gdb` to connect before it starts.  That might make debugging a bit easier.

		gdb-multiarch -ex 'target remote :1234' -ex 'break main' -ex 'layout split' build/deploy/bin/1_helloworld

