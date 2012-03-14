#undef TWSAPI_GLOBALS
#include "twsapi.h"
#include "twsapi-debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
sample/default implementation of the TWSAPI debug print call.
*/
void tws_debug_printf(void *opaque, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
