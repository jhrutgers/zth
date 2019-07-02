# Zth (libzth) - Zeta threads

A predecessor project was called Xi, as the Greek capital symbol suggests
parallel threads.  In this project, preemptive multitasking is implemented. In
the same context, the Z(eta) symbol suggests that threads are not parallel, but
they explicitly yield from one to another.

GNU Pth has been a great inspiration for this library.



## How to build

mkdir build
cd build
cmake ..
cmake --build .

For Mac OSX with MacPorts, one could change the sequence above by something like:
CXX=g++-mp-8 LDFLAGS=-L/opt/local/lib cmake 


## How to run

libzth checks for environment variables, which are listed below.  The
environment is (usually) only checked once, so dynamically changing the
variables after startup has no effect to libzth's behavior.

ZTH_CONFIG_ENABLE_DEBUG_PRINT
	When set to 0, debug prints are suppressed.  Enabled by default. For
	non-debug builds, all debug prints are removed from the binary, so they
	cannot be enabled in that case.

ZTH_CONFIG_DO_PERF_EVENT
	When set to 1, perf VCD file generation is enabeled.  Disabled by default.


## License

The project license is specified in COPYING and COPYING.LESSER.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

