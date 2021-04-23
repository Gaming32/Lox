#ifndef clox_errors_h
#define clox_errors_h

#include <stdarg.h>

#include "value.h"
#include "vm.h"

#define ERR_ARGCOUNT(expected, passed) inlineError("Expected %d arguments but got %d", (expected), (passed))

static inline Value inlineError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vruntimeError(format, args);
    va_end(args);
    return NULL_VAL;
}

#endif
