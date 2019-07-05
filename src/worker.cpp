/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019  Jochem Rutgers
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libzth/worker.h>
#include <libzth/async.h>

namespace zth {

ZTH_TLS_DEFINE(Worker*, currentWorker_, NULL)

void worker_global_init() {
	zth_dbg(banner, "%s", banner());
}
INIT_CALL(worker_global_init)

} // namespace

__attribute__((weak)) void main_fiber(int argc, char** argv) {}
zth_fiber(main_fiber);

__attribute__((weak)) int main(int argc, char** argv) {
	zth::Worker w;
	async main_fiber(argc, argv);
	w.run();
	return 0;
}

