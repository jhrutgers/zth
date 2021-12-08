#ifndef ZTH_CONTEXT_ARCH_GENERIC_H
#define ZTH_CONTEXT_ARCH_GENERIC_H
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
