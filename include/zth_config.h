// Default zth configuration. Copy this file, and override defaults in the Config class below.
// Make sure it is in your include path, and zth will use your config file.

#ifndef __ZTH_CONFIG_H
#  error Do not include this file directly, include <zth> instead.
#endif

#ifdef __cplusplus
namespace zth {
	struct Config : public DefaultConfig {
		// Override defaults from DefaultConfig here for your local setup.
	};
} // namespace
#endif // __cplusplus

