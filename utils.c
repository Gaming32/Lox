#include "utils.h"

uint16_t decode16pointer(uint8_t* ptr) {
    return DECODE16BITS(*ptr, ptr[1]);
}
