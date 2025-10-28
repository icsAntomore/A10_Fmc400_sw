// src/core1/libc_min.c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include "alt_printf.h"

#undef printf
#undef vprintf

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; ++i) p[i] = (uint8_t)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; --i) d[i-1] = s[i-1];
    }
    return dst;
}


int vprintf(const char *format, va_list args)
{
    return alt_vfprintf(DEFAULT_TERM, format, args);
}

int printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int rc = alt_vfprintf(DEFAULT_TERM, format, args);
    va_end(args);
    return rc;
}
