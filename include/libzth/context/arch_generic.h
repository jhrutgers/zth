#ifndef ZTH_CONTEXT_ARCH_GENERIC_H
#define ZTH_CONTEXT_ARCH_GENERIC_H
/*
 * Zth (libzth), a cooperative userspace multitasking library.
 * Copyright (C) 2019-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef ZTH_CONTEXT_CONTEXT_H
#	error This file must be included by libzth/context/context.h.
#endif

#ifdef __cplusplus

namespace zth {
namespace impl {

template <typename Impl>
class ContextArch : public ContextBase<Impl> {
public:
	typedef ContextBase<Impl> base;

protected:
	constexpr explicit ContextArch(ContextAttr const& attr) noexcept
		: base(attr)
	{}
};

} // namespace impl
} // namespace zth
#endif // __cplusplus
#endif // ZTH_CONTEXT_ARCH_GENERIC_H
