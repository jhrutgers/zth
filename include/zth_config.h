// Default zth configuration. Copy this file, and override defaults in the
// Config class below.  Make sure it is in your include path, and zth will use
// your config file.  For CMake, so something like this:
//
//   target_include_directories(libzth BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/include)

#ifndef ZTH_CONFIG_H
#	error Do not include this file directly, include <zth> instead.
#endif

#ifdef __cplusplus
namespace zth {

/*!
 * \brief The configuration of Zth.
 *
 * To override the default values, make sure your \c zth_config.h file is in
 * the include directory list before the default paths.
 *
 * \ingroup zth_api_cpp_config
 */
struct Config : public DefaultConfig {
	// Override defaults from DefaultConfig here for your local setup.
	// static size_t const DefaultFiberStackSize = 0x2000;
};

} // namespace zth
#endif // __cplusplus
