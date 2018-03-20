#ifndef _LOG_H_
#define _LOG_H_

#include <stdarg.h>

#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3

static inline void serverLog(int level, const char *fmt, ...) {
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	printf("[log-%d]:%s\n",level, msg);
}


#endif