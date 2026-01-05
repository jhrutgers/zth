[![CI](https://github.com/jhrutgers/zth/workflows/CI/badge.svg)](https://github.com/jhrutgers/zth/actions?query=workflow%3ACI)

# Zth (libzth) - Zeta threads

This library provides user-space cooperative multitasking, also known as
fibers. One can see fibers as threads, but with the exception that you
explicitly have to indicate when the fiber is allowed to switch to another
fiber. As a result, locking, synchronization, using shared data structures
between fibers is way easier than when using threads. See also
<https://en.wikipedia.org/wiki/Fiber_(computer_science)>.

The main benefits of Zth are:

- Cross-platform (Linux/Windows/macOS/bare metal) library for cross-platform
  application development.
- No dependencies, other than a C library (usually
  [newlib](https://sourceware.org/newlib/) for embedded targets), and
  `clock_gettime()`.  When available, [ZeroMQ](https://zeromq.org/) is
  supported and made fiber-aware, which can be used for inter-thread or
  inter-process communication.
- The configuration can be tailored towards your target, such as fiber stack
  size, time slices, debug/VCD output.
- Usage of dynamic memory is minimized (only during fiber creation and
  cleanup), and it allocator-aware.
- Support for [ASan, LSan, UBSan](https://github.com/google/sanitizers), and
  [Valgrind](https://valgrind.org/).

Working with fibers is very easy. The `examples/1_helloworld` example starts
two fibers by using the `async` keyword:

```cpp
#include <zth>
#include <cstdio>

void world()
{
	printf("World!!1\n");
}
zth_fiber(world)

void hello()
{
	async world();
	printf("Hello\n");
}
zth_fiber(hello)

int main_fiber(int argc, char** argv)
{
	async hello();
	return 0;
}
```

Notable other features include:

- Finite State Machine implementation, which allows defining state transitions
  in an eDSL, and easily transfers a fiber into a fiber-aware FSM, properly
  handling polling timed guards.
- Extensible Poller framework, which allows `poll()`-like calls by fibers to be
  forwarded to a single poller server, which can be user-defined for custom
  pollable types.
- Inter-fiber signaling, such as a mutex, semaphore, signal, and future.

Check out the [examples](https://jhrutgers.github.io/zth/examples.html) for
more details, including the full explanation of the example above.

Supported platforms are:

- Ubuntu/macOS/Windows 32/64-bit
- bare-metal 32-bit ARM (with newlib)
- gcc 4 or newer
- C++98 or newer, C99 or newer. However, compiling for later C++ standards adds
  features and improves performance.

Check out the `dist` directory for example targets.

## How to build

To install all build dependencies, run `dist/<platform>/bootstrap` (as
Administrator under Windows).  Next, run `dist/<platform>/build` to build the
library and all examples. This does effectively:

```bash
mkdir build
cd build
cmake ../../..
cmake --build .
```

By default, release builds are generated. To do debug builds, pass `Debug` as
command line argument to `dist/<platform>/build`, or do something like:

```bash
cmake ../../.. -D CMAKE_BUILD_TYPE=Debug
```

After building, check out the `doxygen/html` directory for
[documentation](https://jhrutgers.github.io/zth).

### How to integrate in your project

Include the Zth top-level `CMakeLists.txt` in your project, and link to
`libzth`.  Configure Zth options as required, like this:

```cmake
set(ZTH_HAVE_LIBZMQ OFF CACHE BOOL "Disable ZMQ" FORCE)
set(ZTH_THREADS OFF CACHE BOOL "Disable threads" FORCE)
set(ZTH_BUILD_EXAMPLES OFF CACHE BOOL "Disable Zth examples" FORCE)
set(ZTH_TESTS OFF CACHE BOOL "Disable Zth tests" FORCE)

add_subdirectory(zth)

# Override search path to your own zth_config.h.
target_include_directories(libzth BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/include)
```

This also works for cross-compiling.  Refer to
`dist/qemu-arm-a15/README-ARM.md` for some hints when compiling for bare-metal
ARM.

Note that `zth_config.h` can be provided by the user to optimize the
configuration for a specific target. However, its include path must be
available while compiling libzth itself.  After installing the library, the
configuration cannot be changed.

## How to run

Zth checks for environment variables, which are listed below.  The environment
is (usually) only checked once, so dynamically changing the variables after
startup has no effect to Zth's behavior.

* `ZTH_CONFIG_ENABLE_DEBUG_PRINT`  
	When set to 1, debug prints are enabled. Disabled by default. For
	non-debug builds, all debug prints are removed from the binary, so they
	cannot be enabled in that case.

* `ZTH_CONFIG_DO_PERF_EVENT`  
	When set to 1, perf VCD file generation is enabeled.  Disabled by default.

* `ZTH_CONFIG_DO_PERF_SYSCALL`  
	When set to 1, the perf VCD logs will contain all calls to Zth's special
	functions.  Enabled by default.

* `ZTH_CONFIG_CHECK_TIMESLICE_OVERRUN`
	When set to 1, check at every context switch if the timeslice was overrun
	significantly.  Only the longest overrun is reported.  Enabled by default
	in the debug build, not available in release builds.

## Related

A predecessor project was called Xi, as the Greek capital symbol suggests
parallel threads.  In this project, preemptive multitasking is implemented. In
the same context, the Z(eta) symbol suggests that threads are not parallel, but
they explicitly yield from one to another.

[GNU Pth](https://www.gnu.org/software/pth/) has been a great inspiration for
this library.

## License

(Most of) this project is licensed under the terms of the Mozilla Public License, v. 2.0, as
specified in LICENSE. This project complies to [REUSE](https://reuse.software/).
