#include <zth>

/*
 * This example shows a few stack-related features.  It is designed for
 * bare-metal ARM Cortex-M4, but it compiles fine on other systems, but the MPU
 * functionality is stripped out in that case.
 */

/*
 * The first example shows how to switch between stacks.  By default the MSP is
 * used for the main worker, and all fibers run on their PSP.  One can switch
 * the stack a fiber is using to another memory region, e.g. malloced(), or
 * switch back to the MSP. The reason to use an other stack can be that the
 * default allocated stack is kept small, but when a fiber needs sporadically
 * more stack, one can use a big stack for a short while. This way, system
 * resources can be used more efficiently.
 *
 * For this, a function call must be wrapped in zth::stack_switch().  Functions
 * of up to 3 arguments are supported for pre-C++11.  C++11 and later support
 * any number of arguments.
 *
 * When no stack region is specified, stack==nullptr, the MSP is used.  In that
 * case, context switching between fibers is disabled, as no one else can use
 * the MSP in that case. When the function returns to the previous stack,
 * context switching is enabled again.
 */

// Define some alternate stacks.
static char altstack[0x1000];
static char altstack2[0x1000];

// Define a few different functions.
static void vf0() noexcept {}

static int f0() noexcept
{
	return 1;
}

static void vf1(int i) noexcept
{
	printf("%d\n", i);
}

static int f1(int /*i*/) noexcept
{
	return 1;
}

static void vf2(int i, double d) noexcept
{
	printf("%d %d\n", i, (int)(d * 100.0));
}

static int f2(int /*i*/, double /*d*/) noexcept
{
	// Switch stack again.
	zth::stack_switch(altstack2, sizeof(altstack), &vf2, 10, 11.1);
	return 1;
}

static void vf3(int i, double d, void* p) noexcept
{
	printf("%d %d %p\n", i, (int)(d * 100.0), p);
}

static int f3(int /*i*/, double /*d*/, void* /*p*/) noexcept
{
	return 1;
}

#if __cplusplus >= 201103L
static void vf4(int i, double d, void* p, int i2) noexcept
{
	printf("%d %d %p %d\n", i, (int)(d * 100.0), p, i2);
}

static int f4(int /*i*/, double /*d*/, void* /*p*/, int /*i2*/) noexcept
{
	return 1;
}
#endif

static void example_stack_switch()
{
	// Normal function call, running on this stack.
	vf0();

	zth::stack_watermark_init(altstack, sizeof(altstack));

	// All functions below are executed using altstack as stack.
	zth::stack_switch(altstack, sizeof(altstack), &vf0);
	zth::stack_switch(altstack, sizeof(altstack), &f0);
	zth::stack_switch(altstack, sizeof(altstack), &vf1, 1);
	zth::stack_switch(altstack, sizeof(altstack), &f1, 1);
	zth::stack_switch(altstack, sizeof(altstack), &vf2, 1, 3.14);
	zth::stack_switch(altstack, sizeof(altstack), &f2, 1, 3.14);
	zth::stack_switch(altstack2, sizeof(altstack2), &vf3, 1, 3.14, (void*)nullptr);
	// The following function runs on the MSP.
	zth::stack_switch(nullptr, 0, &f3, 1, 3.14, (void*)nullptr);
#if __cplusplus >= 201103L
	zth::stack_switch(altstack, sizeof(altstack), &vf4, 1, 3.14, (void*)nullptr, 2);
	zth::stack_switch(altstack, sizeof(altstack), &f4, 1, 3.14, (void*)nullptr, 2);
#endif

	// There is no limit in (nested) switching stacks. When the function
	// returns, the previous stack is restored.

	printf("altstack usage: 0x%x\n", (unsigned)zth::stack_watermark_maxused(altstack));
}



/*
 * The second example shows stack overflow detection.
 * If Config::EnableStackGuard is enabled, the MPU is configured to capture this.
 * At the end of the stack, a small region is marked as non-accessible. A hardfault
 * will occur if the memory is touched.
 *
 * This functionality is compatible with using zth::stack_switch; the MPU is reconfigured
 * upon every switch.
 */
static void overflow2(int i) noexcept
{ // NOLINT
	printf("overflow %d\n", i);

	// Recursive call, this will overflow the stack.
	overflow2(i + 1);
}

static void overflow1(int i) noexcept
{
	// Switch stack again.
	zth::stack_switch(altstack2, sizeof(altstack2), &overflow2, i + 1);
}

static void example_stack_overflow()
{
	// Run the test on another stack.
	zth::stack_switch(altstack, sizeof(altstack), &overflow1, 0);
}



int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	example_stack_switch();
	example_stack_overflow();
	return 0;
}
