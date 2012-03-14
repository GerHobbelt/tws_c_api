#ifndef TWSAPI_DEBUG_H_
#define TWSAPI_DEBUG_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void tws_debug_printf(void *opaque, const char *fmt, ...);

#if defined(TWS_DEBUG)
#define TWS_DEBUG_PRINTF(args)		tws_debug_printf args
#else
#define TWS_DEBUG_PRINTF(args)		
#endif


#ifdef __cplusplus
}
#endif

#endif /* TWSAPI_DEBUG_H_ */
