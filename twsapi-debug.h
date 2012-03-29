#ifndef TWSAPI_DEBUG_H_
#define TWSAPI_DEBUG_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* invoked at various locations in the twsapi code itself */
extern void tws_debug_printf(void *opaque, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
	;

#if defined(TWS_DEBUG)
#define TWS_DEBUG_PRINTF(args)		tws_debug_printf args
#else
#define TWS_DEBUG_PRINTF(args)		
#endif


/* used by the default event callback methods in callbacks.c */
extern void tws_cb_printf(void *opaque, int indent_level, const char *msg, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
	;


#ifdef __cplusplus
}
#endif

#endif /* TWSAPI_DEBUG_H_ */
