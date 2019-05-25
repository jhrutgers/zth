#ifndef __ZTH_VERSION_H
#define __ZTH_VERSION_H

#include <libzth/util.h>

#define ZTH_VERSION_MAJOR 0
#define ZTH_VERSION_MINOR 1
#define ZTH_VERSION_PATCH 0
#define ZTH_VERSION_SUFFIX ""

#define ZTH_VERSION ZTH_STRINGIFY(ZTH_VERSION_MAJOR) "." ZTH_STRINGIFY(ZTH_VERSION_MINOR) "." ZTH_STRINGIFY(ZTH_VERSION_PATCH) ZTH_VERSION_SUFFIX

#ifndef __cplusplus
namespace zth {
	inline char const* version() {
		return ZTH_VERSION;
	}
}
#endif

#endif // _ZTH_VERSION_H
