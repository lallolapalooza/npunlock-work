/* corpus synthetic kernel: variadic function and va_arg */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

#include <stdarg.h>
__attribute__((noinline)) static unsigned sum_n(unsigned n, ...) {
  va_list ap;
  unsigned acc = 0;
  va_start(ap, n);
  for (unsigned i = 0; i < n; ++i) acc += va_arg(ap, unsigned);
  va_end(ap);
  return acc;
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = sum_n(4u, in[0], in[1], in[2], in[3]);
}
