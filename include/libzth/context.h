#ifndef __ZTH_CONTEXT
#define __ZTH_CONTEXT

#ifdef __cplusplus
namespace zth {
	class Context;

	Context* context_create();
	void context_switch(Context* from, Context* to);
	void context_destroy(Context* context);
} // namespace
#endif // __cplusplus
#endif // __ZTH_CONTEXT
