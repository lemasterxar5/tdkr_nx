/* tdkr_libc.c -- the few libc imports of libKRAS.so that the runtime has no
 * shim for and newlib cannot serve as it is. MIT.
 *
 *   rewind        a bionic FILE is not a newlib FILE: through b_fseek
 *   wcscmp/wcscpy bionic's wchar_t is 32 bits (the runtime's b_wcs* use
 *                 uint32_t); newlib's is not guaranteed to be
 */
#include <stdint.h>
#include <stdio.h>

int b_fseek(void *fp, long off, int whence);
void b_clearerr(void *fp);

void b_rewind(void *fp) {
  b_fseek(fp, 0, SEEK_SET);
  b_clearerr(fp);
}

int b_wcscmp(const uint32_t *a, const uint32_t *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (*a > *b) - (*a < *b);
}

uint32_t *b_wcscpy(uint32_t *dst, const uint32_t *src) {
  uint32_t *d = dst;
  while ((*d++ = *src++))
    ;
  return dst;
}
