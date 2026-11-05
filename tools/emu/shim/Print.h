// Print minimo: Arduino_GFX eredita da Print e riceve i caratteri da write(uint8_t).
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "WString.h"

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t n) {
    size_t k = 0;
    while (n--) k += write(*buf++);
    return k;
  }
  size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
  size_t print(const char *s) { return write(s); }
  size_t print(const __FlashStringHelper *s) { return print((const char *)s); }
  size_t print(const String &s) { return print(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v) { return printf("%d", v); }
  size_t print(unsigned v) { return printf("%u", v); }
  size_t print(long v) { return printf("%ld", v); }
  size_t print(unsigned long v) { return printf("%lu", v); }
  size_t println(const char *s) { return print(s) + println(); }
  size_t println() { return write((const uint8_t *)"\r\n", 2); }
  size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    return write((const uint8_t *)buf, (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1);
  }
};
