/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef ZTH_CONFIG_H
#  error Do not include this file directly, include <zth> instead.
#endif

#ifdef __cplusplus
namespace zth {

struct Config : public DefaultConfig {
	static size_t const DefaultFiberStackSize = 0x1000;
};

} // namespace zth
#endif // __cplusplus
