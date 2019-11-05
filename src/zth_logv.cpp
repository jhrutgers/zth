#include <libzth/util.h>

/*!
 * \brief Prints the given printf()-like formatted string to stdout.
 * \details This is a weak symbol. Override when required.
 * \ingroup zth_api_c_util
 */
void zth_logv(char const* fmt, va_list arg)
{
	vprintf(fmt, arg);
}

