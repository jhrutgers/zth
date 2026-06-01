/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <libzth/indirection.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
zth_indirection_t zth_indirection = {};

/*!
 * \brief Set the function pointer for zth_logv.
 * \ingroup zth_api_indirection
 */
void zth_logv_indirect(zth_logv_t* func)
{
	zth_indirection.zth_logv = func;
}

/*!
 * \brief Set all function indirection pointers.
 * \param indirection the struct with function pointers to set. If \c nullptr, all pointers are
 * reset. \ingroup zth_api_indirection
 */
void zth_indirect(zth_indirection_t const* indirection)
{
	if(indirection)
		zth_indirection = *indirection;
	else
		zth_indirection = {};
}
