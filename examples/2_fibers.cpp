#include <zth>

#include <cstdio>

// zth_fiber() is used to allow using `async' on a function to start a fiber
// with the given function as entry point.  zth_fiber() generates some static
// variables to implement this. So, don't use zth_fiber() in a header file, as
// it will result in a linker complaining about multiple symbols. You can split
// zth_fiber() in two parts: the declaration (zth_fiber_declare()) and
// definition (zth_fiber_define()), like you do with most functions in headers
// and source files.

// The same applies in case there is a circular dependency between two function
// calling each other as a fiber.  In that case, you can forward declare the
// fiber using zth_fiber_declare(), just like you would do in a header file.

// In case you have a header file that should make fibers visible, declare your
// function as normal.
void fiber_vv();

// Then, use zth_fiber_declare() instead of zth_fiber() to generate everything
// that is required such that other compilation units can do `async
// fiber_vv()'.
zth_fiber_declare(fiber_vv)

// In one .cpp file, zth_fiber_define() is required to implement the actual
// fiber creation code.
zth_fiber_define(fiber_vv)

// And, of course, you need to implement the function itself in the .cpp file.
void fiber_vv()
{
	printf("fiber_vv()\n");
}

// You can use function parameters of any type in the fiber entry function,
// except for references (like an `int const&' parameter).  In the
// implementation, the arguments are stored and given to the Fiber object.
// Reference variables are const, and cannot be reassigned, so this is not
// possible to save and forward arguments. If you do a pass-by-value, note that
// the data might be copied multiple times by Zth before ending up in the fiber
// entry. So, try to avoid to do pass-by-value of large structs (as is always
// smart to avoid in all C/C++ code).
void fiber_vi(int i)
{
	printf("fiber_vi(%d)\n", i);
}

void fiber_vii(int x, int y)
{
	printf("fiber_vii(%d, %d)\n", x, y);
}
// zth_fiber_declare(), zth_fiber_define() and zth_fiber() allow a list of
// functions. This is identical to calling zth_fiber...() on every function
// separately.
zth_fiber(fiber_vi, fiber_vii)

// Fibers can return data, like normal functions.
double fiber_ddd(double a, double b)
{
	double res = a + b;
	printf("fiber_ddd(%g, %g) = %g\n", a, b, res);
	return res;
}
zth_fiber(fiber_ddd)

#if __cplusplus >= 201103L
// The current implementation for pre-C++11 only support up to three arguments.
// C++11 and higher support arbitrary number of arguments.
double fiber_ddddi(double a, double b, double c, int d)
{
	double res = a * b + c - d;
	printf("fiber_ddddi(%g, %g, %g, %d) = %g\n", a, b, c, d, res);
	return res;
}
zth_fiber(fiber_ddddi)
#endif

void main_fiber(int argc, char** argv)
{
	// `async' is just like a function call, but its execution is postponed.
	// Note that the order in which the fibers get initialized or execute is
	// not defined.
	async fiber_vv();
	async fiber_vi(42);
	async fiber_vii(3, 14);

	// The fiber entry points are still available to be called directly.  So,
	// the following line just calls fiber_vv() right away. There is no
	// side-effect on fiber_vv() as being marked as fiber entry point.
	fiber_vv();

	// fiber_ddd() returns a value. If it would be started as below, the return
	// value is just lost, just like what would happen if you would call
	// fiber_ddd() as a normal function without using its return value.
	async fiber_ddd(1, 2);

	// zth_fiber() also defines a <function>_future type, which allows
	// synchronization between fibers. `async' returns this type.
	fiber_ddd_future fddd = async fiber_ddd(3, 4);

	// Now, fddd is a (smart) pointer to a Future object. fddd is small and can
	// safely be copied or passed using pass-by-value. It gives a few
	// interesting functions:
	// - poll whether the fiber has finished and the future becomes therefore
	//   valid;
	printf("fddd is %s\n", fddd->valid() ? "valid" : "not valid yet");
	
	// - suspend the current fiber and wait until the fiber has finished;
	fddd->wait();

	// - retrieve the returned value by the fiber. If the future is not valid
	//   yet, value() will imply wait().
	printf("fddd = %g\n", fddd->value());
	// In case the return type of the fiber is void, the value() function is
	// not available.

#if __cplusplus >= 201103L
	// Arbitrary fiber argument number example.
	async fiber_ddddi(4, 5, 6, 7);
#endif
}

