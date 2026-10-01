/* corpus synthetic kernel: recursion (non-inlined self calls) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

__attribute__((noinline)) static unsigned collatz(unsigned n) {
  if (n <= 1u) return 0u;
  if ((n & 1u) == 0u) return 1u + collatz(n >> 1);
  return 1u + collatz(3u * n + 1u);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = collatz(in[0] | 1u);
}
