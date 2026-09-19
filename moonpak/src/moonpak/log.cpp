// SPDX-License-Identifier: 0BSD
#include "moonpak/log.hpp"
#include <stdarg.h>

void log(const char *fmt, ...) {
  // check for mGBA
  volatile uint16_t *reg = (volatile uint16_t *)0x4fff780;
  *reg = 0xc0de;
  if (*reg != 0x1dea) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  int len = 0;
  volatile char *here = (volatile char *)0x04fff600;

  #define putchar(c)  do {  \
      *here = c;            \
      here++;               \
      len++;                \
    } while (0)

  for (const char *p = fmt; *p != '\0' && len < 250; p++) {
    char ch = *p;
    if (ch == '%') {
      p++;
      ch = *p;
      switch (ch) {
        case '%':
          putchar('%');
          break;
        case 'x': {
          uint32_t val = va_arg(args, uint32_t);
          const char *hex = "0123456789abcdef";
          if (len < 244) {
            putchar('0');
            putchar('x');
            if (val < 256) {
              for (int i = 4; i >= 0; i -= 4) {
                putchar(hex[(val >> i) & 0xf]);
              }
            } else if (val < 65536) {
              for (int i = 12; i >= 0; i -= 4) {
                putchar(hex[(val >> i) & 0xf]);
              }
            } else {
              for (int i = 28; i >= 0; i -= 4) {
                putchar(hex[(val >> i) & 0xf]);
              }
            }
          }
          break;
        }
        default:
          putchar('%');
          putchar(ch);
          break;
      }
    } else {
      putchar(ch);
    }
  }
  va_end(args);

  #undef putchar

  *here = 0;
  *((volatile uint16_t *)0x04fff700) = 1 | 0x100;
}
