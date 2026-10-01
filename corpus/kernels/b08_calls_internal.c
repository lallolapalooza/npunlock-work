/* corpus synthetic kernel: internal function calls (non-inlined helpers) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

__attribute__((noinline)) static unsigned helper_mul(unsigned a, unsigned b) {
  return a * b + (a ^ b);
}
__attribute__((noinline)) static float helper_scale(float x, float y) {
  return x * y + 1.0f;
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = 0;
  for (unsigned i = 0; i < 8; ++i) acc = helper_mul(acc + i, in[i] | 1u);
  out[0] = acc;
  out[1] = (unsigned)helper_scale((float)acc, 0.5f);
}
