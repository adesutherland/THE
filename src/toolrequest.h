#ifndef THE_TOOLREQUEST_H
#define THE_TOOLREQUEST_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Private host request file: length-prefixed bytes, never shell syntax. */
static int tool_write_uint(FILE *f, uint32_t v) {
   unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16),
                         (unsigned char)(v >> 24)};
   return fwrite(b, 1, 4, f) == 4;
}
static int tool_read_uint(FILE *f, uint32_t *v) {
   unsigned char b[4];
   if (fread(b, 1, 4, f) != 4)
      return 0;
   *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
   return 1;
}
static int tool_write_bytes(FILE *f, const void *s, size_t n) {
   return n <= 4194304 && tool_write_uint(f, (uint32_t)n) && (!n || fwrite(s, 1, n, f) == n);
}
static char *tool_read_bytes(FILE *f, size_t *length) {
   uint32_t n;
   char *s;
   if (!tool_read_uint(f, &n) || n > 4194304)
      return NULL;
   s = malloc((size_t)n + 1);
   if (!s)
      return NULL;
   if (n && fread(s, 1, n, f) != n) {
      free(s);
      return NULL;
   }
   s[n] = 0;
   *length = n;
   return s;
}
#endif
