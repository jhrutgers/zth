#ifndef ZTH_REGS_H
#define ZTH_REGS_H
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

#ifdef __cplusplus
#include <libzth/macros.h>
#include <libzth/util.h>

/*!
 * \defgroup zth_api_cpp_regs regs
 * \ingroup zth_api_cpp
 */

namespace zth {

/*!
 * \brief Helper class to read/write (bitfields in) hardware registers.
 *
 * Registers are assumed to be volatile.
 * Only explicit \c read() and \c write() access the actual register.
 * All other operations are performed on the locally cached value.
 *
 * \ingroup zth_api_cpp_regs
 */
template <typename T, uintptr_t Addr, typename Fields>
struct Register {
	typedef T type;

	Register()
	{
		static_assert(sizeof(Fields) == sizeof(type), "");
		read();
	}

	constexpr explicit Register(type v) noexcept : value(v) {}

	union { type value; Fields field; };

	static type volatile* r() noexcept { return (type volatile*)Addr; }
	type read() const noexcept { return *r(); }
	type read() noexcept { return value = *r(); }
	void write() const noexcept { *r() = value; }
	void write(type v) const noexcept { *r() = v; }
	void write(type v) noexcept { *r() = value = v; }
	operator type() const noexcept { return value; }
};

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
// In case of little-endian, gcc puts the last field at the highest bits.
// This is counter intuitive for registers. Reverse them.
#  define ZTH_REG_BITFIELDS(...) REVERSE(__VA_ARGS__)
#else
#  define ZTH_REG_BITFIELDS(...) __VA_ARGS__
#endif

/*!
 * \brief Define a hardware reference helper class, with bitfields.
 *
 * Example to create \c zth::reg_mpu_type register:
 *
 * \code
 * ZTH_REG_DEFINE(uint32_t, reg_mpu_type, 0xE000ED90,
 * 	// 32-bit register definition in unsigned int fields,
 * 	// ordered from MSB to LSB.
 * 	reserved1 : 8,
 * 	region : 8,
 * 	dregion : 8,
 * 	reserved2 : 7,
 * 	separate : 1)
 * \endcode
 *
 * Afterwards, instantiate the \c zth::reg_mpu_type class to access the hardware register.
 *
 * \see zth::Register
 * \ingroup zth_api_cpp_regs
 */
#define ZTH_REG_DEFINE(T, name, addr, fields...) \
	struct name##__type { T ZTH_REG_BITFIELDS(fields); } __attribute__((packed)); \
	struct name : public zth::Register<T, addr, name##__type> { \
		typedef zth::Register<T, addr, name##__type> base; \
		using typename base::type; \
		name() : base() {} \
		explicit name(type v) : base(v) {} \
	};

#ifdef ZTH_ARCH_ARM
#  ifdef ZTH_ARM_HAVE_MPU
ZTH_REG_DEFINE(uint32_t, reg_mpu_type, 0xE000ED90,
	reserved1 : 8,
	region : 8,
	dregion : 8,
	reserved2 : 7,
	separate : 1)

ZTH_REG_DEFINE(uint32_t, reg_mpu_ctrl, 0xE000ED94,
	reserved : 29,
	privdefena : 1,
	hfnmiena : 1,
	enable : 1)

ZTH_REG_DEFINE(uint32_t, reg_mpu_rnr, 0xE000ED98,
	reserved : 24,
	region : 8)

ZTH_REG_DEFINE(uint32_t, reg_mpu_rbar, 0xE000ED9C,
	addr : 27,
	valid : 1,
	region : 4)

ZTH_REG_DEFINE(uint32_t, reg_mpu_rasr, 0xE000EDA0,
	reserved1 : 3,
	xn : 1,
	reserved2 : 1,
	ap : 3,
	reserved3 : 2,
	tex : 3,
	s : 1,
	c : 1,
	b : 1,
	srd : 8,
	reserved4 : 2,
	size : 5,
	enable : 1)
#  endif // ZTH_ARM_HAVE_MPU
#endif // ZTH_ARCH_ARM

} // namespace
#endif // __cplusplus
#endif // ZTH_REGS_H
