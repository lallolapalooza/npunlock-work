/* minimal repro: large zero-fill lowered to memset */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned char *dst = (unsigned char *)out;
  for (unsigned i = 0; i < 300; ++i) dst[i] = 0;
  out[80] = in[0];
}
