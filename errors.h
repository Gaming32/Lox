#ifndef clox_errors_h
#define clox_errors_h

#include <stdarg.h>

#include "value.h"
#include "vm.h"

#define ERR_ARGCOUNT(expected, passed) \
    inlineError("Expected %d arguments but got %d", (expected), (passed))

#define ERR_PROPERTY(property, value) propertyError(property, value);

static inline Value inlineError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vruntimeError(format, args);
    va_end(args);
    return NULL_VAL;
}

static inline Value propertyError(ObjString* property, Value value) {
    char* errorValue;
    stringifyValue(&errorValue, value);
    return inlineError("Undefined property '%s' of '%s'.", property->chars, errorValue);
}

#endif
