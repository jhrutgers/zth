/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2021  Jochem Rutgers
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

// Default zth configuration. Copy this file, and override defaults in the
// Config class below.  Make sure it is in your include path, and zth will use
// your config file.  For CMake, so something like this:
//
//   target_include_directories(libzth BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/include)

#ifndef ZTH_CONFIG_H
#	error Do not include this file directly, include <zth> instead.
#endif

#ifdef __cplusplus
namespace zth {

/*!
 * \brief The configuration of Zth.
 *
 * To override the default values, make sure your \c zth_config.h file is in
 * the include directory list before the default paths.
 *
 * \ingroup zth_api_cpp_config
 */
struct Config : public DefaultConfig {
	// Override defaults from DefaultConfig here for your local setup.
	// static size_t const DefaultFiberStackSize = 0x2000;
};

} // namespace zth
#endif // __cplusplus
