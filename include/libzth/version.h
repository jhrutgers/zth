#ifndef __ZTH_VERSION_H
#define __ZTH_VERSION_H
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

#include <libzth/util.h>

#define ZTH_VERSION_MAJOR 0
#define ZTH_VERSION_MINOR 1
#define ZTH_VERSION_PATCH 0
#define ZTH_VERSION_SUFFIX ""

#define ZTH_VERSION ZTH_STRINGIFY(ZTH_VERSION_MAJOR) "." ZTH_STRINGIFY(ZTH_VERSION_MINOR) "." ZTH_STRINGIFY(ZTH_VERSION_PATCH) ZTH_VERSION_SUFFIX

#ifndef __cplusplus
namespace zth {
	inline char const* version() {
		return ZTH_VERSION;
	}
}
#endif

#endif // _ZTH_VERSION_H
