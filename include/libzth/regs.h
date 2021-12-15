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
#	include <libzth/macros.h>
#	include <libzth/util.h>

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

	Register() noexcept
	{
		static_assert(sizeof(Fields) == sizeof(type), "");
		read();
	}

	constexpr explicit Register(type v) noexcept
		: value(v)
	{}

	union {
		type value;
		Fields field;
	};

	static type volatile* r() noexcept
	{
		return (type volatile*)Addr;
	}

	type read() const noexcept
	{
		return *r();
	}

	type read() noexcept
	{
		return value = *r();
	}

	void write() const noexcept
	{
		*r() = value;
	}

	void write(type v) const noexcept
	{
		*r() = v;
	}

	void write(type v) noexcept
	{
		*r() = value = v;
	}

	operator type() const noexcept
	{
		return value;
	}
};

#	if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
// In case of little-endian, gcc puts the last field at the highest bits.
// This is counter intuitive for registers. Reverse them.
#		define ZTH_REG_BITFIELDS(...) REVERSE(__VA_ARGS__)
#	else
#		define ZTH_REG_BITFIELDS(...) __VA_ARGS__
#	endif

/*!
 * \brief Define a hardware reference helper class, with bitfields.
 *
 * Example to create \c zth::reg_mpu_type register:
 *
 * \code
 * // clang-format off
 * ZTH_REG_DEFINE(uint32_t, reg_mpu_type, 0xE000ED90,
 * 	// 32-bit register definition in unsigned int fields,
 * 	// ordered from MSB to LSB.
 * 	reserved1 : 8,
 * 	region : 8,
 * 	dregion : 8,
 * 	reserved2 : 7,
 * 	separate : 1)
 * // clang-format on
 * \endcode
 *
 * Afterwards, instantiate the \c zth::reg_mpu_type class to access the hardware register.
 *
 * \see zth::Register
 * \ingroup zth_api_cpp_regs
 */
#	define ZTH_REG_DEFINE(T, name, addr, fields...)                    \
		struct name##__type {                                       \
			T ZTH_REG_BITFIELDS(fields);                        \
		} __attribute__((packed));                                  \
		struct name : public zth::Register<T, addr, name##__type> { \
			typedef zth::Register<T, addr, name##__type> base;  \
			using typename base::type;                          \
			name() noexcept                                     \
				: base()                                    \
			{}                                                  \
			constexpr explicit name(type v) noexcept            \
				: base(v)                                   \
			{}                                                  \
		};

} // namespace zth
#endif // __cplusplus
#endif // ZTH_REGS_H
