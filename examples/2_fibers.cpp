/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <zth>

#include <cstdio>

// For C++11 and up, anything that can be invoked like a function (function pointers, lambda
// functions, functors, std::function, etc.) can be used as fiber entry point.  For pre-C++11, only
// function pointers are supported, with up to three arguments.
static void print(char const* something)
{
	printf("%s\n", something);
}

static void print_something()
{
	zth::fiber(print, "something");
}

static int inc(int i)
{
	int res = i + 1;
	printf("inc(%d) = %d\n", i, res);
	return res;
}

// When you use a reference or pointer argument, make sure the argument passed to the fiber is still
// valid when the fiber runs. It may outlive the scope in which the fiber was created.
static void add(int& a, int x, int y)
{
	a = x + y;
	printf("%d = %d + %d\n", a, x, y);
}

struct Data {
	double operand;
	double result;
};

// Fibers can return data, like normal functions.
static Data* struct_add(Data* a, double b)
{
	a->result = a->operand + b;
	printf("a->operand + b = %g\n", a->result);
	return a;
}

int main_fiber(int /*argc*/, char** /*argv*/)
{
	// Note that the order in which the fibers get initialized or execute is not defined.
	zth::fiber(print_something);
	zth::fiber(print, "Hello, Fiber!");

	// If you would just start a fiber like this, you don't know when it completes.
	zth::fiber(inc, 42);

	// You probably want to wait for its result. You can do this by getting a future from the
	// fiber. Just dereference the future object to get the value.
	zth::fiber_future<int> finc = zth::fiber(inc, 100);
	printf("100 incremented is %d\n", *finc);

	// As we pass a local variable as reference argument to the fiber, we need to make sure that
	// the fiber finishes before the variable goes out of scope.
	int a = 0;
	zth::fiber_future<> f_add = zth::fiber(add, a, 1, 2);

	// Although the future type is void, we can still wait for the fiber to finish by
	// dereferencing it.
	*f_add;

	// You can also do this in one line:
	*zth::fiber(add, a, 3, 4);
	printf("Result of add is %d\n", a);

	Data data = {3.5};
	printf("Result of struct_add is %g\n", zth::fiber(struct_add, &data, 2.5)->result);

#if __cplusplus >= 201103L
	// For C++11 and up, lambda functions and functors can also be used as fiber entry points.
	zth::fiber([](char const* msg) { printf("Lambda says: %s\n", msg); }, "Hello from lambda!");

	struct Functor {
		void operator()(int n)
		{
			printf("Functor called with %d\n", n);
		}
	};
	zth::fiber(Functor(), 123);

	std::function<void()> f = []() { printf("std::function called\n"); };
	*zth::fiber(f);

	// Note that zth::fiber() by default does not create a future. The next line is valid,
	// though.  The returned object allows you to convert it to a future, but that is only safe
	// if you are sure that the fiber has not yet completed.
	auto fiber_object = zth::fiber([]() { return 42; });

	// If you want a future, in combination with auto, use the asFuture manipulator.  Now, ff2
	// is a std::fiber_future<...> object.
	auto fiber_future = zth::fiber([]() { return 84; }) << zth::asFuture();
#endif

#if __cplusplus >= 201703L
	// For C++17 and up, structured bindings are supported on futures.
	auto [data_operand, data_result] = *zth::fiber(
		[](Data& a, double d) {
			a.result = a.operand + d;
			return a;
		},
		data, 1.0);
	printf("Structured binding: data_result = %g\n", data_result);
#endif

	return 0;
}
