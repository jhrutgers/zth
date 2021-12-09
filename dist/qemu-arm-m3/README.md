# qemu-arm-m3

This builds Zth for the Qemu Netduino 2 board (`netduino2`), which contains ARM
Cortex-M3.  It is only an example project, to demstrate how to cross-compile
for ARM, and show that the M3 is supported.

Connect `gdb` to Qemu. If you supply the `-S` argument to Qemu (via `run.sh`),
it waits for `gdb` to connect before it starts.  That might make debugging a
bit easier.

	gdb-multiarch -ex 'layout split' -ex 'target remote :1234' -ex 'break main' build/deploy/bin/1_helloworld

