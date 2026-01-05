#ifndef ZTH_CONTEXT_ARCH_GENERIC_H
#define ZTH_CONTEXT_ARCH_GENERIC_H
/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef ZTH_CONTEXT_CONTEXT_H
#  error This file must be included by libzth/context/context.h.
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
