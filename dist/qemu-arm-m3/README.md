# qemu-arm-m3

This builds Zth for the QEMU Netduino 2 board (`netduino2`), which contains ARM
Cortex-M3.  It is only an example project, to demstrate how to cross-compile
for ARM, and show that the M3 is supported.

To run an example:

1. Run `./bootstrap.sh` once to install all dependencies.
2. Run `./build.sh` to build the project. By default, it builds a `Release`.
   Specify the `CMAKE_BUILD_TYPE` by providing it as first argument to
   `./build.sh`.  For example, set to `Debug` to get debug prints.  All
   additional arguments are passed to `cmake`.
3. Run `./run.sh` to start QEMU. By default, it uses `1_helloworld`.  Provide
   the binary you want to run as the first command line argument to `./run.sh`.
   All additional arguments are passed to QEMU too.
4. When needed, connect `gdb` to QEMU. If you supply the `-S` argument to QEMU
   (via `run.sh`), it waits for `gdb` to connect before it starts.  That might
   make debugging a bit easier.

	gdb-multiarch -ex 'layout split' -ex 'target remote :1234' -ex 'break main' build/deploy/bin/1_helloworld

