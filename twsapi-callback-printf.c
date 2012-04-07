#undef TWSAPI_GLOBALS
#include "twsapi.h"
#include "twsapi-debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/*
sample/default implementation of the TWSAPI event callbacks.c print call.
*/
void tws_cb_printf(void *opaque, int indent_level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
