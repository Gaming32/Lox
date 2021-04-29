#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "utils.h"

uint16_t decode16pointer(uint8_t* ptr) {
    return DECODE16BITS(*ptr, ptr[1]);
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
    int length = vsnprintf(NULL, 0, fmt, ap);
    char* result = malloc(length + 1);
    if (result == NULL) {
        fprintf(stderr, "ERROR: Could not allocate string of length %d", length);
        return -1;
    }
    vsnprintf(result, length + 1, fmt, ap);
    *strp = result;
    return length;
}

int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return r;
}

void* revmemcpy(void *dest, const void *src, size_t len)
{
    char *d = (char*)dest + len - 1;
    const char *s = src;
    while (len--)
        *d-- = *s++;
    return dest;
}
