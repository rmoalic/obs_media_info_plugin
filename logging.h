#ifndef logging_include
#define logging_include 1
#include <obs/util/base.h>

#define log_info(format, ...) blog(LOG_INFO, LOG_PREFIX format __VA_OPT__(,) __VA_ARGS__)
#define log_debug(format, ...) blog(LOG_DEBUG, LOG_PREFIX "[%s:%d]"format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
#define log_warning(format, ...) blog(LOG_WARNING, LOG_PREFIX format __VA_OPT__(,) __VA_ARGS__)
#define log_error(format, ...) blog(LOG_ERROR, LOG_PREFIX format __VA_OPT__(,) __VA_ARGS__)

#endif
