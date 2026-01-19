/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// If you don't like the zth_fiber() and zth_async macros, you may use zth::fiber()
// instead.  This example shows this alternative syntax.

#include <zth>

static int foo(int i)
{
	printf("foo %d\n", i);
	return i;
}

int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	// To just start the fiber, do this:
	zth::factory(foo)(1);
	zth::fiber(foo, 1);

	// zth::factory(...) returns a fiber factory. It takes a function pointer
	// and optional fiber name. The returned factory can be used to create
	// actual fibers, by passing its arguments. zth::fiber() is a shorthand for
	// this: it combines the factory creation and fiber creation in one step.

	// The fiber manipulators still work:
	zth::fiber(foo, 2) << zth::setName("foo(2)");

	// You can also get a future with the fiber's return value.  If you
	// don't use C++11 (or later), the types are a bit cumbersome.
	typedef zth::fiber_type<decltype(foo)> foo_type;
	foo_type::future future3 = zth::factory(foo, "foo(3)")(3).withFuture();
	future3.wait();

	// If you want to split up the steps above, you can also do this:
	foo_type::factory factory4 = zth::factory(foo);
	foo_type::fiber fiber4 = factory4(4);
	fiber4 << zth::setName("foo(4)");
	// With the explicit future type, the call to fiber4.withFuture() is
	// implied.
	foo_type::future future4 = fiber4;
	printf("Returned %d\n", *future4);

#if __cplusplus >= 201103L
	// For C++11 and later, using auto is easier.
	auto future5 = zth::fiber(foo, 5) << zth::asFuture();
	*future5;
#endif

	return 0;
}
