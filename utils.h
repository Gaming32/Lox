#ifndef clox_utils_h
#define clox_utils_h

#include "common.h"

#define DECODE16BITS(a, b) \
    (((uint16_t)(a) << 8) | (b))

uint16_t decode16pointer(uint8_t* ptr);

#endif
