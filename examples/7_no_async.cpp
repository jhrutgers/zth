// If you don't like the zth_fiber() and async macros, you may use zth::fiber()
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
	zth::fiber(foo)(1);

	// zth::fiber(...) returns a fiber factory. It takes a function pointer
	// and optional fiber name. The returned factory can be used to create
	// actual fibers, by passing its arguments.

	// The fiber manipulators still work:
	zth::fiber(foo)(2) << zth::setName("foo(2)");

	// You can also get a future with the fiber's return value.  If you
	// don't use C++11 (or later), the types are a bit cumbersome.
	typedef zth::fiber_type<decltype(&foo)> foo_type;
	foo_type::future future3 = zth::fiber(foo, "foo(3)")(3).withFuture();

	// If you want to split up the steps above, you can also do this:
	foo_type::factory factory4 = zth::fiber(foo);
	foo_type::fiber fiber4 = factory4(4);
	fiber4 << zth::setName("foo(4)");
	// With the explicit future type, the call to fiber4.withFuture() is
	// implied.
	foo_type::future future4 = fiber4;
	printf("Returned %d\n", future4->value());

#if __cplusplus >= 201103L
	// For C++11 and later, using auto is easier.
	auto future5 = zth::fiber(foo)(5).withFuture();
	future5->wait();
#endif

	return 0;
}
