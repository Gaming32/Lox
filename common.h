#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALWAYS_SHOW_BANNER

#ifdef DEBUG_ALL
    #define DEBUG_PRINT_CODE
    #define DEBUG_TRACE_EXECUTION

    #define DEBUG_STRESS_GC
    #define DEBUG_LOG_GC
#endif
#ifdef DEBUG_LOG_ALL
    #define DEBUG_PRINT_CODE
    #define DEBUG_TRACE_EXECUTION
    #define DEBUG_LOG_GC
#endif
#ifdef LOX_DEBUG
    #define DEBUG_PRINT_CODE
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif
