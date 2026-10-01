/* minimal repro: compiler builtins (clz/ctz/popcount/bswap/abs) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned v = in[0];
  unsigned acc = (unsigned)__builtin_clz(v | 1u);
  acc += (unsigned)__builtin_ctz(v | 1u);
  acc += (unsigned)__builtin_popcount(v);
  acc += __builtin_bswap32(v);
  acc += (unsigned)__builtin_abs((int)v);
  out[0] = acc;
  out[1] = (unsigned)__builtin_expect(v == 0u, 0);
}
