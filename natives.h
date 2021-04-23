#ifndef clox_natives_h
#define clox_natives_h

#include "vm.h"
#include "errors.h"

#define EXPECT_ARGS(have, want) \
    if (have != want) { \
        ERR_ARGCOUNT(want, have); \
        return NULL_VAL; \
    }

void initializeNatives();

#endif
