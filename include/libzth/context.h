#ifndef __ZTH_CONTEXT
#define __ZTH_CONTEXT

#include <libzth/config.h>

#ifdef __cplusplus
namespace zth {
	class Context;

	struct ContextAttr {
		typedef void* EntryArg;
		typedef void(*Entry)(EntryArg);

		ContextAttr(Entry entry = NULL, EntryArg arg = EntryArg())
			: stackSize(Config::DefaultFiberStackSize)
			, entry(entry)
			, arg(arg)
		{}

		size_t stackSize;
		Entry entry;
		EntryArg arg;
	};

	int context_init();
	void context_deinit();
	int context_create(Context*& context, ContextAttr const& attr);
	void context_switch(Context* from, Context* to);
	void context_destroy(Context* context);
} // namespace
#endif // __cplusplus
#endif // __ZTH_CONTEXT
