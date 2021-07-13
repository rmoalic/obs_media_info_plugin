#ifndef logging_include
#define logging_include 1
#include <libobs/util/base.h>

#ifndef LOG_PREFIX
#error LOG_PREFIX need to be defined to user logging.h
#endif

#define log_info(format, ...) blog(LOG_INFO, LOG_PREFIX "[%s:%d]"format, __FILE__, __LINE__ , ##__VA_ARGS__)
#define log_debug(format, ...) blog(LOG_DEBUG, LOG_PREFIX "[%s:%d]"format, __FILE__, __LINE__ , ##__VA_ARGS__)
#define log_warning(format, ...) blog(LOG_WARNING, LOG_PREFIX "[%s:%d]"format, __FILE__, __LINE__ , ##__VA_ARGS__)
#define log_error(format, ...) blog(LOG_ERROR, LOG_PREFIX "[%s:%d]"format, __FILE__, __LINE__ , ##__VA_ARGS__)

#endif
